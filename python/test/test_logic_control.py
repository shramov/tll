#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import pytest

import decorator
import os
import pathlib

import tll
from tll import asynctll
from tll.logger import Logger
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

    tinput.post({'channel': 'xxx'}, name='ChannelClose')
    assert tinput.unpack(await tinput.recv()).as_dict() == {'error': "Object 'xxx' not found"}

    tinput.post({'channel': 'input'}, name='ChannelClose')
    assert tinput.unpack(await tinput.recv()).SCHEME.name == 'Ok'

@asyncloop_run
async def test_uplink(asyncloop):
    scheme = pathlib.Path(os.environ.get("SOURCE_DIR", pathlib.Path(tll.__file__).parent.parent.parent)) / "src/logic/control.yaml"

    lproc = asyncloop.Channel('direct://;name=processor', scheme='yaml://' + str(scheme), dump='yes')
    tproc = asyncloop.Channel('direct://;name=processor-client', master=lproc, scheme='yaml://' + str(scheme))
    linput = asyncloop.Channel('direct://;name=input', scheme='yaml://' + str(scheme), dump='yes')
    tinput = asyncloop.Channel('direct://;name=input-client', master=linput, scheme='yaml://' + str(scheme))

    logic = asyncloop.Channel('control://;tll.channel.processor=processor;tll.channel.uplink=input', name='logic')

    lproc.open()
    tproc.open()

    logic.open()

    m = await tproc.recv()
    assert tproc.unpack(m).SCHEME.name == 'StateDump'

    tinput.open()
    linput.open()

    m = await tproc.recv()
    assert tproc.unpack(m).SCHEME.name == 'StateDump'

    tproc.post({'channel': 'test', 'state': 'Error'}, name='StateUpdate')
    tproc.post({}, name='StateDumpEnd')

    m = tinput.unpack(await tinput.recv())
    assert m.as_dict() == {'channel': 'test', 'state': m.state.Error}
    assert tinput.unpack(await tinput.recv()).SCHEME.name == 'StateDumpEnd'

    tinput.post({'path': '*.state'}, name='ConfigGet')
    result = []
    for _ in range(100):
        m = tinput.unpack(await tinput.recv())
        if m.SCHEME.name == 'ConfigEnd':
            break
        result.append((m.key, m.value))
    assert result == [('input-client.state', 'Active'), ('input.state', 'Active'), ('logic.state', 'Active'), ('processor-client.state', 'Active'), ('processor.state', 'Active')]

@asyncloop_run
async def test_message(asyncloop):
    scheme = pathlib.Path(os.environ.get("SOURCE_DIR", pathlib.Path(tll.__file__).parent.parent.parent)) / "src/logic/control.yaml"
    pscheme = pathlib.Path(os.environ.get("SOURCE_DIR", pathlib.Path(tll.__file__).parent.parent.parent)) / "src/processor/processor.yaml"

    lproc = asyncloop.Channel('direct://;name=processor', scheme='yaml://' + str(pscheme), dump='yes')
    tproc = asyncloop.Channel('direct://;name=processor-client', master=lproc, scheme='yaml://' + str(pscheme))
    linput = asyncloop.Channel('direct://;name=input', scheme='yaml://' + str(scheme), dump='yes')
    tinput = asyncloop.Channel('direct://;name=input-client', master=linput, scheme='yaml://' + str(scheme))

    logic = asyncloop.Channel('control://;tll.channel.processor=processor;tll.channel.input=input', name='logic')

    lproc.open()
    tproc.open()

    logic.open()

    m = await tproc.recv()
    assert tproc.unpack(m).SCHEME.name == 'StateDump'

    tinput.open()
    linput.open()

    tinput.post({'dest': 'input', 'data': {'seq': 100, 'addr': 1000, 'type': 'Data', 'name': 'ConfigGet', 'data': b'{"path": "*.state"}'}}, name='MessageForward')
    body = tinput.scheme.messages.ConfigGet.object(path='*.state')

    m = tproc.unpack(await tproc.recv())
    assert m.SCHEME.name == 'MessageForward'
    assert m.as_dict() == {'dest': 'input', 'data': {'seq': 100, 'addr': 1000, 'type': 0, 'msgid': body.SCHEME.msgid, 'data': m.data.data}}
    assert m.data.data.encode('utf-8') == body.pack()

    m = tinput.unpack(await tinput.recv())
    assert m.SCHEME.name == 'Ok'

    tinput.post({'dest': 'input', 'data': {'seq': 100, 'addr': 1000, 'type': 'Data', 'name': 'xxx', 'data': b'{"path": "*.state"}'}}, name='MessageForward')
    m = tinput.unpack(await tinput.recv())
    assert m.SCHEME.name == 'Error'

@asyncloop_run
async def test_message(asyncloop):
    scheme = pathlib.Path(os.environ.get("SOURCE_DIR", pathlib.Path(tll.__file__).parent.parent.parent)) / "src/logic/control.yaml"

    proc = asyncloop.Channel('null://;name=processor;dump=frame')
    linput = asyncloop.Channel('direct://;name=input', scheme='yaml://' + str(scheme), dump='yes')
    tinput = asyncloop.Channel('direct://;name=input-client', master=linput, scheme='yaml://' + str(scheme))

    logic = asyncloop.Channel('control://;tll.channel.processor=processor;tll.channel.input=input', name='logic')

    proc.open()

    logic.open()

    tinput.open()
    linput.open()

    la = Logger('logger-test.a')
    lb = Logger('logger-test.a.b')
    la.level = la.Level.Debug
    lb.level = lb.Level.Info

    assert la.level == la.Level.Debug
    assert lb.level == lb.Level.Info

    tinput.post({'prefix': 'logger-test.a', 'level': 'Warning', 'recursive': 'No'}, name='SetLogLevel')

    assert la.level == la.Level.Warning
    assert lb.level == lb.Level.Info

    tinput.post({'prefix': 'logger-test.a', 'level': 'Error', 'recursive': 'Yes'}, name='SetLogLevel')

    assert la.level == la.Level.Error
    assert lb.level == lb.Level.Error
