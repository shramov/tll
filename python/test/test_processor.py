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
    init: null://
  test:
    init: open-test://;shutdown-on=close
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
    init: mem://;size=1mb;dump=frame
  forward:
    init: forward://
    depends: output
    channels: {input: input, output: output}
  input:
    init: mem://;size=1mb;dump=frame
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

def test_default_stage(context):
    cfg = Config.load('''yamls://
name: processor
processor.objects:
  o0:
    init: null://
  o1:
    init: null://
    depends: o0
  o2:
    init: null://
    depends: o1
  o3:
    init: null://
''')

    p = Processor(cfg, context=context)

    objects = p.config.sub('objects')
    assert objects != None
    objects = objects.as_dict()

    assert sorted(objects.keys()) == ['o0', 'o1', 'o2', 'o3', 'processor/stage/active']
    assert objects['processor/stage/active'] == {'depends': 'o2,o3', 'name': 'processor/stage/active'}

@asyncloop_run
async def test_control(asyncloop, context):
    cfg = Config.load('''yamls://
name: test
processor.objects:
  object:
    init: direct://;master=object-client;tll.processor.ignore-master-dependency=yes
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
    assert client.unpack(m).as_dict() == {'channel': 'object', 'state': State.Opening, 'flags': {'stage': False, 'suspend': False}}

    m = await client.recv()
    assert client.unpack(m).as_dict() == {'channel': 'object', 'state': State.Active, 'flags': {'stage': False, 'suspend': False}}

    m = await client.recv()
    assert client.unpack(m).as_dict() == {'channel': 'test/stage/active', 'state': State.Opening, 'flags': {'stage': True, 'suspend': False}}

    m = await client.recv()
    assert client.unpack(m).as_dict() == {'channel': 'test/stage/active', 'state': State.Active, 'flags': {'stage': True, 'suspend': False}}

    client.post({}, name='StateDump')

    context.get('object').suspend()
    try:
        m = await client.recv()
        assert client.unpack(m).as_dict() == {'channel': 'object', 'state': State.Active, 'flags': {'stage': False, 'suspend': True}}
    finally:
        context.get('object').resume()

    m = await client.recv()
    assert client.unpack(m).as_dict() == {'channel': 'test/stage/active', 'state': State.Active, 'flags': {'stage': True, 'suspend': False}}

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

    assert client.unpack(await client.recv()).as_dict(only = {'channel', 'state'}) == {'channel': 'null', 'state': State.Opening}
    assert client.unpack(await client.recv()).as_dict(only = {'channel', 'state'}) == {'channel': 'null', 'state': State.Active}

    assert client.unpack(await client.recv()).as_dict(only = {'channel', 'state'}) == {'channel': 'middle', 'state': State.Opening}
    assert client.unpack(await client.recv()).as_dict(only = {'channel', 'state'}) == {'channel': 'middle', 'state': State.Active}

    assert client.unpack(await client.recv()).as_dict(only = {'channel', 'state'}) == {'channel': 'leaf', 'state': State.Opening}
    assert client.unpack(await client.recv()).as_dict(only = {'channel', 'state'}) == {'channel': 'leaf', 'state': State.Error}
    assert client.unpack(await client.recv()).as_dict(only = {'channel', 'state'}) == {'channel': 'leaf', 'state': State.Closing}
    assert client.unpack(await client.recv()).as_dict(only = {'channel', 'state'}) == {'channel': 'leaf', 'state': State.Closed}

    client.post({'channel': 'null'}, name='ChannelClose')

    assert client.unpack(await client.recv()).as_dict(only = {'channel', 'state'}) == {'channel': 'null', 'state': State.Closing}
    assert client.unpack(await client.recv()).as_dict(only = {'channel', 'state'}) == {'channel': 'null', 'state': State.Closed}

    assert client.unpack(await client.recv()).as_dict(only = {'channel', 'state'}) == {'channel': 'middle', 'state': State.Closing}
    assert client.unpack(await client.recv()).as_dict(only = {'channel', 'state'}) == {'channel': 'middle', 'state': State.Closed}

    assert client.unpack(await client.recv()).as_dict(only = {'channel', 'state'}) == {'channel': 'null', 'state': State.Opening}
    assert client.unpack(await client.recv()).as_dict(only = {'channel', 'state'}) == {'channel': 'null', 'state': State.Active}

    assert client.unpack(await client.recv()).as_dict(only = {'channel', 'state'}) == {'channel': 'middle', 'state': State.Opening}
    assert client.unpack(await client.recv()).as_dict(only = {'channel', 'state'}) == {'channel': 'middle', 'state': State.Active}

    assert client.unpack(await client.recv()).as_dict(only = {'channel', 'state'}) == {'channel': 'leaf', 'state': State.Opening}

@asyncloop_run
async def test_reopen_chain(asyncloop, context):

    cfg = Config.load('''yamls://
name: test
processor.objects:
  c0:
    init: null://
  c1:
    init: null://
    depends: c0
  c2:
    init: null://
    depends: c1
''')

    p = Processor(cfg, context=context)
    asyncloop._loop.add(p)

    p.open()

    client = asyncloop.Channel('ipc://;mode=client;dump=yes;name=client;scheme=channel://test', master=p)
    client.open()

    assert len(p.workers) == 1
    for w in p.workers:
        w.open()

    async def check_state(c, s):
        assert client.unpack(await client.recv(0.1)).as_dict(only = {'channel', 'state'}) == {'channel': c, 'state': s}

    State = client.scheme.messages.StateUpdate.enums['State'].klass

    for c in ['c0', 'c1', 'c2', 'test/stage/active']:
        await check_state(c, State.Opening)
        await check_state(c, State.Active)

    client.post({'channel': 'c0'}, name='ChannelClose')

    for c in ['c0', 'test/stage/active', 'c2', 'c1']:
        await check_state(c, State.Closing)
        await check_state(c, State.Closed)

    for c in ['c0', 'c1', 'c2', 'test/stage/active']:
        await check_state(c, State.Opening)
        await check_state(c, State.Active)

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
    init: !link /mock/output
  forward:
    init: forward://
    depends: output
    channels: {input: input, output: output}
  input:
    init: !link /mock/input
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
    init: null://;scheme=yaml://{filename}
''')

    mock.open()

    assert [m.name for m in Scheme('yaml://' + filename).messages] == ['SchemePathTest']

    mock.destroy()

    with pytest.raises(RuntimeError):
        Scheme('yaml://' + filename)

