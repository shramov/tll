#!/usr/bin/env python3
# vim: sts=4 sw=4 et

from tll import asynctll
from tll.channel import Context
from tll.channel.base import Base
from tll.channel.logic import Logic
from tll.config import Config
from tll.processor import Processor

import pytest

class OpenTest(Base):
    PROTO = "open-test"

    OPEN_POLICY = Base.OpenPolicy.Manual
    PROCESS_POLICY = Base.ProcessPolicy.Never

    STATE_PARAM = None

    def _open(self, props):
        v = props.get('state', None)

        OpenTest.STATE_PARAM = v
        self.close()

def test_open_link():
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

    ctx = Context()
    ctx.register(OpenTest)
    cfg.copy().copy()

    p = Processor(cfg, context=ctx)
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

def test_forward():
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

    ctx = Context()
    ctx.register(Forward)

    p = Processor(cfg, context=ctx)
    p.open()

    loop = asynctll.Loop(context=ctx)
    loop._loop.add(p)

    assert len(p.workers) == 1
    for w in p.workers:
        w.open()

    async def main(loop):
        ci = loop.Channel('mem://;master=input')
        co = loop.Channel('mem://;master=output')

        await loop.sleep(0.01)

        pi = ctx.get('input')
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
