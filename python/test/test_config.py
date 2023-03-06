#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import enum
import copy
import pytest

from tll.config import Config
from tll.error import TLLError

from tll.test_util import base64gz

def test_basic():
    cfg = Config.load('yamls://{a: [{x: 0, y: 1}, {x: 1, y: 0}], b: {x: 2, y: 2}}')
    assert cfg['b.x'] == '2'
    assert cfg['b.y'] == '2'
    assert 'b.z' not in cfg
    assert 'b.x' in cfg
    assert cfg.as_dict() == {'a': [{'x': '0', 'y': '1'}, {'x': '1', 'y': '0'}], 'b': {'x': '2', 'y': '2'}}
    assert cfg.from_dict({'a':'1', 'b.c':'2'}).as_dict() == {'a': '1', 'b': {'c': '2'}}

    del cfg['b.x']
    assert 'b.x' not in cfg
    assert 'b.y' in cfg
    assert cfg.as_dict() == {'a': [{'x': '0', 'y': '1'}, {'x': '1', 'y': '0'}], 'b': {'y': '2'}}

    cfg.unset('b.y')
    assert 'b.y' not in cfg
    assert cfg.as_dict() == {'a': [{'x': '0', 'y': '1'}, {'x': '1', 'y': '0'}], 'b': {'y': {}}}

    cfg.remove('a')
    assert cfg.as_dict() == {'a': [{'x': '0', 'y': '1'}, {'x': '1', 'y': '0'}], 'b': {'y': {}}}

    cfg.unlink('a')
    assert cfg.as_dict() == {'b': {'y': {}}}

    with pytest.raises(KeyError): cfg.unset('a')
    with pytest.raises(KeyError): cfg.unlink('a')
    with pytest.raises(KeyError): cfg.remove('a')

def test_copy():
    cfg = Config.load('yamls://{a: 1, b: {x: 2, y: 3}}')
    assert dict(cfg.browse('**')) == {'a':'1', 'b.x':'2', 'b.y':'3'}
    cc0 = cfg.copy()
    cc1 = copy.copy(cfg)
    cdc = copy.deepcopy(cfg)
    for c in [cc0, cc1, cdc]:
        assert dict(c.browse('**')) == {'a':'1', 'b.x':'2', 'b.y':'3'}
    cfg['a'] = '9'
    for c in [cc0, cc1, cdc]:
        assert dict(c.browse('**')) == {'a':'1', 'b.x':'2', 'b.y':'3'}

def _test_yaml(data, v):
    c = Config.load('yamls://' + data)
    d = dict(c.browse('**'))
    assert d == v

    c = Config.load('yamls+gz://' + base64gz(data))
    d = dict(c.browse('**'))
    assert d == v

def test_yaml():
    _test_yaml('''a: 1''', {'a': '1'})
    _test_yaml('''{a: 1}''', {'a': '1'})
    _test_yaml('''[a: 1]''', {'0000.a': '1'})
    _test_yaml('''a: [1, 2, 3]''', {'a.0000': '1', 'a.0001': '2', 'a.0002': '3'})
    _test_yaml('''a: [[1, 2], 3, [4]]''', {'a.0000.0000': '1', 'a.0000.0001': '2', 'a.0001': '3', 'a.0002.0000': '4'})
    _test_yaml('''[{a: 1}, {b: 2}]''', {'0000.a': '1', '0001.b': '2'})
    _test_yaml('''[{a: 1}, {b: 2}]''', {'0000.a': '1', '0001.b': '2'})
    _test_yaml('''a: 1\n\nb: 2''', {'a': '1', 'b': '2'})
    _test_yaml('''#comment
a:
  f: 1

b:
  f: 2''', {'a.f': '1', 'b.f': '2'})
    #yield _test_yaml, b'''\xef\xbb\xbfa: 1'''.decode('utf-8'), {'a': '1'}
    _test_yaml('''\ufeffa: 1''', {'a': '1'})

@pytest.mark.parametrize("data", [
    '''{a: b[3]}''',
    '''{a: 1, a: 2}''',
])
def test_yaml_fail(data):
    with pytest.raises(TLLError): Config.load('yamls://' + data)

