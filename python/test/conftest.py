#!/usr/bin/env python
# vim: sts=4 sw=4 et

import pytest

import os
import pathlib
import tempfile

import tll.logger

tll.logger.init()
tll.logger.configure({'levels': {'tll.python.asynctll*':'info', 'tll.channel.asynctll':'info'}})

from tll import asynctll
from tll import scheme as S

version = tuple([int(x) for x in pytest.__version__.split('.')[:2]])

if version < (3, 9):
    @pytest.fixture
    def tmp_path():
        with tempfile.TemporaryDirectory() as tmp:
            yield pathlib.Path(tmp)

@pytest.fixture
def asyncloop(context):
    loop = asynctll.Loop(context)
    yield loop
    loop.destroy()
    loop = None

@pytest.fixture
def path_builddir():
    path = pathlib.Path(tll.logger.__file__).parent.parent.parent / "build/src"
    return pathlib.Path(os.environ.get("BUILD_DIR", path))

@pytest.fixture
def path_srcdir():
    path = pathlib.Path(tll.__file__).parent.parent.parent
    return pathlib.Path(os.environ.get("SOURCE_DIR", path))

@pytest.fixture
def with_scheme_hash():
    try:
        S.Scheme('yamls://{}').dump('sha256')
    except:
        pytest.skip("Scheme SHA256 hashes not supported")
