#!/usr/bin/env python3
# vim: sts=4 sw=4 et

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
