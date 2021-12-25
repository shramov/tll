#!/usr/bin/env python
# vim: sts=4 sw=4 et

import pytest

import pathlib
import tempfile

import tll.logger

tll.logger.init()

version = tuple([int(x) for x in pytest.__version__.split('.')[:2]])

if version < (3, 9):
    @pytest.fixture
    def tmp_path():
        with tempfile.TemporaryDirectory() as tmp:
            yield pathlib.Path(tmp)
