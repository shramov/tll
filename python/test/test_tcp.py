#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import tll.channel as C
from tll.error import TLLError
from tll.test_util import Accum, ports
from tll.processor import Loop
from tll.asynctll import asyncloop_run

import os
import pytest
import select
import socket
import sys
import time

ctx = C.Context()

@pytest.fixture
def context():
    return C.Context()

def check_protocol(proto):
    try:
        socket.socket(socket.AF_INET, socket.SOCK_STREAM, getattr(socket, proto)).close()
    except:
        return False
    return True

WITHOUT_MPTCP = not check_protocol('IPPROTO_MPTCP')
WITHOUT_SCTP = not check_protocol('IPPROTO_SCTP')

class _test_tcp_base:
    PROTO = 'invalid-url'
    ADDR = ('unix', 0)
    FRAME = True
    CLEANUP = []
    TIMESTAMP = False

    def setup_method(self):
        self.s = Accum(self.PROTO, mode='server', name='server', dump='yes', context=ctx, sndbuf='16kb')
        self.c = Accum(self.PROTO, mode='client', name='client', dump='yes', context=ctx, sndbuf='16kb')

    def teardown_method(self):
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

            assert cpoll.poll(100) == [(c.fd, select.POLLOUT)]
            c.process()

        assert spoll.poll(100) != []
        for i in s.children:
            i.process()

        assert c.state == c.State.Active
        assert c.dcaps == c.DCaps.Process | c.DCaps.PollIn

        assert [(m.type, m.msgid) for m in s.result] == [(C.Type.Control, s.scheme_control['Connect'].msgid)]
        host = s.unpack(s.result[0]).host
        assert host.type, host.value == self.ADDR
        addr = s.result[0].addr
        s.result = []

        for i in s.children:
            spoll.register(i.fd, select.POLLIN)
        cpoll.register(c.fd, select.POLLIN)

        c.post(b'xxx', seq=0x6ead, msgid=0x6eef)
        timestamp = time.time()
        assert spoll.poll(10) != []
        for i in s.children:
            i.process()

        assert [(m.addr, m.data.tobytes()) for m in s.result] == [(addr, b'xxx')] # No frame
        assert [(m.seq, m.msgid) for m in s.result] == [(0x6ead, 0x6eef) if self.FRAME else (0, 0)] # No frame
        if self.TIMESTAMP:
            assert s.result[-1].time.seconds == pytest.approx(timestamp, 0.001)

        s.post(b'zzzz', seq=0x6eef, msgid=0x6ead, addr=s.result[-1].addr)
        timestamp = time.time()

        assert cpoll.poll(10) != []
        c.process()

        assert [m.data.tobytes() for m in c.result] == [b'zzzz'] # No frame
        assert [(m.seq, m.msgid) for m in c.result] == [(0x6eef, 0x6ead) if self.FRAME else (0, 0)] # No frame
        if self.TIMESTAMP:
            assert s.result[-1].time.seconds == pytest.approx(timestamp, 0.001)

        s.result = []
        c.close()
        assert spoll.poll(10) != []
        for i in s.children:
            i.process()

        assert [(m.type, m.msgid, m.addr) for m in s.result] == [(C.Type.Control, s.scheme_control['Disconnect'].msgid, addr)]

    @pytest.mark.parametrize('mode', ['process', 'post'])
    def test_cleanup(self, mode):
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

            assert cpoll.poll(100) == [(c.fd, select.POLLOUT)]
            c.process()

        assert spoll.poll(100) != []
        for i in s.children:
            i.process()

        assert c.state == c.State.Active
        assert c.dcaps == c.DCaps.Process | c.DCaps.PollIn

        spoll.register(s.children[-1].fd, select.POLLIN)

        assert [(m.type, m.msgid) for m in s.result] == [(C.Type.Control, s.scheme_control['Connect'].msgid)]
        c.close()
        assert c.state == c.State.Closed

        assert s.dcaps == c.DCaps.Zero
        if mode == 'post':
            # Force post error
            with pytest.raises(TLLError):
                for i in range(100):
                    s.post(b'xxx', seq=i, addr=s.result[-1].addr)
            assert s.children[-1].state == s.State.Error
        else:
            # Poll for closed event
            assert spoll.poll(100) != []
            for i in s.children:
                i.process()
            assert s.children[-1].state == s.State.Closed
        assert s.dcaps == c.DCaps.Process | c.DCaps.Pending
        s.process()
        assert s.dcaps == c.DCaps.Zero
        assert [(m.type, m.msgid) for m in s.result[-1:]] == [(C.Type.Control, s.scheme_control['Disconnect'].msgid)]

    def test_disconnect(self):
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

            assert cpoll.poll(100) == [(c.fd, select.POLLOUT)]
            c.process()

        assert spoll.poll(100) != []
        for i in s.children:
            i.process()

        assert c.state == c.State.Active
        assert c.dcaps == c.DCaps.Process | c.DCaps.PollIn

        assert [(m.type, m.msgid) for m in s.result] == [(C.Type.Control, s.scheme_control['Connect'].msgid)]
        s.post(b'', name='Disconnect', type=s.Type.Control, addr=s.result[0].addr)

        cpoll.register(c.fd, select.POLLIN)

        assert cpoll.poll(10) != []
        c.process()

        assert c.state == c.State.Closed

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

        assert [(r[0], r[1] & select.POLLHUP) for r in poll.poll(10)] == [(c.fd, select.POLLHUP)]

        with pytest.raises(TLLError): c.process()
        assert c.state == c.State.Error

    def _children_count(self): return 1

