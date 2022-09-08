#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import decorator
import pytest

import struct

from tll import asynctll
import tll.channel as C
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
@pytest.mark.parametrize("frame", ['std', 'short', 'tiny', 'l4m4s8', 'l2m2s8', 'l2m2s4'])
async def test(asyncloop, tmp_path, frame):
    s = asyncloop.Channel(f'frame+tcp:///{tmp_path}/tcp.sock;frame.frame={frame};tcp.frame=none;dump=frame;name=server;mode=server')
    c = asyncloop.Channel(f'tcp:///{tmp_path}/tcp.sock;frame={frame};dump=frame;name=client;mode=client')

    s.open()
    c.open()

    c.post(b'xxx', msgid=10, seq=100)
    c.post(b'yyyy', msgid=20, seq=200)
    c.post(b'zzzzz', msgid=30, seq=300)

    m = await s.recv()
    assert m.type == m.Type.Control

    m = await s.recv()
    assert (b'xxx', 10, 100) == (m.data.tobytes(), m.msgid, m.seq)

    m = await s.recv()
    assert (b'yyyy', 20, 200) == (m.data.tobytes(), m.msgid, m.seq)

    m = await s.recv()
    assert (b'zzzzz', 30, 300) == (m.data.tobytes(), m.msgid, m.seq)

    s.post(b'xxx', msgid=10, seq=100, addr = m.addr)
    s.post(b'yyyy', msgid=20, seq=200, addr = m.addr)
    s.post(b'zzzzz', msgid=30, seq=300, addr = m.addr)
    
    m = await c.recv()
    assert (b'xxx', 10, 100) == (m.data.tobytes(), m.msgid, m.seq)

    m = await c.recv()
    assert (b'yyyy', 20, 200) == (m.data.tobytes(), m.msgid, m.seq)

    m = await c.recv()
    assert (b'zzzzz', 30, 300) == (m.data.tobytes(), m.msgid, m.seq)

FRAMES = [
    (('std', 'l4m4s8'), 'Iiq', ('size', 'msgid', 'seq'), 'all'),
    (('short', 'l2m2s8'), 'Hhq', ('size', 'msgid', 'seq'), 'all'),
    (('tiny', 'l2m2s4'), 'Hhi', ('size', 'msgid', 'seq'), 'tcp'),
    (('seq32', 's4'), 'i', ('seq',), 'udp'),
]
def frames():
    for f in FRAMES:
        for n in f[0]:
            yield (n,) + f[1:]

@pytest.mark.parametrize("type", ['tcp', 'udp'])
@pytest.mark.parametrize("frame,pack,fields,ftype", frames())
def test_pack(type, frame, pack, fields, ftype, context):
    if ftype not in (type, 'all'):
        pytest.skip(f'Unsupported mode {ftype}')

    s = Accum(f'frame+direct://;frame={frame};name=server;frame.type={type}', context=context)
    c = context.Channel(f'direct://', master=s)

    s.open()
    c.open()

    data = b'xxxx'
    meta = {'size': len(data), 'msgid': 0x1234, 'seq': 0x56789abc}
    header = struct.pack('=' + pack, *[meta[n] for n in fields])

    c.post(header + data)
    assert [(m.msgid, m.seq, m.data.tobytes()) for m in s.result] == [(meta['msgid'] if 'msgid' in fields else 0, meta['seq'], data)]
