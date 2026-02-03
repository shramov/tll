#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import pytest
from decimal import Decimal
import enum

from tll.channel import Context
from tll.chrono import TimePoint, Duration
from tll.error import TLLError
from tll.test_util import Accum

@pytest.fixture(scope='module')
def context():
    return Context()

@pytest.mark.parametrize("t,v", [
    ('int8', 123),
    ('int16', 12323),
    ('int32', 123123),
    ('int64', 123123),
    ('uint8', 231),
    ('uint16', 53123),
    ('uint32', 123123),
    ('double', 123.123),
    ('byte8', b'abcd\0\0\0\0'),
    ('byte8, options.type: string', 'abcd'),
    ('string', 'abcd'),
    ('decimal128', Decimal('123.123')),
    ('int32, options.type: fixed3', Decimal('-123.123')),
    ('uint32, options.type: fixed3', Decimal('123.123')),
    ('int64, options.type: time_point, options.resolution: us', TimePoint(1609556645, 'second')),
    ('int64, options.type: duration, options.resolution: ns', Duration(123, 'ns')),
])
def test_extend(context, t, v):
    SInner = f'''yamls://
- name: Data
  id: 10
  fields:
    - {{ name: f0, type: {t} }}
'''
    SOuter = f'''yamls://
- name: Data
  id: 10
  fields:
    - {{ name: header, type: int32}}
    - {{ name: f0, type: {t} }}
    - {{ name: footer, type: int32}}
'''
    s = Accum('convert+direct://;name=server', dump='yes', scheme=SOuter, context=context, **{'direct.scheme': SInner, 'direct.dump': 'yes'})
    c = Accum('direct://;name=client', context=context, master=s)

    s.open()
    c.open()

    c.post({'f0': v}, name='Data', seq=100)
    assert [(m.msgid, m.seq) for m in s.result] == [(10, 100)]
    assert s.unpack(s.result[0]).as_dict() == {'f0': v, 'header': 0, 'footer': 0}

    s.post({'f0': v, 'header': 0xff, 'footer': 0xff}, name='Data', seq=200)
    assert [(m.msgid, m.seq) for m in c.result] == [(10, 200)]
    assert c.unpack(c.result[0]).as_dict() == {'f0': v}

@pytest.mark.parametrize("t,v", [
    ('int8', -129),
    ('int8', 128),
    ('int16', -0x8001),
    ('int16', 0x8000),
    ('int32', -0x80000001),
    ('int32', 0x80000000),
    ('int64', -0x8000000000100000),
    ('int64', 0x8000000000000000),
    ('uint8', -1),
    ('uint8', 0x100),
    ('uint16', -1),
    ('uint16', 0x10000),
    ('uint32', -1),
    ('uint32', 0x100000000),
    ('uint64', -1),
    ('uint64', 0x10000000000100000),
#    ('byte4', b'abcdef'),
    ('byte4, options.type: string', b'abcdef'),
#    ('double', Decimal('10e500')),
])
def test_out_of_range(context, t, v):
    SInner = f'''yamls://
- name: Data
  id: 10
  fields:
    - {{ name: f0, type: {t} }}
'''
    if t.startswith('byte4'):
        outer = t.replace('byte4', 'byte8')
    elif 'int' in t:
        outer = 'int64' if v < 0 else 'uint64'
        if outer == t:
            outer = 'double'
    elif isinstance(v, Decimal):
        outer = 'decimal128'
    SOuter = f'''yamls://
- name: Data
  id: 10
  fields:
    - {{ name: f0, type: {outer} }}
'''
    c = Accum('convert+null://;name=server', dump='yes', scheme=SOuter, context=context, **{'null.scheme': SInner, 'direct.dump': 'yes'})
    c.open()

    with pytest.raises(TLLError): c.post({'f0': v}, name='Data')
#    if outer == 'decimal128': c.post({'f0': v}, name='Data')

