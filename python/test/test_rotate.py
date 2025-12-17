#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import os

import pytest

from tll.asynctll import asyncloop_run
from tll.channel import Context, MsgMask
from tll.config import Url
from tll.error import TLLError
from tll.test_util import Accum

@pytest.fixture
def context():
    return Context()

@asyncloop_run
async def test_basic(asyncloop, tmp_path):
    w = asyncloop.Channel(f'rotate+file://{tmp_path}/rotate;filename-key=last', dir='w', name='write', dump='frame')
    r = asyncloop.Channel(f'rotate+file://{tmp_path}/rotate;file.dump=frame', dir='r', name='read', master=w, dump='frame', autoclose='no')

    assert w.scheme_load(w.Type.Control) is not None
    assert [m.name for m in w.scheme_control.messages] == ['Rotate']

    assert r.scheme_load(w.Type.Control) is not None
    assert [m.name for m in r.scheme_control.messages] == ['Seek', 'EndOfData', 'Rotate']

    assert not os.path.exists(tmp_path / 'rotate.current.dat')

    w.open()
    assert w.config['info.seq-begin'] == '-1'
    assert w.config['info.seq'] == '-1'


    assert os.path.exists(tmp_path / 'rotate.current.dat')

    for i in range(30):
        w.post(b'xxx' * (i % 10 + 1), seq=i)
        if i % 10 == 0:
            w.post({}, name='Rotate', type=w.Type.Control)
            assert os.path.exists(tmp_path / f'rotate.{i}.dat')

    assert w.config['info.seq-begin'] == '0'
    assert w.config['info.seq'] == '29'

    r.open()

    assert r.config['info.seq-begin'] == '0'
    assert r.config['info.seq'] == '29'

    for i in range(30):
        m = await r.recv()
        assert (m.type, m.seq, m.data.tobytes()) == (r.Type.Data, i, b'xxx' * (i % 10 + 1))
        if i % 10 == 0:
            m = await r.recv()
            assert (m.type, m.msgid, m.seq) == (r.Type.Control, r.scheme_control.messages.Rotate.msgid, 0)

    m = await r.recv()
    assert (m.type, m.msgid) == (m.Type.Control, r.scheme_control.messages.EndOfData.msgid)

    for i in range(30, 50):
        w.post(b'xxx' * (i % 10 + 1), seq=i)
        if i % 10 == 0:
            w.post({}, name='Rotate', type=w.Type.Control)

    assert w.config['info.seq-begin'] == '0'
    assert w.config['info.seq'] == '49'

    assert r.config['info.seq-begin'] == '0'
    assert r.config['info.seq'] == '49'

    for i in range(30, 50):
        m = await r.recv()
        assert (m.type, m.seq, m.data.tobytes()) == (r.Type.Data, i, b'xxx' * (i % 10 + 1))
        if i % 10 == 0:
            m = await r.recv()
            assert (m.type, m.msgid, m.seq) == (r.Type.Control, r.scheme_control.messages.Rotate.msgid, 0)

    r.close()
    assert r.state == r.State.Closed

    w.close()
    assert w.state == r.State.Closed

    w.open()

    assert w.config['info.seq-begin'] == '0'
    assert w.config['info.seq'] == '49'

    r.open(seq='35')
    assert (await r.recv_state()) == r.State.Active

    assert r.config['info.seq-begin'] == '0'
    assert r.config['info.seq'] == '49'

    for i in range(35, 50):
        m = await r.recv()
        assert (m.seq, m.data.tobytes()) == (i, b'xxx' * (i % 10 + 1))
        if i % 10 == 0:
            m = await r.recv()
            assert (m.type, m.msgid, m.seq) == (r.Type.Control, r.scheme_control.messages.Rotate.msgid, 0)

    m = await r.recv()
    assert (m.type, m.msgid) == (m.Type.Control, r.scheme_control.messages.EndOfData.msgid)

    with pytest.raises(TimeoutError): await r.recv_state(0.001)

@asyncloop_run
async def test_reopen_empty(asyncloop, tmp_path):
    w = asyncloop.Channel(f'rotate+file://{tmp_path}/rotate', dir='w', name='write', dump='frame')

    w.open()
    assert w.config['info.seq-begin'] == '-1'
    assert w.config['info.seq'] == '-1'

    assert os.path.exists(tmp_path / 'rotate.current.dat')

    for i in range(10):
        w.post(b'xxx' * (i % 10 + 1), seq=i)
    w.post({}, name='Rotate', type=w.Type.Control)
    w.close()

    w.open()
    assert w.config['info.seq-begin'] == '0'
    assert w.config['info.seq'] == '9'

    for i in range(10, 20):
        w.post(b'xxx' * (i % 10 + 1), seq=i)

    assert w.config['info.seq-begin'] == '0'
    assert w.config['info.seq'] == '19'

