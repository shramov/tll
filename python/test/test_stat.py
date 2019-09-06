#!/usr/bin/env python3
# vim: sts=4 sw=4 et

from nose.tools import *

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
    assert_equals(len(list(l)), 0)
    l.add(s)
    assert_equals(len(list(l)), 1)

    i = iter(l)
    assert_false(i.empty())
    l.remove(s)
    assert_true(i.empty())
    l.add(s)
    assert_false(i.empty())

    s.update(sum=10, last=40, min=20.5, max=30)
    s.update(sum=11, last=41, min=19.5, max=31)

    fields = i.swap()
    assert_not_equals(fields, None)
    assert_equals([(f.name, f.value) for f in fields], [(b'sum', 21), (b'min', 19.5), (b'max', 31), (b'last', 41.)])

    s.update(min=20.5, max=30, last=40)
    s.update(sum=11, min=21.5, last=41)
    s.update(max=29, sum=9)
    fields = i.swap()
    assert_not_equals(fields, None)
    assert_equals([(f.name, f.value) for f in fields], [(b'sum', 20), (b'min', 20.5), (b'max', 30), (b'last', 41.)])
