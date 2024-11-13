#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import tll.channel as C
from tll.config import Config
from tll import asynctll
from tll.test_util import Accum
from tll.chrono import *
from tll.error import TLLError

import pytest
import decimal

def test_binary():
    url = Config.load(br'''yamls://
tll.proto: yaml
name: yaml
dump: frame
config:
  - msgid: 1
    seq: 10
    data: "abc\x01def"
''')
    c = Accum(url)
    c.open()
    c.process()
    assert c.state == c.State.Active
    assert [(m.msgid, m.seq, m.data.tobytes()) for m in c.result] == [(1, 10, b'abc\x01def')]
    c.process()
    assert c.state == c.State.Closed

@pytest.mark.parametrize("t,v", [
    ('int8', 123),
    ('int16', 12323),
    ('int32', 123123),
    ('int64', 123123),
    ('uint8', 231),
    ('uint16', 53123),
    ('uint32', 123123),
    ('double', 123.123),
    ('byte8', (b'abcd\0\0\0\0', 'abcd')),
    ('byte8, options.type: string', 'abcd'),
    ('string', 'abcd'),
    ('decimal128', (decimal.Decimal('1234567890.e-5'), '1234567890.e-5')),
    ('int32, options.type: fixed3', (decimal.Decimal('123.456'), '123456.e-3')),
    ('int32, options.type: duration, options.resolution: us', (Duration(123000, Resolution.us), '123ms')),
    ('int64, options.type: time_point, options.resolution: s', (TimePoint(1609556645, Resolution.second), '2021-01-02T03:04:05')),
])
def test_simple(t, v):
    if isinstance(v, tuple):
        v, s = v
    else:
        s = str(v)
    scheme = f'''yamls://
- name: msg
  id: 10
  fields:
    - {{name: f0, type: {t} }}
'''
    url = Config.load(f'''yamls://
tll.proto: yaml
name: yaml
dump: scheme
config.0:
  name: msg
''')
    url['scheme'] = scheme
    url['config.0.data.f0'] = s
    c = Accum(url)
    c.open()
    c.process()
    assert [(m.msgid, m.seq) for m in c.result] == [(10, 0)]
    m = c.unpack(c.result[0])
    assert m.as_dict() == {'f0': v}

def test_enum():
    scheme = '''yamls://
- name: msg
  id: 10
  enums:
    Enum: {type: int32, options.type: enum, enum: {A: 10, B: 20}}
  fields:
    - {name: f0, type: Enum}
    - {name: f1, type: Enum}
'''
    url = Config.load(f'''yamls://
tll.proto: yaml
name: yaml
dump: scheme
config.0:
  name: msg
  data:
    f0: A
    f1: 20
''')
    url['scheme'] = scheme
    c = Accum(url)
    c.open()
    c.process()
    assert [(m.msgid, m.seq) for m in c.result] == [(10, 0)]
    m = c.unpack(c.result[0])
    assert m.f0 == m.f0.A
    assert m.f1 == m.f1.B

@pytest.mark.parametrize("t,s,v", [
    ('uint8', '0x3', 0x3),
    ('uint16', 'A', 0x3),
    ('int32', 'A |B', 0x3),
    ('int64', 'A | 0x2', 0x3),
])
def test_bits(t, s, v):
    scheme = f'''yamls://
- name: msg
  id: 10
  bits:
    Bits: {{type: {t}, options.type: bits, bits: [A, B, C]}}
  fields:
    - {{name: f0, type: Bits}}
'''
    url = Config.load(f'''yamls://
tll.proto: yaml
name: yaml
dump: scheme
config.0:
  name: msg
  data:
    f0: {s}
''')
    url['scheme'] = scheme
    c = Accum(url)
    c.open()
    c.process()
    assert [(m.msgid, m.seq) for m in c.result] == [(10, 0)]
    m = c.unpack(c.result[0])
    m.f0._value == v

