#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import tll.channel as C
from tll.processor import Loop

from tll.error import TLLError
from tll.test_util import Accum

import common

from nose.tools import *

def test():
    ctx = C.Context()
    l = Loop()
    c = ctx.Channel('zero://;size=1kb;name=zero')
    assert_equals(l.poll(), None)
    l.add(c)
    assert_equals(l.poll(), None)
    c.open()
    assert_equals(c.state, c.State.Active)
    #assert_equals(l.poll(), c) # Failing
    l.remove(c)
    assert_equals(l.poll(), None)
