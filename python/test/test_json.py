#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# vim: sts=4 sw=4 et

from tll.test_util import Accum
from tll.channel import Context
from tll.error import TLLError

import decimal
import json
import pytest

WITH_JSON = Context().has_impl('json+')

@pytest.fixture
def context():
    return Context()

'''
    def test_list_overflow(self):
        self.input.post(json.dumps({'_tll_name':'list_sub', 'f1':[float(i) for i in range(5)]}).encode('utf-8'))
        assert self.output.result == []

        self.input.post(json.dumps({'_tll_name':'list_test', 'f2':[{'s':str(i)} for i in range(5)]}).encode('utf-8'))
        assert self.output.result == []

    def _test_list_resize(self, msg, data):
        self.input.post(json.dumps(dict(_tll_name=msg, **data)).encode('utf-8'))

        assert self.output.result != []
        r = self.output.unpack(self.output.result[-1])
        print(r.as_dict())
        assert r.as_dict() == data
        self.output.result = []

    def test_list_resize(self):
        self._test_list_resize('list_test', {'f0':[list(range(i, i + 5)) for i in range(5)]})
        self._test_list_resize('list_test', {'f1':[{'f0':[i], 's': '{:d}'.format(i)} for i in range(10)]})
'''

@pytest.mark.skipif(not WITH_JSON, reason='JSON not supported')
@pytest.mark.parametrize("t,v", [
    ('int8', 123),
    ('int16', 12323),
    ('int32', 123123),
    ('int64', 123123123),
    ('uint8', 231),
    ('uint16', 53123),
    ('uint32', 123123),
    ('uint64', 123123123),
    ('double', 123.123),
#    ('byte8', b'abcd\0\0\0\0'),
    ('byte8, options.type: string', 'abcd'),
    ('string', 'abcd'),
#    ('decimal128', (decimal.Decimal('1234567890.e-5'), '1234567890.e-5')),
#    ('int32, options.type: fixed3', decimal.Decimal('123.456')),
#    ('int32, options.type: duration, options.resolution: us', (Duration(123000, Resolution.us), '12300us')),
#    ('int64, options.type: time_point, options.resolution: s', (TimePoint(1609556645, Resolution.second), '2021-01-02T03:04:05')),
    ('"int32[4]"', [1, 2, 3]),
    ('"*int32"', [1, 2, 3]),
    ('"*string"', ['a', 'bc', 'def']),
    ('sub', {'s0': 10, 's1': 'string'}),
    ('"*sub"', [{'s0': 10, 's1': '10'}, {'s0': 20, 's1': '20'}]),
    ('"sub[4]"', [{'s0': 10, 's1': '10'}, {'s0': 20, 's1': '20'}]),
])
def test_simple(t, v):
    if isinstance(v, tuple):
        v, jv = v
    else:
        jv = v
    scheme = f'''yamls://
- name: sub
  fields:
    - {{name: s0, type: int32 }}
    - {{name: s1, type: string }}
- name: msg
  id: 10
  fields:
    - {{name: g0, type: int64 }}
    - {{name: f0, type: {t} }}
    - {{name: g1, type: int64 }}
'''
    s = Accum('json+direct://', scheme=scheme, name='json', dump='scheme')
    c = Accum('direct://', name='raw', master=s, dump='text')
    s.open()
    c.open()

    s.post({'g0': -1, 'f0': v, 'g1': -1}, name='msg', seq=100)

    assert [(m.msgid, m.seq) for m in c.result] == [(10, 100)]
    data = json.loads(c.result[0].data.tobytes())
    assert data == {'_tll_name': 'msg', '_tll_seq': 100, 'f0': jv, 'g0': -1, 'g1': -1}

    c.post(json.dumps(data).encode('utf-8'))
    assert [(m.msgid, m.seq) for m in s.result] == [(10, 100)]
    assert s.unpack(s.result[0]).as_dict() == {'g0': -1, 'g1': -1, 'f0': v}

