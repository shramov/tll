#!/usr/bin/env python3
# vim: sts=4 sw=4 et

from tll.channel import Context
from tll.channel.base import Base
from tll.error import TLLError
from tll.logger import Logger
import tll.channel.reopen as R

import pytest
import time

class Reopen(Base):
    PROTO = "reopen"
    CHILD_POLICY = Base.ChildPolicy.Many

    def _init(self, cfg, master=None):
        self._child = self.context.Channel(self._child_url_parse('file:///nonexistent', 'file'))
        self._child_add(self._child)
        self._reopen = R.Reopen(self, cfg)
        self._reopen.set_channel(self._child)

    def _open(self, cfg):
        self._reopen.open()
        return super()._open(cfg)

    def _close(self):
        self._reopen.close()
        super()._close()


@pytest.fixture
def context():
    return Context()

def test(context):
    with pytest.raises(TLLError): R.ReopenData(Logger('test.reopen'), {'reopen-timeout-min':'10msx'})
    r = R.ReopenData(Logger('test.reopen'))

    c = context.Channel('file:///nonexistent', name='file')
    r.set_channel(c)

    assert r.next == 0.

    with pytest.raises(TLLError): c.open()
    assert c.state == c.State.Error
    assert r.next > 0

    assert r.on_timer(r.next + 0.001) == R.Action.Close
    c.close()
    assert r.next > 0

    assert r.on_timer(r.next + 0.001) == R.Action.Open

def test_channel(context):
    context.register(Reopen)
    c = context.Channel('reopen://;reopen-timeout-min=1ms', name='reopen')

    assert [i.name for i in c.children] == ['reopen/file', 'reopen/reopen-timer']

    c.open()
    assert c.children[0].state == c.State.Error
    assert c.children[1].state == c.State.Active

    time.sleep(0.001)
    c.children[1].process()
    assert c.children[0].state == c.State.Closed
