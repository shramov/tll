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
            c.callback_add(self.logic)

    def logic(self, channel, msg):
        try:
            stat = self.stat
            if stat is None:
                self._logic(channel, msg)
                return
            start = time.time()
            self._logic(channel, msg)
            dt = time.time() - start
            if msg.type == msg.Type.Data:
                stat.update(time=1000000000 * dt)
        except Exception as e:
            self.log.exception("Logic callback failed")
            self.state = self.State.Error

    def _logic(self, channel, msg):
        pass
