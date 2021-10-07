#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import tll.decimal128 as D

from decimal import Decimal
import yaml
import pathlib

import pytest

data = yaml.safe_load(open(pathlib.Path(__file__).parent / "decimal128.yaml"))

@pytest.mark.parametrize("kw", data.values(), ids=lambda x: x['string'])
def test(kw):
    s, binary = kw['string'], kw['bin']
    d = Decimal(s)
    blob = D.pack(d)
    assert blob == bytearray(binary)
    r = D.unpack(blob)
    if s == 'NaN':
        print(r)
        assert r.is_qnan()
    elif s == 'sNaN':
        print(r)
        assert r.is_snan()
    else:
        assert r == d
