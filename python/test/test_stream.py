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

    m = await c.recv()
    assert (m.seq, m.msgid, m.data.tobytes()) == (20, 10, b'bbb')

    m = await c.recv()
    assert (m.seq, m.msgid, m.data.tobytes()) == (30, 10, b'ccc')

    m = await c.recv()
    assert m.type == m.Type.Control
    assert (m.seq, c.unpack(m).SCHEME.name) == (30, 'Online')

    s.post(b'ddd', msgid=10, seq=40)

    m = await c.recv()
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

    m = await c.recv()
    assert (m.seq, m.msgid, m.data.tobytes()) == (20, 10, b'bbb')

    m = await c.recv()
    assert (m.seq, m.msgid, m.data.tobytes()) == (30, 10, b'ccc')

    m = await c.recv()
    assert (m.seq, m.msgid, m.data.tobytes()) == (40, 10, b'ddd')

    m = await c.recv()
    assert (m.seq, m.msgid, m.data.tobytes()) == (50, 10, b'eee')

    m = await c.recv()
    assert (m.seq, m.msgid, m.data.tobytes()) == (60, 10, b'fff')

    m = await c.recv()
    assert m.type == m.Type.Control
    assert (m.seq, c.unpack(m).SCHEME.name) == (60, 'Online')

    s.post(b'ggg', msgid=10, seq=70)

    m = await c.recv()
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

    m = await c.recv()
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

    m = await c.recv()
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
    s = asyncloop.Channel(f'{common};storage=file://{tmp_path}/storage.dat;name=server;mode=server;blocks=blocks://{tmp_path}/blocks.yaml')
    c = asyncloop.Channel(f'{common};name=client;mode=client;peer=test')

    s.open()
    assert s.state == s.State.Active # No need to wait

    with pytest.raises(TLLError): s.post({'type':'default'}, name='Block', type=s.Type.Control)
    s.post(b'aaa', msgid=10, seq=10)
    s.post({'type':'default'}, name='Block', type=s.Type.Control)
    assert yaml.safe_load(open(tmp_path / 'blocks.yaml')) == [{'seq': 10, 'type':'default'}]
    s.post(b'bbb', msgid=10, seq=20)
    s.post({'type':'default'}, name='Block', type=s.Type.Control)
    s.post({'type':'rare'}, name='Block', type=s.Type.Control)
    s.post(b'ccc', msgid=10, seq=30)
    s.post({'type':'last'}, name='Block', type=s.Type.Control)

    assert yaml.safe_load(open(tmp_path / 'blocks.yaml')) == [{'seq': 10, 'type':'default'}, {'seq': 20, 'type': 'default'}, {'seq': 20, 'type': 'rare'}, {'seq': 30, 'type':'last'}]

    s.close()
    s.open()

    c.open('mode=block;' + req)

    if result == []:
        m = await c.recv_state()
        assert m == c.State.Error
        return

    for seq in result[:-1]:
        m = await c.recv()
        assert (m.type, m.seq) == (m.Type.Data, seq)

    m = await c.recv()
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
        m = await c.recv()
        assert (m.type, m.seq) == (m.Type.Data, i)

    m = await c.recv()
    assert m.type == m.Type.Control
    assert (m.seq, c.unpack(m).SCHEME.name) == (9, 'Online')

@asyncloop_run
async def test_block_clear(asyncloop, tmp_path):
    common = f'stream+pub+tcp://{tmp_path}/stream.sock;request=tcp://{tmp_path}/request.sock;dump=frame;pub.dump=frame;request.dump=frame;storage.dump=frame'
    s = asyncloop.Channel(f'{common};storage=file://{tmp_path}/storage.dat;name=server;mode=server;blocks=blocks://{tmp_path}/blocks.yaml')
    c = asyncloop.Channel(f'{common};name=client;mode=client;peer=test')

    s.open()
    assert s.state == s.State.Active # No need to wait

    s.post(b'aaa', msgid=10, seq=10)
    s.post({'type':'default'}, name='Block', type=s.Type.Control)
    assert yaml.safe_load(open(tmp_path / 'blocks.yaml')) == [{'seq': 10, 'type':'default'}]
    s.post(b'bbb', msgid=10, seq=20)


    s.close()
    s.open()

    assert yaml.safe_load(open(tmp_path / 'blocks.yaml')) == [{'seq': 10, 'type':'default'}]

    for i in range(2):
        if i == 0:
            c.open(mode='block', block='0', **{'block-type': 'default'})
        else:
            c.open(mode='seq', seq='20')

        m = await c.recv(0.1)
        assert (m.type, m.seq) == (m.Type.Data, 20)

        m = await c.recv()
        assert m.type == m.Type.Control
        assert (m.seq, c.unpack(m).SCHEME.name) == (20, 'Online')

        c.close()