@pytest.mark.parametrize("t,v", [
    ('"int32[4]"', [0, 1]),
    ('"*int32"', [0, 1, 2, 3]),
    ('"**int32"', [[0, 1], [2, 3], [4]]),
])
def test_list(t, v):
    scheme = f'''yamls://
- name: msg
  id: 10
  fields:
    - {{name: f0, type: {t} }}
'''
    url = Config.load(f'''yamls://
tll.proto: yaml
name: yaml
dump: scheme
config.0:
  name: msg
  data.f0: {v}
''')
    url['scheme'] = scheme
    c = Accum(url)
    c.open()
    c.process()
    assert [(m.msgid, m.seq) for m in c.result] == [(10, 0)]
    m = c.unpack(c.result[0])
    assert m.as_dict() == {'f0': v}

def test_message():
    scheme = '''yamls://
- name: sub
  fields:
    - {name: s0, type: int8}
- name: msg
  id: 10
  fields:
    - {name: f0, type: sub}
'''
    url = Config.load(f'''yamls://
tll.proto: yaml
name: yaml
dump: scheme
config.0:
  name: msg
  data.f0.s0: 123
''')
    url['scheme'] = scheme
    c = Accum(url)
    c.open()
    c.process()
    assert [(m.msgid, m.seq) for m in c.result] == [(10, 0)]
    m = c.unpack(c.result[0])
    assert m.as_dict() == {'f0': {'s0': 123}}

def test_many():
    scheme = '''yamls://
- name: msg
  id: 10
  fields:
    - {name: f0, type: int8}
'''
    url = Config.load('''yamls://
tll.proto: yaml
name: yaml
dump: scheme
autoclose: no
config:
  - {name: msg, seq: 0, data.f0: 0}
  - {name: msg, seq: 1, data.f0: 1}
  - {name: msg, seq: 2, data.f0: 2}
  - {name: msg, seq: 3, data.f0: 3}
''')
    url['scheme'] = scheme
    c = Accum(url)
    c.open()
    for i in range(4):
        c.process()
        assert [(m.msgid, m.seq) for m in c.result] == [(10, j) for j in range(i + 1)]
    assert c.dcaps == c.DCaps.Process | c.DCaps.Pending
    c.process()
    assert c.dcaps == c.DCaps.Zero
    assert [(m.msgid, m.seq) for m in c.result] == [(10, j) for j in range(4)]
    for i in range(4):
        assert c.unpack(c.result[i]).as_dict() == {'f0': i}

@pytest.mark.parametrize("data,r", [
    ('{xxx: 10}', None),
    ('{i8: xxx}', None),
    ('{i8: 10}', {'i8': 10}),
    ('{string: string}', {'string': 'string'}),
    ('{sub: {s0: 10}}', {'sub': {'s0': 10}}),
    ('{array: [1, 2, 3]}', {'array': [1, 2, 3]})
])
def test_union(data, r):
    scheme = '''yamls://
- name: sub
  fields:
    - {name: s0, type: int8}
- name: msg
  id: 10
  fields:
    - {name: f0, type: union, union: [{name: i8, type: int8}, {name: string, type: string}, {name: sub, type: sub}, {name: array, type: 'int8[4]'}]}
'''
    url = Config.load(f'''yamls://
tll.proto: yaml
name: yaml
dump: scheme
config.0:
  name: msg
  data.f0: %s
''' % data)
    url['scheme'] = scheme
    c = Accum(url)
    c.open()
    if r is None:
        with pytest.raises(TLLError): c.process()
        assert c.state == c.State.Error
        return
    c.process()
    assert [(m.msgid, m.seq) for m in c.result] == [(10, 0)]
    m = c.unpack(c.result[0])
    assert m.as_dict() == {'f0': r}

