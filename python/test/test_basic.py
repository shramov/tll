#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import tll.channel as C
from tll.config import Config
from tll.error import TLLError
from tll.scheme import Scheme
from tll.stat import Method, Unit
from tll.test_util import Accum, ports
from tll.processor import Loop

import lz4.block
import os
import pytest
import select
import socket
import struct
import sys
import time

ctx = C.Context()

@pytest.fixture
def context():
    return C.Context()

def test_duplicate(context):
    c0 = context.Channel('null://;name=dup;first=yes')

    assert context.get('dup') == c0
    assert context.config.get('dup.url.first', None) == 'yes'

    c1 = context.Channel('null://;name=dup;first=no')

    assert context.get('dup') == c0
    assert context.config.get('dup.url.first', None) == 'yes'

    c1.free()
    del c1

    assert context.get('dup') == c0
    assert context.config.get('dup.url.first', None) == 'yes'

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

def test_context_scheme(context):
    c = context.Channel('null://;name=null;scheme=yamls://{}')
    c.open()
    s1 = context.scheme_load('yamls://{}')
    s2 = context.scheme_load('channel://null')
    c1 = context.Channel('null://;scheme=channel://null')
    c1.open()
    assert c.scheme != None
    assert c1.scheme != None
    with pytest.raises(TLLError): context.scheme_load('channel://unknown')
    with pytest.raises(TLLError): context.scheme_load('zzz://scheme')

def test_context_scheme_hash(context):
    SCHEME = 'yamls://[{name: Data}]'
    s0 = Scheme(SCHEME)
    assert [m.name for m in s0.messages] == ['Data']
    try:
        sha = s0.dump('sha256')
    except:
        pytest.skip('SHA256 not available for scheme')
        return
    with pytest.raises(TLLError): context.scheme_load(sha)
    s1 = context.scheme_load(SCHEME)
    s2 = context.scheme_load(sha)
    assert [m.name for m in s1.messages] == ['Data']
    assert [m.name for m in s2.messages] == ['Data']

def test_stat():
    ctx = C.Context()

    assert [x.name for x in ctx.stat_list] == []

    null = ctx.Channel('null://;stat=yes;name=null')

    null.open()

    assert [x.name for x in ctx.stat_list] == ['null']

    null_stat = list(ctx.stat_list)[0]

    assert [(f.name, f.method, f.unit) for f in null_stat.swap()] == [
            ('rx', Method.Sum, Unit.Unknown),
            ('rx', Method.Sum, Unit.Bytes),
            ('tx', Method.Sum, Unit.Unknown),
            ('tx', Method.Sum, Unit.Bytes),
    ]

    assert [(f.name, f.value) for f in null_stat.swap()] == [('rx', 0), ('rx', 0), ('tx', 0), ('tx', 0)]

    null.post(b'xxx')
    null.post(b'xxx')
    null.post(b'xxx')

    assert [(f.name, f.value) for f in null_stat.swap()] == [('rx', 0), ('rx', 0), ('tx', 3), ('tx', 9)]
    assert [(f.name, f.value) for f in null_stat.swap()] == [('rx', 0), ('rx', 0), ('tx', 0), ('tx', 0)]

    del null

    assert null_stat.name == None
    assert null_stat.swap() == None

    zero = ctx.Channel('zero://;stat=yes;name=zero;size=1kb')
    zero.open()

    assert [x.name for x in ctx.stat_list] == ['zero']

    assert null_stat.name == "zero"

    assert [(f.name, f.value) for f in null_stat.swap()] == [('rx', 0), ('rx', 0), ('tx', 0), ('tx', 0)]
    zero.process()
    zero.process()
    zero.process()
    assert [(f.name, f.value) for f in null_stat.swap()] == [('rx', 3), ('rx', 3 * 1024), ('tx', 0), ('tx', 0)]

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
    for _ in range(10):
        c.process()
    assert [m.seq for m in c.result] == list(range(10))

    c.post(b'xxxx')

