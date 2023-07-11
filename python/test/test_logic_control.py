#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import pytest

import decorator
import os
import pathlib

import tll
from tll import asynctll
from tll.channel import Context
from tll.test_util import Accum

@pytest.fixture
def context():
    path = pathlib.Path(tll.__file__).parent.parent.parent / "build/src/"
    path = pathlib.Path(os.environ.get("BUILD_DIR", path))
    ctx = Context()
    ctx.load(str(path / 'logic/tll-logic-control'))
    return ctx

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
async def test(asyncloop):
    scheme = pathlib.Path(os.environ.get("SOURCE_DIR", pathlib.Path(tll.__file__).parent.parent.parent)) / "src/logic/control.yaml"

    processor = asyncloop.Channel('null://;name=processor', dump='frame')
    linput = asyncloop.Channel('direct://;name=input', scheme='yaml://' + str(scheme), dump='yes')
    tinput = asyncloop.Channel('direct://;name=input-client', master=linput, scheme='yaml://' + str(scheme))

    logic = asyncloop.Channel('control://;tll.channel.processor=processor;tll.channel.input=input', name='logic')

    logic.open()

    tinput.open()
    linput.open()

    tinput.post({'path': '*.state'}, name='ConfigGet')
    result = []
    for _ in range(100):
        m = tinput.unpack(await tinput.recv())
        if m.SCHEME.name == 'ConfigEnd':
            break
        result.append((m.key, m.value))
    assert result == [('input-client.state', 'Active'), ('input.state', 'Active'), ('logic.state', 'Active'), ('processor.state', 'Closed')]
