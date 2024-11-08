#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import tll.channel as C
from tll.error import TLLError
from tll.test_util import Accum
from tll.chrono import TimePoint

import pytest
import select
import time

@pytest.fixture
def context():
    return C.Context()

def test_post(context):
    c = Accum('rate+null://;initial=32b;max-window=128b;speed=3200b', name='rate', dump='frame', context=context)

    assert [x.name for x in c.children] == ['rate/rate', 'rate/timer']
    timer = c.children[1]

    assert c.scheme_control is not None
    assert [m.name for m in c.scheme_control.messages] == ["WriteFull", "WriteReady"]

    for _ in range(2):
        c.result = []
        c.open()
        c.post(b'x' * 64)

        with pytest.raises(TLLError): c.post(b'x' * 64)
        assert [(m.type, m.msgid) for m in c.result] == [(c.Type.Control, c.scheme_control.messages.WriteFull.msgid)]

        if timer.fd:
            poll = select.poll()
            poll.register(timer.fd, select.POLLIN)
            assert poll.poll(20) != []
        else:
            time.sleep(0.02)

        c.result = []
        timer.process()
        assert [(m.type, m.msgid) for m in c.result] == [(c.Type.Control, c.scheme_control.messages.WriteReady.msgid)]

        c.result = []
        c.post(b'x' * 64)
        assert [(m.type, m.msgid) for m in c.result] == [(c.Type.Control, c.scheme_control.messages.WriteFull.msgid)]

        c.close()

def test_input(context):
    c = Accum('rate+zero://;initial=32b;max-window=128b;speed=3200b;size=64b;rate.dir=r', name='rate', dump='frame', context=context)

    assert c.scheme_control is None

    assert [x.name for x in c.children] == ['rate/rate', 'rate/timer']
    child, timer = c.children

    for _ in range(2):
        c.result = []
        c.open()
        assert (child.dcaps & child.DCaps.Suspend) == 0

        c.post(b'x' * 1024)
        c.post(b'x' * 1024)

        child.process()
        assert [(m.type, len(m.data)) for m in c.result] == [(c.Type.Data, 64)]

        assert child.dcaps & child.DCaps.Suspend

        if timer.fd:
            poll = select.poll()
            poll.register(timer.fd, select.POLLIN)
            assert poll.poll(20) != []
        else:
            time.sleep(0.02)

        timer.process()

        assert (child.dcaps & child.DCaps.Suspend) == 0
        child.process()

        assert [(m.type, len(m.data)) for m in c.result] == [(c.Type.Data, 64), (c.Type.Data, 64)]
        assert child.dcaps & child.DCaps.Suspend

        c.close()

    c.open()
    child.process()

    assert child.dcaps & child.DCaps.SuspendPermanent
    assert child.dcaps & child.DCaps.Suspend

    c.suspend()
    assert c.dcaps & child.DCaps.SuspendPermanent
    assert c.dcaps & child.DCaps.Suspend

    c.close()
    assert (child.dcaps & child.DCaps.SuspendPermanent) == 0
    assert child.dcaps & child.DCaps.Suspend

    c.resume()

    assert (child.dcaps & child.DCaps.SuspendPermanent) == 0
    assert (child.dcaps & child.DCaps.Suspend) == 0

def test_timeline(context):
    TP = lambda v: TimePoint(v, 'ns', int)

    c = Accum('timeline+direct://;speed=0.1', name='timeline', dump='frame', context=context)
    s = context.Channel('direct://', name='source', dump='frame', master=c)

    assert [x.name for x in c.children] == ['timeline/timeline', 'timeline/timer']
    child, timer = c.children

    c.open()
    s.open()

    s.post(b'xxx', seq=0)

    assert [(m.data.tobytes(), m.seq) for m in c.result] == [(b'xxx', 0)]

    s.post(b'xxx', seq=1, time=100)

    assert [(m.seq, m.time) for m in c.result] == [(0, TP(0)), (1, TP(100))]

    s.post(b'xxx', seq=2, time=200)

    assert [(m.seq, m.time) for m in c.result] == [(0, TP(0)), (1, TP(100)), (2, TP(200))]

    s.post(b'xxx', seq=3, time=1000100)

    assert [(m.seq, m.time) for m in c.result] == [(0, TP(0)), (1, TP(100)), (2, TP(200))]

    assert child.dcaps & child.DCaps.Suspend

    time.sleep(0.01)
    timer.process()

    assert [(m.seq, m.time) for m in c.result] == [(0, TP(0)), (1, TP(100)), (2, TP(200)), (3, TP(1000100))]

    assert (child.dcaps & child.DCaps.Suspend) == 0

    s.post(b'xxxzzz', seq=3, time=2000100)

    child.process()

    assert child.dcaps & child.DCaps.SuspendPermanent
    assert child.dcaps & child.DCaps.Suspend

    c.suspend()
    assert c.dcaps & child.DCaps.SuspendPermanent
    assert c.dcaps & child.DCaps.Suspend

    c.close()
    assert (child.dcaps & child.DCaps.SuspendPermanent) == 0
    assert child.dcaps & child.DCaps.Suspend

    c.resume()

    assert (child.dcaps & child.DCaps.SuspendPermanent) == 0
    assert (child.dcaps & child.DCaps.Suspend) == 0

def test_rate_urgent(context):
    c = context.Channel('rate+null://;speed=1kb;max-window=1kb;name=rate;dump=frame')
    c.open()

    c.post(b'x' * 1024)
    with pytest.raises(TLLError): c.post(b'x' * 128)
    c.post(b'x' * 128, flags=c.PostFlags.Urgent)
    with pytest.raises(TLLError): c.post(b'x' * 128)
