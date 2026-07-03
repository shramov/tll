#!/usr/bin/env python3
# vim: sts=4 sw=4 et

from __future__ import annotations # Drop when python 3.8 is not needed

from .base import Base
from .channel import MsgMask
from ..asynctll.bare import AsyncTimer, Entry

import enum
import time

class AsyncPostPolicy(enum.Enum):
    Queue = enum.auto()
    Function = enum.auto()
_AsyncPostPolicy = AsyncPostPolicy

class AsyncBase(Base):
    AsyncPostPolicy = _AsyncPostPolicy
    CHILD_POLICY = Base.ChildPolicy.Many
    OPEN_POLICY = Base.OpenPolicy.Manual
    ASYNC_OPEN_POLICY = Base.OpenPolicy.Auto
    ASYNC_POST_POLICY = AsyncPostPolicy.Function

    def _init(self, url, master = None):
        self._timer = AsyncTimer(self.context.Channel(self._child_url_parse('timer://;clock=realtime', 'async-timer')), self)
        self._child_add(self._timer._timer, "async-timer")
        self._tasks = []
        self._post_queue = Entry(self)

    def _open(self, cfg):
        self._timer.open()
        if self.ASYNC_OPEN_POLICY == self.OpenPolicy.Auto:
            self.state = self.State.Active
        self._tasks = [self.run(cfg)]
        self._tick_one()

    def _close(self):
        self._timer.close()
        for t in self._tasks:
            try:
                t.close()
            except:
                self.log.exception(f"Failed to close task {t}")
        self._tasks = []

    async def run(self, cfg):
        pass

    async def sleep(self, timeout):
        return await self._timer.sleep(timeout)

    def _tick(self, ticks):
        for _ in range(ticks):
            self._tick_one()

    def _tick_one(self):
        if self.state != self.State.Active:
            return
        drop = []
        for t in self._tasks:
            try:
                t.send(None)
            except StopIteration:
                if t == self._tasks[0]:
                    self.log.info("Future stopped")
                    return self.close()
                drop.append(t)
        for t in drop:
            self._tasks.remove(t)

    def _post(self, msg, flags):
        m = msg.copy()
        if self.ASYNC_POST_POLICY == AsyncPostPolicy.Queue:
            self._post_queue.feed(m)
            self._tick_one()
            return

        future = self._apost(m, flags)
        try:
            future.send(None)
        except StopIteration:
            return
        self._tasks.append(future)

    async def _post_wait(self, timeout: float | None = None):
        return await self._post_queue.recv(timeout)

    async def _apost(self, msg, flags):
        pass
