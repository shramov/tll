#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import tll.channel as C
from tll.config import Config
from tll.error import TLLError
from tll.test_util import Accum

import common

import os
import pytest
import select
import socket
import sys

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
    with pytest.raises(TLLError): ctx.Channel('mem://;name=mem')
    defaults['mem.size'] = '1kb'
    c = ctx.Channel('mem://')
    assert c.config['state'] == 'Closed'
    del c
    del defaults['mem.size']

def test_context_scheme():
    c = ctx.Channel('null://;name=null;scheme=yamls://{}')
    c.open()
    s1 = ctx.scheme_load('yamls://{}')
    s2 = ctx.scheme_load('channel://null')
    c1 = ctx.Channel('null://;scheme=channel://null')
    c1.open()
    assert c.scheme != None
    assert c1.scheme != None
    with pytest.raises(TLLError): ctx.scheme_load('channel://unknown')
    with pytest.raises(TLLError): ctx.scheme_load('zzz://scheme')

_test_zero_params = [
    ('fd=no;pending=no', 0),
    ('fd=no;pending=yes', C.DCaps.Pending)
]
if sys.platform.startswith('linux'):
    _test_zero_params += [
        ('fd=yes;pending=no', C.DCaps.PollIn),
        ('fd=yes;pending=yes', C.DCaps.PollIn | C.DCaps.Pending),
    ]

@pytest.mark.parametrize("args,caps", _test_zero_params)
def test_zero(args, caps):
    c = Accum('zero://;' + args, size='1kb', name='server', context=ctx)
    assert c.dcaps == 0
    c.open()
    assert c.dcaps == caps | c.DCaps.Process
    if caps & c.DCaps.PollIn:
        assert c.fd != None

def test_direct():
    s = Accum('direct://', name='server', context=ctx)
    c = Accum('direct://', name='client', master=s, context=ctx)

    assert s.dcaps == 0

    s.open()
    assert s.dcaps == 0

    s.post(b'xxx', seq=10)

    assert c.result == []
    assert s.result == []
    assert s.dcaps == 0

    c.open()
    assert c.dcaps == 0

    s.post(b'yyy', seq=20)
    assert [(m.data.tobytes(), m.seq) for m in c.result], [(b'yyy' == 20)]
    assert s.result == []

    c.post(b'yyy', seq=21)
    assert [(m.data.tobytes(), m.seq) for m in c.result], [(b'yyy' == 20)]
    assert [(m.data.tobytes(), m.seq) for m in s.result], [(b'yyy' == 21)]

    c.close()
    c.result = []
    s.result = []

    s.post(b'yyy', seq=20)
    assert c.result == []
    assert s.result == []

def test_mem(fd=True, **kw):
    s = Accum('mem://;size=1kb', name='server', context=ctx, fd='yes' if fd else 'no', **kw)

    s.open()
    with pytest.raises(TLLError): s.post(b'x' * 1024)
    s.post(b'xxx', seq=10)

    c = Accum('mem://', name='client', master=s, context=ctx)

    assert c.result == []
    assert s.result == []

    c.open()

    if sys.platform.startswith('linux') and fd:
        assert c.fd != None
    else:
        assert c.fd == None

    if c.fd is not None:
        poll = select.poll()
        poll.register(c.fd, select.POLLIN)
        poll.register(s.fd, select.POLLIN)
        assert poll.poll(0), [(c.fd == select.POLLIN)]

    s.post(b'yyy', seq=20)
    c.process()
    assert s.result == []
    assert [(m.data.tobytes(), m.seq) for m in c.result], [(b'xxx' == 10)]
    if c.fd is not None:
        assert poll.poll(0), [(c.fd == select.POLLIN)]
    assert c.dcaps & c.DCaps.Pending == c.DCaps.Pending
    c.result = []

    c.process()
    assert s.result == []
    assert [(m.data.tobytes(), m.seq) for m in c.result], [(b'yyy' == 20)]
    c.result = []
    if c.fd is not None:
        assert poll.poll(0) == []

    c.process()
    assert c.result == []
    if c.fd is not None:
        assert poll.poll(0) == []

    c.post(b'yyy', seq=21)
    if c.fd is not None:
        assert poll.poll(0), [(s.fd == select.POLLIN)]
    s.process()
    assert [(m.data.tobytes(), m.seq) for m in s.result], [(b'yyy' == 21)]

