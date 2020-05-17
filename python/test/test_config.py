#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import common

from tll.config import Config
from tll.error import TLLError
from nose.tools import *

from tll.test_util import base64gz

def test_basic():
    cfg = Config.load('yamls://{a: [{x: 0, y: 1}, {x: 1, y: 0}], b: {x: 2, y: 2}}')
    assert_equal(cfg['b.x'], '2')
    assert_equal(cfg['b.y'], '2')
    assert_false('b.z' in cfg)
    assert_true('b.x' in cfg)
    #del cfg['b.x']
    #assert_false('b.x' in cfg)

def _test_yaml(data, v):
    c = Config.load('yamls://' + data)
    d = dict(c.browse('**'))
    assert_equals(d, v)

    c = Config.load('yamls+gz://' + base64gz(data))
    d = dict(c.browse('**'))
    assert_equals(d, v)

def test_yaml():
    yield _test_yaml, '''a: 1''', {'a': '1'}
    yield _test_yaml, '''{a: 1}''', {'a': '1'}
    yield _test_yaml, '''[a: 1]''', {'0000.a': '1'}
    yield _test_yaml, '''a: [1, 2, 3]''', {'a.0000': '1', 'a.0001': '2', 'a.0002': '3'}
    yield _test_yaml, '''a: [[1, 2], 3, [4]]''', {'a.0000.0000': '1', 'a.0000.0001': '2', 'a.0001': '3', 'a.0002.0000': '4'}
    yield _test_yaml, '''[{a: 1}, {b: 2}]''', {'0000.a': '1', '0001.b': '2'}
    yield _test_yaml, '''[{a: 1}, {b: 2}]''', {'0000.a': '1', '0001.b': '2'}
    yield _test_yaml, '''a: 1\n\nb: 2''', {'a': '1', 'b': '2'}
    yield _test_yaml, '''#comment
a:
  f: 1

b:
  f: 2''', {'a.f': '1', 'b.f': '2'}
    #yield _test_yaml, b'''\xef\xbb\xbfa: 1'''.decode('utf-8'), {'a': '1'}
    yield _test_yaml, '''\ufeffa: 1''', {'a': '1'}

def _test_yaml_fail(data):
    assert_raises(TLLError, Config.load, 'yamls://' + data)

def test_yaml_fail():
    yield _test_yaml_fail, '''{a: b[3]}'''
    yield _test_yaml_fail, '''{a: 1, a: 2}'''

def test_set_sub():
    c = Config.load("yamls://{a: 1, c: 2}")
    sub = Config.load("yamls://{d: 3, e.f: 4}")
    c["b"] = sub

    d = dict(c.browse('**'))
    assert_equals(d, {'a': '1', 'c': '2', 'b.d': '3', 'b.e.f': '4'})

def _test_merge(ow, c0, c1, r):
    c = Config.load('yamls://' + c0)
    c.merge(Config.load('yamls://' + c1), overwrite = ow is 'overwrite')
    d = dict(c.browse('**'))
    assert_equals(d, r)

def test_merge():
    yield _test_merge, 'overwrite','a: 1', 'a: 2', {'a': '2'}
    yield _test_merge, 'overwrite','a: 1', 'b: 2', {'a': '1', 'b': '2'}
    yield _test_merge, 'overwrite','{a: 1, b.c: 1}', 'b.d: 2', {'a': '1', 'b.c': '1', 'b.d': '2'}
    yield _test_merge, 'import','a: 1', 'a: 2', {'a': '1'}
    yield _test_merge, 'import','a: 1', 'b: 2', {'a': '1', 'b': '2'}
    yield _test_merge, 'import','{a: 1, b.c: 1}', 'b.d: 2', {'a': '1', 'b.c': '1', 'b.d': '2'}

def _test_getT(s, k, t, v):
    c = Config.load('yamls://' + s)
    if v is None:
        assert_raises(ValueError, c.getT, k, t())
    else:
        assert_equals(c.getT(k, t()), v)

def test_getT():
    yield _test_getT, '{a: 10}', 'a', int, 10
    yield _test_getT, '{a: 10}', 'a', int, 10
    yield _test_getT, '{a: 10}', 'a', float, 10.
    yield _test_getT, '{a: 10}', 'a', bool, None
    yield _test_getT, '{a: yes}', 'a', bool, True
