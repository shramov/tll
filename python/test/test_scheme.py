#!/usr/bin/env python
# -*- coding: utf-8 -*-
# vim: sts=4 sw=4 et

from common import *

import tll.scheme as S
from tll.scheme import Field as F
from tll.s2b import s2b

import struct

from nose import SkipTest
from nose.tools import *

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
    assert_equals([m.name for m in s.messages], ["sub", "test", "enums"])
    sub = s['sub']
    assert_equals([(f.name, f.type) for f in sub.fields],
        [("s0", F.Int32), ("s1", F.Array)])
    assert_equals(sub['s1'].options, {})
    assert_equals(sub['s1'].count_ptr.type, F.Int8)
    assert_equals(sub['s1'].type_array.type, F.Double)
    assert_equals(sub.size, 4 + 1 + 8 * 4)

    msg = s['test']
    assert_equals([(f.name, f.type) for f in msg.fields],
        [("f0", F.Int8), ("f1", F.Int64), ("f2", F.Double), ("f3", F.Decimal128), ("f4", F.Bytes), ("f5", F.Pointer), ("f6", F.Array), ("f7", F.Pointer), ('f8', F.Pointer)])
    assert_equals(msg['f6'].count_ptr.type, F.Int16)
    assert_equals(msg['f6'].type_array.type, F.Message)
    assert_equals([(f.name, f.sub_type) for f in msg.fields if f.sub_type != F.Sub.NONE],
        [("f7", F.Sub.ByteString)])
    assert_equals([(f.name, f.size) for f in msg.fields],
        [("f0", 1), ("f1", 8), ("f2", 8), ("f3", 16), ("f4", 32), ("f5", 8), ("f6", 2 + 4 * sub.size), ("f7", 8), ("f8", 8)])
    assert_equals(msg.size, 1 + 8 + 8 + 16 + 32 + 8 + (2 + 4 * sub.size) + 8 + 8)

    msg = s['enums']
    assert_equals([(f.name, f.type) for f in msg.fields],
        [("f0", F.Int8), ("f1", F.Int16), ("f2", F.Int32), ("f3", F.Int64)])
    assert_equals([(f.name, f.sub_type) for f in msg.fields],
        [("f0", F.Sub.Enum), ("f1", F.Sub.Enum), ("f2", F.Sub.Enum), ("f3", F.Sub.Enum)])
    assert_equals([(f.name, f.size) for f in msg.fields],
        [("f0", 1), ("f1", 2), ("f2", 4), ("f3", 8)])
    assert_equals(msg.size, 1 + 2 + 4 + 8)

def test():
    return _test(S.Scheme(scheme))

def test_copy():
    return _test(S.Scheme(scheme).copy())

def test_dump():
    print(S.Scheme(scheme).dump().decode('utf-8'))
    return _test(S.Scheme(S.Scheme(scheme).dump()))

def _test_options(s):
    for m in s.messages: print(m.options)
    sub = s['sub']
    msg = s['test']
    assert_equals(s['sub'].options, {'a':'1', 'b':'2'})
    assert_equals(s['test'].options, {'m':'10'})
    assert_equals(s['test']['f0'].options, {'a':'10', 'b':'20'})
    assert_equals(s['test']['f1'].options, {})
    assert_equals(s['test']['f5'].options, {})
    assert_equals(s['test']['f5'].type_ptr.options, {'a': '20'})
    assert_equals(s['test']['f6'].options, {'count-type': 'int16'})
    assert_equals(s['test']['f6'].type_array.options, {'a': '30'})
    assert_equals(s['enums'].enums['e1'].options, {'ea': '30', 'eb': '40'})
    assert_equals(s['enums'].enums['f1'].options, {"_auto": "inline"})

def test_options():
    return _test_options(S.Scheme(scheme))

def test_options_dump():
    return _test_options(S.Scheme(S.Scheme(scheme).dump()))

def test_pack():
    s = S.Scheme(scheme)
    M = s['sub'].object(s0=10, s1=[1., 2., 3.])
    data = M.pack()
    assert_equals(len(data), M.SCHEME.size)
    assert_equals(struct.unpack("=ibdddd", data), (10, 3, 1., 2., 3., 0.))
    m1 = s['sub'].object().unpack(data)
    assert_equals(m1.s0, 10)
    assert_equals(m1.s1, [1., 2., 3.])

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
    assert_equals(len(data), M.size + 2 * 4 + 3 * 8 + 3 * 4)
    assert_equals(struct.unpack("=ibiiQQiiQQQiii", data), (10, 2, 20, 30, optr(16, 2, 1), optr(16, 3, 1), 40, 50, optr(24, 1, 1), optr(20, 0, 1), optr(12, 2, 1), 60, 70, 80))
    m1 = M.object().unpack(data)
    assert_equals(m1.f0.s0, 10)
    assert_equals([x.s0 for x in m1.f1], [20, 30])
    assert_equals([x.s0 for x in m1.f2], [40, 50])
    assert_equals([[x.s0 for x in y] for y in m1.f3], [[60], [], [70, 80]])

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
    assert_equals(len(data), M.size + len("string") + len("nestedtail") + 3 * (8 + 1))
    assert_equals(struct.unpack("=QQQ6s10sQQQ1s1s1s", data), (optr(24, 6, 1), optr(22, 10, 1), optr(24, 3, 1), s2b("ыыы"), b"nestedtail", optr(24, 1, 1), optr(17, 1, 1), optr(10, 1, 1), b'a', b'b', b'c'))
    m1 = M.object().unpack(data)
    assert_equals(m1.f0, "ыыы")
    assert_equals(m1.f1.s0, "nestedtail")

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
    assert_equals([m.name for m in s.messages], ["msg"])
    assert_equals([(f.name, f.type) for f in s.aliases.values()], [("license", F.Bytes), ("license_list", F.Pointer)])

    msg = s['msg']
    assert_equals([(f.name, f.type) for f in msg.fields], [("f0", F.Bytes), ("f1", F.Pointer), ("f2", F.Pointer)])
    assert_equals([(f.name, f.sub_type) for f in msg.fields if f.sub_type != F.Sub.NONE],
        [("f0", F.Sub.ByteString)])
    assert_equals([(f.name, f.type_ptr.type, f.type_ptr.sub_type) for f in msg.fields if hasattr(f, 'type_ptr')],
        [("f1", F.Bytes, F.Sub.ByteString), ("f2", F.Bytes, F.Sub.ByteString)])

    #m0 = M.object(f0 = "license", f1 = ['a', 'b'], f2 = ['c', 'd'])

def test_alias(): return _test_alias(S.Scheme(SCHEME_ALIAS))
def test_alias_copy(): return _test_alias(S.Scheme(SCHEME_ALIAS).copy())
def test_alias_dump():
    print(S.Scheme(SCHEME_ALIAS).dump())
    return _test_alias(S.Scheme(S.Scheme(SCHEME_ALIAS).dump()))
