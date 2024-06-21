#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import datetime
import pytest

from tll.asynctll import asyncloop_run
import tll.stat as S
from tll.channel import Context
from tll.channel.mock import Mock

@pytest.fixture
def context(path_builddir):
    ctx = Context()
    ctx.load(str(path_builddir / 'logic/tll-logic-stat'))
    return ctx

class Fields(S.Base):
    FIELDS = [S.Integer('sum', S.Method.Sum)
             ,S.Float('min', S.Method.Min, unit=S.Unit.Bytes)
             ,S.Integer('max', S.Method.Max, unit=S.Unit.NS)
             ,S.Float('last', S.Method.Last)
             ]

class Groups(S.Base):
    FIELDS = [S.Group('int', unit=S.Unit.NS)
             ,S.Group('float', type=float)
             ]

@asyncloop_run
async def test(asyncloop, context):
    fields = Fields('fields')
    groups = Groups('groups')
    context.stat_list.add(fields)
    context.stat_list.add(groups)

    mock = Mock(asyncloop, f'''yamls://
mock:
  timer: direct://
channel: stat://;tll.channel.timer=timer;node=test-node
''')

    mock.open()

    timer = mock.io('timer')
    stat = mock.channel

    assert stat.scheme != None
    assert [(m.name, m.msgid) for m in stat.scheme.messages if m.msgid] == [('Page', 10)]

    fields.update(sum=1, min=2, max=3, last=4)
    groups.update(int=10, float=0.5)
    groups.update(int=1)
    groups.update(float=0.1)
    now = datetime.datetime.now()
    timer.post(b'')

    Unit = stat.scheme.enums['Unit'].klass
    Method = stat.scheme.enums['Method'].klass

    m = await stat.recv()
    assert (m.type, m.msgid) == (stat.Type.Data, 10)

    msg = stat.unpack(m)
    assert msg.node == 'test-node'
    assert msg.name == 'fields'
    assert msg.time.datetime - now < datetime.timedelta(milliseconds=100)
    assert [f.name for f in msg.fields] == ['sum', 'min', 'max', 'last']
    assert msg.fields[0].as_dict() == {'name': 'sum', 'unit': Unit.Unknown, 'value': {'ivalue': {'method': Method.Sum, 'value': 1}}}
    assert msg.fields[1].as_dict() == {'name': 'min', 'unit': Unit.Bytes, 'value': {'fvalue': {'method': Method.Min, 'value': 2}}}
    assert msg.fields[2].as_dict() == {'name': 'max', 'unit': Unit.NS, 'value': {'ivalue': {'method': Method.Max, 'value': 3}}}
    assert msg.fields[3].as_dict() == {'name': 'last', 'unit': Unit.Unknown, 'value': {'fvalue': {'method': Method.Last, 'value': 4}}}

    m = await stat.recv()
    assert (m.type, m.msgid) == (stat.Type.Data, 10)

    msg = stat.unpack(m)
    assert msg.node == 'test-node'
    assert msg.name == 'groups'
    assert msg.time.datetime - now < datetime.timedelta(milliseconds=100)
    assert [f.name for f in msg.fields] == ['int', 'float']
    assert msg.fields[0].as_dict() == {'name': 'int', 'unit': Unit.NS, 'value': {'igroup': {'count': 2, 'min': 1, 'max': 10, 'avg': 5.5}}}
    assert msg.fields[1].as_dict() == {'name': 'float', 'unit': Unit.Unknown, 'value': {'fgroup': {'count': 2, 'min': 0.1, 'max': 0.5, 'avg': 0.3}}}
