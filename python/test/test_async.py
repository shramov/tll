#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import tll.channel as C
from tll.asynctll import asyncloop_run
from tll.test_util import Accum

import pytest

@pytest.fixture
def context():
    return C.Context()

def test(context):
    server = Accum('direct://', name='server', dump='frame', context=context)
    client = Accum('async+direct://', name='client', master='server', dump='frame', context=context, **{'direct.dump': 'frame'})

    server.open()
    client.open()

    assert client.state == client.State.Active
    assert client.dcaps & client.DCaps.Process == client.DCaps.Process

    client.post(b'xxx', seq=10)
    assert [(m.seq, m.data.tobytes()) for m in server.result] == []
    client.process()
    assert [(m.seq, m.data.tobytes()) for m in server.result] == [(10, b'xxx')]

    server.post(b'zzz', seq=100)
    assert [(m.seq, m.data.tobytes()) for m in client.result] == [(100, b'zzz')]

def test_overflow(context):
    server = Accum('mem://', name='server', size='1kb', dump='frame', context=context)
    client = Accum('async+mem://', name='client', master='server', dump='frame', context=context, **{'mem.dump': 'frame'})

    server.open()
    client.open()

    assert client.state == client.State.Active

    for i in range(10):
        client.post(b'x' * 256, seq=i)

    for _ in range(10):
        client.process()

    assert client.state == client.State.Active

    for _ in range(10):
        server.process()

    assert [m.seq for m in server.result] == list(range(3))

    for _ in range(10):
        client.process()
        server.process()

    assert [m.seq for m in server.result] == list(range(10))

@asyncloop_run
async def test_loop(asyncloop):
    server = asyncloop.Channel('mem://', name='server', size='1kb', dump='frame')
    client = asyncloop.Channel('async+mem://', name='client', master='server', dump='frame', **{'mem.dump': 'frame'})

    server.open()
    client.open()

    assert (await client.recv_state()) == client.State.Active

    for i in range(10):
        client.post(b'x' * 256, seq=i)

    for i in range(10):
        assert (await server.recv()).seq == i
