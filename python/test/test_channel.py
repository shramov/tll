#!/usr/bin/env python3
# vim: sts=4 sw=4 et

from tll import asynctll
import tll.channel as C
from tll.channel.base import Base
from tll.channel.prefix import Prefix
from tll.channel.logic import Logic
from tll.error import TLLError
from tll.test_util import Accum

import common

from nose.tools import *

class Echo(Base):
    PROTO = "echo"

    OPEN_POLICY = Base.OpenPolicy.Manual
    CHILD_POLICY = Base.ChildPolicy.Many

    def _init(self, props, master=None):
        self._child = None
        pass

    def _open(self, props):
        self._child = self.context.Channel('null://;name=child;tll.internal=yes')
        self._child_add(self._child)
        self._child_add(self.context.Channel('null://;name=orphan'))

    def _close(self):
        self._child_del(self._child)

    def _process(self, timeout, flags):
        if self.state == C.State.Opening:
            self.state = C.State.Active

    def _post(self, msg, flags):
        self._callback(msg.copy())

class TestPrefix(Prefix):
    PROTO = "prefix"

    def _init(self, url, master=None):
        super()._init(url, master)

    def _open(self, props):
        super()._open(props)

class TestLogic(Logic):
    PROTO = "logic"

    def _init(self, url, master=None):
        super()._init(url, master)

        if len(self._channels.get('input', [])) != 1:
            raise RuntimeError("Need exactly one input, got: {}".format([c.name for c in self._channels.get('output', [])]))
        self._input = self._channels['input'][0]
        if len(self._channels.get('output', [])) != 1:
            raise RuntimeError("Need exactly one output, got: {}".format([c.name for c in self._channels.get('output', [])]))
        self._input = self._channels['input'][0]
        self._output = self._channels['output'][0]

    def _open(self, props):
        super()._open(props)

    def _logic(self, channel, msg):
        if channel != self._input:
            return
        if msg.type != msg.Type.Data:
            return
        self._output.post(msg)

def test():
    ctx = C.Context()

    assert_raises(TLLError, ctx.Channel, "echo://;name=echo")
    ctx.register(Echo)
    c = Accum("echo://;name=echo", context=ctx)
    cfg = c.config

    assert_equals(c.state, c.State.Closed)
    assert_equals(cfg.get("state", ""), "Closed")
    assert_equals([x.name for x in c.children], [])

    c.open()

    assert_equals([x.name for x in c.children], ['child', 'orphan'])

    assert_equals(c.state, c.State.Opening)
    assert_equals(cfg.get("state", ""), "Opening")

    c.process()

    assert_equals(c.state, c.State.Active)
    assert_equals(cfg.get("state", ""), "Active")

    assert_equals(c.result, [])
    c.post(b'xxx', seq=100)
    assert_equals([(m.seq, m.data.tobytes()) for m in c.result], [(100, b'xxx')])

    c.close()
    assert_equals([x.name for x in c.children], ['orphan'])
    del c

    assert_equals(ctx.get('orphan'), None)

    ctx.unregister(Echo)
    assert_raises(TLLError, ctx.Channel, "echo://;name=echo")

def test_prefix():
    ctx = C.Context()

    assert_raises(TLLError, ctx.Channel, "prefix+null://;name=channel")
    ctx.register(Echo)
    ctx.register(TestPrefix)
    c = Accum("prefix+echo://;name=channel", context=ctx)
    cfg = c.config

    assert_equals(c.state, c.State.Closed)
    assert_equals(cfg.get("state", ""), "Closed")
    assert_equals([x.name for x in c.children], ['channel/prefix'])

    c.open()

    assert_equals([x.name for x in c.children], ['channel/prefix'])

    assert_equals(c.state, c.State.Opening)
    assert_equals(cfg.get("state", ""), "Opening")

    c.process()

    assert_equals(c.state, c.State.Opening)
    assert_equals(cfg.get("state", ""), "Opening")

    c.children[0].process()

    assert_equals(c.state, c.State.Active)
    assert_equals(cfg.get("state", ""), "Active")

    assert_equals(c.result, [])
    c.post(b'xxx', seq=100)
    assert_equals([(m.seq, m.data.tobytes()) for m in c.result], [(100, b'xxx')])

    c.close()
    assert_equals([x.name for x in c.children], ['channel/prefix'])
    del c

    ctx.unregister(TestPrefix)
    assert_raises(TLLError, ctx.Channel, "prefix+null://;name=channel")

