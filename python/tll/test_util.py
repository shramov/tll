#!/usr/bin/env python
# -*- coding: utf-8 -*-
# vim: sts=4 sw=4 et

from nose.tools import *
import zlib
import binascii

from tll.s2b import s2b

def base64gz(s):
    return binascii.b2a_base64(zlib.compress(s2b(s))).strip().decode('utf-8')