def test_set_sub():
    c = Config.load("yamls://{a: 1, c: 2}")
    sub = Config.load("yamls://{d: 3, e.f: 4}")
    c["b"] = sub

    d = dict(c.browse('**'))
    assert d == {'a': '1', 'c': '2', 'b.d': '3', 'b.e.f': '4'}

def _test_merge(ow, c0, c1, r):
    c = Config.load('yamls://' + c0)
    c.merge(Config.load('yamls://' + c1), overwrite = ow == 'overwrite')
    d = dict(c.browse('**'))
    assert d == r

def test_merge():
    _test_merge('overwrite','a: 1', 'a: 2', {'a': '2'})
    _test_merge('overwrite','a: 1', 'b: 2', {'a': '1', 'b': '2'})
    _test_merge('overwrite','{a: 1, b.c: 1}', 'b.d: 2', {'a': '1', 'b.c': '1', 'b.d': '2'})
    _test_merge('import','a: 1', 'a: 2', {'a': '1'})
    _test_merge('import','a: 1', 'b: 2', {'a': '1', 'b': '2'})
    _test_merge('import','{a: 1, b.c: 1}', 'b.d: 2', {'a': '1', 'b.c': '1', 'b.d': '2'})

class E(enum.Enum):
    A = 1
    B = 2

@pytest.mark.parametrize("s,k,t,v", [
    ('{a: 10}', 'a', int, 10),
    ('{a: 10}', 'a', int, 10),
    ('{a: 10}', 'a', 0, 10),
    ('{a: 10}', 'a', float, 10.),
    ('{a: 10}', 'a', bool, None),
    ('{a: yes}', 'a', bool, True),
    ('{a: yes}', 'a', False, True),
    ('{a: A}', 'a', E, E.A),
    ('{a: A}', 'a', E.A, E.A),
    ('{a: x}', 'a', E, None),
])
def test_getT(s, k, t, v):
    c = Config.load('yamls://' + s)
    if v is None:
        with pytest.raises(ValueError): c.getT(k, t)
    else:
        assert c.getT(k, t) == v

def _test_load(proto, data, r):
    c = Config.load_data(proto, data)
    assert dict(c.browse("**")) == r

def test_load():
    _test_load('url', 'proto://host;a=b;c=d', {'tll.proto':'proto', 'tll.host':'host', 'a':'b', 'c':'d'})
    _test_load('props', 'a=b;c=d', {'a':'b', 'c':'d'})

def test_process_imports():
    c = Config.load('''yamls://
import:
 - 'yamls://{a: 1, b.c: 2}'
 - 'yamls://{a: 2, b.d: 3}'
b.c: 10
''')

    assert c.as_dict() == {'b': {'c':'10'}, 'import': ['yamls://{a: 1, b.c: 2}', 'yamls://{a: 2, b.d: 3}']}
    c.process_imports('import')
    assert c.as_dict() == {'a': '2', 'b': {'c':'10', 'd':'3'}, 'import': ['yamls://{a: 1, b.c: 2}', 'yamls://{a: 2, b.d: 3}']}

def test_yaml_alias():
    with pytest.raises(TLLError): Config.load('''yamls://{a: *unknown}''')

    c = Config.load('''yamls://
a: &list
  - &scalar 100
  - 200
b: &map
  a: *scalar
  b: *list
c: *map
''')

    assert c.as_dict() == {'a': ['100', '200'], 'b': {'a': '100', 'b': ['100', '200']}, 'c': {'a': '100', 'b': ['100', '200']}}

def test_yaml_binary():
    with pytest.raises(TLLError): Config.load('''yamls://{a: !!binary ___}''')

    c = Config.load('''yamls://{a: !!binary AAECAw== }''')

    assert c.get('a', decode=False) == b'\x00\x01\x02\x03'

def test_yaml_link():
    c = Config.load('''yamls://
a:
  - 100
  - 200
b:
  a: !link /a/0001/../0000
  b: !link ../../a
c: !link /b
''')

    print(c.as_dict())
    assert c.as_dict() == {'a': ['100', '200'], 'b': {'a': '100', 'b': ['100', '200']}, 'c': {'a': '100', 'b': ['100', '200']}}
