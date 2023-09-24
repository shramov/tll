#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import pytest

import tll
from tll.asynctll import asyncloop_run
from tll.channel import Context
from tll.channel.mock import Mock

@pytest.fixture
def context(path_builddir):
    ctx = Context()
    ctx.load(path_builddir / 'logic/tll-logic-stat')
    return ctx

@asyncloop_run
async def test(asyncloop, context, path_srcdir):
    scheme = path_srcdir / "src/logic/quantile.yaml"

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
