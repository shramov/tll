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

    ci.post({'service': 'service', 'host': 'host', 'tags': ['tag']}, name='ExportService', addr=20)
    assert cu.unpack(await cu.recv()).as_dict() == {'service': 'service', 'host': 'host', 'tags': ['tag']}

    ci.post({'service': 'service', 'channel': 'a', 'config': [{'key': 'tll.proto', 'value': 'a'}]}, name='ExportChannel', addr=20)
    assert cu.unpack(await cu.recv()).as_dict() == {'service': 'service', 'channel': 'a', 'host': '', 'tags': [], 'config': [{'key': 'tll.proto', 'value': 'a'}]}
    with pytest.raises(TimeoutError): await ci.recv(0.0001)

    resolve.inner('uplink').close()
    resolve.inner('uplink').open()

    assert cu.unpack(await cu.recv()).as_dict() == {'service': 'service', 'host': 'host', 'tags': ['tag']}
    assert cu.unpack(await cu.recv()).as_dict() == {'service': 'service', 'channel': 'a', 'host': '', 'tags': [], 'config': [{'key': 'tll.proto', 'value': 'a'}]}
    assert cu.unpack(await cu.recv()).as_dict() == {'service': 'service', 'channel': 'a'}

    ci.post({}, name='Disconnect', type=ci.Type.Control, addr=10)
    assert cu.unpack(await cu.recv()).as_dict() == {'service': 'service', 'channel': 'a'}

    ci.post({}, name='Disconnect', type=ci.Type.Control, addr=20)
    assert cu.unpack(await cu.recv()).as_dict() == {'service': 'service'}

    ci.post({'service': 'service', 'host': 'host'}, name='ExportService', addr=20)
    assert cu.unpack(await cu.recv()).as_dict() == {'service': 'service', 'host': 'host', 'tags': []}

    ci.post({'service': 'service', 'channel': 'a', 'config': [{'key': 'tll.proto', 'value': 'a'}]}, name='ExportChannel', addr=20)
    assert cu.unpack(await cu.recv()).as_dict() == {'service': 'service', 'channel': 'a', 'host': '', 'tags': [], 'config': [{'key': 'tll.proto', 'value': 'a'}]}

    ci.post({'service': 'tag', 'channel': 'b'}, name='Request', addr=10)
    assert cu.unpack(await cu.recv()).as_dict() == {'service': 'tag', 'channel': 'b'}

    cu.post({'service': 'other', 'channel': 'b', 'tags': ['tag']}, name='ExportChannel')
    assert ci.unpack(await ci.recv()).as_dict() == {'service': 'other', 'channel': 'b', 'host': '', 'tags': ['tag'], 'config': []}

    cu.post({'service': 'other', 'channel': 'b', 'tags': ['tag']}, name='DropChannel')
    assert ci.unpack(await ci.recv()).as_dict() == {'service': 'other', 'channel': 'b', 'tags': ['tag']}

    resolve.inner('input').close()
    assert cu.unpack(await cu.recv()).as_dict() == {'service': 'service'}

@asyncloop_run
async def test_standalone(asyncloop, resolve):
    resolve.open(skip=['uplink'])

    ci = resolve.io('input')

    ci.post({'service': 'service', 'channel': 'a'}, name='Request', addr=10)
    with pytest.raises(TimeoutError): await ci.recv(0.0001)

    ci.post({'service': 'service', 'host': 'host', 'tags': ['tag']}, name='ExportService', addr=20)

    ci.post({'service': 'service', 'channel': 'a', 'config': [{'key': 'tll.proto', 'value': 'a'}]}, name='ExportChannel', addr=20)
    assert ci.unpack(await ci.recv()).as_dict() == {'service': 'service', 'channel': 'a', 'host': 'host', 'tags': ['tag'], 'config': [{'key': 'tll.proto', 'value': 'a'}]}

    ci.post({'service': 'tag', 'channel': 'a'}, name='Request', addr=20)
    assert ci.unpack(await ci.recv()).as_dict() == {'service': 'service', 'channel': 'a', 'host': 'host', 'tags': ['tag'], 'config': [{'key': 'tll.proto', 'value': 'a'}]}

@pytest.mark.parametrize('service,channel,result',
                         [('service', 'a', ['a']),
                          ('*', '*', ['a', 'b']),
                          ('service', '*', ['a', 'b']),
                          ('tag', '*', ['a', 'b']),
                          ('*', 'a', ['a']),
                         ])
@asyncloop_run
async def test_wildcard(asyncloop, resolve, service, channel, result):
    resolve.open(skip=['uplink'])

    ci = resolve.io('input')

    ci.post({'service': service, 'channel': channel}, name='Request', addr=10)
    with pytest.raises(TimeoutError): await ci.recv(0.0001)

    ci.post({'service': 'service', 'host': 'host', 'tags': ['tag']}, name='ExportService', addr=20)
    ci.post({'service': 'service', 'channel': 'a', 'config': [{'key': 'tll.proto', 'value': 'a'}]}, name='ExportChannel', addr=20)
    ci.post({'service': 'service', 'channel': 'b', 'config': [{'key': 'tll.proto', 'value': 'b'}]}, name='ExportChannel', addr=20)

    for r in result:
        m = ci.unpack(await ci.recv())
        assert (m.service, m.channel) == ('service', r)

    ci.post({'service': service, 'channel': channel}, name='Request', addr=10)

    for r in result:
        m = ci.unpack(await ci.recv())
        assert (m.service, m.channel) == ('service', r)

@asyncloop_run
async def test_duplicate_export(asyncloop, resolve):
    resolve.open(skip=['uplink'])

    ci = resolve.io('input')

    ci.post({'service': 'service', 'host': 'host', 'tags': ['tag']}, name='ExportService', addr=20)
    ci.post({'service': 'service', 'channel': 'a', 'config': [{'key': 'tll.proto', 'value': 'a'}]}, name='ExportChannel', addr=20)

    ci.post({'service': 'service', 'host': 'host', 'tags': ['tag']}, name='ExportService', addr=30)
    ci.post({'service': 'service', 'channel': 'a', 'config': [{'key': 'tll.proto', 'value': 'b'}]}, name='ExportChannel', addr=30)

    ci.post({'service': 'service', 'channel': 'a'}, name='Request', addr=10)

    m = ci.unpack(await ci.recv())
    assert (m.service, m.channel, {x.key: x.value for x in m.config}) == ('service', 'a', {'tll.proto': 'a'})
