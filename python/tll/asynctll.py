#!/usr/bin/env python3
# vim: sts=4 sw=4 et

from tll.logger import Logger
from tll.processor import Loop as PLoop
import tll.channel as C

import asyncio
import collections
import decorator
import heapq
import queue
import time
import types
import weakref

class Entry:
    def __init__(self):
        self.ref = 1
        self.queue = collections.deque()
        self.future = None

    def reset_future(self, f):
        self.future = None

    def pop(self):
        if self.queue:
            return self.queue.popleft()

    def feed(self, msg):
        self.queue.append(msg)

        if self.future and (m := self.pop()):
            self.future.set_result(m)
            self.future = None

    async def recv(self, timeout):
        if self.future:
            raise RuntimeError("Previous recv is not yet finished")
        if m := self.pop():
            return m

        self.future = asyncio.get_running_loop().create_future()
        self.future.add_done_callback(self.reset_future)
        try:
            return await asyncio.wait_for(self.future, timeout)
        except asyncio.TimeoutError: # Not needed for 3.11 and later
            raise TimeoutError("Timeout waiting for message")

class StateEntry(Entry):
    def __init__(self, *a, **kw):
        super().__init__(*a, **kw)
        self.ignore = set()

    def reset_future(self, f):
        super().reset_future(f)
        self.ignore = set()

    def pop(self):
        while self.queue:
            m = self.queue.popleft()
            if m not in self.ignore:
                return m

class AsyncChannel(C.Channel):
    LOOP_KEY = '_pytll_async_loop'
    MASK = C.MsgMask.All ^ C.MsgMask.State

    def __init__(self, *a, async_mask=None, **kw):
        loop = kw.pop(self.LOOP_KEY, None)
        if loop is None:
            raise ValueError("Need {} parameter".format(self.LOOP_KEY))
        self.MASK = self.MASK if async_mask is None else async_mask

        C.Channel.__init__(self, *a, **kw)
        self._loop = weakref.ref(loop)
        self._result = Entry()
        self._result_state = StateEntry()
        self.callback_add(weakref.ref(self), mask=self.MASK | C.MsgMask.State)

    def __call__(self, c, msg):
        if msg.type == msg.Type.State:
            state = C.State(msg.msgid)
            self._result_state.feed(state)
            if state in (C.State.Opening, C.State.Active):
                # Force cache scheme
                C.Channel._scheme(self, self.Type.Data)
                C.Channel._scheme(self, self.Type.Control)

            if self.MASK & C.MsgMask.State:
                self._result.feed(msg.clone())
        else:
            self._result.feed(msg.clone())

    @property
    def scheme(self):
        return self._scheme(self.Type.Data)

    @property
    def scheme_control(self):
        return self._scheme(self.Type.Control)

    def _scheme(self, t):
        if self.state in (self.State.Opening, self.State.Active):
            return C.Channel._scheme(self, t)
        if t is None:
            t = self.Type.Data
        if t not in (self.Type.Data, self.Type.Control):
            raise ValueError(f"No scheme defined for message type {t}")
        return self._scheme_cache[int(t)]

    def open(self, *a, **kw):
        self.result.clear()
        self._result_state.queue.clear()
        return C.Channel.open(self, *a, **kw)

    @property
    def result(self):
        return self._result.queue

    async def recv(self, timeout=1.):
        return await self._result.recv(timeout)

    async def recv_state(self, timeout=1., ignore={C.State.Opening, C.State.Closing}):
        if ignore is None:
            ignore = set()
        elif isinstance(ignore, (int, C.State)):
            ignore = {C.State(ignore)}
        self._result_state.ignore = ignore
        return await self._result_state.recv(timeout)

class Loop:
    def __init__(self, context = None, tick_interval = 0.1, config={}):
        self.context = context
        self.channels = weakref.WeakKeyDictionary()
        self.asyncchannels = weakref.WeakSet()
        self.log = Logger("tll.python.asynctll")
        self.tick = 0.01
        self._ticks = 0
        self._state = C.State.Closed

        config.setdefault('name', 'tll.python.asynctll/loop')
        self._loop = PLoop(config=config)

    def __del__(self):
        self.destroy()

    def destroy(self):
        if self._state == C.State.Destroy:
            return
        self._state = C.State.Destroy
        self.log.debug("Destroy async helper")
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
        kw.setdefault('context', self.context)
        kw[AsyncChannel.LOOP_KEY] = self
        c = AsyncChannel(*a, **kw)
        self.asyncchannels.add(c)
        self._loop.add(c)
        return c

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
        await asyncio.sleep(timeout)

    async def recv(self, c, timeout=1.):
        if c in self.asyncchannels:
            return await c.recv(timeout)
        entry = self.channels.get(c, None)
        if entry is None:
            raise KeyError("Channel {} not processed by loop".format(c.name))
        return await entry.recv(timeout)

    def step(self, timeout: float = 0):
        self._loop.step(timeout)

    async def _run(self, future):
        async with self:
            return await future

    async def __aenter__(self):
        asyncio.get_running_loop()._add_reader(self._loop.fd, self.step)

    async def __aexit__(self, *a):
        asyncio.get_running_loop()._remove_reader(self._loop.fd)

    def run(self, future):
        asyncio.run(self._run(future))

    def _callback(self, channel, msg):
        self.log.debug("Got message for {}", channel.name)
        if msg.type == msg.Type.State:
            if msg.msgid == channel.State.Destroy:
                self.log.debug("Removing channel {}", channel.name)
                self.channel_del(channel, force=True)
        if msg.type not in (msg.Type.Data, msg.Type.Control):
            return
        if entry := self.channels.get(channel, None):
            entry.feed(msg.clone())

@decorator.decorator
def asyncloop_run(f, asyncloop, *a, **kw):
    asyncloop.run(f(asyncloop, *a, **kw))