def test_blocks_channel(context, tmp_path):
    w = context.Channel(f'blocks://{tmp_path}/blocks.yaml;default-type=def;dir=w')
    assert w.config.get('info.seq', None) == None

    w.open()
    assert w.config['info.seq'] == '-1'

    w.post(b'', seq=10)
    assert w.config['info.seq'] == '10'
    w.post({'type':''}, seq=100, name='Block', type=w.Type.Control)
    assert yaml.safe_load(open(tmp_path / 'blocks.yaml')) == [{'seq': 10, 'type':'def'}]

    w.post({'type':'other'}, seq=10, name='Block', type=w.Type.Control)
    assert yaml.safe_load(open(tmp_path / 'blocks.yaml')) == [{'seq': 10, 'type':'def'}, {'seq': 10, 'type': 'other'}]

    w.close()
    w.open()
    assert w.config['info.seq'] == '10'

    w.post(b'', seq=20)
    w.post({'type':'def'}, name='Block', type=w.Type.Control)
    assert yaml.safe_load(open(tmp_path / 'blocks.yaml')) == [{'seq': 10, 'type':'def'}, {'seq': 10, 'type': 'other'}, {'seq': 20, 'type': 'def'}]

    r = Accum('blocks://', master=w, context=context)
    with pytest.raises(TLLError): r.open({'block': '0'}) # block-type=default
    r.close()

    with pytest.raises(TLLError): r.open({'block': '10', 'block-type':'def'})
    r.close()

    with pytest.raises(TLLError): r.open({'block': '0', 'block-type':'unknown'})
    r.close()

    r.result = []
    r.open({'block': '0', 'block-type':'def'})
    assert r.state == r.State.Closed
    assert [(m.type, m.seq, r.unpack(m).as_dict()) for m in r.result] == [(r.Type.Control, 21, {'begin': 21, 'end': 21})]

    r.result = []
    r.open({'block': '1', 'block-type':'def'})
    assert r.state == r.State.Closed
    assert [(m.type, m.seq, r.unpack(m).as_dict()) for m in r.result] == [(r.Type.Control, 11, {'begin': 11, 'end': 11})]

    r.result = []
    r.open({'block': '0', 'block-type':'other'})
    assert r.state == r.State.Closed
    assert [(m.type, m.seq, r.unpack(m).as_dict()) for m in r.result] == [(r.Type.Control, 11, {'begin': 11, 'end': 11})]

    r = Accum(f'blocks://{tmp_path}/blocks.yaml;dir=r', context=context)

    r.result = []
    r.open({'block': '0', 'block-type':'other'})
    assert r.state == r.State.Closed
    assert [(m.type, m.seq, r.unpack(m).as_dict()) for m in r.result] == [(r.Type.Control, 11, {'begin': 11, 'end': 11})]

@asyncloop_run
async def test_rotate(asyncloop, tmp_path):
    common = f'stream+pub+tcp://{tmp_path}/stream.sock;request=tcp://{tmp_path}/request.sock;dump=frame;pub.dump=frame;request.dump=frame;storage.dump=frame'
    s = asyncloop.Channel(f'{common};storage=rotate+file://{tmp_path}/storage;name=server;mode=server;blocks=blocks://{tmp_path}/blocks.yaml')
    c = asyncloop.Channel(f'{common};name=client;mode=client;peer=test')

    s.open()
    assert s.scheme_control is not None
    assert 'Rotate' in [m.name for m in s.scheme_control.messages]

    s.post(b'xxx', seq=10)
    s.post({}, name='Rotate', type=s.Type.Control)

    assert (tmp_path / "storage.10.dat").exists()