@pytest.mark.parametrize("key", ["first", "last"])
def test_reopen_rotate(context, tmp_path, key):
    w = context.Channel(f'rotate+file://{tmp_path}/rotate;filename-key={key}', dir='w', name='write', dump='frame')

    w.open()
    assert w.config['info.seq-begin'] == '-1'
    assert w.config['info.seq'] == '-1'

    assert os.path.exists(tmp_path / 'rotate.current.dat')

    for i in range(10):
        w.post(b'xxx' * (i % 10 + 1), seq=i)
    w.close()
    w.open()

    assert w.config['info.seq-begin'] == '0'
    assert w.config['info.seq'] == '9'

    for i in range(10, 20):
        w.post(b'xxx' * (i % 10 + 1), seq=i)

    assert w.config['info.seq-begin'] == '0'
    assert w.config['info.seq'] == '19'

    w.post({}, name='Rotate', type=w.Type.Control)
    idx = 0 if key == "first" else 19
    assert os.path.exists(tmp_path / f'rotate.{idx}.dat')

@asyncloop_run
async def test_autoclose(asyncloop, tmp_path):
    w = asyncloop.Channel(f'rotate+file://{tmp_path}/rotate', dir='w', name='write', dump='frame', async_mask=MsgMask.All)

    w.open()
    for i in range(10):
        w.post(b'xxx' * (i % 10 + 1), seq=i)
        if i % 3 == 0:
            w.post({}, name='Rotate', type=w.Type.Control)
    w.close()

    r = asyncloop.Channel(f'rotate+file://{tmp_path}/rotate;file.dump=frame;file.autoclose=no', dir='r', name='read', dump='frame', autoclose='yes')
    r.open(seq='-1')
    assert (await r.recv_state()) == r.State.Active

    for i in range(10):
        m = await r.recv()
        assert (m.type, m.seq) == (r.Type.Data, i)
        if i % 3 == 0:
            m = await r.recv()
            assert (m.type, m.msgid, m.seq) == (r.Type.Control, r.scheme_control['Rotate'].msgid, 0)

    m = await r.recv()
    assert (m.type, m.msgid, m.seq) == (r.Type.Control, r.scheme_control['EndOfData'].msgid, 0)

    assert (await r.recv_state()) == r.State.Closed

def test_scheme_change(context, tmp_path):
    SCHEME = '''yamls://
- options.version: %s
- name: Data
  id: 10
  fields:
  - {name: f0, type: int32}
'''

    cfg = Url.parse(f'rotate+file://{tmp_path}/rotate;dir=w;dump=frame;name=rotate')
    cfg['scheme'] = SCHEME % 'v0'
    w = context.Channel(cfg)

    f = context.Channel(f'file://', dir='r', name='file')
    def scheme_version(c, seq):
        try:
            c.open(filename = tmp_path / f'rotate.{seq}.dat')
            return c.scheme.options['version']
        finally:
            c.close()

    w.open()
    w.post(b'xxx', seq=0)
    w.post({}, name='Rotate', type=w.Type.Control)
    w.close()

    assert os.path.exists(tmp_path / 'rotate.0.dat')
    assert os.path.exists(tmp_path / 'rotate.current.dat')
    assert scheme_version(f, 0) == 'v0'
    assert scheme_version(f, 'current') == 'v0'

    del w
    cfg['scheme'] = SCHEME % 'v1'
    w = context.Channel(cfg)
    w.open()

    assert os.path.exists(tmp_path / 'rotate.0.dat')
    assert os.path.exists(tmp_path / 'rotate.current.dat')
    assert scheme_version(f, 0) == 'v0'
    assert scheme_version(f, 'current') == 'v1'

    w.post(b'xxx', seq=10)
    w.close()

    del w
    cfg['scheme'] = SCHEME % 'v2'
    w = context.Channel(cfg)
    w.open()

    assert os.path.exists(tmp_path / 'rotate.0.dat')
    assert os.path.exists(tmp_path / 'rotate.10.dat')
    assert os.path.exists(tmp_path / 'rotate.current.dat')
    assert scheme_version(f, 0) == 'v0'
    assert scheme_version(f, '10') == 'v1'
    assert scheme_version(f, 'current') == 'v2'

    r = context.Channel(cfg, dir='r', scheme='', name='reader')
    r.open()
    assert r.scheme.options['version'] == 'v2'
    r.close()

    r.open(seq='10')
    assert r.scheme.options['version'] == 'v2'
    r.close()