@pytest.mark.parametrize("outer", ['string', 'byte8, options.type: string'])
@pytest.mark.parametrize("t,v", [
    ('int8', 123),
    ('int16', 12323),
    ('int32', 123123),
    ('int64', 123123),
    ('uint8', 231),
    ('uint16', 53123),
    ('uint32', 123123),
    ('double', 123.123),
    ('byte8, options.type: string', 'abcd'),
    ('string', 'abcd'),
    ('decimal128', (Decimal('123.123'), '123123.E-3')),
])
def test_to_string(context, t, v, outer):
    SInner = f'''yamls://
- name: Data
  id: 10
  fields:
    - {{ name: f0, type: {t} }}
'''
    SOuter = f'''yamls://
- name: Data
  id: 10
  fields:
    - {{ name: f0, type: {outer} }}
'''
    s = Accum('convert+direct://;name=server', dump='yes', scheme=SOuter, context=context, **{'direct.scheme': SInner, 'direct.dump': 'yes'})
    c = Accum('direct://;name=client', context=context, master=s)

    s.open()
    c.open()

    if isinstance(v, tuple):
        v, sv = v
    else:
        sv = str(v)
    c.post({'f0': v}, name='Data', seq=100)
    if outer.startswith('byte8') and len(sv) > 8:
        assert s.state == s.State.Error
    else:
        assert [(m.msgid, m.seq) for m in s.result] == [(10, 100)]
        assert s.unpack(s.result[0]).as_dict() == {'f0': sv }

def _test_convert(context, tfrom, tinto, vfrom, vinto, fheader="", iheader=""):
    SFrom = f'''yamls://
{fheader}
- name: Data
  id: 10
  fields:
    - {{ name: header, type: uint16 }}
    - {{ name: f0, type: {tfrom} }}
    - {{ name: footer, type: uint16 }}
'''
    SInto = f'''yamls://
{iheader}
- name: Data
  id: 10
  fields:
    - {{ name: header, type: uint16 }}
    - {{ name: f0, type: {tinto} }}
    - {{ name: footer, type: uint16 }}
'''
    s = Accum('convert+direct://;name=server', dump='yes', scheme=SInto, context=context, **{'direct.scheme': SFrom, 'direct.dump': 'yes'})
    c = Accum('direct://;name=client', context=context, master=s)

    s.open()
    c.open()

    assert s.state == s.State.Active

    c.post({'header': 0xffff, 'f0': vfrom, 'footer': 0xffff}, name='Data', seq=100)
    assert [(m.msgid, m.seq) for m in s.result] == [(10, 100)]
    if callable(vinto):
        assert vinto(s, s.result[0]) != False
    else:
        assert s.unpack(s.result[0]).as_dict() == {'f0': vinto, 'header': 0xffff, 'footer': 0xffff}

def test_smaller_size(context):
    def check(channel, msg):
        assert len(msg.data) == 5
    _test_convert(context, 'int64', 'int8', 10, check)

@pytest.mark.parametrize("outer", [
    'int8',
    'int16',
    'int32',
    'int64',
    'uint8',
    'uint16',
    'uint32',
    'uint64',
    'double',
    'int16, options.type: fixed2',
    'uint32, options.type: fixed4',
])
@pytest.mark.parametrize("t", [
    'int8',
    'int16',
    'int32',
    'int64',
    'uint8',
    'uint16',
    'uint32',
    'uint64',
    'double',
    'int16, options.type: fixed2',
    'uint32, options.type: fixed4',
])
def test_numeric(context, t, outer):
    vfrom = 123
    if t == 'double':
        vfrom = 123.12
    elif 'fixed' in t:
        vfrom = Decimal('123.12')

    if 'fixed' in outer:
        vinto = Decimal('123.12') if vfrom != 123 else vfrom
    elif outer == 'double':
        vinto = float(vfrom)
    else:
        vinto = int(vfrom)
    _test_convert(context, t, outer, vfrom, vinto)

