#!/usr/bin/env python
# -*- coding: utf-8 -*-
# vim: sts=4 sw=4 et

from nose.tools import *

import binascii
import weakref
import zlib

from tll.channel import Channel, MsgMask
from tll.s2b import s2b

def base64gz(s):
    return binascii.b2a_base64(zlib.compress(s2b(s))).strip().decode('utf-8')

class Accum(Channel):
    MASK = MsgMask.All ^ MsgMask.State

    def __init__(self, *a, **kw):
        Channel.__init__(self, *a, **kw)
        self.result = []
        self.callback_add(weakref.ref(self), mask=self.MASK)

    def __call__(self, c, msg):
        self.result.append(msg.clone())