def test_direct():
    s = Accum('direct://', name='server', context=ctx)
    c = Accum('direct://', name='client', master=s, context=ctx)
    with pytest.raises(TLLError): Accum('direct://', name='invalid', master=c, context=ctx)

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
    assert [(m.data.tobytes(), m.seq) for m in c.result] == [(b'yyy', 20)]
    assert s.result == []

    c.post(b'yyy', seq=21)
    assert [(m.data.tobytes(), m.seq) for m in c.result] == [(b'yyy', 20)]
    assert [(m.data.tobytes(), m.seq) for m in s.result] == [(b'yyy', 21)]

    c.close()
    c.result = []
    s.result = []

    s.post(b'yyy', seq=20)
    assert c.result == []
    assert s.result == []

def test_direct_free(context):
    s = context.Channel('direct://', name='server')
    c = context.Channel('direct://', name='client', master=s)

    s.open()
    c.open()

    s.free()
    del s

    c.close()

def test_direct_state():
    control = 'yamls://[{name: Test, id: 10}]'
    s = Accum(f'direct://;notify-state=yes;scheme-control={control}', name='server', context=ctx)
    c = Accum('direct://;notify-state=xxx', name='client', master=s, context=ctx)

    assert s.scheme_control != None
    assert {m.name: m.msgid for m in s.scheme_control.messages} == {'Test': 10, 'DirectStateUpdate': 0x7f000001}

    s.open()
    assert s.result == []

    c.open()
    assert [s.unpack(m).as_dict() for m in s.result] == [{'state': s.State.Opening}, {'state': s.State.Active}]
    s.result = []

    assert c.state == c.State.Active
    s.post(b'', type=s.Type.State, msgid=int(s.State.Error))
    assert c.state == c.State.Error

    c.close()
    assert [s.unpack(m).as_dict() for m in s.result] == [{'state': s.State.Closing}, {'state': s.State.Closed}]

def test_direct_manual_open(context):
    s = Accum(f'direct://;manual-open=xxx', name='server', context=context)
    c = Accum('direct://;manual-open=yes', name='client', master=s, context=context)

    s.open()
    c.open()

    assert c.state == c.State.Opening
    s.post(b'', type=s.Type.State, msgid=int(s.State.Active))
    assert c.state == c.State.Active

    c.close()
    assert c.state == c.State.Closed

def test_direct_scheme():
    scheme = 'yamls://[{name: Test, id: 10}]'
    s = Accum(f'direct://', name='server', context=ctx, scheme=scheme)
    c = Accum('direct://', name='client', master=s, context=ctx)

    s.open()
    c.open()

    assert c.scheme != None
    assert [m.name for m in c.scheme.messages] == ['Test']

def test_direct_autoseq(context):
    s = Accum(f'direct://;autoseq=yes', name='server', dump='frame', context=context)
    c = Accum('direct://', name='client', master=s, dump='frame', context=context)

    s.open()
    c.open()

    c.post(b'10', seq=10)
    c.post(b'20', seq=20)

    s.post(b'110', seq=10)
    s.post(b'120', seq=20)

    assert [(m.data.tobytes(), m.seq) for m in s.result] == [(b'10', 10), (b'20', 20)]
    assert [(m.data.tobytes(), m.seq) for m in c.result] == [(b'110', 0), (b'120', 1)]

@pytest.mark.parametrize("name,messages", [("stream-client", ["Online", "EndOfBlock"]), ("tcp-client", ["WriteFull", "WriteReady"])])
@pytest.mark.parametrize("state", ["yes", "no"])
@pytest.mark.parametrize("external", [True, False])
def test_direct_emulate(context, name, messages, state, external):
    control = 'yamls://[{name: Test, id: 10000}]'
    if external:
        messages = messages + ["Test"]
    if state == 'yes':
        messages = messages + ["DirectStateUpdate"]

    s = context.Channel(f'direct://;notify-state={state};scheme-control={control if external else ""};emulate-control={name}', name='server')
    client = context.Channel('direct://', name='client', master=s)
    for c in [s, client]:
        assert c.scheme_control != None
        assert sorted([m.name for m in c.scheme_control.messages]) == sorted(messages)

