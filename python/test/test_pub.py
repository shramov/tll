#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import pytest
import yaml

from tll.asynctll import asyncloop_run
import tll.channel as C
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
    c = asyncloop.Channel(f'pub+tcp:///{tmp_path}/pub.sock', mode='client', name='client', dump='frame')
    yield c
    c.free()

def test(server, client):
    s, c = server, client

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

    with pytest.raises(TLLError): s.post(b'x' * (512 - 16 + 1), seq=1) # Message larger then half of buffer

    s.post(b'xxx', seq=1, msgid=10)
    c.process()

    assert [(m.seq, m.msgid, m.data.tobytes()) for m in c.result] == [(1, 10, b'xxx')]

    for i in range(2, 5):
        s.post(b'x' * i, seq=i, msgid=10)

    for i in range(2, 5):
        c.process()
        m = c.result[-1]
        assert (m.seq, m.msgid, m.data.tobytes()) == (i, 10, b'x' * i)

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
async def test_client(asyncloop, server):
    server.open()

    url = server.config.get_url('client')
    assert url.proto == 'pub+tcp'

    c = asyncloop.Channel(url, name='client')
    c.open()
    assert await c.recv_state() == c.State.Active

    server.post(b'xxx', seq=100)

    assert (await c.recv()).seq == 100
