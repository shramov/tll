#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import pytest

import tll
from tll.asynctll import asyncloop_run
from tll.logger import Logger
from tll.channel import Context
from tll.channel.mock import Mock
from tll.channel.prefix import Prefix
from tll.config import Config
from tll.processor.mock import Mock as ProcessorMock
from tll.test_util import ports

@pytest.fixture
def context(path_builddir):
    ctx = Context()
    ctx.load(path_builddir / 'logic/tll-logic-control')
    return ctx

@asyncloop_run
async def test(asyncloop, path_srcdir):
    scheme = path_srcdir / "src/logic/control.yaml"

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

    ci.post({}, name='Ping')
    assert ci.unpack(await ci.recv()).SCHEME.name == 'Pong'

@asyncloop_run
async def test_uplink(asyncloop, path_srcdir):
    scheme = path_srcdir / "src/logic/control.yaml"

    mock = Mock(asyncloop, f'''yamls://
mock:
  processor: direct://;scheme=yaml://{scheme}
  uplink: direct://;scheme=yaml://{scheme}
channel: control://;tll.channel.processor=processor;tll.channel.uplink=uplink;name=logic;service=service-name
''')

    mock.open(skip=['uplink'])

    tproc, tinput = mock.io('processor', 'uplink')

    m = await tproc.recv()
    assert tproc.unpack(m).SCHEME.name == 'StateDump'

    mock.inner('uplink').open()

    m = tinput.unpack(await tinput.recv())
    assert m.SCHEME.name == 'Hello'
    assert m.as_dict() == {'version': 1, 'service': 'service-name'}

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
async def test_message(asyncloop, path_srcdir):
    scheme = path_srcdir / "src/logic/control.yaml"
    pscheme = path_srcdir / "src/processor/processor.yaml"

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

    assert tinput.unpack(await tinput.recv()).SCHEME.name == 'Ok'

    tinput.post({'dest': 'input', 'data': {'seq': 200, 'addr': 2000, 'type': 'Data', 'name': 'Error'}}, name='MessageForward')

    m = tproc.unpack(await tproc.recv())
    assert m.as_dict() == {'dest': 'input', 'data': {'seq': 200, 'addr': 2000, 'type': 0, 'msgid': 50 , 'data': '\0' * 8}}

    assert tinput.unpack(await tinput.recv()).SCHEME.name == 'Ok'

    tinput.post({'dest': 'input', 'data': {'seq': 100, 'addr': 1000, 'type': 'Data', 'name': 'xxx', 'data': b'{"path": "*.state"}'}}, name='MessageForward')
    m = tinput.unpack(await tinput.recv())
    assert m.SCHEME.name == 'Error'

@asyncloop_run
async def test_log_level(asyncloop, path_srcdir):
    scheme = path_srcdir / "src/logic/control.yaml"

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

class ChildExport(Prefix):
    PROTO = 'child-export+'

    def _on_client_export(self, client):
        cfg = client.copy()
        cfg['children'] = Config.from_dict({
                'first.init.tll.proto': 'first',
                'first.children.second.init.tll.proto': 'second',
        })
        self.config['client'] = cfg

@asyncloop_run
async def test_resolve(asyncloop, path_srcdir):
    asyncloop.context.register(ChildExport)

    scheme = path_srcdir / "src/logic/resolve.yaml"
    pscheme = path_srcdir / "src/processor/processor.yaml"

    mock = Mock(asyncloop, f'''yamls://
mock:
  processor: direct://;scheme=yaml://{pscheme}
  resolve: direct://;scheme=yaml://{scheme}
channel:
  url: control://;tll.channel.processor=processor;tll.channel.resolve=resolve;name=logic
  service: test
  hostname: ::1
  service-tags: 'a,b'
''')

    mock.open(skip=['resolve'])
    mock.inner('resolve').open()

    tproc, tinput = mock.io('processor', 'resolve')

    tcp = asyncloop.Channel(f'child-export+tcp://*:{ports.TCP6};mode=server;name=tcp;tll.resolve.export=yes')
    tcp.open()

    m = await tinput.recv()
    assert tinput.unpack(m).as_dict() == {'service': 'test', 'tags': ['a', 'b'], 'host': '::1'}

    tproc.post({'channel': 'tcp', 'state': 'Active'}, name='StateUpdate')
    for name in ('tcp', 'tcp/first', 'tcp/first/second'):
        m = await tinput.recv(0.001)
        m = tinput.unpack(m)
        assert (m.service, m.channel) == ('test', name)

        cfg = Config.from_dict({x.key: x.value for x in m.config})
        assert cfg['init.tll.proto'] == name.split('/')[-1]

        if name == 'tcp':
            client = asyncloop.Channel(cfg.sub('init'))
            client.open()

    m = await tcp.recv()
    assert tcp.unpack(m).SCHEME.name == 'Connect'

    client.post(b'xxx')
    assert (await tcp.recv()).data.tobytes() == b'xxx'

class Echo(Prefix):
    PROTO = "echo+"

    def _on_data(self, msg):
        self._child.post(msg)

@asyncloop_run
async def test_resolve_processor(asyncloop, path_srcdir):
    asyncloop.context.register(Echo)
    scheme = path_srcdir / "src/logic/resolve.yaml"
    mock = ProcessorMock(asyncloop, f'''yamls://
name: processor
processor.objects:
  resolve:
    init: python://;python=tll.channel.resolve:Resolve
    channels: {{ input: _tll_resolve_master }}
  _tll_resolve_master:
    init: ipc://;mode=server;scheme=yaml://{scheme};dump=yes
    depends: resolve
  control:
    init:
      tll.proto: control
      service: test
      hostname: host
      service-tags: 'a,b'
    channels: {{resolve: control-resolve, processor: control-processor}}
  control-resolve:
    init: ipc://;mode=client;master=_tll_resolve_master
    depends: control
  control-processor:
    init: ipc://;mode=client;master=processor/ipc;dump=yes
    depends: control
  tcp:
    init: echo+tcp://::1:{ports.TCP6};mode=server;tll.resolve.export=yes;dump=yes
''')

    mock.open()

    await mock.wait('_tll_resolve_master', 'Active')

    client = asyncloop.Channel(f'resolve://;resolve.service=test;resolve.channel=tcp;name=client')
    client.open()

    await client.recv_state() == client.State.Active
    client.post(b'xxx')
    assert (await client.recv()).data.tobytes() == b'xxx'
