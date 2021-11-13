#!/usr/bin/env python3
# vim: sts=4 sw=4 et

#from nose.tools import *

import tll.stat as S

class Stat(S.Base):
    FIELDS = [S.Integer('sum', S.Method.Sum)
             ,S.Float('min', S.Method.Min, unit=S.Unit.Unknown)
             ,S.Integer('max', S.Method.Max)
             ,S.Float('last', S.Method.Last)
             ]

def test_swap():
    s = Stat('test')
    l = S.List(new=True)
    assert len(list(l)) == 0
    l.add(s)
    assert len(list(l)) == 1

    i = iter(l)
    assert i.empty() == False
    l.remove(s)
    assert i.empty() == True
    l.add(s)
    assert i.empty() == False

    s.update(sum=10, last=40, min=20.5, max=30)
    s.update(sum=11, last=41, min=19.5, max=31)

    fields = i.swap()
    assert fields != None
    assert [(f.name, f.value) for f in fields] == [('sum', 21), ('min', 19.5), ('max', 31), ('last', 41.)]

    s.update(min=20.5, max=30, last=40)
    s.update(sum=11, min=21.5, last=41)
    s.update(max=29, sum=9)
    fields = i.swap()
    assert fields != None
    assert [(f.name, f.value) for f in fields] == [('sum', 20), ('min', 20.5), ('max', 30), ('last', 41.)]

def test_list_iter():
    s0 = Stat('s0')
    s1 = Stat('s1')
    l = S.List(new=True)

    assert len(list(l)) == 0
    l.add(s0)
    l.add(s1)
    assert len(list(l)) == 2

    assert [x.name for x in l] == ['s0' , 's1']

    s0.update(sum=10)
    s1.update(sum=11)

    assert [x.swap()[0].value for x in l] == [10, 11]

class Group(S.Base):
    FIELDS = [S.Group('int', unit=S.Unit.NS)
             ,S.Group('float', type=float)
             ]

def test_group():
    s = Group('test')
    l = S.List(new=True)
    l.add(s)
    assert len(list(l)) == 1

    s.update(int=10)
    s.update(int=20, float=0.5)
    s.update(int=30, float=0.1)

    fields = iter(l).swap()
    assert fields != None
    assert [(f.name, f.count, f.sum, f.min, f.max) for f in fields] == [('int', 3, 60, 10, 30), ('float', 2, 0.6, 0.1, 0.5)]

class Alias(S.Base):
    FIELDS = [S.Integer('rx', S.Method.Sum)
             ,S.Integer('rx', S.Method.Sum, unit=S.Unit.Bytes, alias='rxb')
             ]

def test_alias():
    s = Alias('test')
    l = S.List(new=True)
    assert len(list(l)) == 0
    l.add(s)
    assert len(list(l)) == 1

    s.update(rx=1, rxb=100)
    s.update(rx=1)

    fields = iter(l).swap()
    assert fields != None
    assert [(f.name, f.value) for f in fields] == [('rx', 2), ('rx', 100)]
