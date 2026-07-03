#!/usr/bin/env python3
# vim: sts=4 sw=4 et

from tll.logger import Logger
from tll.processor import Loop as PLoop
import tll.channel as C

from . import common
from .common import asyncloop_run

import collections
import decorator
import heapq
import queue
import time
import types
import weakref

class CancelledError(BaseException):
    ''' Future was cancelled '''
    pass

PENDING = 0
CANCEL = 1
READY = 2

class Result:
    __slots__ = ['result', 'state']

    def __init__(self):
        self.state, self.result = PENDING, None

    def set_result(self, result):
        if self.state != PENDING:
            raise RuntimeError("Not in PENDING state")
        self.result = result
        self.state = READY

    def error(self, error):
        if self.state != PENDING:
            raise RuntimeError("Not in PENDING state")
        self.result = error
        self.state = CANCEL

    def cancel(self):
        self.error(CancelledError())

    def __await__(self):
        while self.state == PENDING:
            yield self
        if self.state == CANCEL:
            raise self.result
        return self.result

class Entry(common.Entry):
    def __init__(self, loop):
        super().__init__(loop)
        self.timer = loop._timer

    async def _recv(self, timeout):
        self.future = Result()
        if timeout is not None:
            ts = self.timer.arm(timeout, self.timeout)
        try:
            return await self.future
        finally:
            if timeout is not None:
                self.timer.done(ts)
            self.reset_future()

    def timeout(self):
        if self.future:
            self.future.error(TimeoutError("Timeout waiting for message"))
        self.reset_future()

class StateEntry(Entry, common.StateEntry):
    pass

class AsyncChannel(common.AsyncChannel):
    Entry = Entry
    StateEntry = StateEntry

    def tick(self):
        if loop := self._loop():
            loop._ticks += 1

class AsyncTimer:
    class Item(float):
        def __new__(cls, ts, cb):
            return float.__new__(cls, ts)

        def __init__(self, ts, cb):
            float.__init__(ts)
            self.cb = cb

    def __init__(self, timer, loop):
        self._timer = timer
        self._timer.callback_add(weakref.ref(self), mask=C.MsgMask.Data)
        self._timer_queue = []
        self._loop = weakref.ref(loop)

    def open(self): self._timer.open()
    def close(self):
        self._timer_queue = []
        self._timer.close()

    def __call__(self, c, m):
        now = time.time()
        ticks = 0
        while self._timer_queue:
            ts = self._timer_queue[0]
            if ts > now:
                self._timer.post({'ts':ts}, name='absolute')
                break
            ts.cb()
            ticks += 1
            self._timer_queue.pop(0)
        if loop := self._loop():
            loop._tick(ticks)

    def arm(self, timeout : float, cb):
        ts = self.Item(time.time() + timeout, cb)
        if self._timer_queue == [] or self._timer_queue[0] > ts:
            self._timer.post({'ts':timeout}, name='relative')
            self._timer_queue.insert(0, ts)
        else:
            heapq.heappush(self._timer_queue, ts)
        return ts

    def done(self, ts):
        if ts not in self._timer_queue:
            return
        idx = self._timer_queue.index(ts)
        self._timer_queue.pop(idx)
        if idx == 0 and self._timer_queue:
            self._timer.post({'ts':self._timer_queue[0]}, name='absolute')

    async def sleep(self, timeout):
        if timeout == 0:
            return
        future = Result()
        ts = self.arm(timeout, lambda: future.set_result(None))
        try:
            return await future
        finally:
            self.done(ts)

class Loop(common.Loop):
    AsyncChannel = AsyncChannel

    def __init__(self, *a, **kw):
        self._timer = None
        super().__init__(*a, **kw)
        self._ticks = 0
        self._ctx = C.Context()
        c = self._ctx.Channel("timer://;clock=realtime;name=asynctll")
        self._timer = AsyncTimer(c, self)
        self._timer.open() #"interval={}ms".format(int(1000 * tick_interval)))
        self._loop.add(self._timer._timer)

    def destroy(self):
        if self._state == C.State.Destroy:
            return
        self.log.debug("Destroy async helper")
        if self._timer:
            self._timer.close()
            self._timer = None
        super().destroy()

    def _tick(self, ticks):
        self._ticks += ticks

    async def sleep(self, timeout):
        return await self._timer.sleep(timeout)

    async def recv(self, c, timeout=1.):
        if c in self.asyncchannels:
            return await c.recv(timeout)
        return await self._recv(c, timeout)

    async def _recv(self, c, timeout):
        entry = self.channels.get(c, None)
        if entry is None:
            raise KeyError("Channel {} not processed by loop".format(c.name))

        return await entry.recv(timeout)

    def run(self, future):
        try:
            future.send(None) # Start future
            while True:
                self._loop.step(0.01)
                if self._ticks:
                    self.log.trace("Ticks: {}", self._ticks)
                for _ in range(self._ticks):
                    future.send(None)
                self._ticks = 0
        except StopIteration as e:
            self.log.debug("Future completed")
            return e.value

    def _callback(self, channel, msg):
        self._ticks += 1
        super()._callback(channel, msg)