def test_mem_nofd():
    test_mem(fd=False)

def test_mem_free():
    s = ctx.Channel('mem://;size=1kb;name=master')
    c = ctx.Channel('mem://;name=slave;master=master')

    s.open()
    c.open()

    del s
    del c

def check_openpty():
    try:
        m, s = os.openpty()
    except:
        return True
    os.close(m)
    os.close(s)
    return False

@pytest.mark.skipif(check_openpty(), reason='PTY not supported')
class TestSerial:
    def setup(self):
        self.m, s = os.openpty()
        try:
            self.tty = os.ttyname(s)
        finally:
            os.close(s)

    def teardown(self):
        os.close(self.m)
        self.m = None

    def test(self):
        import termios
        p = termios.tcgetattr(self.m)
        print(p)
        p[0] = p[1] = p[3] = 0
        p[4] = p[5] = termios.B9600
        termios.tcsetattr(self.m, 0, p)

        c = Accum('serial://{}'.format(self.tty), context=ctx)
        c.open()
        assert c.fd != -1

        poll = select.poll()
        poll.register(self.m, select.POLLIN)
        poll.register(c.fd, select.POLLIN)

        assert poll.poll(0) == []

        c.post(b'xxx');
        assert poll.poll(0), [(self.m == select.POLLIN)]
        assert os.read(self.m, 100) == b'xxx'

        os.write(self.m, b'data')
        assert poll.poll(0), [(c.fd == select.POLLIN)]

        c.process()
        assert [x.data.tobytes() for x in c.result] == [b'data']

class _test_tcp_base:
    PROTO = 'invalid-url'
    FRAME = True
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
        assert s.state == s.State.Active
        assert len(s.children) == self._children_count()

        spoll = select.poll()
        for i in s.children:
            spoll.register(i.fd, select.POLLIN)
        cpoll = select.poll()

        c.open()
        if c.state == c.State.Opening:
            assert c.state == c.State.Opening
            assert c.dcaps == c.DCaps.Process | c.DCaps.PollOut
            cpoll.register(c.fd, select.POLLOUT)

            assert cpoll.poll(100), [(c.fd == select.POLLOUT)]
            c.process()

        assert spoll.poll(100) != []
        for i in s.children:
            i.process()

        assert c.state == c.State.Active
        assert c.dcaps == c.DCaps.Process | c.DCaps.PollIn

        for i in s.children:
            spoll.register(i.fd, select.POLLIN)
        cpoll.register(c.fd, select.POLLIN)

        c.post(b'xxx', seq=0x6ead, msgid=0x6eef)
        assert spoll.poll(10) != []
        for i in s.children:
            i.process()
        assert [m.data.tobytes() for m in s.result] == [b'xxx'] # No frame
        assert [(m.seq, m.msgid) for m in s.result] == [(0x6ead, 0x6eef) if self.FRAME else (0, 0)] # No frame

    def test_open_fail(self):
        c = self.c
        try:
            c.open()
        except TLLError:
            return

        assert c.state == c.State.Opening
        assert c.dcaps == c.DCaps.Process | c.DCaps.PollOut

        poll = select.poll()
        poll.register(c.fd, select.POLLOUT)
        assert poll.poll(10), [(c.fd == select.POLLOUT | select.POLLERR | select.POLLHUP)]

        with pytest.raises(TLLError): c.process()
        assert c.state == c.State.Error

    def _children_count(self): return 1

class TestTcpUnix(_test_tcp_base):
    PROTO = 'tcp://./test.sock'
    CLEANUP = ['test.sock']

class TestTcp4(_test_tcp_base):
    PROTO = 'tcp://127.0.0.1:{}'.format(PORTS.tcp4)

class TestTcp6(_test_tcp_base):
    PROTO = 'tcp://::1:5555'.format(PORTS.tcp6)

class TestTcpAny(_test_tcp_base):
    PROTO = 'tcp://localhost:5555'.format(PORTS.tcp4)

    def _children_count(self):
        import socket
        return len(socket.getaddrinfo('localhost', 0, type=socket.SOCK_STREAM))

class TestTcpShort(_test_tcp_base):
    PROTO = 'tcp://./test.sock;frame=short'

class TestTcpNone(_test_tcp_base):
    PROTO = 'tcp://./test.sock;frame=none'
    CLEANUP = ['./test.sock']
    FRAME = False
