#!/usr/bin/env python3
# vim: sts=4 sw=4 et

from tll.channel.logic import Logic
import tll.channel.prefix as P

class Echo(Logic):
    PROTO = 'echo'

    def _init(self, url, master=None):
        super()._init(url, master)

        if len(self._channels.get('input', [])) != 1:
            raise RuntimeError("Need exactly one input, got: {}".format([c.name for c in self._channels.get('input', [])]))
        self._input = self._channels['input'][0]

    def _open(self, props):
        super()._open(props)

    def _logic(self, channel, msg):
        if channel != self._input:
            return
        if msg.type != msg.Type.Data:
            return
        self._input.post(msg)

class Prefix(P.Prefix):
    PROTO = 'prefix+'

    def _init(self, url, master=None):
        super()._init(url, master)

    def _open(self, props):
        super()._open(props)