@asyncloop_run
async def test_close_decay(asyncloop, context):

    cfg = Config.load('''yamls://
name: processor
processor.objects:
  root-0:
    init: null://
  root-1:
    init: null://
  leaf:
    init: null://
    depends: root-0, root-1
''')

    p = Processor(cfg, context=context)
    asyncloop._loop.add(p)

    p.open()

    client = asyncloop.Channel('ipc://;mode=client;dump=yes;name=client;scheme=channel://processor', master=p)
    client.open()

    assert len(p.workers) == 1
    for w in p.workers:
        w.open()

    State = client.scheme.messages.StateUpdate.enums['State'].klass

    async def check_state(c, s):
        assert client.unpack(await client.recv()).as_dict(only = {'channel', 'state'}) == {'channel': c, 'state': s}

    for c in ['root-0', 'root-1', 'leaf', 'processor/stage/active']:
        await check_state(c, State.Opening)
        await check_state(c, State.Active)

    p.close()

    context.get('root-0').close()

    for c in ['root-0', 'processor/stage/active', 'leaf', 'root-1']:
        await check_state(c, State.Closing)
        await check_state(c, State.Closed)
    assert p.state == p.State.Closed

class LongClose(Base):
    PROTO = "long-close"

    CLOSE_POLICY = Base.ClosePolicy.Long
    POST_CLOSING_POLICY = Base.PostPolicy.Enable
    PROCESS_POLICY = Base.ProcessPolicy.Never

    def _close(self, force : bool):
        if force:
            return Base._close(self, force)

    def _post(self, msg, flags):
        if self.state == self.State.Closing:
            Base._close(self, True)

@asyncloop_run
async def test_close_pending_reopen(asyncloop, context):
    context.register(LongClose)

    cfg = Config.load('''yamls://
name: processor
processor.objects:
  root:
    init: null://
  l0:
    init: long-close://
    depends: root
  l1:
    init: null://;reopen-timeout=10ms;reopen-active-min=1s
    depends: root
''')

    mock = Mock(asyncloop, cfg)
    mock.open()

    State = mock._control.scheme.messages.StateUpdate.enums['State'].klass

    async def wait_state(c, s):
        while True:
            m = mock.control.unpack(await mock.control.recv(0.01))
            if m.as_dict(only = {'channel', 'state'}) == {'channel': c, 'state': s}:
                break

    await wait_state('processor/stage/active', State.Active)

    context.get('l1').close()
    await wait_state('processor/stage/active', State.Closed)
    mock._processor.close()

    await asyncloop.sleep(0.015) # Wait more then reopen timeout

    context.get('l0').post(b'')

    await mock.wait('root', 'Closed', timeout=1)
