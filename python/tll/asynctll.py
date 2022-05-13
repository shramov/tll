#!/usr/bin/env python3
# vim: sts=4 sw=4 et

from tll.logger import Logger
from tll.processor import Loop as PLoop
import tll.channel as C

import collections
import heapq
import queue
import time
import types
import weakref

class Entry:
    def __init__(self):
        self.ref = 1
        self.queue = queue.Queue()

class AsyncChannel(C.Channel):
    LOOP_KEY = '_pytll_async_loop'
    MASK = C.MsgMask.All ^ C.MsgMask.State ^ C.MsgMask.Channel

    def __init__(self, *a, **kw):
        loop = kw.pop(self.LOOP_KEY, None)
        if loop is None:
            raise ValueError("Need {} parameter".format(self.LOOP_KEY))

        C.Channel.__init__(self, *a, **kw)
        self._loop = weakref.ref(loop)
        self._result = collections.deque()
        self._result_state = collections.deque()
        self.callback_add(weakref.ref(self), mask=self.MASK | C.MsgMask.State)

    def __call__(self, c, msg):
        if msg.type == msg.Type.State:
            self._result_state.append(C.State(msg.msgid))
            if self.MASK & C.MsgMask.State:
                self._result.append(msg.clone())
        else:
            self._result.append(msg.clone())
        l = self._loop()
        if l:
            l._ticks += 1

    def open(self, *a, **kw):
        self._result.clear()
        self._result_state.clear()
        return C.Channel.open(self, *a, **kw)

    @property
    def result(self):
        return self._result

    async def recv(self, timeout=1.):
        l = self._loop()
        if not l:
            raise RuntimeError("Async TLL loop destroyed, bailing out")

        if self._result:
            return self._result.popleft()
        ts = l._timer_arm(timeout)
        try:
            while True:
                await l._wait()
                if self._result:
                    return self._result.popleft()
                if time.time() > ts:
                    raise TimeoutError("Timeout waiting for message")
        finally:
            l._timer_done(ts)

    def _filter_state(self, ignore):
        while self._result_state:
            m = self._result_state.popleft()
            if ignore is not None:
                if isinstance(ignore, (int, C.State)):
                    if m == C.State(ignore):
                        continue
                elif m in ignore:
                    continue
            return m
        return None

    async def recv_state(self, timeout=1., ignore={C.State.Opening, C.State.Closing}):
        l = self._loop()
        if not l:
            raise RuntimeError("Async TLL loop destroyed, bailing out")

        s = self._filter_state(ignore)
        if s is not None:
            return s

        ts = l._timer_arm(timeout)
        try:
            while True:
                await l._wait()
                s = self._filter_state(ignore)
                if s is not None:
                    return s
                if time.time() > ts:
                    raise TimeoutError("Timeout waiting for state")
        finally:
            l._timer_done(ts)