class TestTcp4(_test_tcp_base):
    ADDR = ('ipv4', 0x0100007f)
    PROTO = 'tcp://127.0.0.1:{}'.format(ports.TCP4)

class TestTcp6(_test_tcp_base):
    ADDR = ('ipv6', socket.inet_pton(socket.AF_INET6, '::1'))
    PROTO = 'tcp://::1:{}'.format(ports.TCP6)

@pytest.mark.skipif(sys.platform != 'linux', reason='Network timestamping not supported')
class TestTcp6TS(_test_tcp_base):
    ADDR = ('ipv6', socket.inet_pton(socket.AF_INET6, '::1'))
    PROTO = 'tcp://::1:{};timestamping=yes'.format(ports.TCP6)
    TIMESTAMP = True

localhost = socket.getaddrinfo('localhost', 0, type=socket.SOCK_STREAM)
localhost_af = localhost[0][0]

class TestTcpAny(_test_tcp_base):
    ADDR = TestTcp6.ADDR if localhost_af == socket.AF_INET6 else TestTcp4.ADDR
    PROTO = 'tcp://localhost:{}'.format(ports.TCP4)

    def _children_count(self):
        import socket
        return len(localhost)

class TestTcpShort(_test_tcp_base):
    PROTO = 'tcp://./test.sock;frame=short'

class TestTcpTiny(_test_tcp_base):
    PROTO = 'tcp://./test.sock;frame=tiny'

class TestTcpNone(_test_tcp_base):
    PROTO = 'tcp://./test.sock;frame=none'
    CLEANUP = ['./test.sock']
    FRAME = False

class TestTcpUnix(_test_tcp_base):
    PROTO = 'tcp://./test.sock'
    CLEANUP = ['test.sock']

    def test_output_pending(self):
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

            assert cpoll.poll(100) == [(c.fd, select.POLLOUT)]
            c.process()

        assert spoll.poll(100) != []
        for i in s.children:
            i.process()
        for i in s.children[self._children_count():]:
            spoll.register(i.fd, select.POLLIN)

        assert c.state == c.State.Active
        assert c.dcaps == c.DCaps.Process | c.DCaps.PollIn

        assert [(m.type, m.msgid) for m in s.result] == [(C.Type.Control, s.scheme_control['Connect'].msgid)]
        addr = s.result[0].addr

        cpoll.register(c.fd, select.POLLIN)

        FULL_MSGID = c.scheme_control['WriteFull'].msgid
        READY_MSGID = c.scheme_control['WriteReady'].msgid

        for i in range(20):
            c.post(b'0123456789abcdef' * 1024, seq=i)
            if c.dcaps & c.DCaps.PollOut != 0:
                break

        assert [(m.type, m.msgid) for m in c.result[-1:]] == [(c.Type.Control, FULL_MSGID)]
        assert c.dcaps & c.DCaps.PollOut != 0
        with pytest.raises(TLLError): c.post(b'0123456789abcdef' * 1024, seq=i + 1)

        s.result = []
        assert spoll.poll(100) == [(s.children[-1].fd, select.POLLIN)]
        for j in range(i + 1):
            s.children[-1].process()
        assert [(len(m.data), m.seq) for m in s.result] == [(16 * 1024, j) for j in range(i)]
        c.process()
        assert [(m.type, m.msgid) for m in c.result[-1:]] == [(c.Type.Control, READY_MSGID)]
        s.children[-1].process()
        assert [(len(m.data), m.seq) for m in s.result] == [(16 * 1024, j) for j in range(i + 1)]

