#!/usr/bin/env python3
# vim: sts=4 sw=4 et

from tll.asynctll import asyncloop_run
from tll.channel import Context
from tll.channel.base import Base
from tll.channel.logic import Logic
from tll.config import Config
from tll.error import TLLError
from tll.processor import Processor
from tll.processor.mock import Mock
from tll.scheme import Scheme

import tll.asynctll.asyncio
import tll.asynctll.bare

import pytest

@pytest.fixture
def context():
    return Context()

@pytest.fixture(params=['bare', 'asyncio'])
def asynctll(request):
    return getattr(tll.asynctll, request.param)

@pytest.fixture
def asyncloop(context, asynctll):
    loop = asynctll.Loop(context)
    yield loop
    loop.destroy()
    loop = None

class OpenTest(Base):
    PROTO = "open-test"

    OPEN_POLICY = Base.OpenPolicy.Manual
    PROCESS_POLICY = Base.ProcessPolicy.Never

    STATE_PARAM = None

    def _open(self, props):
        v = props.get('state', None)

        OpenTest.STATE_PARAM = v
        self.close()

def test_open_link(context):
    cfg = Config.load('''yamls://
processor.objects:
  null:
    url: null://
  test:
    url: open-test://;shutdown-on=close
    open:
      state: !link /sys/null/state
    depends: null
''')

    context.register(OpenTest)
    cfg.copy().copy()

    p = Processor(cfg, context=context)
    p.open()

    for w in p.workers:
        w.open()

    workers = [p] + p.workers
    for _ in range(100):
        if p.state == p.State.Closed:
            break
        for w in workers:
            w.step(timeout=0.001)
    assert OpenTest.STATE_PARAM == 'Active'

class Forward(Logic):
    PROTO = "forward"

    def _init(self, url, master=None):
        super()._init(url, master)

        for n in ('input', 'output'):
            l = self._channels.get(n, [])
            if len(l) != 1:
                raise RuntimeError("Need exactly one {}, got: {}".format(n, [c.name for c in l]))
            setattr(self, '_' + n, l[0])

    def _open(self, props):
        super()._open(props)

    def _logic(self, channel, msg):
        if channel != self._input:
            return
        if msg.type != msg.Type.Data:
            return
        self._output.post(msg)

def test_forward(context, asynctll):
    cfg = Config.load('''yamls://
name: test
processor.objects:
  output:
    url: mem://;size=1mb;dump=frame
  forward:
    url: forward://
    depends: output
    channels: {input: input, output: output}
  input:
    url: mem://;size=1mb;dump=frame
    depends: forward
''')

    context.register(Forward)

    p = Processor(cfg, context=context)
    p.open()

    loop = asynctll.Loop(context=context)
    loop._loop.add(p)

    assert len(p.workers) == 1
    for w in p.workers:
        w.open()

    async def main(loop):
        ci = loop.Channel('mem://;master=input')
        co = loop.Channel('mem://;master=output')

        pi = context.get('input')

        for i in range(10):
            await loop.sleep(0.01)
            if pi.state == pi.State.Active:
                break

        assert pi.state == pi.State.Active

        ci.open()
        co.open()

        ci.post(b'xxx', msgid=10, seq=20)
        m = await co.recv(0.001)
        assert (m.seq, m.msgid, m.data.tobytes()) == (20, 10, b'xxx')

    try:
        loop.run(main(loop))
    finally:
        loop.destroy()

@asyncloop_run
async def test_control(asyncloop, context):
    cfg = Config.load('''yamls://
name: test
processor.objects:
  object:
    url: direct://;master=object-client;tll.processor.ignore-master-dependency=yes
''')

    oclient = asyncloop.Channel('direct://;name=object-client;dump=yes')
    oclient.open()

    p = Processor(cfg, context=context)
    asyncloop._loop.add(p)

    p.open()

    client = asyncloop.Channel('ipc://;mode=client;master=test;dump=yes;name=client;scheme=channel://test')
    client.open()

    assert len(p.workers) == 1
    for w in p.workers:
        w.open()

    State = client.scheme.messages.StateUpdate.enums['State'].klass
    m = await client.recv()
    assert client.unpack(m).as_dict() == {'channel': 'object', 'state': State.Opening, 'flags': {'stage': False}}

    m = await client.recv()
    assert client.unpack(m).as_dict() == {'channel': 'object', 'state': State.Active, 'flags': {'stage': False}}

    client.post({}, name='StateDump')

    m = await client.recv()
    assert client.unpack(m).as_dict() == {'channel': 'object', 'state': State.Active, 'flags': {'stage': False}}

    m = await client.recv()
    assert client.unpack(m).as_dict() == {'channel': 'test/stage/active', 'state': State.Closed, 'flags': {'stage': True}}

    m = await client.recv()
    assert client.unpack(m).SCHEME.name == 'StateDumpEnd'

    client.post({'dest': 'object', 'data': {'msgid': 10, 'seq': 100, 'addr': 1000, 'type': client.Type.Control, 'data': b'xxx'}}, name='MessageForward')

    m = await oclient.recv()

    assert (m.type, m.msgid, m.seq, m.addr) == (oclient.Type.Control, 10, 100, 1000)
    assert m.data.tobytes() == b'xxx'

