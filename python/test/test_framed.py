#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import pytest

import struct

from tll.asynctll import asyncloop_run
import tll.channel as C
from tll.test_util import Accum

@pytest.fixture
def context():
    return C.Context()

@asyncloop_run
@pytest.mark.parametrize("frame", ['std', 'short', 'tiny', 'size32', 'l4m4s8', 'l2m2s8', 'l2m2s4', 'l4', 'bson'])
async def test(asyncloop, tmp_path, frame):
    s = asyncloop.Channel(f'frame+tcp:///{tmp_path}/tcp.sock;frame.frame={frame};tcp.frame=none;dump=frame;name=server;mode=server')
    c = asyncloop.Channel(f'tcp:///{tmp_path}/tcp.sock;frame={frame};dump=frame;name=client;mode=client')

    s.open()
    c.open()

    result = post = [(b'xxx', 10, 100), (b'yyyy', 20, 200), (b'zzzzz', 30, 300)]

    if frame in ('bson',):
        result = post = [(struct.pack('=I', 4 + len(data)) + data, 0, 0) for (data, msgid, seq) in result]

    if frame in ('size32', 'l4', 'bson'):
        result = [(data, 0, 0) for (data, msgid, seq) in result]

    for data, msgid, seq in post:
        c.post(data, msgid=msgid, seq=seq)

    m = await s.recv()
    assert m.type == m.Type.Control

    for r in result:
        m = await s.recv()
        assert r == (m.data.tobytes(), m.msgid, m.seq)

    for data, msgid, seq in post:
        s.post(data, msgid=msgid, seq=seq, addr=m.addr)

    for r in result:
        m = await c.recv()
        assert r == (m.data.tobytes(), m.msgid, m.seq)

FRAMES = [
    (('std', 'l4m4s8'), 'Iiq', ('size', 'msgid', 'seq'), 'all'),
    (('short', 'l2m2s8'), 'Hhq', ('size', 'msgid', 'seq'), 'all'),
    (('tiny', 'l2m2s4'), 'Hhi', ('size', 'msgid', 'seq'), 'tcp'),
    (('size32', 'l4'), 'I', ('size',), 'tcp'),
    (('bson',), 'I', ('size',), 'tcp'),
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

    s = Accum(f'frame+direct://;frame={frame};name=server;frame.type={type}', context=context, dump='text+hex')
    c = context.Channel(f'direct://', master=s, dump='text+hex')

    s.open()
    c.open()

    data = b'xxxx'
    meta = {'size': len(data), 'msgid': 0x1234, 'seq': 0x56789abc}
    meta = {k: v for k,v in meta.items() if k in fields}
    if frame in ('bson',):
        meta['size'] += 4
    header = struct.pack('=' + pack, *[meta[n] for n in fields])

    c.post(header + data)
    if frame in ('bson',):
        data = header + data
    assert [(m.msgid, m.seq, m.data.tobytes()) for m in s.result] == [(meta.get('msgid', 0), meta.get('seq', 0), data)]
