#!/usr/bin/env python
# -*- coding: utf-8 -*-
# vim: sts=4 sw=4 et

import tll.scheme as S
from tll.scheme import Field as F
from tll.s2b import s2b
from tll.chrono import *
from tll.config import Config

import copy
import datetime
import decimal
import enum
import hashlib
import pytest
import struct

scheme = """yamls://
- name: ''
  options: {a: 2, b: 3, c: 4, d: {a: 1, b: 2}, l: [1, 2]}

- name: sub
  fields:
    - {name: s0, type: int32}
    - {name: s1, type: 'double[4]'}
  options: {a: 1, b: 2}

- name: test
  id: 1
  fields:
    - {name: f0, type: int8, options: {a: 10, b: 20}}
    - {name: f1, type: int64}
    - {name: f2, type: double}
    - {name: f3, type: decimal128}
    - {name: f4, type: byte32}
    - {name: f5, type: '*int16', options.a: 20}
    - {name: f6, type: 'sub[4]', list-options.count-type: int16, options.a: 30}
    - {name: f7, type: string}
    - {name: f8, type: '*string'}
  options: {m: 10}

- name: enums
  id: 10
  enums:
    e1: {type: int8, enum: {A: 1, B: 2}, options: {ea: 30, eb: 40}}
    e4: {type: int32, enum: {E: 1, F: 2}}
    e8: {type: int64, enum: {G: 1, H: 2}}
  fields:
    - {name: f0, type: e1}
    - {name: f1, type: int16, options.type: enum, enum: {C: 1, D: 2}}
    - {name: f2, type: e4}
    - {name: f3, type: e8}
"""

def _test(s):
    for m in s.messages: print(m.fields)
    assert [m.name for m in s.messages] == ["sub", "test", "enums"]
    sub = s['sub']
    assert [(f.name, f.type) for f in sub.fields] == \
        [("s0", F.Int32), ("s1", F.Array)]
    assert sub['s1'].options == {}
    assert sub['s1'].count_ptr.type == F.Int8
    assert sub['s1'].type_array.type == F.Double
    assert sub.size == 4 + 1 + 8 * 4

    msg = s['test']
    assert msg.msgid == 1
    assert [(f.name, f.type) for f in msg.fields] == \
        [("f0", F.Int8), ("f1", F.Int64), ("f2", F.Double), ("f3", F.Decimal128), ("f4", F.Bytes), ("f5", F.Pointer), ("f6", F.Array), ("f7", F.Pointer), ('f8', F.Pointer)]
    assert msg['f6'].count_ptr.type == F.Int16
    assert msg['f6'].type_array.type == F.Message
    assert [(f.name, f.sub_type) for f in msg.fields if f.sub_type != F.Sub.NONE] == \
        [("f7", F.Sub.ByteString)]
    assert [(f.name, f.size) for f in msg.fields] == \
        [("f0", 1), ("f1", 8), ("f2", 8), ("f3", 16), ("f4", 32), ("f5", 8), ("f6", 2 + 4 * sub.size), ("f7", 8), ("f8", 8)]
    assert msg.size == 1 + 8 + 8 + 16 + 32 + 8 + (2 + 4 * sub.size) + 8 + 8

    msg = s['enums']
    assert msg.msgid == 10
    assert [(f.name, f.type) for f in msg.fields] == \
        [("f0", F.Int8), ("f1", F.Int16), ("f2", F.Int32), ("f3", F.Int64)]
    assert [(f.name, f.sub_type) for f in msg.fields] == \
        [("f0", F.Sub.Enum), ("f1", F.Sub.Enum), ("f2", F.Sub.Enum), ("f3", F.Sub.Enum)]
    assert [(f.name, f.size) for f in msg.fields] == \
        [("f0", 1), ("f1", 2), ("f2", 4), ("f3", 8)]
    assert msg.size == 1 + 2 + 4 + 8

    with pytest.raises(KeyError): s.find('Missing')
    with pytest.raises(KeyError): s.find(0xffff)
    assert 'Missing' not in s
    assert 'test' in s
    assert 0xffff not in s
    assert 1 in s

def test():
    return _test(S.Scheme(scheme))

def test_copy():
    return _test(S.Scheme(scheme).copy())

def test_dump():
    print(S.Scheme(scheme).dump())
    return _test(S.Scheme(S.Scheme(scheme).dump()))

def _test_options(s):
    for m in s.messages: print(m.options)
    sub = s['sub']
    msg = s['test']
    assert s['sub'].options == {'a':'1', 'b':'2'}
    assert s['test'].options == {'m':'10'}
    assert s['test']['f0'].options == {'a':'10', 'b':'20'}
    assert s['test']['f1'].options == {}
    assert s['test']['f5'].options == {}
    assert s['test']['f5'].type_ptr.options == {'a': '20'}
    assert s['test']['f6'].options == {'count-type': 'int16'}
    assert s['test']['f6'].type_array.options == {'a': '30'}
    assert s['enums'].enums['e1'].options == {'ea': '30', 'eb': '40'}
    assert s['enums'].enums['f1'].options == {"_auto": "inline"}

