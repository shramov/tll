#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import tll.channel as C
from tll.config import Config
from tll.error import TLLError
from tll.test_util import Accum

import common

from nose.tools import *
from nose import SkipTest

import os
import select
import socket

ctx = C.Context()

class Ports:
    def __init__(self):
        self.cache = {}

    def cached(self, af = socket.AF_INET, type = socket.SOCK_STREAM):
        if (af, type) not in self.cache:
            self.cache[(af, type)] = self.get(af, type)
        return self.cache[(af, type)]

    tcp4 = property(lambda s: s.cached(socket.AF_INET, socket.SOCK_STREAM))
    tcp6 = property(lambda s: s.cached(socket.AF_INET6, socket.SOCK_STREAM))

    @staticmethod
    def get(af = socket.AF_INET, type = socket.SOCK_STREAM):
        with socket.socket(af, type) as s:
            s.bind(('::1' if af == socket.AF_INET6 else '127.0.0.1', 0))
            return s.getsockname()[1]

PORTS = Ports()

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

def test_mem_free():
    s = ctx.Channel('mem://;size=1kb;name=master')
    c = ctx.Channel('mem://;name=slave;master=master')

    s.open()
    c.open()

    del s
    del c

class _test_tcp_base():
    PROTO = 'invalid-url'
    CLEANUP = []

    def setup(self):
        self.s = Accum(self.PROTO, mode='server', name='server', dump='yes', context=ctx)
        self.c = Accum(self.PROTO, mode='client', name='client', dump='yes', context=ctx)

    def teardown(self):
        self.c = None
        self.s = None
        for f in self.CLEANUP:
            if os.path.exists(f):
                os.unlink(f)

    def test(self):
        s, c = self.s, self.c

        s.open()
        assert_equals(s.state, s.State.Active)
        assert_equals(len(s.children), self._children_count())

        spoll = select.poll()
        for i in s.children:
            spoll.register(i.fd, select.POLLIN)
        cpoll = select.poll()

        c.open()
        if c.state == c.State.Opening:
            assert_equals(c.state, c.State.Opening)
            assert_equals(c.dcaps, c.DCaps.Process | c.DCaps.PollOut)
            cpoll.register(c.fd, select.POLLOUT)

            assert_equals(cpoll.poll(100), [(c.fd, select.POLLOUT)])
            c.process()

        assert_not_equals(spoll.poll(100), [])
        for i in s.children:
            i.process()

        assert_equals(c.state, c.State.Active)
        assert_equals(c.dcaps, c.DCaps.Process | c.DCaps.PollIn)

        for i in s.children:
            spoll.register(i.fd, select.POLLIN)
        cpoll.register(c.fd, select.POLLIN)

        c.post(b'xxx', seq=100)
        assert_not_equals(spoll.poll(10), [])
        for i in s.children:
            i.process()
        assert_equals([(m.data.tobytes(), m.seq) for m in s.result], [(b'xxx', 0)]) # No frame

    def test_open_fail(self):
        c = self.c
        try:
            c.open()
        except TLLError:
            return

        assert_equals(c.state, c.State.Opening)
        assert_equals(c.dcaps, c.DCaps.Process | c.DCaps.PollOut)

        poll = select.poll()
        poll.register(c.fd, select.POLLOUT)
        assert_equals(poll.poll(10), [(c.fd, select.POLLOUT | select.POLLERR | select.POLLHUP)])

        assert_raises(TLLError, c.process)
        assert_equals(c.state, c.State.Error)

    def _children_count(self): return 1

class test_tcp_unix(_test_tcp_base):
    PROTO = 'tcp://./test.sock'
    CLEANUP = ['test.sock']

class test_tcp4(_test_tcp_base):
    PROTO = 'tcp://127.0.0.1:{}'.format(PORTS.tcp4)

class test_tcp6(_test_tcp_base):
    PROTO = 'tcp://::1:5555'.format(PORTS.tcp6)

class test_tcp_any(_test_tcp_base):
    PROTO = 'tcp://localhost:5555'.format(PORTS.tcp4)

    def _children_count(self):
        import socket
        return len(socket.getaddrinfo('localhost', 0, type=socket.SOCK_STREAM))