@pytest.mark.skipif(not WITH_JSON, reason='JSON not supported')
@pytest.mark.parametrize("t,v", [
    ('int8', 123),
    ('int16', 12312),
    ('int32', 123123123),
    ('int64', 123123123123),
    ('uint8', 123),
    ('uint16', 12312),
    ('uint32', 123123123),
    ('uint64', 123123123123),
])
def test_enum(context, t,v):
    scheme = '''yamls://
- name: msg
  id: 10
  enums:
    e1: {type: %(t)s,  enum: {A: %(v)s, B: 2}}
    e2: {type: %(t)s, enum: {C: %(v)s, D: 2}}
  fields:
    - {name: g0, type: int64}
    - {name: f0, type: e1, options.json.enum-as-int: yes}
    - {name: f1, type: e2}
    - {name: g1, type: int64}
''' % {'t': t, 'v': v}
    s = Accum('json+direct://', scheme=scheme, name='json', dump='scheme', context=context)
    c = Accum('direct://', name='raw', master=s, dump='text', context=context)
    s.open()
    c.open()

    s.post({'g0': -1, 'f0': 'A', 'f1': v, 'g1': -1}, name='msg', seq=100)

    assert [(m.msgid, m.seq) for m in c.result] == [(10, 100)]
    data = json.loads(c.result[0].data.tobytes())
    assert data == {'_tll_name': 'msg', '_tll_seq': 100, 'f0': v, 'f1': 'C', 'g0': -1, 'g1': -1}

    c.post(json.dumps(data).encode('utf-8'))
    assert [(m.msgid, m.seq) for m in s.result] == [(10, 100)]
    r = s.unpack(s.result[0])
    assert r.as_dict() == {'g0': -1, 'g1': -1, 'f0': r.f0.A, 'f1': r.f1.C}

def test_default_message(context):
    scheme = '''yamls://
- name: Default
  id: 10
  fields:
    - {name: f0, type: int64}
- name: Data
  id: 20
  fields:
    - {name: f0, type: double}
'''
    s = Accum('json+direct://;default-message=Default', scheme=scheme, name='json', dump='yes', context=context)
    c = context.Channel('direct://', name='raw', master=s, dump='text')
    s.open()
    c.open()

    c.post(json.dumps({'f0': 123.456, '_tll_name': 'Data', '_tll_seq': 100}).encode('utf-8'))
    c.post(json.dumps({'f0': 1000}).encode('utf-8'))

    assert [(m.msgid, m.seq) for m in s.result] == [(20, 100), (10, 0)]
    assert s.unpack(s.result[0]).as_dict() == {'f0': 123.456}
    assert s.unpack(s.result[1]).as_dict() == {'f0': 1000}

def test_pointer_large(context):
    scheme = """yamls://
- name: Item
  fields:
    - {name: body, type: byte266, options.type: string}
- name: Data
  id: 10
  fields:
    - {name: list, type: '*Item'}
"""
    s = Accum('json+direct://', scheme=scheme, name='json', dump='yes', context=context)
    c = Accum('direct://', name='raw', master=s, dump='text', context=context)

    s.open()
    c.open()

    c.post(json.dumps({'_tll_name': 'Data', 'list': [{"body": "0000"}, {"body": "1111"}]}).encode('utf-8'))

    assert [m.seq for m in s.result] == [0]
    m = s.result[0]
    assert m.data[:12].tobytes() == b'\x08\x00\x00\x00\x02\x00\x00\xff\x0a\x01\x00\x00'
    assert s.unpack(m).as_dict() == {'list': [{'body': '0000'}, {'body': '1111'}]}

    s.post(s.unpack(m).as_dict(), name='Data')

    assert json.loads(c.result[0].data.tobytes()) == {'_tll_name': 'Data', '_tll_seq': 0, 'list': [{"body": "0000"}, {"body": "1111"}]}

def test_inverted(context):
    scheme = """yamls://
- name: Data
  id: 10
  fields:
    - {name: f0, type: int32}
"""
    s = Accum('json+direct://', scheme=scheme, name='json', dump='yes', inverted='yes', context=context)
    c = Accum('direct://', name='raw', master=s, dump='text', context=context)

    s.open()
    c.open()

    s.post(json.dumps({'_tll_name': 'Data', '_tll_seq': 100, 'f0': 1000}).encode('utf-8'))

    m = c.result[0]
    assert (m.seq, m.msgid) == (100, 10)
    assert c.unpack(m).as_dict() == {'f0': 1000}

    c.post(m)

    assert json.loads(s.result[0].data.tobytes()) == {'_tll_name': 'Data', '_tll_seq': 100, 'f0': 1000}
