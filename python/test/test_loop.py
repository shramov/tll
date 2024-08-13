#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import tll.channel as C
from tll.processor import Loop

from tll.error import TLLError
from tll.test_util import Accum

import pytest
import sys

def test_config():
    with pytest.raises(TypeError): Loop(config='abc')
    with pytest.raises(TLLError): Loop(config={'poll':'abc'})
    Loop(config={'poll':'yes'})
    Loop(config={'poll':'no'})

@pytest.mark.skipif(sys.platform != 'linux', reason='Event notifications are linux only')
def test_fd():
    ctx = C.Context()
    l = Loop()
    c = ctx.Channel('zero://;size=1kb;name=zero;zero.fd=yes;zero.pending=no')
    assert l.poll() == None
    l.add(c)
    assert l.poll() == None
    assert not l.pending
    c.open()
    assert c.state == c.State.Active
    assert not l.pending
    assert l.poll() == c
    assert l.poll() == c

@pytest.mark.skipif(sys.platform != 'linux', reason='Event notifications are linux-only')
def test_pending_fd():
    ctx = C.Context()
    l = Loop()
    c = ctx.Channel('zero://;size=1kb;name=zero;zero.fd=yes;zero.pending=yes')
    assert l.poll() == None
    l.add(c)
    assert l.poll() == None
    assert not l.pending
    c.open()
    assert c.state == c.State.Active
    assert l.pending
    assert l.poll() == c # First poll on zero
    assert l.poll() == None # Second poll on pending

def test_pending_nofd():
    ctx = C.Context()
    l = Loop()
    c = ctx.Channel('zero://;size=1kb;name=zero;zero.fd=no;zero.pending=yes')
    assert l.poll() == None
    l.add(c)
    assert l.poll() == None
    assert not l.pending
    c.open()
    assert c.state == c.State.Active
    assert l.pending
    assert l.poll() == None # Poll on pending

def test_nofd():
    ctx = C.Context()
    l = Loop()
    c = ctx.Channel('zero://;size=1kb;name=zero;zero.fd=no;zero.pending=no')
    assert l.poll() == None
    l.add(c)
    assert l.poll() == None
    assert not l.pending
    c.open()
    assert c.state == c.State.Active
    assert l.pending
    assert l.poll() == None # Nothing to poll
