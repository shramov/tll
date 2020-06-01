#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import tll.channel as C
from tll.config import Config
from tll.error import TLLError
from tll.test_util import Accum

import common

from nose.tools import *
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

    assert_not_equals(c.fd, -1)
    assert_not_equals(c.scheme, None)

def _test(init='', open='', wait=[]):
    c = Accum('timer://;{}'.format(init), name='timer', dump='yes', context=ctx)
    MSGID = 2

    c.open(open)

    poll = select.poll()
    poll.register(c.fd, select.POLLIN)

    assert_equals(c.result, [])
    assert_equals(poll.poll(0), [])

    for w in wait:
        if w is None:
            return
        ts = str2ms(w)
        print("Check {}: {:.3f}ms".format(w, ts))
        assert_equals(c.dcaps, c.DCaps.Process | c.DCaps.PollIn)
        assert_equals(poll.poll(0), [])
        c.process()
        assert_equals(c.dcaps, c.DCaps.Process | c.DCaps.PollIn)
        assert_equals([m.msgid for m in c.result], [])
        dt = time.time()
        assert_equals(poll.poll(2 * ts), [(c.fd, select.POLLIN)])
        dt = 1000 * (time.time() - dt)
        assert_true(ts / 2 < dt < 1.5 * ts, "Sleep time {:.3f}ms not in range {:.3f}ms < {:.3f}ms)".format(dt, ts / 2, 1.5 * ts))
        c.process()
        assert_equals([m.msgid for m in c.result], [MSGID])
        c.result = []
    assert_equals(poll.poll(0), [])
    assert_equals(c.dcaps, 0)

def test():
    yield _test, '', '', []
    yield _test, 'initial=5ms', '', ['5ms']
    yield _test, '', 'initial=5ms', ['5ms']
    yield _test, 'interval=10ms', '', ['10ms', '10ms', None]
    yield _test, '', 'interval=10ms', ['10ms', '10ms', None]
    yield _test, 'initial=5ms;interval=10ms', '', ['5ms', '10ms', None]
    yield _test, '', 'initial=5ms;interval=10ms', ['5ms', '10ms', None]
    yield _test, 'initial=5ms', 'interval=10ms', ['5ms', '10ms', None]
    yield _test, 'interval=10ms', 'initial=5ms', ['5ms', '10ms', None]
    yield _test, 'clock=realtime;interval=10ms', 'initial=5ms', ['5ms', '10ms', None]

def _test_post_clear(clock, msg, fail=False):
    c = Accum('timer://', name='timer', clock=clock, dump='yes', context=ctx)

    c.open(initial='10ms')

    assert_equals(c.result, [])
    assert_equals(c.dcaps, c.DCaps.Process | c.DCaps.PollIn)
    try:
        c.post({'ts':0}, name=msg)
        assert_equals(c.dcaps, 0)
    except TLLError as e:
        if fail != 'fail': raise
    else:
        assert_false(fail == 'fail', "Exception not raised in post")

def test_post_clear():
    yield _test_post_clear, 'realtime', 'relative'
    yield _test_post_clear, 'realtime', 'absolute'
    yield _test_post_clear, 'monotonic', 'relative'
    yield _test_post_clear, 'monotonic', 'absolute', 'fail'

def _test_post(clock, msg, wait):
    ms = str2ms(wait)

    c = Accum('timer://', name='timer', clock=clock, dump='yes', context=ctx)

    c.open()

    poll = select.poll()
    poll.register(c.fd, select.POLLIN)

    assert_equals(c.result, [])
    assert_equals(c.dcaps, 0)

    ts = ms / 1000
    if name == 'absolute':
        ts += time.time() * 1000000000
    c.post({'ts':ts * 1000000000}, name=msg) # nanoseconds

    assert_equals(c.dcaps, c.DCaps.Process | c.DCaps.PollIn)
    assert_equals(poll.poll(0), [])

    dt = time.time()
    assert_equals(poll.poll(2 * ms), [(c.fd, select.POLLIN)])
    dt = 1000 * (time.time() - dt)
    assert_true(ts / 2 < dt < 1.5 * ts, "Sleep time {:.3f}ms not in range {:.3f}ms < {:.3f}ms)".format(dt, ts / 2, 1.5 * ts))

    c.process()
    assert_equals([m.msgid for m in c.result], [MSGID])
    assert_equals(c.dcaps, 0)

def test_post():
    yield _test_post_clear, 'realtime', 'relative', '3ms'
    yield _test_post_clear, 'realtime', 'absolute', '3ms'
    yield _test_post_clear, 'monotonic', 'relative', '3ms'