class Loop:
    def __init__(self, context = None, tick_interval = 0.1):
        self.context = context
        self.channels = weakref.WeakKeyDictionary()
        self.asyncchannels = weakref.WeakSet()
        self._loop = PLoop()
        self.log = Logger("tll.python.loop")
        self.tick = 0.01
        self._ticks = 0
        self._ctx = C.Context()
        self._timer = self._ctx.Channel("timer://;clock=realtime;name=asynctll;dump=scheme")
        self._loop.add(self._timer)
        self._timer_cb_ref = self._timer_cb
        self._timer.callback_add(self._timer_cb, mask=C.MsgMask.Data)
        self._timer.open("interval={}ms".format(int(1000 * tick_interval)))
        self._timer_queue = []
        self._state = C.State.Closed

    def __del__(self):
        self.destroy()

    def destroy(self):
        if self._state == C.State.Destroy:
            return
        self._state = C.State.Destroy
        self.log.debug("Destroy async helper")
        if self._timer:
            self._timer.callback_del(self._timer_cb, mask=C.MsgMask.Data)
            self._timer.close()
            self._timer = None
        for r in self.channels.keys():
            c.callback_del(self._callback, mask=C.MsgMask.All)
        self.channels = {}
        for c in self.asyncchannels:
            c.close()
        self.asyncchannels = set()

    def Channel(self, *a, **kw):
        if self.context is None:
            raise RuntimeError("Can not create channel without loop context")
        kw = dict(kw)
        kw['context'] = self.context
        kw[AsyncChannel.LOOP_KEY] = self
        c = AsyncChannel(*a, **kw)
        self.asyncchannels.add(c)
        self._loop.add(c)
        return c

    def _timer_cb(self, c, m):
        self.log.info("Timer cb")
        self._ticks += 1
        now = time.time()
        for ts in self._timer_queue:
            if ts <= now:
                self._ticks += 1
            self._timer.post({'ts':ts}, name='absolute')
            break

    def _timer_arm(self, timeout):
        ts = time.time() + timeout
        self.log.debug("Arm timer {}: {}", timeout, ts)
        if self._timer_queue == [] or self._timer_queue[0] > ts:
            self._timer.post({'ts':timeout}, name='relative')
            self._timer_queue.insert(0, ts)
        else:
            heapq.heappush(self._timer_queue, ts)
        return ts

    def _timer_done(self, ts):
        self.log.debug("Timer {} done", ts)
        if ts not in self._timer_queue:
            return
        idx = self._timer_queue.index(ts)
        self._timer_queue.pop(idx)
        if idx == 0:
            if self._timer_queue:
                self._timer.post({'ts':self._timer_queue[0]}, name='absolute')

    def channel_add(self, c):
        self.log.debug("Add channel {}", c.name)
        if c in self.asyncchannels:
            return
        if c not in self.channels:
            self._loop.add(c)
            self.channels[c] = Entry()
            c.callback_add(self._callback, mask=C.MsgMask.All)
        else:
            self.channels[c].ref += 1

    def channel_del(self, c, force=False):
        self.log.debug("Del channel {}", c.name)
        if c in self.asyncchannels:
            return
        entry  = self.channels.get(c, None)
        if entry is None:
            raise KeyError("Channel {} not processed by loop".format(c.name))
        entry.ref -= 1
        if force or entry.ref == 0:
            c.callback_del(self._callback, mask=C.MsgMask.All)
            self._loop.remove(c)
            del self.channels[c]

    async def sleep(self, timeout):
        if timeout == 0:
            return
        ts = self._timer_arm(timeout)
        while time.time() < ts:
            await self._wait()
        self._timer_done(ts)

    async def recv(self, c, timeout=1.):
        if c in self.asyncchannels:
            return await c.recv(timeout)
        return await self._recv(c, timeout)

    async def _recv(self, c, timeout):
        entry = self.channels.get(c, None)
        if entry is None:
            raise KeyError("Channel {} not processed by loop".format(c.name))

        if not entry.queue.empty():
            return entry.queue.get()

        ts = self._timer_arm(timeout)
        try:
            while True:
                if not entry.queue.empty():
                    return entry.queue.get()
                if time.time() > ts:
                    raise TimeoutError("Timeout waiting for message")
                await self._wait()
        finally:
            self._timer_done(ts)

    @types.coroutine
    def _wait(self):
        r = yield None

    def run(self, future):
        try:
            future.send(None) # Start future
            while True:
                c = self._loop.poll(0.01)
                if c is not None:
                    c.process()
                else:
                    if self._loop.pending:
                        self._loop.process()
                if self._ticks:
                    self.log.info("Ticks: {}", self._ticks)
                for _ in range(self._ticks):
                    future.send(None)
                self._ticks = 0
        except StopIteration as e:
            self.log.info("Future completed")
            return e.value

    def _callback(self, channel, msg):
        self.log.debug("Got message for {}", channel.name)
        self._ticks += 1
        if msg.type == msg.Type.State:
            if msg.msgid == channel.State.Destroy:
                self.log.debug("Removing channel {}", channel.name)
                self.channel_del(channel, force=True)
        if msg.type not in (msg.Type.Data, msg.Type.Control):
            return
        entry = self.channels.get(channel, None)
        if entry:
            entry.queue.put(msg.clone())
