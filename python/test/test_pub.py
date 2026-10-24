#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import pytest
import yaml

from tll.asynctll import asyncloop_run
import tll.channel as C
from tll.config import Config
from tll.error import TLLError
from tll.test_util import Accum

@pytest.fixture
def context():
    return C.Context()

@pytest.fixture
def server(asyncloop, tmp_path):
    c = asyncloop.Channel(f'pub+tcp:///{tmp_path}/pub.sock', mode='server', name='server', dump='frame', size='1kb')
    yield c
    c.free()

@pytest.fixture
def client(asyncloop, tmp_path):
    c = asyncloop.Channel(f'pub+tcp:///{tmp_path}/pub.sock', mode='client', name='client', dump='frame', **{'recv-buffer-size': '64kb'})
    yield c
    c.free()

def test(server, client):
    s, c = server, client

    assert (s.caps & s.Caps.InOut) == s.Caps.Output
    assert (c.caps & c.Caps.InOut) == c.Caps.Input

    s.open()
    assert s.state == s.State.Active
    assert len(s.children) == 1
    assert s.dcaps == s.DCaps.Zero

    ss = s.children[0]
    assert ss.dcaps == ss.DCaps.Process | ss.DCaps.PollIn

    c.open()

    assert c.state == c.State.Opening
    assert len(c.children) == 0

    ss.process()
    assert len(s.children) == 2

    sc = s.children[1]
    assert sc.dcaps == sc.DCaps.Process | sc.DCaps.PollIn

    assert sc.state == c.State.Opening

    sc.process()

    assert sc.state == c.State.Active

    c.process()
    assert c.state == c.State.Active
    assert c.dcaps == c.DCaps.Process | c.DCaps.PollIn
    assert c.config['info.seq'] == '-1'

    with pytest.raises(TLLError): s.post(b'x' * (512 - 16 + 1), seq=1) # Message larger then half of buffer

    s.post(b'xxx', seq=1, msgid=10)
    c.process()

    assert c.config['info.seq'] == '1'
    assert [(m.seq, m.msgid, m.data.tobytes()) for m in c.result] == [(1, 10, b'xxx')]

    for i in range(2, 5):
        s.post(b'x' * i, seq=i, msgid=10)

    for i in range(2, 5):
        c.process()
        m = c.result[-1]
        assert c.config['info.seq'] == f'{i}'
        assert (m.seq, m.msgid, m.data.tobytes()) == (i, 10, b'x' * i)

@asyncloop_run
async def test_hello_seq(asyncloop, server, client):
    s, c = server, client

    s.open({'last-seq': '9'})
    assert (await s.recv_state()) == s.State.Active

    c.open()
    assert (await c.recv_state()) == c.State.Active
    assert c.config['info.seq'] == '9'

    for i in range(10, 20):
        s.post(b'xxx', msgid=10, seq=i)

    m = await c.recv()
    assert (m.msgid, m.seq, m.data.tobytes()) == (10, 10, b'xxx')

    c.close()
    c.open()
    assert (await c.recv_state()) == c.State.Active
    assert c.config['info.seq'] == '19'

@asyncloop_run
async def test_disconnect(asyncloop, server, client):
    s, c = server, client

    s.open()
    c.open()

    assert (await s.recv_state()) == s.State.Active
    assert (await c.recv_state()) == c.State.Active

    assert (await s.recv()).msgid == s.scheme_control['Connect'].msgid

    c.close()
    assert (await c.recv_state()) == c.State.Closed

    assert (await s.recv()).msgid == s.scheme_control['Disconnect'].msgid

@asyncloop_run
async def test_more(asyncloop, server, client):
    s, c = server, client
    s.open()
    c.open()
    assert await c.recv_state() == c.State.Active

    s.post(b'xxx', seq=1, msgid=10, flags=s.PostFlags.More)
    c.process()

    assert list(c.result) == []

    s.post(b'zzz', seq=2, msgid=10)

    m = await c.recv()
    assert (m.seq, m.msgid, m.data.tobytes()) == (1, 10, b'xxx')

    m = await c.recv()
    assert (m.seq, m.msgid, m.data.tobytes()) == (2, 10, b'zzz')

@asyncloop_run
async def test_eagain(asyncloop, tmp_path, client):
    s = asyncloop.Channel(f'pub+tcp:///{tmp_path}/pub.sock', mode='server', name='server', dump='frame', size='64kb', sndbuf='1kb')
    c = client

    s.open()
    c.open()
    assert await c.recv_state() == c.State.Active

    s.post(b'xxx', seq=1, msgid=10)

    m = await c.recv()
    assert (m.seq, m.msgid, m.data.tobytes()) == (1, 10, b'xxx')

    for i in range(2, 10):
        s.post(b'x' * 256 + b'x' * i, seq=i, msgid=10)

    for i in range(2, 5):
        c.process()
        m = c.result[-1]
        assert (m.seq, m.msgid, m.data.tobytes()) == (i, 10, b'x' * 256 + b'x' * i)
    c.result.clear()

    for i in range(5, 10):
        m = await c.recv()
        assert (m.seq, m.msgid, m.data.tobytes()) == (i, 10, b'x' * 256 + b'x' * i)

