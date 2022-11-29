#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import decorator
import pytest

from tll import asynctll
import tll.channel as C
from tll.error import TLLError
from tll.test_util import Accum

@pytest.fixture
def context():
    return C.Context()

@pytest.fixture
def asyncloop(context):
    loop = asynctll.Loop(context)
    yield loop
    loop.destroy()
    loop = None

@decorator.decorator
def asyncloop_run(f, asyncloop, *a, **kw):
    asyncloop.run(f(asyncloop, *a, **kw))

@asyncloop_run
async def test(asyncloop, tmp_path):
    common = f'stream+pub+tcp:///{tmp_path}/stream.sock;request=tcp:///{tmp_path}/request.sock;dump=frame;pub.dump=frame;request.dump=frame;storage.dump=frame'
    s = asyncloop.Channel(f'{common};storage=file:///{tmp_path}/storage.dat;name=server;mode=server')
    c = asyncloop.Channel(f'{common};name=client;mode=client;peer=test')

    assert [x.name for x in s.children] == ['server/stream', 'server/request']
    assert [x.name for x in c.children] == ['client/stream', 'client/request']

    s.open()
    assert s.state == s.State.Active # No need to wait

    s.post(b'aaa', msgid=10, seq=10)
    s.post(b'bbb', msgid=10, seq=20)
    s.post(b'ccc', msgid=10, seq=30)

    with pytest.raises(TLLError): s.post(b'ddd', msgid=10, seq=25)
    with pytest.raises(TLLError): s.post(b'ddd', msgid=10, seq=30)

    c.open(seq='20')

    m = await s.recv()
    assert m.type == m.Type.Control
    assert s.unpack(m).SCHEME.name == 'Connect'

    await asyncloop.sleep(0.01)
    assert c.children[0].state == c.State.Active
    assert c.state == c.State.Active

    m = await c.recv(0.01)
    assert (m.seq, m.msgid, m.data.tobytes()) == (20, 10, b'bbb')

    m = await c.recv(0.01)
    assert (m.seq, m.msgid, m.data.tobytes()) == (30, 10, b'ccc')

    m = await c.recv(0.01)
    assert m.type == m.Type.Control
    assert (m.seq, c.unpack(m).SCHEME.name) == (30, 'Online')

    s.post(b'ddd', msgid=10, seq=40)

    m = await c.recv(0.01)
    assert (m.seq, m.msgid, m.data.tobytes()) == (40, 10, b'ddd')

    c.close()

    m = await s.recv()
    assert m.type == m.Type.Control
    assert s.unpack(m).SCHEME.name == 'Disconnect'

    s.close()
    assert s.state == c.State.Closed

@asyncloop_run
async def test_overlapped(asyncloop, tmp_path):
    common = f'stream+pub+tcp:///{tmp_path}/stream.sock;request=tcp:///{tmp_path}/request.sock;dump=frame;pub.dump=frame;request.dump=frame;storage.dump=frame'
    s = asyncloop.Channel(f'{common};storage=file:///{tmp_path}/storage.dat;name=server;mode=server')
    c = asyncloop.Channel(f'{common};name=client;mode=client;peer=test')

    s.open()
    assert s.state == s.State.Active # No need to wait

    s.post(b'aaa', msgid=10, seq=10)
    s.post(b'bbb', msgid=10, seq=20)
    s.post(b'ccc', msgid=10, seq=30)

    c.open(seq='20')

    m = await s.recv()
    assert m.type == m.Type.Control
    assert s.unpack(m).SCHEME.name == 'Connect'

    s.post(b'ddd', msgid=10, seq=40)
    s.post(b'eee', msgid=10, seq=50)
    s.post(b'fff', msgid=10, seq=60)

    m = await c.recv(0.01)
    assert (m.seq, m.msgid, m.data.tobytes()) == (20, 10, b'bbb')

    m = await c.recv(0.01)
    assert (m.seq, m.msgid, m.data.tobytes()) == (30, 10, b'ccc')

    m = await c.recv(0.01)
    assert (m.seq, m.msgid, m.data.tobytes()) == (40, 10, b'ddd')

    m = await c.recv(0.01)
    assert (m.seq, m.msgid, m.data.tobytes()) == (50, 10, b'eee')

    m = await c.recv(0.01)
    assert (m.seq, m.msgid, m.data.tobytes()) == (60, 10, b'fff')

    m = await c.recv(0.01)
    assert m.type == m.Type.Control
    assert (m.seq, c.unpack(m).SCHEME.name) == (60, 'Online')

    s.post(b'ggg', msgid=10, seq=70)

    m = await c.recv(0.01)
    assert (m.seq, m.msgid, m.data.tobytes()) == (70, 10, b'ggg')

@asyncloop_run
async def test_recent(asyncloop, tmp_path):
    common = f'stream+pub+tcp:///{tmp_path}/stream.sock;request=tcp:///{tmp_path}/request.sock;dump=frame;pub.dump=frame;request.dump=frame;storage.dump=frame'
    s = asyncloop.Channel(f'{common};storage=file:///{tmp_path}/storage.dat;name=server;mode=server')
    c = asyncloop.Channel(f'{common};name=client;mode=client;peer=test')

    s.open()
    assert s.state == s.State.Active # No need to wait

    s.post(b'aaa', msgid=10, seq=10)
    s.post(b'bbb', msgid=10, seq=20)
    s.post(b'ccc', msgid=10, seq=30)

    c.open(seq='30')

    m = await s.recv()
    assert m.type == m.Type.Control
    assert s.unpack(m).SCHEME.name == 'Connect'

    m = await c.recv(0.01)
    assert m.type == m.Type.Control
    assert (m.seq, c.unpack(m).SCHEME.name) == (30, 'Online')

@asyncloop_run
async def test_reopen(asyncloop, tmp_path):
    common = f'stream+pub+tcp:///{tmp_path}/stream.sock;request=tcp:///{tmp_path}/request.sock;dump=frame;pub.dump=frame;request.dump=frame;storage.dump=frame'
    s = asyncloop.Channel(f'{common};storage=file:///{tmp_path}/storage.dat;name=server;mode=server')
    c = asyncloop.Channel(f'{common};name=client;mode=client;peer=test')

    s.open()
    assert s.state == s.State.Active # No need to wait

    s.post(b'aaa', msgid=10, seq=10)
    s.post(b'bbb', msgid=10, seq=20)
    s.post(b'ccc', msgid=10, seq=30)

    s.close()
    s.open()

    c.open(seq='30')

    m = await s.recv()
    assert m.type == m.Type.Control
    assert s.unpack(m).SCHEME.name == 'Connect'

    m = await c.recv(0.01)
    assert m.type == m.Type.Control
    assert (m.seq, c.unpack(m).SCHEME.name) == (30, 'Online')
