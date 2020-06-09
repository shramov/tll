#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import tll.channel as C
from tll.config import Config
from tll.error import TLLError
from tll.test_util import Accum

import common

from nose.tools import *

ctx = C.Context()

def test_defaults():
    defaults = Config()
    ctx = C.Context(defaults)

    defaults['mem.size'] = 'zzz'
    assert_raises(TLLError, ctx.Channel, 'mem://;name=mem')
    defaults['mem.size'] = '1kb'
    c = ctx.Channel('mem://')
    assert_equals(c.config['state'], 'Closed')
    del c
    del defaults['mem.size']

def test_context_scheme():
    c = ctx.Channel('null://;name=null;scheme=yamls://{}')
    c.open()
    s1 = ctx.scheme_load('yamls://{}')
    s2 = ctx.scheme_load('channel://null')
    c1 = ctx.Channel('null://;scheme=channel://null')
    c1.open()
    assert_not_equals(c.scheme, None)
    assert_not_equals(c1.scheme, None)
    assert_raises(TLLError, ctx.scheme_load, 'channel://unknown')
    assert_raises(TLLError, ctx.scheme_load, 'zzz://scheme')

def test_direct():
    s = Accum('direct://', name='server', context=ctx)
    c = Accum('direct://', name='client', master=s, context=ctx)

    assert_equals(s.dcaps, 0)

    s.open()
    assert_equals(s.dcaps, 0)

    s.post(b'xxx', seq=10)

    assert_equals(c.result, [])
    assert_equals(s.result, [])
    assert_equals(s.dcaps, 0)

    c.open()
    assert_equals(c.dcaps, 0)

    s.post(b'yyy', seq=20)
    assert_equals([(m.data.tobytes(), m.seq) for m in c.result], [(b'yyy', 20)])
    assert_equals(s.result, [])

    c.post(b'yyy', seq=21)
    assert_equals([(m.data.tobytes(), m.seq) for m in c.result], [(b'yyy', 20)])
    assert_equals([(m.data.tobytes(), m.seq) for m in s.result], [(b'yyy', 21)])

    c.close()
    c.result = []
    s.result = []

    s.post(b'yyy', seq=20)
    assert_equals(c.result, [])
    assert_equals(s.result, [])

def test_mem():
    import select

    s = Accum('mem://;size=1kb', name='server', context=ctx)

    s.open()
    assert_raises(TLLError, s.post, b'x' * 1024)
    s.post(b'xxx', seq=10)

    c = Accum('mem://', name='client', master=s, context=ctx)

    assert_equals(c.result, [])
    assert_equals(s.result, [])

    c.open()

    poll = select.poll()
    poll.register(c.fd, select.POLLIN)
    poll.register(s.fd, select.POLLIN)
    assert_equals(poll.poll(0), [(c.fd, select.POLLIN)])

    s.post(b'yyy', seq=20)
    c.process()
    assert_equals(s.result, [])
    assert_equals([(m.data.tobytes(), m.seq) for m in c.result], [(b'xxx', 10)])
    assert_equals(poll.poll(0), [(c.fd, select.POLLIN)])
    assert_equals(c.dcaps & c.DCaps.Pending, c.DCaps.Pending)
    c.result = []

    c.process()
    assert_equals(s.result, [])
    assert_equals([(m.data.tobytes(), m.seq) for m in c.result], [(b'yyy', 20)])
    c.result = []
    assert_equals(poll.poll(0), [])

    c.process()
    assert_equals(c.result, [])
    assert_equals(poll.poll(0), [])

    c.post(b'yyy', seq=21)
    assert_equals(poll.poll(0), [(s.fd, select.POLLIN)])
    s.process()
    assert_equals([(m.data.tobytes(), m.seq) for m in s.result], [(b'yyy', 21)])
