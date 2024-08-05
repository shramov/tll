#!/usr/bin/env python3
# vim: sts=4 sw=4 et

from .base import Base
from .. import stat

import time

class Logic(Base):
    PROCESS_POLICY = Base.ProcessPolicy.Never

    class Stat(stat.Base):
        FIELDS = Base.Stat.FIELDS + [stat.Group('time', stat.Unit.NS)]

    def _init(self, url, master=None):
        super()._init(url, master)
        self._channels = {}
        self._callbacks = []
        channels_unique = set()
        for k,v in url.browse('tll.channel.**'):
            if not k.startswith('tll.channel.'):
                continue
            if not v.strip():
                continue
            tag = k[len('tll.channel.'):]
            r = []
            for n in v.split(','):
                n = n.strip()
                if not n:
                    raise ValueError("Invalid channel list {}: '{}'".format(k, v))
                c = self.context.get(n)
                if not c:
                    raise ValueError("Channel '{}' not found".format(n))
                r.append(c)
                channels_unique.add(c)
            self._channels[tag] = r

        self._callback_ref = self.logic
        for c in channels_unique:
            self._callbacks.append(c.callback_add(self._callback_ref, store=False))

    def _free(self):
        for cb in self._callbacks:
            cb.finalize()
        self._callbacks = []
        self._callback_ref = None
        self._channels = {}

    def logic(self, channel, msg):
        if self.state not in (self.State.Opening, self.State.Active):
            return
        try:
            stat = self.stat
            if stat is None:
                self._logic(channel, msg)
                return
            start = time.time()
            self._logic(channel, msg)
            dt = time.time() - start
            if msg.type == msg.Type.Data:
                stat.update(time=1000000000 * dt, rx=1, rxb=len(msg.data))
        except Exception as e:
            self.log.exception("Logic callback failed")
            self.state = self.State.Error

    def _logic(self, channel, msg):
        pass

class AsyncLogic(Logic):
    def __init__(self, *a, **kw):
        super(self).__init__(*a, **kw)
        self._future = None

    def _open(self, cfg):
        self._future = self.main(cfg)
        try:
            self._future.send(None) # Start future
        except StopIteration as e:
            self.log.debug("Future completed in open")
            self._future = None
            return

    def close(self, force : bool = False):
        if self._future is not None:
            self.log.debug("Terminate future")
            self._future.send(AsyncCloseException())
        super(self).close(force)

    def _logic(self, channel, msg):
        if self._future is not None:
            self._future.send((channel, msg))

    async def main(self, cfg):
        pass