def test_mem(fd=True, **kw):
    s = Accum('mem://;size=1kb', name='server', context=ctx, fd='yes' if fd else 'no', **kw)

    s.open()
    with pytest.raises(TLLError): s.post(b'x' * 1024)
    s.post(b'xxx', seq=10)

    c = Accum('mem://', name='client', master=s, context=ctx, **kw)

    assert c.result == []
    assert s.result == []

    c.open()

    with pytest.raises(TLLError): c.post(b'x' * 512)

    if sys.platform.startswith('linux') and fd:
        assert c.fd != None
    else:
        assert c.fd == None

    if c.fd is not None:
        poll = select.poll()
        poll.register(c.fd, select.POLLIN)
        poll.register(s.fd, select.POLLIN)
        assert poll.poll(0) == [(c.fd, select.POLLIN)]

    s.post(b'yyy', seq=20)
    c.process()
    assert s.result == []
    assert [(m.data.tobytes(), m.seq) for m in c.result] == [(b'xxx', 10)]
    if c.fd is not None:
        assert poll.poll(0) == [(c.fd, select.POLLIN)]
    assert c.dcaps & c.DCaps.Pending == c.DCaps.Pending
    c.result = []

    c.process()
    assert s.result == []
    assert [(m.data.tobytes(), m.seq) for m in c.result] == [(b'yyy', 20)]
    c.result = []
    if c.fd is not None:
        assert poll.poll(0) == []

    c.process()
    assert c.result == []
    if c.fd is not None:
        assert poll.poll(0) == []

    c.post(b'yyy', seq=21)
    if c.fd is not None:
        assert poll.poll(0) == [(s.fd, select.POLLIN)]
    s.process()
    assert [(m.data.tobytes(), m.seq) for m in s.result] == [(b'yyy', 21)]

def test_mem_nofd():
    test_mem(fd=False)

def test_mem_full():
    test_mem(frame='full')

def test_mem_full_control():
    class ControlAccum(Accum):
        MASK = Accum.MsgMask.Control

    s = Accum('mem://;size=1kb', name='server', context=ctx, frame='full')
    c = ControlAccum('mem://', name='client', master=s, context=ctx, frame='full')

    s.open()
    c.open()

    s.post(b'xxx', type=s.Type.Control, msgid=10, seq=100, addr=0xbeef, time=1000)
    c.process()
    m = c.result[-1]
    assert (s.Type.Control, 10, 100, 0xbeef, 1000) == (m.type, m.msgid, m.seq, m.addr, m.time.value)

def test_mem_free():
    s = ctx.Channel('mem://;size=1kb;name=master')
    c = ctx.Channel('mem://;name=slave;master=master')

    s.open()
    c.open()

    del s
    del c

def test_mem_open_order(fd=True, **kw):
    s = ctx.Channel('mem://;size=1kb', name='server')
    c = ctx.Channel('mem://', name='client', master=s)

    with pytest.raises(TLLError): c.open()

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
    def setup_method(self):
        self.m, s = os.openpty()
        try:
            self.tty = os.ttyname(s)
        finally:
            os.close(s)

    def teardown_method(self):
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
        assert poll.poll(0) == [(self.m, select.POLLIN)]
        assert os.read(self.m, 100) == b'xxx'

        os.write(self.m, b'data')
        assert poll.poll(0) == [(c.fd, select.POLLIN)]

        c.process()
        assert [x.data.tobytes() for x in c.result] == [b'data']

