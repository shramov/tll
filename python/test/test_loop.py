#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import tll.channel as C
from tll.processor import Loop

from tll.error import TLLError
from tll.test_util import Accum

import common

from nose.tools import *

def test_fd():
    ctx = C.Context()
    l = Loop()
    c = ctx.Channel('zero://;size=1kb;name=zero;zero.fd=yes;zero.pending=no')
    assert_equals(l.poll(), None)
    l.add(c)
    assert_equals(l.poll(), None)
    assert_false(l.pending)
    c.open()
    assert_equals(c.state, c.State.Active)
    assert_false(l.pending)
    assert_equals(l.poll(), c)
    assert_equals(l.poll(), c)

def test_pending_fd():
    ctx = C.Context()
    l = Loop()
    c = ctx.Channel('zero://;size=1kb;name=zero;zero.fd=yes;zero.pending=yes')
    assert_equals(l.poll(), None)
    l.add(c)
    assert_equals(l.poll(), None)
    assert_false(l.pending)
    c.open()
    assert_equals(c.state, c.State.Active)
    assert_true(l.pending)
    assert_equals(l.poll(), c) # First poll on zero
    assert_equals(l.poll(), None) # Second poll on pending

def test_pending_nofd():
    ctx = C.Context()
    l = Loop()
    c = ctx.Channel('zero://;size=1kb;name=zero;zero.fd=no;zero.pending=yes')
    assert_equals(l.poll(), None)
    l.add(c)
    assert_equals(l.poll(), None)
    assert_false(l.pending)
    c.open()
    assert_equals(c.state, c.State.Active)
    assert_true(l.pending)
    assert_equals(l.poll(), None) # Poll on pending

def test_nofd():
    ctx = C.Context()
    l = Loop()
    c = ctx.Channel('zero://;size=1kb;name=zero;zero.fd=no;zero.pending=no')
    assert_equals(l.poll(), None)
    l.add(c)
    assert_equals(l.poll(), None)
    assert_false(l.pending)
    c.open()
    assert_equals(c.state, c.State.Active)
    assert_true(l.pending)
    assert_equals(l.poll(), None) # Nothing to poll