def test_prefix():
    s = Accum('tcp://./tcp.sock;mode=server;socket=lz4+tcp://;socket.tcp.dump=frame;dump=frame')
    c = Accum('lz4+tcp://./tcp.sock;mode=client')
    s.open()
    c.open()
    for x in s.children:
        x.process()
    c.process()

    assert [(m.type, m.msgid) for m in s.result] == [(C.Type.Control, s.scheme_control['Connect'].msgid)]

    c.post(b'abc' * 10, msgid=10, seq=100)
    s.children[-1].children[0].process()

    assert [(m.msgid, m.seq, m.data.tobytes()) for m in s.result[1:]] == [(10, 100, b'abc' * 10)]

def test_open_peer():
    s = Accum('tcp://./tcp.sock;mode=server;dump=frame')
    c = Accum('tcp://;mode=client')
    s.open()
    c.open('af=unix;tcp.host=tcp.sock')

    for x in s.children:
        x.process()
    c.process()

    assert [(m.type, m.msgid) for m in s.result] == [(C.Type.Control, s.scheme_control['Connect'].msgid)]


@pytest.mark.parametrize("host,af",
        [ ("./tcp.sock", "unix")
        , (f"127.0.0.1:{ports.TCP4}", "ipv4")
        , (f"::1:{ports.TCP6}", "ipv6")
        , ((f"localhost:{ports.TCP6}", f"{localhost[0][4][0]}:{ports.TCP6}"), "ipv6" if localhost_af == socket.AF_INET6 else "ipv4")
        ])
def test_client(host, af):
    if isinstance(host, tuple):
        host, result = host
    else:
        result = host
    s = Accum(f'tcp://{host};mode=server;dump=frame')

    s.open()

    assert s.config.sub('client').as_dict() == {'init': {'tll': {'proto': 'tcp', 'host': result}, 'af': af, 'mode': 'client'}}

    c = Accum(s.config.sub('client.init'), name='client')
    c.open()

@pytest.mark.parametrize("host,af",
        [ (f"127.0.0.1", "ipv4")
        , (f"::1", "ipv6")
        ])
@asyncloop_run
async def test_client_ephemeral(asyncloop, host, af):
    s = asyncloop.Channel(f'tcp://{host}:0;mode=server;dump=frame')

    s.open()

    h, port = s.config['client.init.tll.host'].rsplit(':', 1)
    assert h == host
    assert port != '0'
    assert s.config.sub('client').as_dict() == {'init': {'tll': {'proto': 'tcp', 'host': f'{host}:{port}'}, 'af': af, 'mode': 'client'}}

    c = asyncloop.Channel(s.config.sub('client.init'), name='client')
    c.open()

    assert await c.recv_state() == c.State.Active
    m = await s.recv()
    assert m.type == m.Type.Control

    c.post(b'xxx')

    m = await s.recv()
    assert m.data.tobytes() == b'xxx'

