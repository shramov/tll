#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import tll.channel as C
from tll.channel.base import Base
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
            return

    def _post(self, msg, flags):
        self._callback(msg)

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
