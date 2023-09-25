#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import pytest

import tll
from tll.asynctll import asyncloop_run
from tll.channel import Context
from tll.channel.mock import Mock

@pytest.fixture
def context():
    ctx = Context()
    return ctx

@pytest.fixture
def resolve(asyncloop, path_srcdir):
    scheme = path_srcdir / "src/logic/resolve.yaml"

    mock = Mock(asyncloop, f'''yamls://
mock:
  input: direct://;scheme=yaml://{scheme};emulate-control=tcp-server
  uplink: direct://;scheme=yaml://{scheme}
channel:
  url: python://;python=tll.channel.resolve:Resolve;tll.channel.input=input;tll.channel.uplink=uplink;name=logic
''')

    yield mock

    mock.destroy()

@asyncloop_run
async def test_uplink(asyncloop, resolve):
    resolve.open()

    ci, cu = resolve.io('input', 'uplink')

    ci.post({'service': 'service', 'channel': 'a'}, name='Request', addr=10)

    assert cu.unpack(await cu.recv()).as_dict() == {'service': 'service', 'channel': 'a'}
    with pytest.raises(TimeoutError): await ci.recv(0.0001)

    ci.post({'service': 'service', 'host': 'host'}, name='ExportService', addr=20)
    assert cu.unpack(await cu.recv()).as_dict() == {'service': 'service', 'host': 'host', 'tags': []}

    ci.post({'service': 'service', 'channel': 'a', 'config': [{'key': 'tll.proto', 'value': 'a'}]}, name='ExportChannel', addr=20)
    assert cu.unpack(await cu.recv()).as_dict() == {'service': 'service', 'channel': 'a', 'host': '', 'config': [{'key': 'tll.proto', 'value': 'a'}]}
    with pytest.raises(TimeoutError): await ci.recv(0.0001)

    resolve.inner('uplink').close()
    resolve.inner('uplink').open()

    assert cu.unpack(await cu.recv()).as_dict() == {'service': 'service', 'host': 'host', 'tags': []}
    assert cu.unpack(await cu.recv()).as_dict() == {'service': 'service', 'channel': 'a', 'host': '', 'config': [{'key': 'tll.proto', 'value': 'a'}]}
    assert cu.unpack(await cu.recv()).as_dict() == {'service': 'service', 'channel': 'a'}

    ci.post({}, name='Disconnect', type=ci.Type.Control, addr=10)
    assert cu.unpack(await cu.recv()).as_dict() == {'service': 'service', 'channel': 'a'}

    ci.post({}, name='Disconnect', type=ci.Type.Control, addr=20)
    assert cu.unpack(await cu.recv()).as_dict() == {'service': 'service'}

    ci.post({'service': 'service', 'host': 'host'}, name='ExportService', addr=20)
    assert cu.unpack(await cu.recv()).as_dict() == {'service': 'service', 'host': 'host', 'tags': []}

    ci.post({'service': 'service', 'channel': 'a', 'config': [{'key': 'tll.proto', 'value': 'a'}]}, name='ExportChannel', addr=20)
    assert cu.unpack(await cu.recv()).as_dict() == {'service': 'service', 'channel': 'a', 'host': '', 'config': [{'key': 'tll.proto', 'value': 'a'}]}

    resolve.inner('input').close()
    assert cu.unpack(await cu.recv()).as_dict() == {'service': 'service'}

@asyncloop_run
async def test_standalone(asyncloop, resolve):
    resolve.open(skip=['uplink'])

    ci = resolve.io('input')

    ci.post({'service': 'service', 'channel': 'a'}, name='Request', addr=10)
    with pytest.raises(TimeoutError): await ci.recv(0.0001)

    ci.post({'service': 'service', 'host': 'host'}, name='ExportService', addr=20)

    ci.post({'service': 'service', 'channel': 'a', 'config': [{'key': 'tll.proto', 'value': 'a'}]}, name='ExportChannel', addr=20)
    assert ci.unpack(await ci.recv()).as_dict() == {'service': 'service', 'channel': 'a', 'host': 'host', 'config': [{'key': 'tll.proto', 'value': 'a'}]}
