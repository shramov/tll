#!/usr/bin/env python
# -*- coding: utf-8 -*-
# vim: sts=4 sw=4 et

import binascii
import contextlib
import socket
import weakref
import zlib

from tll.channel import Channel, MsgMask
from tll.s2b import s2b

def base64gz(s):
    return binascii.b2a_base64(zlib.compress(s2b(s))).strip().decode('utf-8')

class Accum(Channel):
    MASK = MsgMask.Data | MsgMask.Control

    def __init__(self, *a, **kw):
        Channel.__init__(self, *a, **kw)
        self.result = []
        self.callback_add(weakref.ref(self), mask=self.MASK)

    def __call__(self, c, msg):
        self.result.append(msg.clone())

class Ports:
    def __init__(self):
        self._cache = {}

    def __call__(self, af = socket.AF_INET, sock = socket.SOCK_STREAM):
        with contextlib.closing(socket.socket(af, sock)) as s:
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            s.bind(('', 0))
            return s.getsockname()[1]

    def cached(self, *args):
        key = tuple(args)
        p = self._cache.get(key, None)
        if p is None:
            p = self(*key)
            self._cache[key] = p
        return p

    @property
    def TCP4(self): return self.cached(socket.AF_INET, socket.SOCK_STREAM)

    @property
    def UDP4(self): return self.cached(socket.AF_INET, socket.SOCK_DGRAM)

    @property
    def TCP6(self): return self.cached(socket.AF_INET6, socket.SOCK_STREAM)

    @property
    def UDP6(self): return self.cached(socket.AF_INET6, socket.SOCK_DGRAM)

ports = Ports()