def test_client_scheme():
    scheme = 'yamls://[{name: Test, id: 10}]'
    s = Accum(f'tcp://::1:{ports.TCP6};mode=server', scheme=scheme)

    s.open()
    assert [m.name for m in s.scheme.messages] == ['Test']

    body = s.scheme.dump('yamls+gz')
    try:
        hashed = s.scheme.dump('sha256')
        assert s.config.sub('client').as_dict() == {'init': {'tll': {'proto': 'tcp', 'host': f'::1:{ports.TCP6}'}, 'af': 'ipv6', 'mode': 'client', 'scheme': hashed}, 'scheme': {hashed: body}}
    except:
        assert s.config.sub('client').as_dict() == {'init': {'tll': {'proto': 'tcp', 'host': f'::1:{ports.TCP6}'}, 'af': 'ipv6', 'mode': 'client', 'scheme': body}}

    c = Accum(s.config.sub('client.init'), name='client')
    c.open()

    assert [m.name for m in s.scheme.messages] == ['Test']

@pytest.mark.skipif(WITHOUT_MPTCP, reason="MPTCP not available")
@pytest.mark.parametrize("client", ['mptcp', 'tcp'])
@pytest.mark.parametrize("server", ['mptcp', 'tcp'])
@pytest.mark.parametrize("host", [f"127.0.0.1:{ports.TCP4}", f"::1:{ports.TCP6}", "./tcp.sock"])
def test_mptcp(host, client, server):
    s = Accum(f'tcp://{host};mode=server;dump=frame;nodelay=no', protocol=server)
    c = Accum(f'tcp://{host};mode=client;dump=frame;nodelay=no', protocol=client)

    loop = Loop()

    loop.add(s)
    loop.add(c)

    s.open()
    c.open()

    try:
        for _ in range(10):
            if s.result:
                break
            loop.step(0.001)
        addr = s.result[-1].addr
        s.result = []

        c.post(b'xxx', seq=10)
        s.post(b'zzz', seq=20, addr=addr)

        for _ in range(10):
            if s.result and c.result:
                break
            loop.step(0.001)
        assert [(m.data.tobytes(), m.seq) for m in s.result] == [(b'xxx', 10)]
        assert [(m.data.tobytes(), m.seq) for m in c.result] == [(b'zzz', 20)]
    finally:
        s.close()
        c.close()

@asyncloop_run
@pytest.mark.skipif(WITHOUT_SCTP, reason="SCTP not available")
@pytest.mark.parametrize("client", ['::1', '127.0.0.1'])
async def test_sctp(asyncloop, client):
    s = asyncloop.Channel(f'tcp://{client}:{ports.TCP6};mode=server;dump=frame', name='server', protocol='sctp')
    c = asyncloop.Channel(f'tcp://{client}:{ports.TCP6};mode=client;dump=frame', name='client', protocol='sctp')

    s.open()
    c.open()

    try:
        assert c.State.Active == await c.recv_state()

        m = await s.recv()
        addr = m.addr

        c.post(b'xxx', seq=10)
        s.post(b'zzz', seq=20, addr=addr)

        m = await s.recv()
        assert (m.data.tobytes(), m.seq) == (b'xxx', 10)

        m = await c.recv()
        assert (m.data.tobytes(), m.seq) == (b'zzz', 20)
    finally:
        s.close()
        c.close()

@asyncloop_run
async def test_send_hwm(asyncloop, tmp_path):
    base = f'tcp://{tmp_path}/server.sock;dump=frame;send-buffer-size=1mb'
    s = asyncloop.Channel(f'{base};send-buffer-hwm=512kb', mode='server', name='server')
    c = asyncloop.Channel(f'{base};send-buffer-hwm=512kb', name='client')
    craw = asyncloop.Channel(f'{base}', name='client-raw')

    s.open()
    c.open()

    m = await s.recv()
    assert m.type == m.Type.Control
    addr = m.addr

    craw.open()

    m = await s.recv()
    assert m.type == m.Type.Control

    data = b'x' * 64 * 1024
    for i in range(8):
        s.post(data, seq=i, addr=addr)
        c.post(data, seq=i)
        craw.post(data, seq=i)
        if craw.result:
            break

    for j in range(8):
        s.post(data, seq=i + j, addr=addr)
        c.post(data, seq=i + j)

    assert [(m.type, m.msgid) for m in s.result] == [(s.Type.Control, s.scheme_control.messages.WriteFull.msgid)]
    assert [(m.type, m.msgid) for m in c.result] == [(s.Type.Control, s.scheme_control.messages.WriteFull.msgid)]
