#!/usr/bin/env python3
# vim: sts=4 sw=4 et

from nose.tools import *

from tll.url import Url, Params
from tll.conv import PrefixedDict, ChainedDict

def _test_parse(s, d):
    u = Params.from_string(s)
    assert_equals(dict(u), d)

def _test_parse_url(s, proto, host, d):
    u = Url.from_string(s)
    assert_equals(u.proto, proto)
    assert_equals(u.host, host)
    assert_equals(dict(u), d)

def _test_parse_invalid(klass, s):
    assert_raises(ValueError, klass.from_string, s)

def test_parse_url():
    yield _test_parse_url, 'a://', 'a', '', {}
    yield _test_parse_url, 'a://;', 'a', '', {}
    yield _test_parse_url, 'a://;;', 'a', '', {}
    yield _test_parse_url, 'a://b', 'a', 'b', {}

def test_parse():
    yield _test_parse, '', {}
    yield _test_parse, ';', {}
    yield _test_parse, ';;', {}
    yield _test_parse, ';a=b;', {'a':'b'}

def test_parse_invalid():
    yield _test_parse_invalid, Params, 'a'
    yield _test_parse_invalid, Params, 'a=1;a=2'

    yield _test_parse_invalid, Url, ''
    yield _test_parse_invalid, Url, 'a:/b'
    yield _test_parse_invalid, Url, 'a://b;c'
    yield _test_parse_invalid, Url, 'a://b;c;'

def _test_getT(s, k, t, v):
    p = Params.from_string(s) if isinstance(s, str) else s
    if v is None:
        assert_raises(ValueError, p.getT, k, t())
    else:
        assert_equals(p.getT(k, t()), v)

def test_getT():
    yield _test_getT, Params(a=10), 'a', int, 10
    yield _test_getT, Url(a='10'), 'a', int, 10
    yield _test_getT, 'a=10', 'a', int, 10
    yield _test_getT, 'a=10', 'a', int, 10
    yield _test_getT, 'a=10', 'a', float, 10.
    yield _test_getT, 'a=10', 'a', bool, None
    yield _test_getT, 'a=yes', 'a', bool, True

    yield _test_getT, PrefixedDict('p', Params({'a': 'yes'})), 'a', bool, bool()
    yield _test_getT, PrefixedDict('p', Params({'p.a': 'yes'})), 'a', bool, True

def test_chain():
    p = Params({'a': 'yes', 'p.a': 'no', 'b': '10', 'p.c': '20'})
    c = ChainedDict(PrefixedDict('p', p), p)

    assert_equals(c.getT('a', True), False)
    assert_equals(c.getT('b', 0), 10)
    assert_equals(c.getT('c', 0), 20)