@pytest.mark.parametrize("tinto", [
    '"*int32"',
    '"*int32", list-options.offset-ptr-type: legacy-short',
    '"*int32", list-options.offset-ptr-type: legacy-short',
    '"int32[4]"',
    '"int32[8]"',
    '"int32[12]"',
])
@pytest.mark.parametrize("tfrom", [
    '"*int16"',
    '"*int32"',
    '"*int64"',
    '"*int16", list-options.offset-ptr-type: legacy-short',
    '"*int16", list-options.offset-ptr-type: legacy-short',
    '"int16[8]"',
    '"int32[8]"',
    '"int64[8]"',
])
def test_list(context, tinto, tfrom):
    _test_convert(context, tfrom, tinto, [1, 2, 3], [1, 2, 3])

@pytest.mark.parametrize("stype", ['Sub', '"*Sub"', '"Sub[4]"'])
def test_sub(context, stype):
    t, outer = 'int32', 'int64'
    SInner = f'''yamls://
- name: Sub
  fields:
    - {{ name: s0, type: {t} }}
- name: Data
  id: 10
  fields:
    - {{ name: header, type: uint16 }}
    - {{ name: f0, type: {stype} }}
    - {{ name: footer, type: uint16 }}
'''
    SOuter = f'''yamls://
- name: Sub
  fields:
    - {{ name: sheader, type: int32 }}
    - {{ name: s0, type: {outer} }}
- name: Data
  id: 10
  fields:
    - {{ name: header, type: uint32 }}
    - {{ name: f0, type: {stype} }}
    - {{ name: footer, type: uint32 }}
'''
    s = Accum('convert+direct://;name=server', dump='yes', scheme=SOuter, context=context, **{'direct.scheme': SInner, 'direct.dump': 'yes'})
    c = Accum('direct://;name=client', context=context, master=s)

    s.open()
    c.open()
    v = [{'s0': 123}, {'s0': 456}]
    if stype == 'Sub':
        v = v[0]

    c.post({'header': 0xffff, 'f0': v, 'footer': 0xffff}, name='Data', seq=100)
    assert [(m.msgid, m.seq) for m in s.result] == [(10, 100)]

    if stype != 'Sub':
        for i in v:
            i['sheader'] = 0
    else:
        v['sheader'] = 0
    assert s.unpack(s.result[0]).as_dict() == {'f0': v, 'header': 0xffff, 'footer': 0xffff}

@pytest.mark.parametrize("einto", [
    '{A: 10}',
    '{A: 10, B: 100}',
    '{A: 100, B: 10}',
])
@pytest.mark.parametrize("tinto", [
    'uint16',
    'int32',
])
@pytest.mark.parametrize("efrom", [
    '{A: 10}',
    '{A: 10, B: 100}',
    '{A: 100, B: 10}',
])
@pytest.mark.parametrize("tfrom", [
    'uint16',
    'int32',
])
def test_enum(context, tinto, tfrom, einto, efrom):
    class f0(enum.Enum):
        A = 10
    _test_convert(context, f'{tfrom}, options.type: enum, enum: {efrom}', f'{tinto}, options.type: enum, enum: {einto}', 'A', f0.A)

def test_enum_extend(context):
    class Enum(enum.Enum):
        A = 10
    _test_convert(context,
                  'Enum', 'Enum', 'New', Enum.A,
                  iheader='- enums.Enum: {type: int32, enum: {A: 10}}',
                  fheader='- enums.Enum: {type: int32, enum: {A: 10, New: 20}, options.fallback-value: A}')