def test_convert(context, tmp_path):
    SV0 = '''yamls://[{name: Data, id: 10, fields: [{name: f0, type: int32}]}]'''
    SV1 = '''yamls://[{name: Data, id: 10, fields: [{name: h0, type: int16}, {name: f0, type: int32}]}]'''
    SV2 = '''yamls://[{name: Data, id: 10, fields: [{name: h0, type: int16}, {name: h1, type: int16}, {name: f0, type: int32}]}]'''

    cfg = Url.parse(f'rotate+file://{tmp_path}/rotate;dir=w;dump=frame;name=rotate')

    for i, scheme in enumerate([SV0, SV1, SV2]):
        cfg['scheme'] = scheme
        w = context.Channel(cfg)
        w.open()
        d = {'f0': 1000 + i}
        if i > 0: d['h0'] = 10 + i
        if i > 1: d['h1'] = 100 + i
        w.post(d, name='Data', seq=100 + i)
        w.close()
        del w

    assert os.path.exists(tmp_path / 'rotate.100.dat')
    assert os.path.exists(tmp_path / 'rotate.101.dat')
    assert os.path.exists(tmp_path / 'rotate.current.dat')

    r = Accum(cfg, dir='r', scheme='', name='reader', context=context, convert='yes')
    r.open()
    assert r.scheme['Data'].size == 8

    for _ in range(5):
        r.process()
        r.children[0].process()
    data = [m for m in r.result if m.type == m.Type.Data]
    assert [m.seq for m in data] == [100, 101, 102]
    assert [r.unpack(m).as_dict() for m in data] == [{'h0': 0, 'h1': 0, 'f0': 1000}, {'h0': 11, 'h1': 0, 'f0': 1001}, {'h0': 12, 'h1': 102, 'f0': 1002}]

def test_scheme_init(context, tmp_path):
    SCHEME = '''yamls://[{name: Data, id: 10, fields: [{name: f0, type: int32}]}]'''

    w = context.Channel(f'rotate+file://{tmp_path}/rotate;dump=frame', name='writer', dir='w', scheme=SCHEME)
    w.open()
    w.post({'f0': 10}, name='Data', seq=10)

    r = Accum(f'rotate+file://{tmp_path}/rotate;dump=frame', name='reader', context=context, convert='yes', master=w)
    r.open()
    assert r.scheme != None
    assert r.state == r.State.Active

def test_empty_nodir(context, tmp_path):
    w = context.Channel(f'rotate+file://{tmp_path}/rotate;dump=frame', name='writer', dir='w')
    w.open()
    w.post(b'xxx', seq=10)
    w.post(b'', name='Rotate', type=w.Type.Control)

    cwd = os.getcwd()
    try:
        os.chdir(tmp_path)
        r = Accum(f'rotate+file://rotate;dump=frame', name='reader', context=context)
        r.open()
        assert r.state == r.State.Active
        assert r.config['info.seq'] == '10'
    finally:
        os.chdir(cwd)

def test_autoseq(context, tmp_path):
    w = context.Channel(f'rotate+file://{tmp_path}/rotate;dump=frame;file.dump=frame', name='writer', dir='w', autoseq='yes')
    w.open()
    w.post(b'aaa', seq=10)
    w.post(b'bbb', seq=20)
    w.post(b'ccc', seq=30)
    assert w.config['info.seq'] == '2'

    w.post(b'', name='Rotate', type=w.Type.Control)
    assert os.path.exists(tmp_path / 'rotate.0.dat')

    w.close()
    w.open()
    assert w.config['info.seq'] == '2'

    w.post(b'xxx', seq=10)
    w.post(b'yyy', seq=20)
    w.post(b'zzz', seq=30)
    assert w.config['info.seq'] == '5'

    w.post(b'', name='Rotate', type=w.Type.Control)
    assert os.path.exists(tmp_path / 'rotate.0.dat')
    assert os.path.exists(tmp_path / 'rotate.3.dat')

@pytest.mark.parametrize("with_master", [True])
def test_rotate_after_eod(context, tmp_path, with_master):
    w = context.Channel(f'rotate+file://{tmp_path}/rotate;dump=frame;file.dump=frame', name='writer', dir='w')
    w.open()
    w.post(b'aaa', msgid=100, seq=10)
    w.post(b'bbb', msgid=100, seq=20)
    w.post(b'ccc', msgid=100, seq=30)

    r = Accum(f'rotate+file://{tmp_path}/rotate;dump=frame;file.dump=frame', autoclose='no', name='reader', context=context, master=w if with_master else None)
    r.open()
    assert r.state == r.State.Active

    for _ in range(5):
        r.process()
        r.children[0].process()

    assert [(m.type, m.seq) for m in r.result] == [(r.Type.Data, 10), (r.Type.Data, 20), (r.Type.Data, 30), (r.Type.Control, 0)]

    w.post(b'xxx', msgid=100, seq=40)
    w.post(b'', name='Rotate', type=w.Type.Control)
    w.post(b'yyy', msgid=100, seq=50)
    assert os.path.exists(tmp_path / 'rotate.10.dat')

    r.result = []
    for _ in range(5):
        r.process()
        r.children[0].process()
    assert [(m.type, m.seq) for m in r.result] == [(r.Type.Data, 40), (r.Type.Control, 0), (r.Type.Data, 50)]
