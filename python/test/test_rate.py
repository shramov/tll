#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import tll.channel as C
from tll.config import Config
from tll.error import TLLError
from tll.test_util import Accum
from tll.chrono import TimePoint
from tll.processor import Loop

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

def test_rate_messages(context):
    c = context.Channel('rate+null://;speed=100b;max-window=2b;initial=2b;unit=message;name=rate;dump=frame')
    c.open()

    c.post(b'x' * 1024)
    c.post(b'x' * 1024)
    with pytest.raises(TLLError): c.post(b'x' * 128)
    time.sleep(0.01)
    c.post(b'x' * 128)

def test_rate_buckets(context):
    cfg = Config.load(
f'''yamls://
tll.proto: rate+null
name: rate
dump: frame
speed: 1kb
max-window: 1kb
initial: 1kb
bucket.packets:
  speed: 100b
  max-window: 2b
  initial: 2b
  unit: message
''')
    c = context.Channel(cfg)

    c.open()
    c.post(b'x' * 1024)
    with pytest.raises(TLLError): c.post(b'x' * 128)
    time.sleep(0.01)
    c.post(b'x' * 128)

    c.close()
    c.open()
    c.post(b'x' * 128)
    c.post(b'x' * 128)
    with pytest.raises(TLLError): c.post(b'x' * 128)

def test_rate_buckets_input(context):
    cfg = Config.load(
f'''yamls://
tll.proto: rate+direct
name: rate
dump: frame
rate.dir: in
speed: 1kb
max-window: 1kb
initial: 1kb
bucket.packets:
  speed: 100b
  max-window: 2b
  initial: 2b
  unit: message
''')
    c = Accum(cfg, context = context)
    client = context.Channel('direct://', name='client', master=c)

    c.open()
    client.open()
    client.post(b'x' * 1024)
    assert (c.children[0].dcaps & c.DCaps.Suspend) == c.DCaps.Suspend
    time.sleep(0.01)
    c.children[-1].process()
    assert (c.children[0].dcaps & c.DCaps.Suspend) == c.DCaps.Zero
    c.post(b'x' * 128)

    client.close()
    c.close()
    c.open()
    client.open()

    client.post(b'x' * 128)
    client.post(b'x' * 128)
    assert (c.children[0].dcaps & c.DCaps.Suspend) == c.DCaps.Suspend
    time.sleep(0.01)
    c.children[-1].process()
    assert (c.children[0].dcaps & c.DCaps.Suspend) == c.DCaps.Zero

def test_client(context):
    c = context.Channel('rate+tcp://::1:5555;mode=server;speed=1kb', name='rate')
    c.open()

    assert c.config['client.init.tll.proto'] == 'tcp'

def test_many(context):
    r0 = Accum('rate+null://;initial=32b;max-window=128b;speed=3200b', name='r0', dump='frame', context=context)
    r1 = Accum('rate+null://', name='r1', dump='frame', context=context, master=r0)
    r2 = Accum('rate+null://', name='r2', dump='frame', context=context, master=r0)

    writeFull = r0.scheme_control.messages.WriteFull.msgid
    writeReady = r0.scheme_control.messages.WriteReady.msgid

    loop = Loop()
    assert r0.children[-1].name == "r0/timer"
    loop.add(r0.children[1])

    r0.open()
    r1.open()

    r0.post(b'x' * 64)

    assert [m.msgid for m in r0.result] == [writeFull]
    assert [m.msgid for m in r1.result] == [writeFull]
    assert [m.msgid for m in r2.result] == []

    r2.open()
    assert [m.msgid for m in r2.result] == [writeFull]

    loop.step(0.1)

    assert [m.msgid for m in r0.result] == [writeFull, writeReady]
    assert [m.msgid for m in r1.result] == [writeFull, writeReady]
    assert [m.msgid for m in r2.result] == [writeFull, writeReady]

    for r in [r0, r1, r2]:
        r.result = []

    r1.post(b'x' * 128)
    assert [m.msgid for m in r0.result] == [writeFull]
    assert [m.msgid for m in r1.result] == [writeFull]
    assert [m.msgid for m in r2.result] == [writeFull]

def test_watermark(context):
    cfg = Config.load(
f'''yamls://
tll.proto: rate+null
name: rate
dump: frame
speed: 1kb
max-window: 1kb
initial: 1kb
watermark: 100b
''')
    c = Accum(cfg, context=context)

    c.open()
    c.post(b'x' * 1024)
    assert [(m.type, m.msgid) for m in c.result] == [(c.Type.Control, c.scheme_control['WriteFull'].msgid)]
    c.result.clear()
    time.sleep(0.01)
    c.children[1].process()
    assert [(m.type, m.msgid) for m in c.result] == []
    time.sleep(0.1)
    c.children[1].process()
    assert [(m.type, m.msgid) for m in c.result] == [(c.Type.Control, c.scheme_control['WriteReady'].msgid)]
    c.post(b'x' * 128)