@pytest.mark.parametrize("dir,process", [("", True), ("r", True), ("rw", True), ("w", False)])
def test_udp_caps(context, tmp_path, dir, process):
    s = context.Channel(f'udp://{tmp_path}/udp.sock', mode='server', name='server', dir=dir)
    c = context.Channel(f'udp://{tmp_path}/udp.sock', mode='client', name='client', dir=dir)

    s.open()
    c.open()

    caps = (s.DCaps.Process | s.DCaps.PollIn) if process else s.DCaps.Zero

    assert (s.dcaps & (s.DCaps.Process | s.DCaps.PollMask)) == caps
    assert (c.dcaps & (c.DCaps.Process | c.DCaps.PollMask)) == caps

class _test_udp_base:
    PROTO = 'invalid-url'
    FRAME = ('seq', 'msgid')
    TIMESTAMP = False
    CLEANUP = []

    def setup_method(self):
        self.s = Accum(self.PROTO, mode='server', name='server', dump='yes', context=ctx)
        self.c = Accum(self.PROTO, mode='client', name='client', dump='yes', context=ctx)

    def teardown_method(self):
        self.c.close()
        self.s.close()
        self.c = None
        self.s = None
        for f in self.CLEANUP:
            if os.path.exists(f):
                os.unlink(f)

    def test(self):
        s, c = self.s, self.c

        s.open()
        assert s.state == s.State.Active
        assert len(s.children) == 0

        spoll = select.poll()
        spoll.register(s.fd, select.POLLIN)

        assert s.state == s.State.Active
        assert s.dcaps == s.DCaps.Process | s.DCaps.PollIn

        c.open()

        cpoll = select.poll()
        cpoll.register(c.fd, select.POLLIN)

        assert c.state == c.State.Active
        assert len(c.children) == 0

        assert c.state == c.State.Active
        assert c.dcaps == c.DCaps.Process | c.DCaps.PollIn

        c.post(b'xxx', seq=0x6ead, msgid=0x6eef)
        timestamp = time.time()
        if self.TIMESTAMP:
            if c.result == []:
                assert cpoll.poll(10) != []
                c.process()
            assert [(m.seq, m.msgid) for m in c.result if m.type == m.Type.Control] == [(0x6ead, 10)]
            assert c.result[-1].time.seconds == pytest.approx(timestamp, 0.001)

        assert spoll.poll(10) != []
        s.process()

        assert [m.data.tobytes() for m in s.result] == [b'xxx']
        assert [m.seq for m in s.result] == [0x6ead] if 'seq' in self.FRAME else [0]
        assert [m.msgid for m in s.result] == [0x6eef] if 'msgid' in self.FRAME else [0]

        if self.TIMESTAMP:
            assert s.result[-1].time.seconds == pytest.approx(timestamp, 0.001)

        if self.CLEANUP: return # Unix sockets don't have return address
        s.post(b'zzzz', seq=0x6eef, msgid=0x6ead, addr=s.result[0].addr)

        assert cpoll.poll(10) != []
        c.process()
        assert [m.data.tobytes() for m in c.result if m.type == m.Type.Data] == [b'zzzz']
        assert [(m.seq, m.msgid) for m in c.result if m.type == m.Type.Data] == [(0x6eef, 0x6ead) if self.FRAME else (0, 0)] # No frame

class TestUdpUnix(_test_udp_base):
    PROTO = 'udp://./test.sock'
    CLEANUP = ['test.sock']

class TestUdpUnixPath(_test_udp_base):
    PROTO = 'udp://;tll.host.path=test.sock'
    CLEANUP = ['test.sock']

class TestUdp4(_test_udp_base):
    PROTO = 'udp://127.0.0.1:{}'.format(ports.UDP4)

class TestUdp4Port(_test_udp_base):
    PROTO = 'udp://;tll.host.host=127.0.0.1;tll.host.port={}'.format(ports.UDP4)

class TestUdp6(_test_udp_base):
    PROTO = 'udp://::1:{};udp.ttl=1'.format(ports.UDP6)

class TestUdpShort(_test_udp_base):
    PROTO = 'udp://./test.sock;frame=short'
    CLEANUP = ['./test.sock']