def test_options():
    return _test_options(S.Scheme(scheme))

def test_options_dump():
    return _test_options(S.Scheme(S.Scheme(scheme).dump()))

def test_as_dict():
    s = S.Scheme(scheme)
    M = s.messages.sub(s0=10, s1=[1., 2., 3.])
    assert M.as_dict() == {'s0':10, 's1':[1., 2., 3.]}
    data = M.pack()

    r = M.SCHEME.reflection(data)
    assert r.as_dict() == M.as_dict()

def test_pack():
    s = S.Scheme(scheme)
    M = s['sub'].object(s0=10, s1=[1., 2., 3.])
    data = M.pack()
    assert len(data) == M.SCHEME.size
    assert struct.unpack("=ibdddd", data) == (10, 3, 1., 2., 3., 0.)
    m1 = s['sub'].object().unpack(data)
    assert m1.s0 == 10
    assert m1.s1 == [1., 2., 3.]

    r = M.SCHEME.reflection(data)
    assert getattr(r, 'xxx', None) == None
    assert r.s0 == m1.s0
    assert r.s1 == m1.s1

def optr(off, size, entity):
    return (entity << 56) | (size << 32) | off

def test_sub_pack():
    s = S.Scheme('''yamls://
- name: sub
  fields:
    - {name: s0, type: int32}

- name: msg
  fields:
    - {name: f0, type: sub}
    - {name: f1, type: 'sub[2]'}
    - {name: f2, type: '*sub'}
    - {name: f3, type: '**sub'}
''')

    M = s['msg']
    m0 = M.object(f0 = {'s0':10}, f1 = [{'s0':20}, {'s0':30}], f2 = [{'s0':40}, {'s0':50}], f3 = [[{'s0':60}], [], [{'s0':70}, {'s0':80}]])
    data = m0.pack()
    assert len(data) == M.size + 2 * 4 + 3 * 8 + 3 * 4
    assert struct.unpack("=ibiiQQiiQQQiii", data) == (10, 2, 20, 30, optr(16, 2, 4), optr(16, 3, 8), 40, 50, optr(24, 1, 4), optr(20, 0, 4), optr(12, 2, 4), 60, 70, 80)
    m1 = M.object().unpack(data)
    assert m1.f0.s0 == 10
    assert [x.s0 for x in m1.f1] == [20, 30]
    assert [x.s0 for x in m1.f2] == [40, 50]
    assert [[x.s0 for x in y] for y in m1.f3] == [[60], [], [70, 80]]

    r = M.reflection(data)
    assert r.f0.s0 == m1.f0.s0
    assert [x.s0 for x in r.f1] == [20, 30]
    assert [x.s0 for x in r.f2] == [40, 50]
    assert [[x.s0 for x in y] for y in r.f3] == [[60], [], [70, 80]]

def test_string():
    s = S.Scheme('''yamls://
- name: sub
  fields:
    - {name: s0, type: string}

- name: msg
  fields:
    - {name: f0, type: string}
    - {name: f1, type: sub}
    - {name: f2, type: '*string'}
''')

    M = s['msg']
    m0 = M.object(f0 = "ыыы", f1 = {'s0':"nestedtail"}, f2 = ["a", "b", "c"])
    data = m0.pack()
    assert len(data) == M.size + len(s2b("ыыы\0")) + len(b"nestedtail\0") + 3 * (8 + 2)
    assert struct.unpack("=QQQ7s11sQQQ2s2s2s", data) == (optr(24, 6 + 1, 1), optr(23, 10 + 1, 1), optr(26, 3, 8), s2b("ыыы\0"), b"nestedtail\0", optr(24, 1 + 1, 1), optr(18, 1 + 1, 1), optr(12, 1 + 1, 1), b'a\0', b'b\0', b'c\0')
    m1 = M.object().unpack(data)
    assert m1.f0 == "ыыы"
    assert m1.f1.s0 == "nestedtail"

def test_from_string():
    s = S.Scheme('''yamls://
- name: msg
  fields:
    - {name: int8, type: int8}
    - {name: int16, type: int16}
    - {name: int32, type: int32}
    - {name: int64, type: int64}
    - {name: double, type: double}
    - {name: string, type: string}
    - {name: byte16, type: byte16}
    - {name: string16, type: byte16, option.type: string}
    - {name: array, type: 'int8[8]'}
    - {name: ptr, type: '*int8'}
    - {name: duration, type: int32, options.type: duration, options.resolution: ns}
    - {name: ts, type: int32, options.type: time_point, options.resolution: ns}
    - {name: bits, type: int8, options.type: bits, bits: [a, {name: b, offset: 2, size: 1}, c]}
''')

    msg = s['msg']
    assert msg['int8'].from_string("100") == 100
    assert msg['int16'].from_string("0x1234") == 0x1234
    assert msg['int32'].from_string("0o777777777") == 0o777777777
    assert msg['int64'].from_string("12345678") == 12345678
    assert msg['double'].from_string("123.456") == 123.456
    assert msg['string'].from_string("string") == "string"
    assert msg['byte16'].from_string("string") == b"string"

    assert msg['duration'].from_string("100ns") == Duration(100, 'ns')
    assert msg['duration'].from_string("10.5us") == Duration(10500, 'ns')

    assert msg['ts'].from_string("1970-01-01 00:00:00.000000100") == TimePoint(100, 'ns')
    assert str(msg['bits'].from_string('{c, a}')) == '{a, c}'
    assert str(msg['bits'].from_string('b')) == '{b}'
    assert str(msg['bits'].from_string('0x5')) == '{a, b}'

    with pytest.raises(OverflowError):
        msg['int8'].from_string("1000")
    with pytest.raises(ValueError):
        msg['int8'].from_string("string")
    with pytest.raises(TypeError):
        msg['array'].from_string("[]")
    with pytest.raises(TypeError):
        msg['ptr'].from_string("[]")