def test_control():
    scheme = f'''yamls://
- name: msg
  id: 10
  fields:
    - {{name: f0, type: int32}}
'''
    url = Config.load(f'''yamls://
tll.proto: yaml
name: yaml
dump: scheme
config.0:
  name: msg
  type: control
  seq: 20
  data.f0: 100
''')
    url['scheme-control'] = scheme

    class ControlAccum(Accum):
        MASK = Accum.MsgMask.Control
    c = ControlAccum(url)
    c.open()
    c.process()
    assert [(m.type, m.msgid, m.seq) for m in c.result] == [(c.Type.Control, 10, 20)]
    m = c.unpack(c.result[0])
    assert m.as_dict() == {'f0': 100}

def test_pmap():
    scheme = '''yamls://
- name: msg
  id: 10
  options.defaults.optional: yes
  fields:
    - {name: f0, type: int32}
    - {name: f1, type: int32}
    - {name: pmap, type: uint8, options.pmap: yes}
    - {name: f2, type: int32}
    - {name: f3, type: int32, options.optional: no}
'''

    url = Config.load('''yamls://
tll.proto: yaml
name: yaml
dump: scheme
config:
  - name: msg
    data: {f0: 100, f2: 200}
  - name: msg
    data: {f1: 300}
''')
    url['scheme'] = scheme
    c = Accum(url)
    c.open()
    c.process()
    c.process()
    assert [(m.type, m.msgid) for m in c.result] == [(Accum.Type.Data, 10)] * 2
    m = c.unpack(c.result[0])
    assert m.as_dict() == {'f0': 100, 'f2': 200, 'f3': 0}
    m = c.unpack(c.result[1])
    assert m.as_dict() == {'f1': 300, 'f3': 0}

@pytest.mark.parametrize("strict", ["yes", "no"])
def test_strict(strict):
    scheme = '''yamls://
- name: msg
  id: 10
  fields:
    - {name: f0, type: int32}
'''

    url = Config.load('''yamls://
tll.proto: yaml
name: yaml
dump: scheme
config:
  - name: msg
    data: {f0: 100}
  - name: msg
    data: {f0: 200, f1: 200}
''')
    url['scheme'] = scheme
    url['strict'] = strict
    c = Accum(url)
    c.open()
    c.process()
    assert [(m.type, m.msgid) for m in c.result] == [(Accum.Type.Data, 10)]
    m = c.unpack(c.result[0])
    assert m.as_dict() == {'f0': 100}

    if strict == 'yes':
        with pytest.raises(TLLError): c.process()
    else:
        c.process()
        m = c.unpack(c.result[-1])
        assert m.as_dict() == {'f0': 200}

def test_autoseq():
    url = Config.load('''yamls://
tll.proto: yaml
name: yaml
dump: frame
autoseq: yes
config:
  - msgid: 1
    seq: 10
    data: "xxx"
  - msgid: 1
    data: "yyy"
  - msgid: 1
    seq: 10
    data: "zzz"
''')
    c = Accum(url)
    c.open()
    c.process()
    c.process()
    c.process()
    c.process()
    assert c.state == c.State.Closed
    assert [(m.seq, m.data.tobytes()) for m in c.result] == [(10, b'xxx'), (11, b'yyy'), (12, b'zzz')]

def test_pointer_large():
    cfg = Config.load('''yamls://
tll.proto: yaml
name: yaml
dump: yes
config:
  - name: Data
    seq: 10
    data:
      list:
        - { body: 0000 }
        - { body: 1111 }
''')
    cfg['scheme'] = """yamls://
- name: Item
  fields:
    - {name: body, type: byte266, options.type: string}
- name: Data
  id: 10
  fields:
    - {name: list, type: '*Item'}
"""
    c = Accum(cfg)
    c.open()
    c.process()

    assert [m.seq for m in c.result] == [10]
    m = c.result[0]
    assert m.data[:12].tobytes() == b'\x08\x00\x00\x00\x02\x00\x00\xff\x0a\x01\x00\x00'
    assert c.unpack(m).as_dict() == {'list': [{'body': '0000'}, {'body': '1111'}]}