@asyncloop_run
async def test_overflow(asyncloop, tmp_path, client):
    s = asyncloop.Channel(f'pub+tcp:///{tmp_path}/pub.sock', mode='server', name='server', dump='frame', size='1kb', sndbuf='1kb')
    c = client

    s.open()
    c.open()
    assert await c.recv_state() == c.State.Active

    s.post(b'xxx', seq=1, msgid=10)

    m = await c.recv()
    assert (m.seq, m.msgid, m.data.tobytes()) == (1, 10, b'xxx')

    for i in range(2, 10):
        s.post(b'x' * 256 + b'x' * i, seq=i, msgid=10)

    for i in range(2, 6):
        m = await c.recv()
        assert (m.seq, m.msgid, m.data.tobytes()) == (i, 10, b'x' * 256 + b'x' * i)

    assert c.state == c.State.Active

    c.process()

    assert c.state == c.State.Error

@asyncloop_run
async def test_many(asyncloop, server, client):
    s, c = server, client
    s.open()
    c.open()
    assert await c.recv_state() == c.State.Active

    for i in range(0, 1000):
        data = b'z' * 16 + b'x' * (i % 100)
        s.post(data, seq=i, msgid=10)
        m = await c.recv()
        assert (m.seq, m.msgid, m.data.tobytes()) == (i, 10, data)

@asyncloop_run
async def test_cleanup(asyncloop, server, client):
    server.open()
    assert len(server.children) == 1
    client.open()
    assert await client.recv_state() == client.State.Active
    assert len(server.children) == 2
    client.close()

    for i in range(0, 100):
        server.post(b'xxx', seq=i, msgid=10)
        if server.children[-1].state != server.State.Active:
            break
    assert server.children[-1].state == server.State.Closed
    server.process()
    assert len(server.children) == 1

@asyncloop_run
async def test_client(asyncloop, server):
    server.open()

    url = server.config.get_url('client.init')
    assert url.proto == 'pub+tcp'

    c = asyncloop.Channel(url, name='client')
    c.open()
    assert await c.recv_state() == c.State.Active

    server.post(b'xxx', seq=100)

    assert (await c.recv()).seq == 100

@asyncloop_run
async def test_mem(asyncloop, tmp_path):
    s = asyncloop.Channel(f'pub+mem:///{tmp_path}/memory', mode='server', name='server', dump='frame', size='16kb')
    c = asyncloop.Channel(f'pub+mem:///{tmp_path}/memory', mode='client', name='client', dump='frame')

    assert (s.caps & s.Caps.InOut) == s.Caps.Output
    assert (c.caps & c.Caps.InOut) == c.Caps.Input

    s.open()
    c.open()
    assert await c.recv_state() == c.State.Active

    for i in range(0, 1000):
        data = b'z' * 16 + b'x' * (i % 100)
        s.post(data, seq=i, msgid=10)
        m = await c.recv(0.001)
        assert (m.seq, m.msgid, m.data.tobytes()) == (i, 10, data)

@asyncloop_run
async def test_mem_close(asyncloop, tmp_path):
    s = asyncloop.Channel(f'pub+mem:///{tmp_path}/memory', mode='server', name='server', dump='frame', size='16kb')
    c = asyncloop.Channel(f'pub+mem:///{tmp_path}/memory', mode='client', name='client', dump='frame')
    s.open()
    c.open()
    assert await c.recv_state() == c.State.Active

    for i in range(0, 10):
        s.post(b'xxx', seq=i, msgid=10)
    s.close()

    for i in range(0, 10):
        m = await c.recv(0.001)
        assert (m.seq, m.msgid, m.data.tobytes()) == (i, 10, b'xxx')

    assert c.state == c.State.Active
    c.process()
    assert c.state == c.State.Closed

@asyncloop_run
async def test_mem_inverted(asyncloop, tmp_path):
    c = asyncloop.Channel(f'pub+mem:///{tmp_path}/memory', mode='sub-server', name='server', dump='frame', size='16kb')
    s = asyncloop.Channel(f'pub+mem:///{tmp_path}/memory', mode='pub-client', name='client', dump='frame')

    c.open()
    s.open()

    assert c.state == c.State.Active
    assert c.state == s.State.Active

    s1 = asyncloop.Channel(f'pub+mem:///{tmp_path}/memory', mode='pub-client', name='p1')
    with pytest.raises(TLLError): s1.open()

    for i in range(0, 10):
        s.post(b'xxx', seq=i, msgid=10)

    s.close()
    s.open()

    for i in range(10, 20):
        s.post(b'xxx', seq=i, msgid=20)

    s.close()

    for j in range(2):
        m = await c.recv(0.001)
        assert (m.type, c.unpack(m).SCHEME.name) == (m.Type.Control, "Connect")

        for i in range(10 * j, 10 * j + 10):
            m = await c.recv(0.001)
            assert (m.seq, m.msgid, m.data.tobytes()) == (i, 10 + 10 * j, b'xxx')

        m = await c.recv(0.001)
        assert (m.type, c.unpack(m).SCHEME.name) == (m.Type.Control, "Disconnect")

