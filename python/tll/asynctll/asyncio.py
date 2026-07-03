#!/usr/bin/env python3
# vim: sts=4 sw=4 et

from tll.logger import Logger
from tll.processor import Loop as PLoop
import tll.channel as C

from . import common
from .common import asyncloop_run

import asyncio
import collections
import decorator
import heapq
import queue
import time
import types
import weakref

class Entry(common.Entry):
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

class StateEntry(Entry, common.StateEntry):
    pass

class AsyncChannel(common.AsyncChannel):
    Entry = Entry
    StateEntry = StateEntry

class Loop(common.Loop):
    AsyncChannel = AsyncChannel

    async def sleep(self, timeout):
        await asyncio.sleep(timeout)

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
