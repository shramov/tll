#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import tll.channel as C
from tll.config import Config
from tll.error import TLLError
from tll.test_util import Accum

import pytest
import select
import time

ctx = C.Context()

def str2ms(s):
    if s[-2:] != 'ms':
        raise ValueError("Expected 'ms' suffix in '{}'".format(s))
    return float(s[:-2])

def test_properties():
    c = ctx.Channel('timer://')
    c.open()

    assert c.fd != -1
    assert c.scheme != None


@pytest.mark.parametrize("init,open,wait", [
    ('', '', []),
    ('initial=5ms', '', ['5ms']),
    ('', 'initial=5ms', ['5ms']),
    ('interval=10ms', '', ['10ms', '10ms', None]),
    ('', 'interval=10ms', ['10ms', '10ms', None]),
    ('initial=5ms;interval=10ms', '', ['5ms', '10ms', None]),
    ('', 'initial=5ms;interval=10ms', ['5ms', '10ms', None]),
    ('initial=5ms', 'interval=10ms', ['5ms', '10ms', None]),
    ('interval=10ms', 'initial=5ms', ['5ms', '10ms', None]),
    ('clock=realtime;interval=10ms', 'initial=5ms', ['5ms', '10ms', None]),
])
def test(init, open, wait):
    c = Accum('timer://;{}'.format(init), name='timer', dump='yes', context=ctx)
    MSGID = 2

    c.open(open)

    poll = select.poll()
    poll.register(c.fd, select.POLLIN)

    assert c.result == []
    assert poll.poll(0) == []

    for w in wait:
        if w is None:
            return
        ts = str2ms(w)
        print("Check {}: {:.3f}ms".format(w, ts))
        assert c.dcaps == c.DCaps.Process | c.DCaps.PollIn
        assert poll.poll(0) == []
        c.process()
        assert c.dcaps == c.DCaps.Process | c.DCaps.PollIn
        assert [m.msgid for m in c.result] == []
        dt = time.time()
        assert poll.poll(2 * ts), [(c.fd == select.POLLIN)]
        dt = 1000 * (time.time() - dt)
        assert ts / 2 < dt < 2 * ts, "Sleep time {:.3f}ms not in range {:.3f}ms < {:.3f}ms)".format(dt, ts / 2, 2 * ts)
        c.process()
        assert [m.msgid for m in c.result] == [MSGID]
        c.result = []
    assert poll.poll(0) == []
    assert c.dcaps == 0

@pytest.mark.parametrize("clock,msg,fail", [
    ('realtime', 'relative', ''),
    ('realtime', 'absolute', ''),
    ('monotonic', 'relative', ''),
    ('monotonic', 'absolute', 'fail'),
])
def test_post_clear(clock, msg, fail):
    c = Accum('timer://', name='timer', clock=clock, dump='yes', context=ctx)

    c.open(initial='10ms')

    assert c.result == []
    assert c.dcaps == c.DCaps.Process | c.DCaps.PollIn

    if fail  == 'fail':
        with pytest.raises(TLLError): c.post({'ts':0}, name=msg)
    else:
        c.post({'ts':0}, name=msg)
        assert c.dcaps == 0

@pytest.mark.parametrize("clock,msg,wait", [
    ('realtime', 'relative', '3ms'),
    ('realtime', 'absolute', '3ms'),
    ('monotonic', 'relative', '3ms'),
])
def test_post(clock, msg, wait):
    ms = str2ms(wait)

    c = Accum('timer://', name='timer', clock=clock, dump='yes', context=ctx)

    c.open()

    poll = select.poll()
    poll.register(c.fd, select.POLLIN)

    assert c.result == []
    assert c.dcaps == 0

    ts = ms / 1000
    if msg == 'absolute':
        ts += time.time()
    c.post({'ts':ts}, name=msg) # nanoseconds

    assert c.dcaps == c.DCaps.Process | c.DCaps.PollIn
    assert poll.poll(0) == []

    dt = time.time()
    assert poll.poll(2 * ms) == [(c.fd, select.POLLIN)]
    dt = 1000 * (time.time() - dt)
    assert not (ts / 2 < dt < 2 * ts), "Sleep time {:.3f}ms not in range {:.3f}ms < {:.3f}ms)".format(dt, ts / 2, 2 * ts)

    c.process()
    assert [m.msgid for m in c.result] == [2]
    assert c.dcaps == 0