def test_prefix():
    ctx = C.Context()

    assert_raises(TLLError, ctx.Channel, "prefix+null://;name=channel")
    ctx.register(Echo)
    ctx.register(TestPrefix)
    c = Accum("prefix+echo://;name=channel", context=ctx)
    cfg = c.config

    assert_equals(c.state, c.State.Closed)
    assert_equals(cfg.get("state", ""), "Closed")
    assert_equals([x.name for x in c.children], ['channel/prefix'])

    c.open()

    assert_equals([x.name for x in c.children], ['channel/prefix'])

    assert_equals(c.state, c.State.Opening)
    assert_equals(cfg.get("state", ""), "Opening")

    c.process()

    assert_equals(c.state, c.State.Opening)
    assert_equals(cfg.get("state", ""), "Opening")

    c.children[0].process()

    assert_equals(c.state, c.State.Active)
    assert_equals(cfg.get("state", ""), "Active")

    assert_equals(c.result, [])
    c.post(b'xxx', seq=100)
    assert_equals([(m.seq, m.data.tobytes()) for m in c.result], [(100, b'xxx')])

    c.close()
    assert_equals([x.name for x in c.children], ['channel/prefix'])
    del c

    ctx.unregister(TestPrefix)
    assert_raises(TLLError, ctx.Channel, "prefix+null://;name=channel")

def test_logic():
    ctx = C.Context()

    assert_raises(TLLError, ctx.Channel, "logic://;name=logic")
    ctx.register(TestLogic)

    assert_raises(TLLError, ctx.Channel, "logic://;name=logic;tll.channel.input=input;tll.channel.output=input")

    i = ctx.Channel('mem://;name=input;dump=yes')
    o = Accum('mem://;name=output;dump=yes', master=i, context=ctx)
    l = ctx.Channel("logic://;name=logic;tll.channel.input=input;tll.channel.output=input")

    l.open()

    i.open()
    o.open()

    o.post(b'xxx')

    assert_equals([m.data.tobytes() for m in o.result], [])

    i.process()

    assert_equals([m.data.tobytes() for m in o.result], [])

    o.process()

    assert_equals([m.data.tobytes() for m in o.result], [b'xxx'])

class _test_base_logic:
    CHANNELS = {}
    SCHEME = None

    def __new__(cls):
        obj = object.__new__(cls)
        for k,v in cls.__dict__.items():
            if not k.startswith('async_test') or not callable(v):
                continue
            #print("Wrap {}: {}".format(k, v))
            def wrap(x):
                def f(*a, **kw):
                    future = x(obj, *a, **kw)
                    if future is None:
                        return
                    obj.loop.run(future)
                return f
            setattr(obj, k, wrap(v))
        return obj

    def setup(self):
        self.channels = {}
        self.ctx = C.Context()
        self.loop = asynctll.Loop(self.ctx)
        channels = set()
        for k,l in self.CHANNELS.items():
            if not l: continue
            for c in [x.strip() for x in l.split(',')]:
                if c == '':
                    raise ValueError("Empty channel name in '{}'", l)
                channels.add(c)
        kw = {}
        if self.SCHEME:
            kw['scheme'] = self.SCHEME
        for c in channels:
            self.channels['test/' + c] = self.loop.Channel('direct://;name=test/{name}'.format(name=c), **kw)
            self.channels[c] = self.loop.Channel('direct://;name={name};master=test/{name}'.format(name=c), **kw)

        for n,c in self.channels.items():
            if n.startswith('test/'):
                c.open()

    def teardown(self):
        for c in self.channels.values():
            c.close()
        self.channels = None
        self.loop = None
        self.ctx = None

class test_logic_async(_test_base_logic):
    CHANNELS = {'input': 'input', 'output': 'output'}
    SCHEME = 'yamls://[{name: msg, id: 10, fields: [{name: f0, type: int32}]}]'

    async def async_test(self):
        self.ctx.register(TestLogic)
        logic = self.loop.Channel('logic://;name=logic;tll.channel.input=input;tll.channel.output=output')
        logic.open()

        i, o = self.channels['test/input'], self.channels['test/output']

        self.channels['input'].open()
        self.channels['output'].open()

        msg = C.Message(msgid=10, data=i.scheme['msg'].object(f0=0xbeef).pack())

        i.post(msg, seq=10)
        i.post(b'zzz', seq=20)

        m = await o.recv()
        assert_equals(m.data.tobytes(), b'\xef\xbe\x00\x00')
        assert_equals(m.seq, 10)

        m = await o.recv()
        assert_equals(m.data.tobytes(), b'zzz')
        assert_equals(m.seq, 20)

def test_logic_async_func():
    ctx = C.Context()
    ctx.register(TestLogic)
    loop = asynctll.Loop(ctx)
    async def main(loop):
        i, o = loop.Channel('direct://;name=test/input'), loop.Channel('direct://;name=test/output')
        li, lo = loop.Channel('direct://;name=input;master=test/input'), loop.Channel('direct://;name=output;master=test/output')
        i.open(), o.open()
        li.open(), lo.open()

        logic = loop.Channel('logic://;name=logic;tll.channel.input=input;tll.channel.output=output')
        logic.open()

        i.post(b'xxx', seq=10)
        i.post(b'zzz', seq=20)

        m = await o.recv()
        assert_equals(m.data.tobytes(), b'xxx')
        assert_equals(m.seq, 10)

        m = await o.recv()
        assert_equals(m.data.tobytes(), b'zzz')
        assert_equals(m.seq, 20)

    loop.run(main(loop))
