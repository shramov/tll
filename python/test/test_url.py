#!/usr/bin/env python3
# vim: sts=4 sw=4 et

from tll.url import Url, Params
from tll.conv import PrefixedDict, ChainedDict

import pytest

@pytest.mark.parametrize("s,d", [
    ('', {}),
    (';', {}),
    (';;', {}),
    (';a=b;', {'a':'b'}),
])
def test_parse(s, d):
    u = Params.from_string(s)
    assert dict(u) == d

@pytest.mark.parametrize("s,proto,host,d", [
    ('a://', 'a', '', {}),
    ('a://;', 'a', '', {}),
    ('a://;;', 'a', '', {}),
    ('a://b', 'a', 'b', {}),
])
def test_parse_url(s, proto, host, d):
    u = Url.from_string(s)
    assert u.proto == proto
    assert u.host == host
    assert dict(u) == d

@pytest.mark.parametrize("klass,s", [
    (Params, 'a'),
    (Params, 'a=1;a=2'),

    (Url, ''),
    (Url, 'a:/b'),
    (Url, 'a://b;c'),
    (Url, 'a://b;c;'),
])
def test_parse_invalid(klass, s):
    with pytest.raises(ValueError): klass.from_string(s)

@pytest.mark.parametrize("s,k,t,v", [
    (Params(a=10), 'a', int, 10),
    (Url(a='10'), 'a', int, 10),
    ('a=10', 'a', int, 10),
    ('a=10', 'a', int, 10),
    ('a=10', 'a', float, 10.),
    ('a=10', 'a', bool, None),
    ('a=yes', 'a', bool, True),

    (PrefixedDict('p', Params({'a': 'yes'})), 'a', bool, bool()),
    (PrefixedDict('p', Params({'p.a': 'yes'})), 'a', bool, True),
])
def test_getT(s, k, t, v):
    p = Params.from_string(s) if isinstance(s, str) else s
    if v is None:
        with pytest.raises(ValueError): p.getT(k, t())
    else:
        assert p.getT(k, t()) == v

def test_chain():
    p = Params({'a': 'yes', 'p.a': 'no', 'b': '10', 'p.c': '20'})
    c = ChainedDict(PrefixedDict('p', p), p)

    assert c.getT('a', True) == False
    assert c.getT('b', 0) == 10
    assert c.getT('c', 0) == 20
