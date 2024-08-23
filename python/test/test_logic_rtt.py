#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import datetime
import pytest

from tll.asynctll import asyncloop_run
import tll.stat as S
from tll.channel import Context
from tll.channel.mock import Mock

@pytest.fixture
def context(path_builddir):
    ctx = Context()
    ctx.load(str(path_builddir / 'logic/tll-logic-rtt'))
    return ctx

@asyncloop_run
async def test(asyncloop, context):
    mock = Mock(asyncloop, '''yamls://
mock:
  timer: direct://
  result: direct://
  output: direct://
  input: direct://
channel:
    tll.proto: rtt
    name: mock
    tll.channel: {output: output, input: input, timer: timer, result: result}
    payload: 128b
    chained: yes
''')

    mock.open()

    timer, ci, co, result = mock.io('timer', 'output', 'input', 'result')
    rtt = mock.channel

    timer.post(b'')

    m = await ci.recv()
    assert len(m.data) == 128 + 8

    with pytest.raises(TimeoutError): await ci.recv(0.01)

    timer.post(b'')
    m = await ci.recv()
    assert len(m.data) == 128 + 8

    co.post(m)

    m = await ci.recv()
    assert len(m.data) == 128 + 8

    m = await rtt.recv()
    assert m.data.tobytes() == (await result.recv()).data.tobytes()
    m = rtt.unpack(m)
    assert m.name == ''
    assert m.value < 100000000 # Faster then 100ms