SCHEME_ALIAS = '''yamls://
- name:
  aliases:
    - {name: license, type: byte32, options.type: string}
    - {name: license_list, type: '*license'}

- name: msg
  fields:
    - {name: f0, type: license}
    - {name: f1, type: license_list}
    - {name: f2, type: '*license'}
'''

def _test_alias(s):
    assert [m.name for m in s.messages] == ["msg"]
    assert [(f.name, f.type) for f in s.aliases.values()] == [("license", F.Bytes), ("license_list", F.Pointer)]

    msg = s['msg']
    assert [(f.name, f.type) for f in msg.fields] == [("f0", F.Bytes), ("f1", F.Pointer), ("f2", F.Pointer)]
    assert [(f.name, f.sub_type) for f in msg.fields if f.sub_type != F.Sub.NONE] == \
        [("f0", F.Sub.ByteString)]
    assert [(f.name, f.type_ptr.type, f.type_ptr.sub_type) for f in msg.fields if hasattr(f, 'type_ptr')] == \
        [("f1", F.Bytes, F.Sub.ByteString), ("f2", F.Bytes, F.Sub.ByteString)]

    #m0 = M.object(f0 = "license", f1 = ['a', 'b'], f2 = ['c', 'd'])

def test_alias(): return _test_alias(S.Scheme(SCHEME_ALIAS))
def test_alias_copy(): return _test_alias(S.Scheme(SCHEME_ALIAS).copy())
def test_alias_dump():
    print(S.Scheme(SCHEME_ALIAS).dump())
    return _test_alias(S.Scheme(S.Scheme(SCHEME_ALIAS).dump()))

def test_import():
    SCHEME = '''yamls://
- name:
  options: { outer: outer }
  import:
    - |
        yamls://
        - name:
          options: { outer: inner0, inner: inner0 }
          aliases:
            - {name: license, type: byte64, options.type: string}
    - |
        yamls://
        - options: { outer: inner1, inner: inner1 }
        - name: sub
          fields:
            - {name: s0, type: license}

- name: msg
  fields:
    - {name: f0, type: license}
    - {name: fs, type: sub}
'''

    s = S.Scheme(SCHEME)
    assert [(f.name, f.type, f.sub_type) for f in s.aliases.values()] == [("license", F.Bytes, F.Sub.ByteString)]
    assert [m.name for m in s.messages] == ['sub', 'msg']

    assert s.options == {'outer': 'outer', 'inner': 'inner0'}

    sub = s.messages.sub
    assert list(sub.options.keys()) == ['_import']
    assert sub.options['_import'].startswith('yamls://')
    assert 'outer: inner1' in sub.options['_import']

