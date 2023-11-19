#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import os
import pathlib
import pytest
import yaml

import tll
import tll.channel as C
import tll.scheme as S
from tll.config import Config
from tll.channel.base import Base
from tll.error import TLLError
from tll.test_util import Accum

@pytest.fixture
def context():
    return C.Context(Config.from_dict({'resolve.request': 'direct://;master=resolve-server;dump=frame'}))

def test_resolve(context):
    scheme = pathlib.Path(os.environ.get("SOURCE_DIR", pathlib.Path(tll.__file__).parent.parent.parent)) / "src/logic/resolve.yaml"
    rserver = Accum('direct://', name='resolve-server', dump='yes', scheme=f'yaml://{scheme}', context=context)
    server = Accum('direct://', name='server', dump='yes', context=context)
    c = Accum('resolve://;resolve.service=service;resolve.channel=channel;name=resolve', context=context)

    server.open()
    rserver.open()

    c.open()
    assert c.state == c.State.Opening
    assert rserver.result != []
    assert rserver.unpack(rserver.result[0]).as_dict() == {'service': 'service', 'channel': 'channel'}
    rserver.post({'config': [{'key': 'init.tll.proto', 'value': 'direct'}, {'key': 'init.master', 'value': 'server'}]}, name='ExportChannel')

    assert c.state == c.State.Active
    c.post(b'xxx', msgid=100)
    assert [(m.msgid, m.data.tobytes()) for m in server.result] == [(100, b'xxx')]
    server.post(b'yyy', msgid=200)
    assert [(m.msgid, m.data.tobytes()) for m in c.result] == [(200, b'yyy')]

    c.close()
    rserver.result = []

    c.open()
    assert rserver.result == []
    c.post(b'xxx', msgid=200)
    assert [(m.msgid, m.data.tobytes()) for m in server.result] == [(100, b'xxx'), (200, b'xxx')]


def test_scheme_hash(context, with_scheme_hash):
    scheme = pathlib.Path(os.environ.get("SOURCE_DIR", pathlib.Path(tll.__file__).parent.parent.parent)) / "src/logic/resolve.yaml"
    rserver = Accum('direct://', name='resolve-server', dump='yes', scheme=f'yaml://{scheme}', context=context)
    server = Accum('direct://', name='server', dump='yes', context=context)
    c = Accum('resolve://;resolve.service=service;resolve.channel=channel;name=resolve', context=context)

    server.open()
    rserver.open()

    s = S.Scheme('yamls://[{name: Message}]')
    sbody = s.dump('yamls+gz')
    shash = s.dump('sha256')
    with pytest.raises(TLLError): context.Channel('null://', scheme=shash).open()

    body = {'init.tll.proto': 'direct', 'init.master': 'server', 'init.scheme': shash, f'scheme.{shash}': sbody}

    c.open()
    assert c.state == c.State.Opening
    assert rserver.result != []
    assert rserver.unpack(rserver.result[0]).as_dict() == {'service': 'service', 'channel': 'channel'}
    rserver.post({'config': [{'key': k, 'value': v} for k,v in body.items()]}, name='ExportChannel')

    assert c.state == c.State.Active
    c.post(b'xxx', msgid=100)
    assert [(m.msgid, m.data.tobytes()) for m in server.result] == [(100, b'xxx')]
    server.post(b'yyy', msgid=200)
    assert [(m.msgid, m.data.tobytes()) for m in c.result] == [(200, b'yyy')]

    c.close()
    rserver.result = []

    c.open()
    assert rserver.result == []
    c.post(b'xxx', msgid=200)
    assert [(m.msgid, m.data.tobytes()) for m in server.result] == [(100, b'xxx'), (200, b'xxx')]
    assert c.scheme != None
    assert [m.name for m in c.scheme.messages] == ['Message']
