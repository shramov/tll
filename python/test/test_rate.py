#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import tll.channel as C
from tll.error import TLLError
from tll.test_util import Accum

import pytest
import select
import time

@pytest.fixture
def context():
    return C.Context()

def test_post(context):
    c = Accum('rate+null://;initial=32b;max-window=128b;speed=3200b', name='rate', dump='frame', context=context)

    for _ in range(2):
        c.result = []
        c.open()
        c.post(b'x' * 64)

        timer = c.children[1]

        with pytest.raises(TLLError): c.post(b'x' * 64)
        assert [(m.type, m.msgid) for m in c.result] == [(c.Type.Control, c.scheme_control.messages.WriteFull.msgid)]

        poll = select.poll()
        poll.register(timer.fd, select.POLLIN)
        assert poll.poll(20) != []

        c.result = []
        timer.process()
        assert [(m.type, m.msgid) for m in c.result] == [(c.Type.Control, c.scheme_control.messages.WriteReady.msgid)]

        c.result = []
        c.post(b'x' * 64)
        assert [(m.type, m.msgid) for m in c.result] == [(c.Type.Control, c.scheme_control.messages.WriteFull.msgid)]

        c.close()