@asyncloop_run
async def test_reopen_closed(asyncloop, context, tmp_path):

    cfg = Config.load('''yamls://
name: test
processor.objects:
  null:
    init.url: null://
  middle:
    init: {tll.proto: null}
    depends: null
  leaf:
    init: tcp:///dev/null;mode=client
    depends: middle
''')

    p = Processor(cfg, context=context)
    asyncloop._loop.add(p)

    p.open()

    client = asyncloop.Channel('ipc://;mode=client;dump=yes;name=client;scheme=channel://test', master=p)
    client.open()

    assert len(p.workers) == 1
    for w in p.workers:
        w.open()

    State = client.scheme.messages.StateUpdate.enums['State'].klass

    assert client.unpack(await client.recv()).as_dict() == {'channel': 'null', 'state': State.Opening, 'flags': {'stage': False}}
    assert client.unpack(await client.recv()).as_dict() == {'channel': 'null', 'state': State.Active, 'flags': {'stage': False}}

    assert client.unpack(await client.recv()).as_dict() == {'channel': 'middle', 'state': State.Opening, 'flags': {'stage': False}}
    assert client.unpack(await client.recv()).as_dict() == {'channel': 'middle', 'state': State.Active, 'flags': {'stage': False}}

    assert client.unpack(await client.recv()).as_dict() == {'channel': 'leaf', 'state': State.Opening, 'flags': {'stage': False}}
    assert client.unpack(await client.recv()).as_dict() == {'channel': 'leaf', 'state': State.Error, 'flags': {'stage': False}}
    assert client.unpack(await client.recv()).as_dict() == {'channel': 'leaf', 'state': State.Closing, 'flags': {'stage': False}}
    assert client.unpack(await client.recv()).as_dict() == {'channel': 'leaf', 'state': State.Closed, 'flags': {'stage': False}}

    client.post({'channel': 'null'}, name='ChannelClose')

    assert client.unpack(await client.recv()).as_dict() == {'channel': 'null', 'state': State.Closing, 'flags': {'stage': False}}
    assert client.unpack(await client.recv()).as_dict() == {'channel': 'null', 'state': State.Closed, 'flags': {'stage': False}}

    assert client.unpack(await client.recv()).as_dict() == {'channel': 'middle', 'state': State.Closing, 'flags': {'stage': False}}
    assert client.unpack(await client.recv()).as_dict() == {'channel': 'middle', 'state': State.Closed, 'flags': {'stage': False}}

    assert client.unpack(await client.recv()).as_dict() == {'channel': 'null', 'state': State.Opening, 'flags': {'stage': False}}
    assert client.unpack(await client.recv()).as_dict() == {'channel': 'null', 'state': State.Active, 'flags': {'stage': False}}

    assert client.unpack(await client.recv()).as_dict() == {'channel': 'middle', 'state': State.Opening, 'flags': {'stage': False}}
    assert client.unpack(await client.recv()).as_dict() == {'channel': 'middle', 'state': State.Active, 'flags': {'stage': False}}

    assert client.unpack(await client.recv()).as_dict() == {'channel': 'leaf', 'state': State.Opening, 'flags': {'stage': False}}

@asyncloop_run
async def test_forward_helper(asyncloop, tmp_path):
    asyncloop.context.register(Forward)

    mock = Mock(asyncloop, '''yamls://
mock:
  input: direct://
  output: mem://
name: processor
processor.objects:
  output:
    url: !link /mock/output
  forward:
    url: forward://
    depends: output
    channels: {input: input, output: output}
  input:
    url: !link /mock/input
    depends: forward
''')


    mock.open()

    i, o = mock.io('input', 'output')

    with pytest.raises(TimeoutError):
        await mock.wait('input', 'Active', timeout=0.00001)
    await mock.wait_many(timeout=1, input='Active', output=mock.State.Active)

    mock.io('input').post(b'xxx')
    assert (await o.recv()).data.tobytes() == b'xxx'

@asyncloop_run
async def test_scheme_path(asyncloop, tmp_path):
    filename = 'scheme-path-test.yaml'
    open(tmp_path / filename, 'w').write('''- name: SchemePathTest''')

    with pytest.raises(RuntimeError):
        Scheme('yaml://' + filename)

    mock = Mock(asyncloop, f'''yamls://
name: processor
processor.scheme-path: [{tmp_path}]
processor.objects:
  null:
    url: null://;scheme=yaml://{filename}
''')

    mock.open()

    assert [m.name for m in Scheme('yaml://' + filename).messages] == ['SchemePathTest']

    mock.destroy()

    with pytest.raises(RuntimeError):
        Scheme('yaml://' + filename)
