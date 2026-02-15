#!/usr/bin/env python3

import tll.version as V

def test_version():
    assert tuple(int(i) for i in V.version.split('.')) == V.version_tuple