@asyncloop_run
async def test_large(asyncloop, tmp_path, client):
    s = asyncloop.Channel(f'pub+tcp:///{tmp_path}/pub.sock', mode='server', name='server', dump='frame', size='16mb')
    c = client

    s.open()
    c.open()

    assert (await c.recv_state()) == c.State.Active

    for i in range(4):
        s.post(b'x' * 128 * 1024, seq=i)

    assert (await c.recv_state()) == c.State.Error

@asyncloop_run
async def test_filter_basic(asyncloop, tmp_path):
    cfg = Config.from_dict({
        'tll.proto': 'pub+tcp',
        'tll.host': tmp_path / 'pub.sock',
        'mode': 'server',
        'name': 'server',
        'dump': 'frame',
        'filter.a': '10,20',
        'filter.b': ['10', '30'],
        })
    s = asyncloop.Channel(cfg)
    clients = [
            asyncloop.Channel(f'pub+tcp://{tmp_path}/pub.sock', name='c0', dump='frame'),
            asyncloop.Channel(f'pub+tcp://{tmp_path}/pub.sock', name='c1', dump='frame', filter='a'),
            asyncloop.Channel(f'pub+tcp://{tmp_path}/pub.sock', name='c2', dump='frame', filter='b'),
        ]

    s.open()
    for c in clients:
        c.open()

    for c in clients:
        assert (await c.recv_state()) == c.State.Active

    for i in range(6):
        s.post(b'x' * 128, msgid=10 * (i % 2 + 1), seq=i, flags = 0 if i == 5 else s.PostFlags.More)

    for i in range(0, 6, 2):
        m = await clients[2].recv()
        assert (m.seq, m.msgid, m.data.tobytes()) == (i, 10, b'x' * 128)

    for c in clients[:2]:
        for i in range(0, 6, 2):
            m = await c.recv()
            assert (m.seq, m.msgid, m.data.tobytes()) == (i, 10, b'x' * 128)
            m = await c.recv()
            assert (m.seq, m.msgid, m.data.tobytes()) == (i + 1, 20, b'x' * 128)

@pytest.mark.parametrize("fstring", ['A,C', '!B', '10,30'])
@asyncloop_run
async def test_filter(asyncloop, tmp_path, fstring):
    cfg = Config.from_dict({
        'tll.proto': 'pub+tcp',
        'tll.host': tmp_path / 'pub.sock',
        'mode': 'server',
        'name': 'server',
        'dump': 'frame',
        'filter.a': fstring,
        })
    if not fstring[0].isdigit():
        cfg['scheme'] = '''yamls://
- {name: A, id: 10, fields: [{name: f0, type: uint32}]}
- {name: B, id: 20, fields: [{name: f0, type: uint32}]}
- {name: C, id: 30, fields: [{name: f0, type: uint32}]}
'''
    s = asyncloop.Channel(cfg)
    c = asyncloop.Channel(f'pub+tcp://{tmp_path}/pub.sock', name='c0', dump='frame', filter='a')

    s.open()
    c.open()

    assert (await c.recv_state()) == c.State.Active

    for i in range(6):
        s.post(b'\x08' * 4, msgid=10 * (i % 3 + 1), seq=i)

    for i in range(0, 6):
        if i % 3 == 1:
            continue
        m = await c.recv()
        assert (m.seq, m.msgid, m.data.tobytes()) == (i, 10 * (i % 3 + 1), b'\x08' * 4)

@asyncloop_run
async def test_filter_default(asyncloop, tmp_path):
    cfg = Config.from_dict({
        'tll.proto': 'pub+tcp',
        'tll.host': tmp_path / 'pub.sock',
        'mode': 'server',
        'name': 'server',
        'dump': 'frame',
        'filter.default': '!B',
        'filter.all': 'A,B,C',
        'filter-default': 'default',
        })
    cfg['scheme'] = '''yamls://
- {name: A, id: 10, fields: [{name: f0, type: uint32}]}
- {name: B, id: 20, fields: [{name: f0, type: uint32}]}
- {name: C, id: 30, fields: [{name: f0, type: uint32}]}
'''
    s = asyncloop.Channel(cfg)
    c0 = asyncloop.Channel(f'pub+tcp://{tmp_path}/pub.sock', name='c0', peer='c0', dump='frame')
    c1 = asyncloop.Channel(f'pub+tcp://{tmp_path}/pub.sock', name='c1', peer='c1', dump='frame', filter='all')

    s.open()
    c0.open()
    c1.open()

    assert (await c0.recv_state()) == c0.State.Active
    assert (await c1.recv_state()) == c1.State.Active

    for i in range(6):
        s.post(b'\x08' * 4, msgid=10 * (i % 3 + 1), seq=i)

    for i in range(0, 6):
        m = await c1.recv()
        assert (m.seq, m.msgid, m.data.tobytes()) == (i, 10 * (i % 3 + 1), b'\x08' * 4)
        if i % 3 == 1:
            continue
        m = await c0.recv()
        assert (m.seq, m.msgid, m.data.tobytes()) == (i, 10 * (i % 3 + 1), b'\x08' * 4)
