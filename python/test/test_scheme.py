#!/usr/bin/env python
# -*- coding: utf-8 -*-
# vim: sts=4 sw=4 et

from common import *

import tll.scheme as S
from tll.scheme import Field as F
from tll.s2b import s2b
from tll.chrono import *

import copy
import datetime
import decimal
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
    M = s['sub'].object(s0=10, s1=[1., 2., 3.])
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

    assert msg['ts'].from_string("100ns") == TimePoint(100, 'ns') # FIXME: Change to normal time format?

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
  import:
    - |
        yamls://
        - name:
          aliases:
            - {name: license, type: byte64, options.type: string}
    - |
        yamls://
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

def test_unsigned():
    scheme = S.Scheme("""yamls://
- name: msg
  fields:
    - {name: u8, type: uint8}
    - {name: u16, type: uint16}
    - {name: u32, type: uint32}
""")

    msg = scheme['msg']
    m = msg.object(u8 = 200, u16 = 50000, u32 = 0xf0000000)
    assert m.u8 == 200
    assert m.u16 == 50000
    assert m.u32 == 0xf0000000
    with pytest.raises(OverflowError): msg['u8'].from_string("0x100")
    with pytest.raises(OverflowError): msg['u16'].from_string("0x10000")
    with pytest.raises(OverflowError): msg['u32'].from_string("0x100000000")
    for f in msg.fields:
        with pytest.raises(OverflowError): f.from_string("-1")
        with pytest.raises(OverflowError): setattr(m, f.name, -1)
    data = memoryview(m.pack())
    u = msg.unpack(data)
    r = msg.reflection(data)
    assert u.u8 == m.u8
    assert u.u16 == m.u16
    assert u.u32 == m.u32
    assert r.u8 == m.u8
    assert r.u16 == m.u16
    assert r.u32 == m.u32

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
""")

    msg = scheme['msg']
    for (i,f) in enumerate(msg.fields):
        assert f.fixed_precision == i + 1, f"field {f.name}: {f.fixed_precision} != {i + 1}"

    m = msg.object(i8=-1, i16=2, i32=-3, i64=456, u8='0.0007', u16='0.012345', u32='123')
    assert m.i8 == decimal.Decimal(-1)
    assert m.i16 == 2
    assert m.i32 == decimal.Decimal(-3)
    assert m.i64 == decimal.Decimal(456)
    assert m.u8 == decimal.Decimal('0.0007')
    assert m.u16 == decimal.Decimal('0.012345')
    assert m.u32 == decimal.Decimal('123')

    data = memoryview(m.pack())
    u = msg.unpack(data)
    assert m.as_dict() == u.as_dict()

    assert m.SCHEME['i16'].from_string('123.4') == decimal.Decimal('123.4')
    assert m.SCHEME['u16'].from_string('123.4') == decimal.Decimal('123.4')

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

def test_time_point():
    scheme = S.Scheme("""yamls://
- name: msg
  fields:
    - {name: us, type: int64, options.type: time_point, options.resolution: us}
    - {name: fms, type: double, options.type: time_point, options.resolution: ms}
    - {name: second, type: uint32, options.type: time_point, options.resolution: second}
    - {name: day, type: int32, options.type: time_point, options.resolution: day}
""")

    now = datetime.datetime(2021, 1, 27, 21, 47, 49, 123456)

    msg = scheme['msg']
    m = msg.object(us = now, fms=now, second = now, day = now)

    assert m.us.value == int(now.timestamp()) * 1000000 + now.microsecond
    assert m.fms.value == now.timestamp() * 1000
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

def test_bits():
    scheme = S.Scheme("""yamls://
- name: msg
  fields:
    - {name: i8, type: int8, options.type: bits, bits: [a, b, c]}
    - {name: u32, type: uint32, options.type: bits, bits: [c, d, e]}
""")

    msg = scheme['msg']
    m = msg.object(i8 = 0, u32 = 0)
    m.i8.a = 1
    m.i8.b = 0
    m.i8.c = 1
    m.u32.d = 1

    assert m.i8._value == 5

    assert m.i8.a == True
    assert m.i8.b == False
    assert m.i8.c == True
    assert m.u32.c == False
    assert m.u32.d == True
    assert m.u32.e == False

    u = msg.unpack(memoryview(m.pack()))
    assert u.as_dict() == m.as_dict()

    assert u.i8.a == True
    assert u.i8.b == False
    assert u.i8.c == True
    assert u.u32.c == False
    assert u.u32.d == True
    assert u.u32.e == False
