#!/usr/bin/env python3

import tll.version as V

def test_version():
    assert V.version_tuple != (0, 0, 0)
    assert tuple(int(i) for i in V.version.split('.')) == V.version_tuple
