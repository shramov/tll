#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import os

import pytest

from tll.asynctll import asyncloop_run
from tll.channel import Context
from tll.error import TLLError

@pytest.fixture
def context():
    return Context()

@asyncloop_run
async def test_basic(asyncloop, tmp_path):
    w = asyncloop.Channel(f'rotate+file://{tmp_path}/rotate', dir='w', name='write', dump='frame')
    r = asyncloop.Channel(f'rotate+file://{tmp_path}/rotate;file.dump=frame', dir='r', name='read', master=w, dump='frame', autoclose='no')

    assert w.scheme_load(w.Type.Control) is not None
    assert [m.name for m in w.scheme_control.messages] == ['Rotate']

    assert r.scheme_load(w.Type.Control) is not None
    assert [m.name for m in r.scheme_control.messages] == ['Seek', 'EndOfData']

    assert not os.path.exists(tmp_path / 'rotate.current.dat')

    w.open()
    assert w.config['info.seq-begin'] == '-1'
    assert w.config['info.seq'] == '-1'


    assert os.path.exists(tmp_path / 'rotate.current.dat')

    for i in range(30):
        w.post(b'xxx' * (i % 10 + 1), seq=i)
        if i % 10 == 0:
            w.post({}, name='Rotate', type=w.Type.Control)

    assert w.config['info.seq-begin'] == '0'
    assert w.config['info.seq'] == '29'

    r.open()

    assert r.config['info.seq-begin'] == '0'
    assert r.config['info.seq'] == '29'

    for i in range(30):
        m = await r.recv()
        assert (m.seq, m.data.tobytes()) == (i, b'xxx' * (i % 10 + 1))

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
        assert (m.seq, m.data.tobytes()) == (i, b'xxx' * (i % 10 + 1))

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

@asyncloop_run
async def test_reopen_rotate(asyncloop, tmp_path):
    w = asyncloop.Channel(f'rotate+file://{tmp_path}/rotate', dir='w', name='write', dump='frame')

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

    w.post({}, name='Rotate', type=w.Type.Control)
    assert os.path.exists(tmp_path / 'rotate.0.dat')

@asyncloop_run
async def test_autoclose(asyncloop, tmp_path):
    w = asyncloop.Channel(f'rotate+file://{tmp_path}/rotate', dir='w', name='write', dump='frame')

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
        assert m.seq == i
        assert r.state == r.State.Active

    assert (await r.recv_state()) == r.State.Closed