@pytest.mark.parametrize("mode", ["duration", "time_point"])
@pytest.mark.parametrize("rinto", [
    '',
    'ms',
    'minute',
])
@pytest.mark.parametrize("tinto", [
    'int64',
    'uint32',
])
@pytest.mark.parametrize("rfrom", [
    '',
    'ms',
    'minute',
])
@pytest.mark.parametrize("tfrom", [
    'int64',
    'uint32',
])
def test_time(context, mode, tinto, tfrom, rinto, rfrom):
    sinto = f', options.type: {mode}, options.resolution: {rinto}' if rinto else ''
    sfrom = f', options.type: {mode}, options.resolution: {rfrom}' if rfrom else ''
    kfrom = kinto = Duration if mode == 'duration' else TimePoint
    if rfrom == '': kfrom = lambda v, _: v
    if rinto == '': kinto = lambda v, _: v if rfrom in ('', 'minute') else v * (60 * 1000)
    _test_convert(context, f'{tfrom}{sfrom}', f'{tinto}{sinto}', kfrom(15, 'minute'), kinto(15, 'minute' if rfrom else rinto))

def test_pmap(context):
    SFrom = f'''yamls://
- name: Data
  id: 10
  options.defaults.optional: yes
  fields:
    - {{ name: pmap, type: uint16, options.pmap: yes }}
    - {{ name: f0, type: uint16, options.optional: no }}
    - {{ name: f1, type: uint16, options.optional: no }}
    - {{ name: f2, type: uint16 }}
    - {{ name: f3, type: uint16 }}
'''
    SInto = f'''yamls://
- name: Data
  id: 10
  options.defaults.optional: yes
  fields:
    - {{ name: e0, type: uint16 }}
    - {{ name: e1, type: uint16, options.optional: no }}
    - {{ name: f0, type: uint16, options.optional: no }}
    - {{ name: f1, type: uint16 }}
    - {{ name: f2, type: uint16, options.optional: no }}
    - {{ name: f3, type: uint16 }}
    - {{ name: pmap, type: uint16, options.pmap: yes }}
'''
    s = Accum('convert+direct://;name=server', dump='yes', scheme=SInto, context=context, **{'direct.scheme': SFrom, 'direct.dump': 'yes'})
    c = Accum('direct://;name=client', context=context, master=s)

    s.open()
    c.open()

    assert s.state == s.State.Active

    c.post({'f0': 100, 'f1': 200}, name='Data', seq=100)
    assert [(m.msgid, m.seq) for m in s.result] == [(10, 100)]
    assert s.unpack(s.result[0]).as_dict() == {'e1': 0, 'f0': 100, 'f1': 200, 'f2': 0}

    s.result = []
    c.post({'f2': 100, 'f3': 200}, name='Data', seq=200)
    assert [(m.msgid, m.seq) for m in s.result] == [(10, 200)]
    assert s.unpack(s.result[0]).as_dict() == {'e1': 0, 'f0': 0, 'f1': 0, 'f2': 100, 'f3': 200}

def test_msgid(context):
    SFrom = '''yamls://
- name: Data
  id: 10
  fields:
    - { name: f0, type: int16 }
- name: New
  id: 20
  fields:
    - { name: f0, type: int16 }
'''
    SInto = '''yamls://
- name: Data
  id: 10
  fields:
    - { name: f0, type: int16 }
'''
    s = Accum('convert+direct://;name=server', dump='yes', scheme=SFrom, context=context, **{'direct.scheme': SInto, 'direct.dump': 'yes'})
    c = Accum('direct://;name=client', context=context, master=s)

    s.open()
    c.open()

    assert s.state == s.State.Active

    s.post({'f0': 100}, name='Data', seq=100)
    assert [(m.msgid, m.seq) for m in c.result] == [(10, 100)]

    s.post({'f0': 100}, name='New', seq=200)
    assert [(m.msgid, m.seq) for m in c.result] == [(10, 100)]

    with pytest.raises(TLLError):
        s.post(b'xx', msgid=30, seq=300)

