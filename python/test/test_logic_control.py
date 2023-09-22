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
from tll.channel.mock import Mock

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

    mock = Mock(asyncloop, f'''yamls://
mock:
  processor: null://
  input: direct://;scheme=yaml://{scheme}
channel: control://;tll.channel.processor=processor;tll.channel.input=input;name=logic
''')

    mock.open(skip=['processor'])

    ci = mock.io('input')

    ci.post({'path': '*.state'}, name='ConfigGet')
    result = []
    for _ in range(100):
        m = ci.unpack(await ci.recv())
        if m.SCHEME.name == 'ConfigEnd':
            break
        result.append((m.key, m.value))
    assert result == [('_mock_master_input.state', 'Active'), ('input.state', 'Active'), ('logic.state', 'Active'), ('processor.state', 'Closed')]

    ci.post({'channel': 'xxx'}, name='ChannelClose')
    assert ci.unpack(await ci.recv()).as_dict() == {'error': "Object 'xxx' not found"}

    ci.post({'channel': 'input'}, name='ChannelClose')
    assert ci.unpack(await ci.recv()).SCHEME.name == 'Ok'

@asyncloop_run
async def test_uplink(asyncloop):
    scheme = pathlib.Path(os.environ.get("SOURCE_DIR", pathlib.Path(tll.__file__).parent.parent.parent)) / "src/logic/control.yaml"

    mock = Mock(asyncloop, f'''yamls://
mock:
  processor: direct://;scheme=yaml://{scheme}
  uplink: direct://;scheme=yaml://{scheme}
channel: control://;tll.channel.processor=processor;tll.channel.uplink=uplink;name=logic
''')

    mock.open(skip=['uplink'])

    tproc, tinput = mock.io('processor', 'uplink')

    m = await tproc.recv()
    assert tproc.unpack(m).SCHEME.name == 'StateDump'

    mock.inner('uplink').open()

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
    assert sorted(result) == [(f'{x}.state', 'Active') for x in sorted(['_mock_master_uplink', 'uplink', 'logic', '_mock_master_processor', 'processor'])]

@asyncloop_run
async def test_message(asyncloop):
    scheme = pathlib.Path(os.environ.get("SOURCE_DIR", pathlib.Path(tll.__file__).parent.parent.parent)) / "src/logic/control.yaml"
    pscheme = pathlib.Path(os.environ.get("SOURCE_DIR", pathlib.Path(tll.__file__).parent.parent.parent)) / "src/processor/processor.yaml"

    mock = Mock(asyncloop, f'''yamls://
mock:
  processor: direct://;scheme=yaml://{pscheme}
  input: direct://;scheme=yaml://{scheme}
channel: control://;tll.channel.processor=processor;tll.channel.input=input;name=logic
''')

    mock.open()

    tproc, tinput = mock.io('processor', 'input')

    m = await tproc.recv()
    assert tproc.unpack(m).SCHEME.name == 'StateDump'

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
async def test_log_level(asyncloop):
    scheme = pathlib.Path(os.environ.get("SOURCE_DIR", pathlib.Path(tll.__file__).parent.parent.parent)) / "src/logic/control.yaml"

    mock = Mock(asyncloop, f'''yamls://
mock:
  processor: null://
  input: direct://;scheme=yaml://{scheme}
channel: control://;tll.channel.processor=processor;tll.channel.input=input;name=logic
''')

    mock.open()

    tinput = mock.io('input')

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