class TestUdpSeq32(_test_udp_base):
    PROTO = 'udp://./test.sock;frame=seq32'
    CLEANUP = ['./test.sock']
    FRAME = ('seq',)

class TestUdpNone(_test_udp_base):
    PROTO = 'udp://./test.sock;frame=none'
    CLEANUP = ['./test.sock']
    FRAME = []

@pytest.mark.skipif(sys.platform != 'linux', reason='Network timestamping not supported')
class TestUdpTS(_test_udp_base):
    PROTO = 'udp://::1:{};timestamping=yes;timestamping-tx=yes'.format(ports.UDP6)
    TIMESTAMP = True

@pytest.mark.multicast
class TestMUdp4(_test_udp_base):
    PROTO = 'mudp://239.255.11.12:{};loop=yes'.format(ports.UDP6)

@pytest.mark.multicast
class TestMUdp6(_test_udp_base):
    PROTO = 'mudp://ff13::beef:{};loop=yes'.format(ports.UDP6)
    #PROTO = 'mudp://ff11::beef%{iface}:{port};udp.interface={iface};udp.loop=yes'.format(port=ports.UDP6, iface='wlp3s0')

def test_lz4():
    s = Accum('lz4+direct://', name='server', context=ctx)
    c = Accum('direct://', name='client', master=s, context=ctx)

    s.open()
    c.open()

    s.post(b'xxx', seq=10)

    assert [(m.data.tobytes(), m.seq) for m in c.result] == [(lz4.block.compress(b'xxx', store_size=False), 10)]
    assert s.result == []

    c.post(lz4.block.compress(b'yyy', store_size=False), seq=21)
    assert [(m.data.tobytes(), m.seq) for m in s.result] == [(b'yyy', 21)]

    c.post(b'xxx')
    assert s.state == s.State.Error

def test_lz4_block():
    s = Accum('lz4b+direct://;dump=frame;direct.dump=yes', name='server', context=ctx)
    c = Accum('direct://', name='client', master=s, context=ctx)

    s.open()
    c.open()

    for i in range(10):
        s.post(b'xxxyyyzzzaaabbbccc', seq=i)
        s.post(b'XXXYYYZZZAAABBBCCC', seq=i)

    #assert [(m.data.tobytes(), m.seq) for m in c.result] == [(lz4.block.compress(b'xxx', store_size=False), 10)]
    assert s.result == []

    for m in c.result:
        c.post(m)
    for i in range(10):
        assert [(m.data.tobytes(), m.seq) for m in s.result[2 * i:2 * i + 2]] == [(b'xxxyyyzzzaaabbbccc', i), (b'XXXYYYZZZAAABBBCCC', i)]
    #c.post(b'xxx')
    #assert s.state == s.State.Error

@pytest.mark.fuzzy
def test_lz4_block_many():
    s = Accum('lz4b+direct://', name='server', context=ctx, block='128kb')
    c = Accum('lz4b+direct://', name='client', master=s, context=ctx, block='64kb')

    s.open()
    c.open()

    data = b'aaabbbcccdddeeefffggghhhiiijjjkkklllmmmnnnooopppqqqrrrssstttuuuvvvwwwxxxyyyzzz'
    for i in range(10000):
        s.post(data, seq=i)
        assert [(m.data.tobytes(), m.seq) for m in c.result] == [(data, i)]

        c.post(c.result[-1])
        assert [(m.data.tobytes(), m.seq) for m in s.result] == [(data, i)]

        s.result = []
        c.result = []

        data = data[1:] + data[:1]

