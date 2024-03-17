#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import pytest

from tll.chrono import *
from datetime import timedelta

D = Duration
T = TimePoint

def test_convert():
    assert str(D(123, 'second', int).convert('ms')) == '123000ms'
    assert str(D(123, 'ms', int).convert(Resolution.second, float)) == '0.123s'
    assert str(D(123, 'us', int).convert('ns')) == '123000ns'
    assert str(D(123456, 'ns', int).convert('us')) == '123us'

def test_math():
    assert str(D(1, 'us', int) + D('2', 'ms', int)) == '2001us'
    assert str(D(1, 'us', int) - D('2', 'ms', int)) == '-1999us'
    assert str(D(2, 'ms', int) + D('1', 'us', int)) == '2ms'
    assert str(D(2, 'ms', int) - D('1', 'us', int)) == '2ms'
    assert str(D(2, 'ms', float) + D('1', 'us', int)) == '2.001ms'
    assert str(D(2, 'ms', float) - D('1', 'us', int)) == '1.999ms'

def test_str():
    assert str(Duration(100, Resolution.ns, type=float)) == '100.0ns'
    assert str(Duration(100, 'ns', type=int)) == '100ns'
    assert str(Duration(100, (1, 1000000000), type=int)) == '100ns'

    assert str(Duration(100, Resolution.us, type=int)) == '100us'
    assert str(Duration(100, Resolution.ms, type=int)) == '100ms'
    assert str(Duration(100, Resolution.second, type=int)) == '100s'
    assert str(Duration(100, Resolution.minute, type=int)) == '100m'
    assert str(Duration(100, Resolution.hour, type=int)) == '100h'
    assert str(Duration(100, Resolution.day, type=int)) == '100d'

def test_from_str():
    assert Duration.from_str('100ns') == Duration(100, Resolution.ns, type=int)
    assert Duration.from_str('-100ns') == Duration(-100, Resolution.ns, type=int)
    assert Duration.from_str('100.0ns') == Duration(100, Resolution.ns, type=float)
    assert Duration.from_str('1e2ns') == Duration(100.0, Resolution.ns, type=float)

    assert Duration.from_str('100us') == Duration(100, Resolution.us, type=int)
    assert Duration.from_str('100ms') == Duration(100, Resolution.ms, type=int)
    assert Duration.from_str('100s') == Duration(100, Resolution.second, type=int)
    assert Duration.from_str('100m') == Duration(100, Resolution.minute, type=int)
    assert Duration.from_str('100h') == Duration(100, Resolution.hour, type=int)
    assert Duration.from_str('100d') == Duration(100, Resolution.day, type=int)

def test_duration_eq():
    assert Duration(100, 'us').timedelta == timedelta(microseconds=100)
    assert Duration(100, 'us') == timedelta(microseconds=100)
    assert Duration(100000, 'ns') == timedelta(microseconds=100)
    assert Duration(100100, 'ns') != timedelta(microseconds=100)
    assert Duration(100, 'ns') != timedelta(microseconds=0)

    assert Duration(100, 'ns') != 100
    with pytest.raises(TypeError): Duration(100, 'ns') < 100
