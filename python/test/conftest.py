#!/usr/bin/env python
# vim: sts=4 sw=4 et

import pytest

import pathlib
import tempfile

import tll.logger

tll.logger.init()
tll.logger.configure({'levels': {'tll.python.asynctll*':'info', 'tll.channel.asynctll':'info'}})

from tll import asynctll

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
