#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import pytest

import os
import pathlib

import tll
from tll.channel import Context
from tll.test_util import Accum

@pytest.fixture
def context():
    path = pathlib.Path(tll.__file__).parent.parent.parent / "build/src/"
    path = pathlib.Path(os.environ.get("BUILD_DIR", path))
    ctx = Context()
    ctx.load(str(path / 'logic/tll-logic-stat'))
    return ctx

def test(context):
    scheme = pathlib.Path(tll.__file__).parent.parent.parent / "src/logic/quantile.yaml"

    timer = context.Channel('direct://;name=timer', dump='frame')
    tclient = context.Channel('direct://;name=timer-client', master=timer)
    ci = context.Channel('direct://;name=input', scheme='yaml://' + str(scheme), dump='yes')
    ciclient = context.Channel('direct://;name=input-client', master=ci, scheme='yaml://' + str(scheme))

    logic = context.Channel('quantile://;tll.channel.timer=timer;tll.channel.input=input', skip='1', name='logic', quantile='95,50,75')

    timer.open()
    tclient.open()

    ci.open()
    ciclient.open()

    logic.open()

    ciclient.post({'value': 4}, name='Data')

    tclient.post(b'')

    ciclient.post({'value': 4}, name='Data')
    ciclient.post({'value': 3}, name='Data')
    ciclient.post({'value': 2}, name='Data')
    ciclient.post({'value': 1}, name='Data')

    tclient.post(b'')
