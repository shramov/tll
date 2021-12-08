#!/usr/bin/env python3
# vim: sts=4 sw=4 et

from tll.channel import Context
from tll.channel.base import Base
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
