#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import pytest

import os
import pathlib

import tll
from tll.asynctll import asyncloop_run
from tll.channel import Context
from tll.channel.mock import Mock

@pytest.fixture
def context():
    path = pathlib.Path(tll.__file__).parent.parent.parent / "build/src/"
    path = pathlib.Path(os.environ.get("BUILD_DIR", path))
    ctx = Context()
    ctx.load(str(path / 'logic/tll-logic-stat'))
    return ctx

@asyncloop_run
async def test(asyncloop, context):
    scheme = pathlib.Path(os.environ.get("SOURCE_DIR", pathlib.Path(tll.__file__).parent.parent.parent)) / "src/logic/quantile.yaml"

    config = f'''yamls://
mock:
  timer: direct://
  input: direct://;scheme=yaml://{scheme}
channel:
  url: 'quantile://;tll.channel.timer=timer;tll.channel.input=input'
  skip: 1
  quantile: '95,50,75'
'''

    mock = Mock(asyncloop, config)
    mock.open()

    ci = mock.io('input')

    ci.post({'value': 4}, name='Data')
    mock.io('timer').post(b'')

    for i in range(4, 0, -1):
        ci.post({'value': i}, name='Data')

    mock.io('timer').post(b'')