@pytest.mark.parametrize("t", ['int8', 'int16', 'int32', 'int64', 'uint8', 'uint16', 'uint32', 'uint64'])
def test_scalar(t):
    scheme = S.Scheme("""yamls://
- name: msg
  fields:
    - {name: f, type: %s}
""" % t)

    msg = scheme['msg']
    field = msg['f']

    if t[0] == 'u':
        size = int(t[4:])
        imin = 0
        imax = 2 ** size - 1
        assert field.type == getattr(field, f'UInt{size}')
    else:
        size = int(t[3:])
        imin = -1 * 2 ** (size - 1)
        imax = 2 ** (size - 1) - 1
        assert field.type == getattr(field, f'Int{size}')

    assert msg.size == size // 8
    assert field.size == size // 8

    m = msg.object()

    for v in [imin, imax // 2, imax]:
        m.f = v
        assert m.f == v

        u = msg.unpack(memoryview(m.pack()))
        assert m.as_dict() == u.as_dict()

        r = msg.reflection(memoryview(m.pack()))
        assert m.f == r.f

        assert msg['f'].from_string(str(v)) == v
        assert msg['f'].from_string(hex(v)) == v

    with pytest.raises(OverflowError): msg['f'].from_string(str(imax + 1))
    with pytest.raises(OverflowError): msg['f'].from_string(str(imin - 1))
    with pytest.raises(OverflowError): setattr(m, 'f', imax + 1)
    with pytest.raises(OverflowError): setattr(m, 'f', imin - 1)

def test_pointer_type():
    scheme = S.Scheme("""yamls://
- name: msg
  fields:
    - {name: default, type: '*int8'}
    - {name: lshort, type: '*int8', list-options.offset-ptr-type: legacy-short}
    - {name: llong, type: '*int8', list-options.offset-ptr-type: legacy-long}
""")

    msg = scheme['msg']
    assert msg['default'].options == {}
    assert msg['lshort'].options == {'offset-ptr-type': 'legacy-short'}
    assert msg['llong'].options == {'offset-ptr-type': 'legacy-long'}

    s1 = S.Scheme(scheme.dump())
    assert [(f.name, f.options) for f in scheme['msg'].values()] == [(f.name, f.options) for f in s1['msg'].values()]

    m = msg.object(default = [1, 2, 3], lshort = [10, 11], llong = [100, 101, 102, 103])
    data = memoryview(m.pack())
    u = msg.unpack(data)
    assert m.as_dict() == u.as_dict()

def test_fixed():
    scheme = S.Scheme("""yamls://
- name: msg
  fields:
    - {name: i8, type: int8, options.type: fixed1}
    - {name: i16, type: int16, options.type: fixed2}
    - {name: i32, type: int32, options.type: fixed3}
    - {name: i64, type: int64, options.type: fixed4}
    - {name: u8,  type: uint8, options.type: fixed5}
    - {name: u16, type: uint16, options.type: fixed6}
    - {name: u32, type: uint32, options.type: fixed7}
    - {name: u64, type: uint64, options.type: fixed8}
""")

    msg = scheme['msg']
    for (i,f) in enumerate(msg.fields):
        assert f.fixed_precision == i + 1, f"field {f.name}: {f.fixed_precision} != {i + 1}"

    m = msg.object(i8=-1, i16=2, i32=-3, i64=41.4, u8='0.0007', u16='0.012345', u32='123', u64='0.12345678')
    assert m.i8 == decimal.Decimal(-1)
    assert m.i16 == 2
    assert m.i32 == decimal.Decimal(-3)
    assert m.i64 == decimal.Decimal('41.4')
    assert m.u8 == decimal.Decimal('0.0007')
    assert m.u16 == decimal.Decimal('0.012345')
    assert m.u32 == decimal.Decimal('123')
    assert m.u64 == decimal.Decimal('0.12345678')

    data = memoryview(m.pack())
    u = msg.unpack(data)
    assert m.as_dict() == u.as_dict()

    assert m.SCHEME['i16'].from_string('123.4') == decimal.Decimal('123.4')
    assert m.SCHEME['u16'].from_string('0.01234') == decimal.Decimal('0.01234')
    with pytest.raises(ValueError): m.SCHEME['u16'].from_string('123.4')
    with pytest.raises(ValueError): m.SCHEME['u16'].from_string('-0.001')

    with pytest.raises(ValueError): m.SCHEME['i16'].from_string('xxx')
    with pytest.raises(ValueError): m.i16 = 'xxx'
    with pytest.raises(ValueError): m.i16 = 1000
    with pytest.raises(ValueError): m.u16 = -0.001

@pytest.mark.parametrize("version", ['default', 'legacy-long', 'legacy-short'])
def test_list(version):
    scheme = S.Scheme("""yamls://
- name: sub
  fields:
    - {name: f0, type: int8}

- name: msg
  fields:
    - {name: scalar, type: '*int8', list-options.offset-ptr-type: %(version)s}
    - {name: msg, type: '*sub', list-options.offset-ptr-type: %(version)s}
    - {name: fixed, type: 'int8[8]'}
""" % {'version': version})

    msg = scheme['msg']

    m = msg.object(scalar = [1, 2, 3], fixed = [4, 3, 2, 1], msg = [{'f0': 10}, {'f0': 20}])

    with pytest.raises(TypeError): m.msg.append(None)
    with pytest.raises(TypeError): m.msg.append(10)
    with pytest.raises(TypeError): m.fixed.append(None)
    with pytest.raises(TypeError): m.fixed.append({})

    m.msg.append({'f0':30})
    assert m.msg[2].f0 == 30

    m.msg = m.msg[:1]
    m.msg += [{'f0': 20}]
    assert m.msg[-1].f0 == 20

    u = msg.unpack(memoryview(m.pack()))
    assert m.as_dict() == u.as_dict()

    c = copy.deepcopy(m)
    assert c.as_dict() == m.as_dict()

    c.msg[1].f0 = 100

    assert m.msg[1].f0 == 20

    u = msg.unpack(memoryview(c.pack()))
    assert c.as_dict() == u.as_dict()

    data = m.pack()
    with pytest.raises(ValueError): msg.unpack(memoryview(data[:-10]))

    m = msg.object(scalar = [], fixed = [], msg = [])
    u = msg.unpack(memoryview(m.pack()))
    assert m.as_dict() == u.as_dict()

@pytest.mark.parametrize("version", ['default', 'legacy-long', 'legacy-short'])
def test_list_empty(version):
    scheme = S.Scheme("""yamls://
- name: msg
  fields:
    - {name: list, type: '*int8', list-options.offset-ptr-type: %s}
""" % version)

    msg = scheme['msg']
    m = msg.object(list = [])
    u = msg.unpack(memoryview(m.pack()))
    assert m.as_dict() == u.as_dict()

def test_field_del():
    scheme = S.Scheme("""yamls://
- name: msg
  fields:
    - {name: f0, type: int32}
""")

    msg = scheme['msg']
    m = msg.object(f0 = 0xbeef)

    assert m.as_dict() == {'f0': 0xbeef}
    del m.f0
    assert m.as_dict() == {}

    u = msg.unpack(memoryview(m.pack()))
    assert u.as_dict() == {'f0': 0}

def test_bytes():
    scheme = S.Scheme("""yamls://
- name: msg
  fields:
    - {name: f0, type: byte1}
    - {name: f1, type: byte1, options.type: string}
    - {name: f2, type: byte14, options.type: string}
""")

    msg = scheme['msg']
    m = msg.object(f0 = b'A', f1='B', f2='CDE')

    u = msg.unpack(memoryview(m.pack()))
    assert u.as_dict() == m.as_dict()

    with pytest.raises(ValueError):
        m.f0 = 'abc'

    with pytest.raises(ValueError):
        m.f1 = 'abc'

    m.f2 = 'abc'

    with pytest.raises(TypeError):
        m.f0 = 1

    with pytest.raises(TypeError):
        m.f1 = 1

def test_time_point():
    scheme = S.Scheme("""yamls://
- name: msg
  fields:
    - {name: ns, type: uint64, options.type: time_point, options.resolution: ns}
    - {name: us, type: int64, options.type: time_point, options.resolution: us}
    - {name: fms, type: double, options.type: time_point, options.resolution: ms}
    - {name: second, type: uint32, options.type: time_point, options.resolution: second}
    - {name: day, type: int32, options.type: time_point, options.resolution: day}
""")

    msg = scheme['msg']
    for now in [datetime.datetime(2021, 1, 27, 21, 47, 49, 123456), TimePoint.from_str("2000-01-02T03:04:05.123456789")]:
        m = msg.object(ns = now, us = now, fms=now, second = now, day = now)

        if isinstance(now, TimePoint):
            assert m.ns.value == now.value
            assert m.us.value == now.value // 1000
            now = now.datetime
        else:
            assert m.ns.value == int(now.timestamp()) * 1000000000 + now.microsecond * 1000
            assert m.us.value == int(now.timestamp()) * 1000000 + now.microsecond
        assert m.fms.value == pytest.approx(now.timestamp() * 1000, 0.001)
        assert m.second.value == int(now.timestamp())
        assert m.day.value == m.second.value // 86400

        u = msg.unpack(memoryview(m.pack()))
        assert u.as_dict() == m.as_dict()

def test_duration():
    scheme = S.Scheme("""yamls://
- name: msg
  fields:
    - {name: us, type: int64, options.type: duration, options.resolution: us}
    - {name: fms, type: double, options.type: duration, options.resolution: ms}
    - {name: second, type: uint32, options.type: duration, options.resolution: second}
    - {name: day, type: int32, options.type: duration, options.resolution: day}
""")

    dt = datetime.timedelta(days=5, seconds=12345, microseconds=123456)

    msg = scheme['msg']
    m = msg.object(us = dt, fms = dt, second = dt, day = dt)

    assert m.us.value == int(dt.total_seconds()) * 1000000 + dt.microseconds
    assert m.fms.value == dt.total_seconds() * 1000
    assert m.second.value == int(dt.total_seconds())
    assert m.day.value == m.second.value // 86400


    u = msg.unpack(memoryview(m.pack()))
    assert u.as_dict() == m.as_dict()

    m.us = '123ms'
    assert m.us == Duration(123000, 'us', int)

def _test_bits(scheme):
    scheme = S.Scheme(scheme)

    msg = scheme['msg']
    i8 = msg['i8'].type_bits
    u32 = msg['u32'].type_bits

    assert i8.name == 'i8'
    assert i8.type == msg['i8'].Type.Int8
    assert [(x.name, x.offset, x.size) for x in i8.values()] == [('a', 0, 1), ('b', 2, 1), ('c', 3, 1)]
    assert u32.name == 'u32'
    assert u32.type == msg['u32'].Type.UInt32
    assert [(x.name, x.offset, x.size) for x in u32.values()] == [('c', 0, 1), ('d', 1, 1), ('e', 2, 1)]

    m = msg.object(i8 = 0, u32 = 0)
    assert str(m.i8) == '{}'
    m.i8.a = 1
    assert str(m.i8) == '{a}'
    m.i8.b = 0
    m.i8.c = 1
    assert str(m.i8) == '{a, c}'
    m.i8 = 'b'
    assert str(m.i8) == '{b}'
    m.i8 = ' { c , c  , a } '
    assert str(m.i8) == '{a, c}'
    m.u32.d = 1

    assert m.i8._value == 9

    assert m.i8.a == True
    assert m.i8.b == False
    assert m.i8.c == True
    assert m.u32.c == False
    assert m.u32.d == True
    assert m.u32.e == False

    assert m.as_dict() == {'i8': {'a': True, 'b': False, 'c': True}, 'u32': {'c': False, 'd': True, 'e': False}}

    u = msg.unpack(memoryview(m.pack()))
    assert u.as_dict() == m.as_dict()

    m1 = msg.object(**m.as_dict())
    assert m1.as_dict() == m.as_dict()
    assert m1.pack() == m.pack()

    assert u.i8.a == True
    assert u.i8.b == False
    assert u.i8.c == True
    assert u.u32.c == False
    assert u.u32.d == True
    assert u.u32.e == False

    m.i8 = 0

    assert m.i8.a == False
    assert m.i8.b == False
    assert m.i8.c == False

    m.i8 = {'a', 'c'}

    assert m.i8.a == True
    assert m.i8.b == False
    assert m.i8.c == True

def test_bits_inline():
    _test_bits(
    """yamls://
- name: msg
  fields:
    - {name: i8, type: int8, options.type: bits, bits: [a, {name: b, offset: 2, size: 1}, c]}
    - {name: u32, type: uint32, options.type: bits, bits: [c, d, e]}
""")

def test_bits():
    _test_bits(
    """yamls://
- name:
  bits:
    i8: {type: int8, options.type: bits, bits: [a, {name: b, offset: 2, size: 1}, c]}
- name: msg
  bits:
    u32: {type: uint32, options.type: bits, bits: [c, d, e]}
  fields:
    - {name: i8, type: i8}
    - {name: u32, type: u32}
""")

def test_enum():
    scheme = S.Scheme("""yamls://
- name: msg
  enums:
    e8:
      type: uint8
      enum: {A: 1, B: 2, C: 3}
    e64:
      type: int64
      enum: {A: 10000, B: 20000, C: 30000}
  fields:
    - {name: e8, type: e8}
    - {name: e64, type: e64}
""")

    msg = scheme['msg']
    m = msg.object(e8 = 1, e64 = 'A')

    assert m.e8.value == 1
    assert m.e8 == m.e8.A

    assert m.e64.value == 10000
    assert m.e64 == m.e64.A

    u = msg.unpack(memoryview(m.pack()))
    assert u.as_dict() == m.as_dict()

    assert u.e8.value == 1
    assert u.e8 == u.e8.A

    assert u.e64.value == 10000
    assert u.e64 == u.e64.A

    assert m.SCHEME['e8'].from_string('1') == m.e8.A
    assert m.SCHEME['e8'].from_string('A') == m.e8.A

    class e8(enum.Enum):
        A = 10
        B = 20
        Other = 30

    with pytest.raises(ValueError): m.e64 = e8.A
    with pytest.raises(ValueError): m.e8 = e8.Other
    m.e8 = e8.A
    assert m.e8.name == 'A'

def test_enum_eq():
    SCHEME = """yamls://
- enums:
    e8: {type: uint8, enum: {A: 1, B: 2, C: 3}}
    e64: {type: int64, enum: {A: 1, B: 2, C: 3}}
"""
    s0 = S.Scheme(SCHEME)
    s1 = S.Scheme(SCHEME)

    class e8(enum.Enum):
        A = 10
        Other = 3

    assert s0.enums['e8'].klass.A == s0.enums['e8'].klass.A
    assert s0.enums['e8'].klass.A == s1.enums['e8'].klass.A
    assert s0.enums['e8'].klass.A != s0.enums['e64'].klass.A
    assert s0.enums['e8'].klass.A != s1.enums['e64'].klass.A

    assert s0.enums['e8'].klass.A == e8.A
    assert s0.enums['e8'].klass.C != e8.Other

def test_decimal128():
    scheme = S.Scheme("""yamls://
- name: msg
  fields:
    - {name: dec, type: decimal128}
    - {name: inf, type: decimal128}
    - {name: ninf, type: decimal128}
    - {name: nan, type: decimal128}
    - {name: snan, type: decimal128}
""")

    msg = scheme['msg']
    m = msg.object(dec=decimal.Decimal('1234567890.e-5'), inf='Inf', ninf='-Inf', nan='NaN', snan='sNaN')

    assert m.dec == decimal.Decimal('1234567890.e-5')
    assert m.inf == decimal.Decimal('Inf')
    assert m.ninf == decimal.Decimal('-Inf')
    assert m.nan.is_qnan()
    assert m.snan.is_snan()

    data = memoryview(m.pack())
    u = msg.unpack(data)

    assert u.dec == decimal.Decimal('1234567890.e-5')
    assert u.inf == decimal.Decimal('Inf')
    assert u.ninf == decimal.Decimal('-Inf')
    assert u.nan.is_qnan()
    assert u.snan.is_snan()

    assert m.SCHEME['dec'].from_string('123.4') == decimal.Decimal('123.4')

    with pytest.raises(ValueError): assert m.SCHEME['dec'].from_string('xxx')
    with pytest.raises(ValueError): m.dec = 'xxx'

def test_union_pack():
    scheme = S.Scheme("""yamls://
- name: sub
  fields:
    - {name: s0, type: int64}
- name: msg
  fields:
    - {name: pre, type: uint32}
    - name: u
      type: union
      union:
        - {name: i8, type: int8}
        - {name: array, type: 'int8[4]'}
        - {name: string, type: string}
        - {name: sub, type: sub}
    - {name: post, type: uint32}
""")

    msg = scheme['msg']
    u0 = msg['u'].type_union
    assert u0.union_size == 8
    assert [f.name for f in u0.fields] == ['i8', 'array', 'string', 'sub']
    assert [f.union_index for f in u0.fields] == list(range(4))

    m = msg.object(pre=0xffffffff, u=('sub', {'s0': 0xbeef}), post=0xffffffff)

    assert m.u.type == 'sub'
    assert m.u.value.as_dict() == {'s0': 0xbeef}

    assert hasattr(m.u, 'sub')
    assert not hasattr(m.u, 'array')
    assert m.u.sub == m.u.value

    assert m.as_dict() == {'pre':0xffffffff, 'u': {'sub': {'s0': 0xbeef}}, 'post':0xffffffff}

    data = memoryview(m.pack())
    u = msg.unpack(data)

    assert u.as_dict() == m.as_dict()

    m.u = {'i8': 100}

    assert m.as_dict() == {'pre':0xffffffff, 'u': {'i8': 100}, 'post':0xffffffff}

UNION_SCHEME ='''yamls://
- name: ''
  unions:
    uglobal: {union: [{name: i8, type: int8}, {name: d, type: double}]}
- name: msg
  unions:
    u0: {union: [{name: i8, type: int8}, {name: d, type: double}, {name: s, type: string}]}
  fields:
    - {name: u0, type: '*u0'}
    - {name: u1, type: u0}
    - {name: ug, type: uglobal}

'''
def _test_union(scheme):
    s = S.Scheme(scheme)

    assert [u.name for u in s.unions.values()] == ['uglobal']
    assert [u.name for u in s['msg'].unions.values()] == ['u0']

    u0 = s.unions['uglobal']
    assert u0.name == 'uglobal'
    assert u0.union_size == 8
    assert [f.name for f in u0.fields] == ['i8', 'd']
    assert [f.union_index for f in u0.fields] == list(range(2))

    u0 = s['msg'].unions['u0']
    assert u0.name == 'u0'
    assert u0.union_size == 8
    assert [f.name for f in u0.fields] == ['i8', 'd', 's']
    assert [f.union_index for f in u0.fields] == list(range(3))

    f0 = s['msg']['u0']
    assert f0.type == f0.Type.Pointer
    assert f0.type_ptr.type == f0.type_ptr.Type.Union

    u0 = f0.type_ptr.type_union
    assert u0.name == 'u0'
    assert u0.union_size == 8
    assert [f.name for f in u0.fields] == ['i8', 'd', 's']
    assert [f.union_index for f in u0.fields] == list(range(3))

def test_union():
    _test_union(UNION_SCHEME)

def test_union_dump():
    _test_union(S.Scheme(UNION_SCHEME).dump())

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

    scheme = S.Scheme(scheme)
    msg = scheme['msg']

    assert msg['f0'].optional == True
    assert msg['f1'].optional == True
    assert msg['pmap'].optional == False
    assert msg['f2'].optional == True
    assert msg['f3'].optional == False

    m = msg.klass(f0=100, f2=200)
    m.pmap = 0xff
    u = msg.unpack(memoryview(m.pack()))

    assert m.as_dict() == {'f0':100, 'f2':200, 'pmap': 0xff}
    assert u.as_dict() == {'f0':100, 'f2':200, 'f3':0}

    u.f0 = None
    assert u.as_dict() == {'f2':200, 'f3': 0}
    u.f0 = None
    assert u.as_dict() == {'f2':200, 'f3': 0}
    u.f3 = None
    assert u.as_dict() == {'f2':200}

def test_inline():
    scheme = '''yamls://
- name: a
  fields:
    - {name: a, type: int16, options: {common: a, oa: a}}
- name: b
  fields:
    - {name: a, type: a, options: {inline: yes, common: b, ob: b}}
    - {name: b, type: int32}
- name: msg
  id: 10
  fields:
    - {name: b, type: b, options: {inline: yes, common: c, oc: c}}
    - {name: c, type: int64}
'''

    scheme = S.Scheme(scheme)
    msg = scheme['msg']

    assert [(f.name, f.type) for f in msg.fields] == [('a', F.Int16), ('b', F.Int32), ('c', F.Int64)]
    assert scheme['a']['a'].options == {'common': 'a', 'oa': 'a'}
    assert scheme['b']['a'].options == {'common': 'b', 'oa': 'a', 'ob': 'b'}
    assert scheme['b']['b'].options == {}
    assert scheme['msg']['a'].options == {'common': 'c', 'oa': 'a', 'ob': 'b', 'oc': 'c'}
    assert scheme['msg']['b'].options == {'common': 'c', 'oc': 'c'}

    with pytest.raises(RuntimeError):
        S.Scheme('''yamls://
- name: a
  fields:
    - {name: a, type: int16}
- name: b
  fields:
    - {name: m, type: a, options.inline: yes}
    - {name: a, type: int32}
''')

    with pytest.raises(RuntimeError):
        S.Scheme('''yamls://
- name: a
  fields:
    - {name: a, type: int16}
- name: b
  fields:
    - {name: a, type: int32}
    - {name: m, type: a, options.inline: yes}
''')


def test_sub_create():
    scheme = '''yamls://
- name: Sub
  fields:
    - {name: s0, type: int16}
- name: Msg
  id: 10
  fields:
    - {name: pmap, type: uint16, options.pmap: yes}
    - {name: f0, type: int16}
    - {name: f1, type: Sub}
    - {name: f2, type: Sub, options.optional: yes}
    - {name: f3, type: '*int8'}
    - {name: f4, type: 'int8[4]'}
'''

    scheme = S.Scheme(scheme)
    msg = scheme.messages.Msg()

    msg.f1.s0 = 10
    assert msg.f1.s0 == 10
    assert not hasattr(msg, 'f2')
    assert msg.f3 == []
    assert msg.f4 == []


def test_sha256(with_scheme_hash):
    scheme = '''yamls://
- name: Data
  id: 10
  fields:
    - {name: f0, type: int16}
'''

    scheme = S.Scheme(scheme)
    yamls = scheme.dump('yamls')
    ssha = scheme.dump('sha256')
    with pytest.raises(OSError): scheme.dump('shaXXX')

    assert ssha == 'sha256://' + hashlib.sha256(yamls[len('yamls://'):].encode('ascii')).hexdigest()

def test_scheme_path(tmp_path):
    filename = 'scheme-path-test.yaml'
    open(tmp_path / filename, 'w').write('''- name: SchemePathTest''')

    with pytest.raises(RuntimeError):
        S.Scheme('yaml://' + filename)

    S.path_add(tmp_path)

    assert [m.name for m in S.Scheme('yaml://' + filename).messages] == ['SchemePathTest']

    S.path_remove(tmp_path)

    with pytest.raises(RuntimeError):
        S.Scheme('yaml://' + filename)

def test_large_pointer():
    scheme = S.Scheme("""yamls://
- name: Item
  fields:
    - {name: body, type: byte266, options.type: string}
- name: Data
  fields:
    - {name: list, type: '*Item'}
""")

    msg = scheme['Data']

    m = msg.object(list = [{'body': '0'}, {'body': '1'}])
    data = memoryview(m.pack())
    assert data[:12].tobytes() == b'\x08\x00\x00\x00\x02\x00\x00\xff\x0a\x01\x00\x00'

    u = msg.unpack(data)
    assert m.as_dict() == u.as_dict()

def test_sorted_import():
    SCHEME = """yamls://
meta.import:
    e00: "yamls://[{options.node: e00, enums.E09: {type: int8, enum: {A: 9}}}]"
    e09: "yamls://[{options.node: e09, enums.E00: {type: int8, enum: {A: 0}}}]"
    b00: "yamls://[{options.node: b00, bits.B09: {type: uint8, bits: {A, B}}}]"
    b09: "yamls://[{options.node: b09, bits.B00: {type: uint8, bits: {D, E}}}]"
    u00: "yamls://[{options.node: u00, unions.U09.union: [{name: i8, type: int8}]}]"
    u09: "yamls://[{options.node: u09, unions.U00.union: [{name: u8, type: uint8}]}]"
"""

    s0 = S.Scheme(SCHEME)
    cfg = Config.load(SCHEME)

    for k,v in cfg.sub('meta.import').browse('*'):
        assert v in s0.imports
        assert s0.imports[v].options == {'node': k}

    assert list(s0.enums.keys()) == sorted(['E09', 'E00'])
    assert list(s0.bits.keys()) == sorted(['B09', 'B00'])
    assert list(s0.unions.keys()) == sorted(['U09', 'U00'])

    assert s0.enums['E00'].options.get('_import') == cfg.get('meta.import.e09')
    assert s0.enums['E09'].options.get('_import') == cfg.get('meta.import.e00')

    assert s0.bits['B00'].options.get('_import') == cfg.get('meta.import.b09')
    assert s0.bits['B09'].options.get('_import') == cfg.get('meta.import.b00')

    assert s0.unions['U00'].options.get('_import') == cfg.get('meta.import.u09')
    assert s0.unions['U09'].options.get('_import') == cfg.get('meta.import.u00')

    s1 = S.Scheme(s0.dump('yamls'))
    print(s0.dump('yamls'))

    assert s1.imports == {}
    assert list(s1.enums.keys()) == list(s0.enums.keys())
    assert list(s1.bits.keys()) == list(s0.bits.keys())
    assert list(s1.unions.keys()) == list(s0.unions.keys())
    for i in s1.enums.values(): assert i.options == {}
    for i in s1.bits.values(): assert i.options == {}
    for i in s1.unions.values(): assert i.options == {}
