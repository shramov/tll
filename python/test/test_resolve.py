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

@pytest.mark.parametrize("url", ['resolve://;resolve.service=service;resolve.channel=channel', 'resolve://service/channel'])
@pytest.mark.parametrize("mode", ['once', 'always'])
def test_resolve(context, url, mode):
    scheme = pathlib.Path(os.environ.get("SOURCE_DIR", pathlib.Path(tll.__file__).parent.parent.parent)) / "src/logic/resolve.yaml"
    rserver = Accum('direct://', name='resolve-server', dump='yes', scheme=f'yaml://{scheme}', context=context)
    server = Accum('direct://', name='server', dump='yes', context=context)
    url += f';resolve.mode={mode}'
    c = Accum(url, name='resolve', context=context)

    server.open()
    rserver.open()

    c.open()
    assert c.state == c.State.Opening
    assert rserver.result != []
    assert rserver.unpack(rserver.result[0]).as_dict() == {'service': 'service', 'channel': 'channel'}
    rserver.post({'config': [{'key': 'init.tll.proto', 'value': 'direct'}, {'key': 'init.master', 'value': 'server'}]}, name='ExportChannel')

    assert c.state == c.State.Active
    assert [i.name for i in c.children] == ['resolve/request', 'resolve/resolve']
    ccfg = c.children[-1].config

    c.post(b'xxx', msgid=100)
    assert [(m.msgid, m.data.tobytes()) for m in server.result] == [(100, b'xxx')]
    server.post(b'yyy', msgid=200)
    assert [(m.msgid, m.data.tobytes()) for m in c.result] == [(200, b'yyy')]

    c.close()
    rserver.result = []

    c.open()
    if mode == 'once':
        assert rserver.result == []
    else:
        assert rserver.result != []
        rserver.post({'config': [{'key': 'init.tll.proto', 'value': 'direct'}, {'key': 'init.master', 'value': 'server'}]}, name='ExportChannel')

    assert c.state == c.State.Active
    assert [i.name for i in c.children] == ['resolve/request', 'resolve/resolve']
    assert ccfg.parent != None # Old channel is not deleted

    c.post(b'xxx', msgid=200)
    assert [(m.msgid, m.data.tobytes()) for m in server.result] == [(100, b'xxx'), (200, b'xxx')]

    if mode == 'once':
        return

    c.close()
    rserver.result = []

    c.open()
    assert rserver.result != []
    rserver.post({'config': [{'key': 'init.tll.proto', 'value': 'direct'}, {'key': 'init.master', 'value': 'server'}, {'key': 'init.a', 'value': 'b'}]}, name='ExportChannel')

    assert c.state == c.State.Active
    assert [i.name for i in c.children] == ['resolve/request', 'resolve/resolve']
    assert ccfg.parent == None # Old channel is deleted and it's config is detached
    c.children[-1].config['init.a'] == 'b'

    c.post(b'xxx', msgid=300)
    assert [(m.msgid, m.data.tobytes()) for m in server.result] == [(100, b'xxx'), (200, b'xxx'), (300, b'xxx')]

def test_scheme_hash(context, with_scheme_hash):
    scheme = pathlib.Path(os.environ.get("SOURCE_DIR", pathlib.Path(tll.__file__).parent.parent.parent)) / "src/logic/resolve.yaml"
    rserver = Accum('direct://', name='resolve-server', dump='yes', scheme=f'yaml://{scheme}', context=context)
    server = Accum('direct://', name='server', dump='yes', context=context)
    c = Accum('resolve://;resolve.service=service;resolve.channel=channel;resolve.mode=once;name=resolve', context=context)

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

def test_scheme_override(context):
    scheme_outer = '''yamls://
- name: Data
  id: 16
  fields:
    - {name: header, type: int16}
    - {name: f0, type: int32}
'''
    scheme_inner = '''yamls://
- name: Data
  id: 16
  fields:
    - {name: f0, type: int16}
'''
    scheme = pathlib.Path(os.environ.get("SOURCE_DIR", pathlib.Path(tll.__file__).parent.parent.parent)) / "src/logic/resolve.yaml"
    rserver = Accum('direct://', name='resolve-server', dump='yes', scheme=f'yaml://{scheme}', context=context)
    c = Accum('resolve://;resolve.service=service;resolve.channel=channel;name=resolve', scheme=scheme_outer, context=context, dump='yes')

    rserver.open()

    body = {'init.tll.proto': 'direct', 'init.scheme': scheme_inner, 'init.dump': 'yes'}

    c.open()
    assert c.state == c.State.Opening
    assert rserver.result != []
    assert rserver.unpack(rserver.result[0]).as_dict() == {'service': 'service', 'channel': 'channel'}
    rserver.post({'config': [{'key': k, 'value': v} for k,v in body.items()]}, name='ExportChannel')

    assert c.state == c.State.Active
    assert c.children[1].name == 'resolve/resolve'
    assert c.children[1].scheme != None
    assert [(m.name, m.msgid, m.size) for m in c.children[1].scheme.messages] == [('Data', 16, 2)]
    assert c.scheme != None
    assert [(m.name, m.msgid, m.size) for m in c.scheme.messages] == [('Data', 16, 6)]

    client = Accum('direct://', master=c.children[1], context=context, name='client')
    client.open()

    client.post({'f0': 100}, name='Data', seq=10)
    assert [(m.msgid, m.seq) for m in c.result] == [(16, 10)]
    assert c.unpack(c.result[-1]).as_dict() == {'header': 0, 'f0': 100}

    c.post({'header': 100, 'f0': 200}, name='Data', seq=20)
    assert [(m.msgid, m.seq) for m in client.result] == [(16, 20)]
    assert client.unpack(client.result[-1]).as_dict() == {'f0': 200}

def test_early_close(context, path_srcdir):
    scheme = path_srcdir / "src/logic/resolve.yaml"
    rserver = Accum('direct://', name='resolve-server', dump='yes', scheme=f'yaml://{scheme}', context=context)
    server = Accum('direct://', name='server', dump='yes', context=context)
    c = Accum('resolve://service/channel', name='resolve', context=context)

    server.open()
    rserver.open()

    c.open()
    assert c.state == c.State.Opening
    assert rserver.result != []
    assert rserver.unpack(rserver.result[0]).as_dict() == {'service': 'service', 'channel': 'channel'}

    c.close()

    assert c.state == c.State.Closed
