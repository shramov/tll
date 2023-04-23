#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import decorator
import pytest
import yaml

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

    c.open(seq='20', mode='seq')

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

    c.open(seq='20', mode='seq')

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

    c.open(seq='31', mode='seq')

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

    c.open(seq='31', mode='seq')

    m = await s.recv()
    assert m.type == m.Type.Control
    assert s.unpack(m).SCHEME.name == 'Connect'

    m = await c.recv(0.01)
    assert m.type == m.Type.Control
    assert (m.seq, c.unpack(m).SCHEME.name) == (30, 'Online')

@pytest.mark.parametrize("req,result", [
        ('block=0', [30, 30]),
        ('block=0;block-type=rare', [30, 30]),
        ('block=1', [20, 30, 30]),
        ('block=1;block-type=default', [20, 30, 30]),
        ('block=0;block-type=last', [30]),
        ('block=10;block-type=rare', []),
        ('block=0;block-type=unknown', []),
        ])
@asyncloop_run
async def test_block(asyncloop, tmp_path, req, result):
    common = f'stream+pub+tcp://{tmp_path}/stream.sock;request=tcp://{tmp_path}/request.sock;dump=frame;pub.dump=frame;request.dump=frame;storage.dump=frame'
    s = asyncloop.Channel(f'{common};storage=file://{tmp_path}/storage.dat;name=server;mode=server;blocks={tmp_path}/blocks.yaml')
    c = asyncloop.Channel(f'{common};name=client;mode=client;peer=test')

    s.open()
    assert s.state == s.State.Active # No need to wait

    with pytest.raises(TLLError): s.post({'type':'default'}, name='Block', type=s.Type.Control)
    s.post(b'aaa', msgid=10, seq=10)
    s.post({'type':'default'}, name='Block', type=s.Type.Control)
    assert yaml.safe_load(open(tmp_path / 'blocks.yaml')) == [{'seq': 11, 'type':'default'}]
    s.post(b'bbb', msgid=10, seq=20)
    s.post({'type':'default'}, name='Block', type=s.Type.Control)
    s.post({'type':'rare'}, name='Block', type=s.Type.Control)
    s.post(b'ccc', msgid=10, seq=30)
    s.post({'type':'last'}, name='Block', type=s.Type.Control)

    assert yaml.safe_load(open(tmp_path / 'blocks.yaml')) == [{'seq': 11, 'type':'default'}, {'seq': 21, 'type': 'default'}, {'seq': 21, 'type': 'rare'}, {'seq': 31, 'type':'last'}]

    s.close()
    s.open()

    c.open('mode=block;' + req)

    if result == []:
        m = await c.recv_state(0.01)
        assert m == c.State.Error
        return

    for seq in result[:-1]:
        m = await c.recv(0.01)
        assert (m.type, m.seq) == (m.Type.Data, seq)

    m = await c.recv(0.01)
    assert m.type == m.Type.Control
    assert (m.seq, c.unpack(m).SCHEME.name) == (result[-1], 'Online')

@asyncloop_run
async def test_autoseq(asyncloop, tmp_path):
    common = f'stream+pub+tcp://{tmp_path}/stream.sock;request=tcp://{tmp_path}/request.sock;dump=frame;pub.dump=frame;request.dump=frame;storage.dump=frame'
    s = asyncloop.Channel(f'{common};storage=file://{tmp_path}/storage.dat;name=server;mode=server;autoseq=yes')
    c = asyncloop.Channel(f'{common};name=client;mode=client;peer=test')

    s.open()
    assert s.state == s.State.Active # No need to wait

    assert s.config['info.seq'] == '-1'
    for i in range(5):
        s.post(b'aaa' * i, seq=100)
    assert s.config['info.seq'] == '4'

    s.close()
    s.open()
    assert s.config['info.seq'] == '4'
    for i in range(5):
        s.post(b'aaa' * i, seq=100)
    assert s.config['info.seq'] == '9'

    c.open(seq='0', mode='seq')
    for i in range(10):
        m = await c.recv(0.01)
        assert (m.type, m.seq) == (m.Type.Data, i)

    m = await c.recv(0.01)
    assert m.type == m.Type.Control
    assert (m.seq, c.unpack(m).SCHEME.name) == (9, 'Online')

@asyncloop_run
async def test_block_clear(asyncloop, tmp_path):
    common = f'stream+pub+tcp://{tmp_path}/stream.sock;request=tcp://{tmp_path}/request.sock;dump=frame;pub.dump=frame;request.dump=frame;storage.dump=frame'
    s = asyncloop.Channel(f'{common};storage=file://{tmp_path}/storage.dat;name=server;mode=server;blocks={tmp_path}/blocks.yaml')
    c = asyncloop.Channel(f'{common};name=client;mode=client;peer=test')

    s.open()
    assert s.state == s.State.Active # No need to wait

    s.post(b'aaa', msgid=10, seq=10)
    s.post({'type':'default'}, name='Block', type=s.Type.Control)
    assert yaml.safe_load(open(tmp_path / 'blocks.yaml')) == [{'seq': 11, 'type':'default'}]
    s.post(b'bbb', msgid=10, seq=20)


    s.close()
    s.open()

    assert yaml.safe_load(open(tmp_path / 'blocks.yaml')) == [{'seq': 11, 'type':'default'}]

    for i in range(2):
        if i == 0:
            c.open(mode='block', block='0', **{'block-type': 'default'})
        else:
            c.open(mode='seq', seq='20')

        m = await c.recv(0.1)
        assert (m.type, m.seq) == (m.Type.Data, 20)

        m = await c.recv(0.01)
        assert m.type == m.Type.Control
        assert (m.seq, c.unpack(m).SCHEME.name) == (20, 'Online')

        c.close()
