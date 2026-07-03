#!/usr/bin/env python3
# vim: sts=4 sw=4 et

from tll import asynctll, chrono
import tll.channel as C
from tll.channel.asyncbase import AsyncBase
from tll.processor import Loop
from tll.config import Config, Url
from tll.error import TLLError
from tll.test_util import Accum

import pytest

class EchoRun(AsyncBase):
    PROTO = "echo-run"
    ASYNC_POST_POLICY = AsyncBase.AsyncPostPolicy.Queue

    async def run(self, cfg):
        while True:
            m = await self._post_wait()
            if m.type != m.Type.Data:
                continue
            await self.sleep(0.01)
            self._callback(m)

class EchoPost(AsyncBase):
    PROTO = "echo-post"

    async def run(self, cfg):
        while True:
            await self.sleep(1)

    async def _apost(self, m, flags):
        if m.type != m.Type.Data:
            return
        await self.sleep(0.01)
        self._callback(m)

@pytest.fixture
def context():
    ctx = C.Context()
    ctx.register(EchoRun)
    ctx.register(EchoPost)
    return ctx

@pytest.mark.parametrize("proto", ['echo-run', 'echo-post'])
def test_echo(context, proto):
    loop = Loop()
    c = Accum(f'{proto}://', name='echo', context=context)
    loop.add(c)
    c.open()

    assert c.state == c.State.Active

    c.post(b'xxx')
    c.post(b'yyy')
    assert [m.data.tobytes() for m in c.result] == []

    loop.step(0.1)
    loop.step(0.001)
    if proto == 'echo-run':
        assert [m.data.tobytes() for m in c.result] == [b'xxx']
        loop.step(0.1)
    assert [m.data.tobytes() for m in c.result] == [b'xxx', b'yyy']