@pytest.mark.parametrize("smin,smax", [(10, 100), (300, 400), (400, 400)])
@pytest.mark.parametrize("mode,pattern", [("random", ""), ("seq", ""), ("pattern", "0x1122334455667788")])
def test_random(smin, smax, mode, pattern):
    s = Accum(f'random+direct://;data-mode={mode};pattern={pattern};min={smin}b;max={smax}b', name='server', context=ctx, validate='yes')
    c = Accum('direct://', name='client', master=s, context=ctx)

    s.open()
    c.open()

    c.post(b'')
    c.post(b'')

    assert [m.seq for m in s.result] == [0, 1]
    for m in s.result:
        assert len(m.data) >= smin
        assert len(m.data) <= smax
        if mode == 'seq':
            assert m.data.tobytes() == bytes([x % 256 for x in range(len(m.data))])
        elif mode == 'pattern':
            p = struct.pack('Q', int(pattern, 0))
            assert m.data.tobytes() == (p * (len(m.data) // 8 + 1))[:len(m.data)]
        assert s.post(m) == 0

    if mode != 'random':
        with pytest.raises(TLLError): s.post(b'\x88\x77XXX', seq=10)

def test_ipc():
    s = Accum('ipc://', mode='server', name='server', dump='yes', context=ctx)
    c = Accum('ipc://', mode='client', name='client', dump='yes', master=s, context=ctx)

    s.open()
    c.open()

    s.process()
    assert [(m.msgid, m.type) for m in s.result] == [(10, s.Type.Control)]
    s.result = []

    with pytest.raises(TLLError): s.post(b'xxx', msgid=10, seq=100)

    c.post(b'yyy', msgid=20, seq=200)
    s.process()

    assert [(m.msgid, m.seq, m.data.tobytes()) for m in s.result] == [(20, 200, b'yyy')]

    s.post(b'zzz', msgid=30, seq=300, addr=s.result[-1].addr)
    c.process()

    assert [(m.msgid, m.seq, m.data.tobytes()) for m in c.result] == [(30, 300, b'zzz')]

    s.result = []
    c.close()
    s.process()
    assert [(m.msgid, m.type) for m in s.result] == [(20, s.Type.Control)]
    c.open()
    c.result = []

    with pytest.raises(TLLError): s.post(b'aaa', msgid=40, seq=400, addr=s.result[-1].addr)

def test_ipc_scheme(context):
    s = context.Channel('ipc://', mode='server', name='server', scheme='yamls://[{name: Server}]')
    c0 = context.Channel('ipc://', mode='client', master=s, name='c0')
    c1 = context.Channel('ipc://', mode='client', master=s, name='c1', scheme='yamls://[{name: Client}]')

    s.open()
    c0.open()
    c1.open()

    assert [m.name for m in s.scheme.messages] == ['Server']
    assert [m.name for m in c0.scheme.messages] == ['Server']
    assert [m.name for m in c1.scheme.messages] == ['Client']

    s.close()

    assert [m.name for m in c0.scheme.messages] == ['Server']

def test_ipc_broadcast():
    s = Accum('ipc://', mode='server', name='server', dump='yes', broadcast='yes', context=ctx)
    c0 = Accum('ipc://', mode='client', name='c0', dump='yes', master=s, context=ctx)
    c1 = Accum('ipc://', mode='client', name='c1', dump='yes', master=s, context=ctx)

    s.open()
    c0.open()
    c1.open()

    s.process()
    s.process()
    assert [(m.type, m.msgid) for m in s.result] == [(s.Type.Control, 10)] * 2

    s.post(b'xxx', msgid=10, seq=100)
    c0.process()
    c1.process()

    assert [(m.msgid, m.seq, m.data.tobytes()) for m in c0.result] == [(10, 100, b'xxx')]
    assert [(m.msgid, m.seq, m.data.tobytes()) for m in c1.result] == [(10, 100, b'xxx')]

def test_ipc_destroy():
    s = Accum('ipc://', mode='server', name='server', dump='yes', broadcast='yes', context=ctx)
    c = Accum('ipc://', mode='client', name='client', dump='yes', master=s, context=ctx)

    with pytest.raises(TLLError): c.open()

    c.close()

    s.open()
    c.open()

    s.process()
    assert [(m.type, m.msgid) for m in s.result] == [(s.Type.Control, 10)]

    s.free()
    c.close()