@pytest.mark.parametrize("tfrom,tinto", [
    ('Sub', 'int16'),
    ('Union', 'int16'),
    ('string', 'int16'),
    ('"*int16"', 'int16'),
    ('"int16[4]"', 'int16'),
    ('Sub', 'string'),
    ('Union', 'string'),
    ('"*int16"', 'string'),
    ('"int16[4]"', 'string'),
    ('Sub', '"*int16"'),
    ('Union', '"*int16"'),
    ('Sub', '"int16[4]"'),
    ('Union', '"int16[4]"'),
])
@pytest.mark.parametrize("fail_on", ["", "init", "data"])
def test_incompatible(context, tfrom, tinto, fail_on):
    SFrom = f'''yamls://
- name: Sub
- name: Data
  id: 10
  unions:
    Union: {{union: [{{name: i8, type: int8}}, {{name: d, type: double}}]}}
  enums:
    Enum: {{type: int16, enum: {{A: 1, B: 2}} }}
  fields:
    - {{ name: f0, type: {tfrom} }}
'''
    SInto = f'''yamls://
- name: Sub
- name: Data
  id: 10
  unions:
    Union: {{union: [{{name: i8, type: int8}}, {{name: d, type: double}}]}}
  enums:
    Enum: {{type: int16, enum: {{A: 1, B: 2}} }}
  fields:
    - {{ name: f0, type: {tinto} }}
'''
    extra = {'null.scheme': SInto}
    if fail_on != '':
        extra['convert-fail-on'] = fail_on
    s = Accum('convert+null://;name=convert', scheme=SFrom, context=context, **extra)

    s.open()
    if fail_on != 'data':
        assert s.state == s.State.Error
        return
    assert s.state == s.State.Active
    with pytest.raises(TLLError): s.post({}, name='Data')

def test_control(context):
    SFrom = '''yamls://
- name: Data
  id: 10
  fields:
    - { name: f0, type: int64 }
'''

    SInto = '''yamls://
- name: Data
  id: 10
  fields:
    - { name: f0, type: int8 }
'''

    s = Accum('direct://;name=server', context=context, scheme=SInto, dump='yes', **{'scheme-control': SInto})
    c = context.Channel('convert+direct://', name='client', master=s, dump='yes', **{'convert.scheme': SFrom, 'direct.dump': 'yes'})

    s.open()
    c.open()

    c.post({'f0': 10}, name='Data', type=c.Type.Control)

def test_input(context, tmp_path):
    SFrom = '''yamls://
- name: Data
  id: 10
  enums.E: { type: int8, enum: { A: 10 }}
  fields:
    - { name: f0, type: E }
'''

    SInto = '''yamls://
- name: Data
  id: 10
  enums.E: { type: int8, enum: { A: 10, Fallback: 0 }, options.fallback-value: Fallback }
  fields:
    - { name: f0, type: E }
'''

    fw = context.Channel(f'file://{tmp_path}/file.dat', scheme=SFrom, name='writer', dir='w')
    fw.open()
    fw.post({'f0': 'A'}, name='Data')
    fw.close()

    c = Accum(f'convert+file://{tmp_path}/file.dat', name='convert', autoclose='yes', context=context, **{'convert.scheme': SInto})
    c.open()
    assert (c.caps & c.Caps.InOut) == c.Caps.Input
    assert c.state == c.State.Active

    crw = Accum(f'convert+file://{tmp_path}/file.dat', name='convert', autoclose='yes', context=context, **{'convert.scheme': SInto, 'convert.dir': 'rw'})
    crw.open()
    assert crw.state == crw.State.Error

@pytest.mark.parametrize("into", ["int8", "int16"])
def test_msgid(context, into):
    SFrom = '''yamls://
- name: Data
  id: 10
  fields:
    - { name: f0, type: int8 }
'''

    SInto = f'''yamls://
- name: Data
  id: 100
  fields:
    - {{ name: f0, type: {into} }}
'''

    s = Accum('convert+direct://;name=server', dump='yes', scheme=SInto, context=context, **{'direct.scheme': SFrom, 'direct.dump': 'yes'})
    c = Accum('direct://;name=client', context=context, master=s)
    s.open()
    c.open()

    c.post({'f0': 10}, name='Data', seq=1)
    assert [(m.msgid, m.seq) for m in s.result] == [(100, 1)]
