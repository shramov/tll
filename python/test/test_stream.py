#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import pytest
import time
import yaml

from tll.asynctll import asyncloop_run
import tll.channel as C
from tll.channel.base import Base
from tll.config import Url
from tll.error import TLLError
from tll.test_util import Accum, ports

@pytest.fixture
def asyncloop_config():
    return {'pending-steps': '0'}

@pytest.fixture
def context():
    return C.Context()

@asyncloop_run
@pytest.mark.parametrize("protocol", ["old", "new"])
async def test(asyncloop, tmp_path, protocol):
    common = f'stream+pub+tcp:///{tmp_path}/stream.sock;request=tcp:///{tmp_path}/request.sock;dump=frame;pub.dump=frame;request.dump=frame;storage.dump=frame'
    s = asyncloop.Channel(f'{common};storage=file:///{tmp_path}/storage.dat;name=server;mode=server')
    c = asyncloop.Channel(f'{common};name=client;mode=client;peer=test;stream.protocol={protocol}')

    assert [x.name for x in s.children] == ['server/stream', 'server/request', 'server/storage']
    assert [x.name for x in c.children] == ['client/stream', 'client/request']

    s.open()
    assert s.state == s.State.Active # No need to wait

    s.post(b'aaa', msgid=10, seq=10)
    s.post(b'bbb', msgid=10, seq=20)
    s.post(b'ccc', msgid=10, seq=30)

    with pytest.raises(TLLError): s.post(b'ddd', msgid=10, seq=25)
    with pytest.raises(TLLError): s.post(b'ddd', msgid=10, seq=30)

    c.open(seq='20', mode='seq')

    m = await s.recv()
    assert m.type == m.Type.Control
    assert s.unpack(m).SCHEME.name == 'Connect'

    assert await c.recv_state() == c.State.Active
    assert c.children[0].state == c.State.Active

    m = await c.recv()
    assert (m.seq, m.msgid, m.data.tobytes()) == (20, 10, b'bbb')

    m = await c.recv()
    assert (m.seq, m.msgid, m.data.tobytes()) == (30, 10, b'ccc')

    m = await c.recv()
    assert m.type == m.Type.Control
    assert (m.seq, c.unpack(m).SCHEME.name) == (30, 'Online')

    s.post(b'ddd', msgid=10, seq=40)

    m = await c.recv()
    assert (m.seq, m.msgid, m.data.tobytes()) == (40, 10, b'ddd')

    c.close()

    m = await s.recv()
    assert m.type == m.Type.Control
    assert s.unpack(m).SCHEME.name == 'Disconnect'

    s.close()
    assert s.state == c.State.Closed

@asyncloop_run
async def test_seq_data(asyncloop, tmp_path):
    common = f'stream+pub+tcp:///{tmp_path}/stream.sock;request=tcp:///{tmp_path}/request.sock;dump=frame;pub.dump=frame;request.dump=frame;storage.dump=frame'
    s = asyncloop.Channel(f'{common};storage=file:///{tmp_path}/storage.dat;name=server;mode=server')
    c = asyncloop.Channel(f'{common};name=client;mode=client;peer=test')

    s.open()
    assert s.state == s.State.Active # No need to wait

    s.post(b'aaa', msgid=10, seq=10)
    s.post(b'bbb', msgid=10, seq=20)
    s.post(b'ccc', msgid=10, seq=30)

    c.open(seq='20', mode='seq-data')

    m = await s.recv()
    assert m.type == m.Type.Control
    assert s.unpack(m).SCHEME.name == 'Connect'

    m = await c.recv()
    assert (m.seq, m.msgid, m.data.tobytes()) == (30, 10, b'ccc')

    m = await c.recv()
    assert m.type == m.Type.Control
    assert (m.seq, c.unpack(m).SCHEME.name) == (30, 'Online')

@asyncloop_run
async def test_overlapped(asyncloop, tmp_path):
    common = f'stream+pub+tcp:///{tmp_path}/stream.sock;request=tcp:///{tmp_path}/request.sock;dump=frame;pub.dump=frame;request.dump=frame;storage.dump=frame'
    s = asyncloop.Channel(f'{common};storage=file:///{tmp_path}/storage.dat;name=server;mode=server')
    c = asyncloop.Channel(f'{common};name=client;mode=client;peer=test')

    s.open()
    assert s.state == s.State.Active # No need to wait

    s.post(b'aaa', msgid=10, seq=10)
    s.post(b'bbb', msgid=10, seq=20)
    s.post(b'ccc', msgid=10, seq=30)

    c.open(seq='20', mode='seq')

    m = await s.recv()
    assert m.type == m.Type.Control
    assert s.unpack(m).SCHEME.name == 'Connect'

    s.post(b'ddd', msgid=10, seq=40)
    s.post(b'eee', msgid=10, seq=50)
    s.post(b'fff', msgid=10, seq=60)

    m = await c.recv()
    assert (m.seq, m.msgid, m.data.tobytes()) == (20, 10, b'bbb')

    m = await c.recv()
    assert (m.seq, m.msgid, m.data.tobytes()) == (30, 10, b'ccc')

    m = await c.recv()
    assert (m.seq, m.msgid, m.data.tobytes()) == (40, 10, b'ddd')

    m = await c.recv()
    assert (m.seq, m.msgid, m.data.tobytes()) == (50, 10, b'eee')

    m = await c.recv()
    assert (m.seq, m.msgid, m.data.tobytes()) == (60, 10, b'fff')

    m = await c.recv()
    assert m.type == m.Type.Control
    assert (m.seq, c.unpack(m).SCHEME.name) == (60, 'Online')

    s.post(b'ggg', msgid=10, seq=70)

    m = await c.recv()
    assert (m.seq, m.msgid, m.data.tobytes()) == (70, 10, b'ggg')

@asyncloop_run
async def test_slow_online(asyncloop, tmp_path):
    common = f'stream+pub+tcp:///{tmp_path}/stream.sock;request=tcp:///{tmp_path}/request.sock;dump=frame;pub.dump=frame;request.dump=frame;storage.dump=frame'
    s = asyncloop.Channel(f'{common};storage=file:///{tmp_path}/storage.dat;name=server;mode=server')
    c = asyncloop.Channel(f'{common};name=client;mode=client;peer=test')

    assert s.children[0].name == 'server/stream'
    assert s.children[1].name == 'server/request'
    assert c.children[0].name == 'client/stream'
    assert c.children[1].name == 'client/request'

    s.open()
    assert s.state == s.State.Active # No need to wait

    for i in range(10, 31, 10):
        s.post(b'xxx', msgid=10, seq=i)

    c.open(seq='20', mode='seq')
    for _ in range(20):
        if c.children[0].state == c.State.Active:
            break
        c.children[0].process()
        if len(s.children[0].children) > 1:
            s.children[0].children[1].process()
        else:
            s.children[0].children[0].process()
        time.sleep(0.001)
    c.children[0].suspend()

    for i in range(40, 61, 10):
        s.post(b'xxx', msgid=10, seq=i)

    m = await s.recv()
    assert m.type == m.Type.Control
    assert s.unpack(m).SCHEME.name == 'Connect'

    assert (await c.recv_state()) == c.State.Active

    for i in range(20, 61, 10):
        m = await c.recv()
        assert (m.seq, m.msgid, m.data.tobytes()) == (i, 10, b'xxx')

    m = await c.recv()
    assert m.type == m.Type.Control
    assert (m.seq, c.unpack(m).SCHEME.name) == (60, 'Online')

    s.post(b'online', msgid=10, seq=70)
    c.children[0].resume()

    m = await c.recv()
    assert (m.seq, m.msgid, m.data.tobytes()) == (70, 10, b'online')

@asyncloop_run
async def test_recent(asyncloop, tmp_path):
    common = f'stream+pub+tcp:///{tmp_path}/stream.sock;request=tcp:///{tmp_path}/request.sock;dump=frame;pub.dump=frame;request.dump=frame;storage.dump=frame'
    s = asyncloop.Channel(f'{common};storage=file:///{tmp_path}/storage.dat;name=server;mode=server')
    c = asyncloop.Channel(f'{common};name=client;mode=client;peer=test')

    s.open()
    assert s.state == s.State.Active # No need to wait

    s.post(b'aaa', msgid=10, seq=10)
    s.post(b'bbb', msgid=10, seq=20)
    s.post(b'ccc', msgid=10, seq=30)

    c.open(seq='31', mode='seq')

    m = await s.recv()
    assert m.type == m.Type.Control
    assert s.unpack(m).SCHEME.name == 'Connect'

    m = await c.recv()
    assert m.type == m.Type.Control
    assert (m.seq, c.unpack(m).SCHEME.name) == (30, 'Online')

@asyncloop_run
async def test_reopen(asyncloop, tmp_path):
    common = f'stream+pub+tcp:///{tmp_path}/stream.sock;request=tcp:///{tmp_path}/request.sock;dump=frame;pub.dump=frame;request.dump=frame;storage.dump=frame'
    s = asyncloop.Channel(f'{common};storage=file:///{tmp_path}/storage.dat;name=server;mode=server')
    c = asyncloop.Channel(f'{common};name=client;mode=client;peer=test')

    s.open()
    assert s.state == s.State.Active # No need to wait

    s.post(b'aaa', msgid=10, seq=10)
    s.post(b'bbb', msgid=10, seq=20)
    s.post(b'ccc', msgid=10, seq=30)

    s.close()
    s.open()

    c.open(seq='31', mode='seq')

    m = await s.recv()
    assert m.type == m.Type.Control
    assert s.unpack(m).SCHEME.name == 'Connect'

    m = await c.recv()
    assert m.type == m.Type.Control
    assert (m.seq, c.unpack(m).SCHEME.name) == (30, 'Online')

@pytest.mark.parametrize("protocol", ["old", "new"])
@pytest.mark.parametrize("req,result", [
        ('block=0', [20, 30, 30]),
        ('block=0;block-type=rare', [20, 30, 30]),
        ('block=1', [10, 20, 30, 30]),
        ('block=1;block-type=default', [10, 20, 30, 30]),
        ('block=0;block-type=last', [30, 30]),
        ('block=10;block-type=rare', []),
        ('block=0;block-type=unknown', []),
        ])
@asyncloop_run
async def test_block(asyncloop, tmp_path, req, result, protocol):
    common = f'stream+pub+tcp://{tmp_path}/stream.sock;request=tcp://{tmp_path}/request.sock;dump=frame;pub.dump=frame;request.dump=frame;storage.dump=frame'
    s = asyncloop.Channel(f'{common};storage=file://{tmp_path}/storage.dat;name=server;mode=server;blocks=blocks://{tmp_path}/blocks.yaml')
    c = asyncloop.Channel(f'{common};name=client;mode=client;peer=test;stream.protocol={protocol}')

    assert [x.name for x in s.children] == ['server/stream', 'server/request', 'server/storage', 'server/blocks']

    s.open()
    assert s.state == s.State.Active # No need to wait

    with pytest.raises(TLLError): s.post({'type':'default'}, name='Block', type=s.Type.Control)
    s.post(b'aaa', msgid=10, seq=10)
    s.post({'type':'default'}, name='Block', type=s.Type.Control)
    assert yaml.safe_load(open(tmp_path / 'blocks.yaml')) == [{'seq': 10, 'type':'default'}]
    s.post(b'bbb', msgid=10, seq=20)
    s.post({'type':'default'}, name='Block', type=s.Type.Control)
    s.post({'type':'rare'}, name='Block', type=s.Type.Control)
    s.post(b'ccc', msgid=10, seq=30)
    s.post({'type':'last'}, name='Block', type=s.Type.Control)

    assert yaml.safe_load(open(tmp_path / 'blocks.yaml')) == [{'seq': 10, 'type':'default'}, {'seq': 20, 'type': 'default'}, {'seq': 20, 'type': 'rare'}, {'seq': 30, 'type':'last'}]

    s.close()
    s.open()

    c.open('mode=block;' + req)

    cfg = dict([x.split('=') for x in ('mode=block;' + req).split(';')])
    cfg.setdefault('block-type', 'default')
    assert c.config.sub('info.reopen').as_dict() == cfg

    if result == []:
        m = await c.recv_state()
        assert m == c.State.Error
        return

    m = await c.recv()
    assert (m.type, m.msgid, m.seq) == (m.Type.Control, c.scheme_control['BeginOfBlock'].msgid, result[0])
    assert c.unpack(m).as_dict() == {'last_seq': result[0]}

    m = await c.recv()
    assert (m.type, m.msgid, m.seq) == (m.Type.Control, c.scheme_control['EndOfBlock'].msgid, result[0])

    for seq in result[1:-1]:
        m = await c.recv()
        assert (m.type, m.seq) == (m.Type.Data, seq)

    m = await c.recv()
    assert m.type == m.Type.Control
    assert (m.seq, c.unpack(m).SCHEME.name) == (result[-1], 'Online')

    assert c.config.sub('info.reopen').as_dict() == {'mode': 'seq', 'seq': f'{result[-1]}'}

@pytest.mark.parametrize("init_seq,init_block", [
        ('', ''),
        (10, ''),
        (10, 'other'),
        ])
@asyncloop_run
async def test_init_message(asyncloop, tmp_path, init_seq, init_block):
    SCHEME = '''yamls://
 - name: Initial
   id: 10
   fields: [{name: i64, type: int64}]
 - name: Data
   id: 20
   fields: [{name: i32, type: int32}]
'''
    common = f'stream+pub+tcp://{tmp_path}/stream.sock;request=tcp://{tmp_path}/request.sock;dump=frame;storage.dump=frame;blocks.dump=frame'
    s = asyncloop.Channel(f'{common};storage.url=file://{tmp_path}/storage.dat;name=server;mode=server;blocks.url=blocks://{tmp_path}/blocks.yaml;init-message=Initial;init-seq={init_seq};init-block={init_block};init-message-data.i64=100', scheme=SCHEME)
    c = asyncloop.Channel(f'{common};name=client;mode=client;peer=test', scheme=SCHEME)

    s.open()
    assert s.state == s.State.Active # No need to wait

    assert yaml.safe_load(open(tmp_path / 'blocks.yaml')) == [{'seq': init_seq or 0, 'type': init_block or 'default'}]

    s.close()
    s.open()

    assert s.state == s.State.Active # No need to wait

    assert yaml.safe_load(open(tmp_path / 'blocks.yaml')) == [{'seq': init_seq or 0, 'type': init_block or 'default'}]

    c.open(f'mode=seq;seq={init_seq or 0}')

    assert (await c.recv_state()) == c.State.Active

    m = await c.recv()
    assert (m.type, m.seq, m.msgid) == (m.Type.Data, init_seq or 0, 10)
    assert c.unpack(m).as_dict() == {'i64': 100}

    m = await c.recv()
    assert m.type == m.Type.Control
    assert (m.seq, c.unpack(m).SCHEME.name) == (init_seq or 0, 'Online')

    c.close()
    c.open(f'mode=block;block=0;block-type={init_block or "default"}')

    m = await c.recv()
    assert (m.type, m.seq, c.unpack(m).SCHEME.name) == (m.Type.Control, init_seq or 0, 'BeginOfBlock')

    m = await c.recv()
    assert (m.type, m.seq, c.unpack(m).SCHEME.name) == (m.Type.Control, init_seq or 0, 'EndOfBlock')

    m = await c.recv()
    assert (m.type, m.seq, c.unpack(m).SCHEME.name) == (m.Type.Control, init_seq or 0, 'Online')

@asyncloop_run
async def test_autoseq(asyncloop, tmp_path):
    common = f'stream+pub+tcp://{tmp_path}/stream.sock;request=tcp://{tmp_path}/request.sock;dump=frame;pub.dump=frame;request.dump=frame;storage.dump=frame'
    s = asyncloop.Channel(f'{common};storage=file://{tmp_path}/storage.dat;name=server;mode=server;autoseq=yes')
    c = asyncloop.Channel(f'{common};name=client;mode=client;peer=test')

    s.open()
    assert s.state == s.State.Active # No need to wait

    assert s.config['info.seq'] == '-1'
    for i in range(5):
        s.post(b'aaa' * i, seq=100)
    assert s.config['info.seq'] == '4'

    s.close()
    s.open()
    assert s.config['info.seq'] == '4'
    for i in range(5):
        s.post(b'aaa' * i, seq=100)
    assert s.config['info.seq'] == '9'

    c.open(seq='0', mode='seq')
    for i in range(10):
        m = await c.recv()
        assert (m.type, m.seq) == (m.Type.Data, i)

    m = await c.recv()
    assert m.type == m.Type.Control
    assert (m.seq, c.unpack(m).SCHEME.name) == (9, 'Online')

@asyncloop_run
async def test_block_clear(asyncloop, tmp_path):
    common = f'stream+pub+tcp://{tmp_path}/stream.sock;request=tcp://{tmp_path}/request.sock;dump=frame;pub.dump=frame;request.dump=frame;storage.dump=frame'
    s = asyncloop.Channel(f'{common};storage=file://{tmp_path}/storage.dat;name=server;mode=server;blocks=blocks://{tmp_path}/blocks.yaml')
    c = asyncloop.Channel(f'{common};name=client;mode=client;peer=test;report-block-begin=no;report-block-end=no')

    s.open()
    assert s.state == s.State.Active # No need to wait

    s.post(b'aaa', msgid=10, seq=10)
    s.post({'type':'default'}, name='Block', type=s.Type.Control)
    assert yaml.safe_load(open(tmp_path / 'blocks.yaml')) == [{'seq': 10, 'type':'default'}]
    s.post(b'bbb', msgid=10, seq=20)

    s.close()
    s.open()

    assert s.state == s.State.Opening
    child = s.children[-1]
    assert child.name == 'server/storage/client'
    child.process()

    child.process()
    assert child.state == child.State.Closed
    assert s.state == s.State.Active

    assert child.name == 'server/storage/client'

    assert yaml.safe_load(open(tmp_path / 'blocks.yaml')) == [{'seq': 10, 'type':'default'}]

    for i in range(2):
        if i == 0:
            c.open(mode='block', block='0', **{'block-type': 'default'})
        else:
            c.open(mode='seq', seq='20')

        m = await c.recv(0.1)
        assert (m.type, m.seq) == (m.Type.Data, 20)

        m = await c.recv()
        assert m.type == m.Type.Control
        assert (m.seq, c.unpack(m).SCHEME.name) == (20, 'Online')

        c.close()

    s.post({'type': 'default'}, name='Block', type=s.Type.Control)
    assert yaml.safe_load(open(tmp_path / 'blocks.yaml')) == [{'seq': 10, 'type': 'default'}, {'seq': 20, 'type': 'default'}]

def test_blocks_channel(context, tmp_path):
    w = context.Channel(f'blocks://{tmp_path}/blocks.yaml;default-type=def;dir=w')
    assert w.config.get('info.seq', None) == None

    w.open()
    assert w.config['info.seq'] == '-1'
    assert w.config['info.seq-begin'] == '-1'

    w.post(b'', seq=10)
    assert w.config['info.seq'] == '10'
    assert w.config['info.seq-begin'] == '-1'
    w.post({'type':''}, seq=100, name='Block', type=w.Type.Control)
    assert yaml.safe_load(open(tmp_path / 'blocks.yaml')) == [{'seq': 10, 'type':'def'}]

    w.post({'type':'other'}, seq=10, name='Block', type=w.Type.Control)
    assert yaml.safe_load(open(tmp_path / 'blocks.yaml')) == [{'seq': 10, 'type':'def'}, {'seq': 10, 'type': 'other'}]

    w.close()
    w.open()
    assert w.config['info.seq'] == '10'
    assert w.config['info.seq-begin'] == '-1'

    w.post(b'', seq=20)
    w.post({'type':'def'}, name='Block', type=w.Type.Control)
    assert yaml.safe_load(open(tmp_path / 'blocks.yaml')) == [{'seq': 10, 'type':'def'}, {'seq': 10, 'type': 'other'}, {'seq': 20, 'type': 'def'}]

    r = Accum('blocks://', master=w, context=context)
    with pytest.raises(TLLError): r.open({'block': '0'}) # block-type=default
    r.close()

    with pytest.raises(TLLError): r.open({'block': '10', 'block-type':'def'})
    r.close()

    with pytest.raises(TLLError): r.open({'block': '0', 'block-type':'unknown'})
    r.close()

    r.result = []
    r.open({'block': '0', 'block-type':'def'})
    assert r.state == r.State.Closed
    assert r.config['info.seq'] == '20'
    assert r.config['info.seq-begin'] == '-1'

    r.result = []
    r.open({'block': '1', 'block-type':'def'})
    assert r.state == r.State.Closed
    assert r.config['info.seq'] == '10'
    assert r.config['info.seq-begin'] == '-1'

    r.result = []
    r.open({'block': '0', 'block-type':'other'})
    assert r.state == r.State.Closed
    assert r.config['info.seq'] == '10'
    assert r.config['info.seq-begin'] == '-1'

    r = Accum(f'blocks://{tmp_path}/blocks.yaml;dir=r', context=context)

    r.result = []
    r.open({'block': '0', 'block-type':'other'})
    assert r.state == r.State.Closed
    assert r.config['info.seq'] == '10'
    assert r.config['info.seq-begin'] == '-1'

@asyncloop_run
async def test_rotate(asyncloop, tmp_path):
    common = f'stream+pub+tcp://{tmp_path}/stream.sock;request=tcp://{tmp_path}/request.sock;dump=frame;pub.dump=frame;request.dump=frame;storage.dump=frame'
    s = asyncloop.Channel(f'{common};storage=rotate+file://{tmp_path}/storage;name=server;mode=server;blocks=blocks://{tmp_path}/blocks.yaml')
    c = asyncloop.Channel(f'{common};name=client;mode=client;peer=test')

    s.open()
    assert s.scheme_control is not None
    assert 'Rotate' in [m.name for m in s.scheme_control.messages]

    s.post(b'xxx', seq=10)
    s.post({}, name='Rotate', type=s.Type.Control)

    assert (tmp_path / "storage.10.dat").exists()

    s.close()
    s.open()

@asyncloop_run
async def test_rotate_on_block(asyncloop, tmp_path):
    common = f'stream+pub+tcp://{tmp_path}/stream.sock;request=tcp://{tmp_path}/request.sock;dump=frame;pub.dump=frame;request.dump=frame;storage.dump=frame'
    s = asyncloop.Channel(f'{common};storage=rotate+file://{tmp_path}/storage;name=server;mode=server;blocks=blocks://{tmp_path}/blocks.yaml;rotate-on-block=rotate')
    c = asyncloop.Channel(f'{common};name=client;mode=client;peer=test')

    s.open()
    assert s.scheme_control is not None
    assert 'Rotate' in [m.name for m in s.scheme_control.messages]
    assert 'Block' in [m.name for m in s.scheme_control.messages]

    s.post(b'xxx', seq=10)

    s.post({}, name='Block', type=s.Type.Control)
    assert not (tmp_path / "storage.10.dat").exists()

    s.post({'type': 'other'}, name='Block', type=s.Type.Control)
    assert not (tmp_path / "storage.10.dat").exists()

    s.post({'type': 'rotate'}, name='Block', type=s.Type.Control)
    assert (tmp_path / "storage.10.dat").exists()

class Aggregate(Base):
    PROTO = 'aggr'
    SCHEME_CONTROL = '''yamls://
- name: Block
  id: 100
  fields:
    - {name: type, type: byte64, options: {type: string}}
'''
    STORAGE = []

    def _init(self, props, master=None):
        if self.caps & self.Caps.Output:
            self.scheme_control = self.context.scheme_load(self.SCHEME_CONTROL)
            self.PROCESS_POLICY = Base.ProcessPolicy.Never
        self._fail_mode = props.getT("fail-mode", False)

    def _open(self, props):
        if self.caps & self.Caps.Output:
            if self.STORAGE:
                self._seq, self._data = self.STORAGE[-1]
            else:
                self._seq, self._data = -1, {}
            self._data = dict(self._data)
            self.config_info['seq'] = lambda: str(self._seq)
            self._feed_seq = props.getT("feed-seq", -1)
            return
        assert props.get('block-type', 'default') == 'default'
        block = int(props.get('block'))

        self._seq, self._data = self.STORAGE[-(block + 1)]
        self.config_info['seq'] = str(self._seq)

        self._data = list(sorted(self._data.items()))
        self._seq -= len(self._data) - 1

        self.config_info['seq-begin'] = str(self._seq)

        self._update_pending(True)

    def _close(self, force=False):
        self.config_info['seq'] = str(self._seq)
        self._data = None
        self._seq = -1

    def _process(self, timeout, flags):
        if self.caps & self.Caps.Output:
            return

        if self._fail_mode:
            raise RuntimeError("Fail mode")

        if not self._data:
            self.close()
            return

        msgid, data = self._data.pop(0)
        msg = C.Message(data = data, msgid = msgid, seq=self._seq)
        self._seq += 1
        self._callback(msg)

    def _post(self, msg, flags):
        if self.caps & self.Caps.Input:
            return
        if msg.type == msg.Type.Control:
            m = self.scheme_control.unpack(msg)
            if m.SCHEME.name != 'Block':
                return
            self.STORAGE.append((self._seq, dict(self._data)))
            return

        self._seq = msg.seq
        self._data[msg.msgid] = msg.data.tobytes()
        if self._feed_seq >= 0:
            if self._seq != self._feed_seq:
                assert flags == C.PostFlags.More
            elif self._seq == self._feed_seq:
                assert flags == C.PostFlags.Zero

def test_aggregate(context):
    context.register(Aggregate)

    Aggregate.STORAGE = []

    w = context.Channel('aggr://;dir=w')
    w.open()

    assert w.config['info.seq'] == '-1'

    w.post(b'xxx', msgid=10, seq=5)
    w.post(b'yyy', msgid=10, seq=10)
    w.post({'type':'default'}, type=w.Type.Control, name='Block')

    assert Aggregate.STORAGE == [(10, {10: b'yyy'})]

    w.post(b'zzz', msgid=10, seq=15)
    w.close()

    w.open()
    assert w.config['info.seq'] == '10'

    w.post(b'zzz', msgid=20, seq=20)
    w.post({'type':'default'}, type=w.Type.Control, name='Block')

    assert Aggregate.STORAGE == [(10, {10: b'yyy'}), (20, {10: b'yyy', 20: b'zzz'})]

    r = Accum('aggr://;dir=r', context=context)
    r.open(block='0')
    assert r.state == r.State.Active

    assert r.config['info.seq'] == '20'

    r.process()
    r.process()
    assert r.state == r.State.Active

    assert [(m.msgid, m.data.tobytes()) for m in r.result] == [(10, b'yyy'), (20, b'zzz')]

    r.process()
    assert r.state == r.State.Closed

@asyncloop_run
async def test_stream_aggregate(asyncloop, context, tmp_path):
    context.register(Aggregate)

    Aggregate.STORAGE = []

    common = f'stream+pub+tcp://{tmp_path}/stream.sock;request=tcp://{tmp_path}/request.sock;dump=frame;pub.dump=frame;request.dump=frame;storage.dump=frame'
    s = asyncloop.Channel(f'{common};storage=file://{tmp_path}/storage;name=server;mode=server;blocks=aggr://')
    c = asyncloop.Channel(f'{common};name=client;mode=client;peer=test')

    s.open()

    s.post(b'xxx', msgid=10, seq=0)
    s.post(b'yyy', msgid=20, seq=10)
    s.post({'type':'default'}, name='Block', type=s.Type.Control)
    s.post(b'xxz', msgid=10, seq=20)
    s.post(b'yyz', msgid=20, seq=30)

    assert Aggregate.STORAGE == [(10, {10: b'xxx', 20: b'yyy'})]

    c.open(block='0', mode='block')
    assert c.config.sub('info.reopen').as_dict() == {'mode': 'block', 'block': '0', 'block-type': 'default'}

    m = await c.recv()
    assert (m.type, m.msgid, m.seq) == (c.Type.Control, c.scheme_control['BeginOfBlock'].msgid, 9)
    assert c.unpack(m).as_dict() == {'last_seq': 10}

    m = await c.recv()
    assert (m.seq, m.msgid, m.data.tobytes()) == (9, 10, b'xxx')
    assert c.config.sub('info.reopen').as_dict() == {'mode': 'block', 'block': '0', 'block-type': 'default'}

    m = await c.recv()
    assert (m.seq, m.msgid, m.data.tobytes()) == (10, 20, b'yyy')
    assert c.config.sub('info.reopen').as_dict() == {'mode': 'block', 'block': '0', 'block-type': 'default'}

    m = await c.recv()
    assert (m.type, m.msgid, m.seq) == (c.Type.Control, c.scheme_control['EndOfBlock'].msgid, 10)

    m = await c.recv()
    assert (m.seq, m.msgid, m.data.tobytes()) == (20, 10, b'xxz')
    assert c.config.sub('info.reopen').as_dict() == {'mode': 'seq', 'seq': c.config['info.seq']}

    m = await c.recv()
    assert (m.seq, m.msgid, m.data.tobytes()) == (30, 20, b'yyz')
    assert c.config.sub('info.reopen').as_dict() == {'mode': 'seq', 'seq': c.config['info.seq']}

    c.close()

    s.post({'type':'default'}, name='Block', type=s.Type.Control)
    c.open(block='0', mode='block')
    assert c.State.Active == await c.recv_state()
    assert [i.name for i in s.children if '/client' in i.name] == ['server/blocks/client']

    m = await c.recv()
    assert (m.type, m.msgid, m.seq) == (c.Type.Control, c.scheme_control['BeginOfBlock'].msgid, 29)
    assert c.unpack(m).as_dict() == {'last_seq': 30}

    m = await c.recv()
    assert (m.seq, m.msgid, m.data.tobytes()) == (29, 10, b'xxz')
    m = await c.recv()
    assert (m.seq, m.msgid, m.data.tobytes()) == (30, 20, b'yyz')

    s.result.clear()
    c.close()
    assert c.State.Closed == await c.recv_state()
    assert s.unpack(await s.recv()).SCHEME.name == 'Disconnect'
    assert [i.name for i in s.children if '/client' in i.name] == []

@asyncloop_run
async def test_stream_aggregate_error(asyncloop, context, tmp_path):
    context.register(Aggregate)

    Aggregate.STORAGE = []

    common = f'stream+pub+tcp://{tmp_path}/stream.sock;request=tcp://{tmp_path}/request.sock;dump=frame;pub.dump=frame;request.dump=frame;storage.dump=frame'
    s = asyncloop.Channel(f'{common};storage=file://{tmp_path}/storage;name=server;mode=server;blocks.url=aggr://;blocks.fail-mode=yes')
    c = asyncloop.Channel(f'{common};name=client;mode=client;peer=test')

    s.open()

    s.post(b'xxx', msgid=10, seq=0)
    s.post({'type':'default'}, name='Block', type=s.Type.Control)

    assert Aggregate.STORAGE == [(0, {10: b'xxx'})]

    c.open(block='0', mode='block')

    assert c.State.Active == await c.recv_state()
    assert c.State.Error == await c.recv_state()

@asyncloop_run
async def test_stream_aggregate_feed(asyncloop, context, tmp_path):
    context.register(Aggregate)

    Aggregate.STORAGE = []

    common = f'stream+null://;request=null://;dump=frame;pub.dump=frame;request.dump=frame;storage.dump=frame'
    s = asyncloop.Channel(f'{common};storage=file://{tmp_path}/storage;name=server;mode=server;blocks=aggr://')
    f = asyncloop.Channel(f'file://{tmp_path}/storage', dir='w', name='file')
    f.open()
    for i in range(5):
        f.post(b'xxx', msgid=10, seq=i)
    f.free()

    s.open({'blocks.feed-seq': '4'})
    assert s.State.Active == await s.recv_state()

    assert Aggregate.STORAGE == []
    s.post({'type':'default'}, name='Block', type=s.Type.Control)
    assert Aggregate.STORAGE == [(4, {10: b'xxx'})]

    s.post(b'xxx', msgid=10, seq=5, flags=s.PostFlags.More)

@asyncloop_run
async def test_ring(asyncloop, tmp_path):
    common = f'stream+pub+tcp:///{tmp_path}/stream.sock;request=tcp:///{tmp_path}/request.sock;dump=frame;pub.dump=frame;request.dump=frame;storage.dump=frame'
    s = asyncloop.Channel(f'{common};storage=file:///{tmp_path}/storage.dat;name=server;mode=server')
    c = asyncloop.Channel(f'{common};name=client;mode=client;peer=test')

    s.open()
    assert s.state == s.State.Active # No need to wait

    for i in range(10):
        s.post(b'aaa', msgid=10, seq=i)

    c.open(seq='0', mode='seq')

    m = await s.recv()
    assert m.type == m.Type.Control
    assert s.unpack(m).SCHEME.name == 'Connect'

    m = await c.recv()
    assert (m.seq, m.msgid, m.data.tobytes()) == (0, 10, b'aaa')

    for i in range(10, 20):
        s.post(b'bbb', msgid=10, seq=i)

    for _ in range(10):
        s.children[0].process()
        c.children[0].process()

    for i in range(1, 10):
        m = await c.recv()
        assert (m.seq, m.msgid, m.data.tobytes()) == (i, 10, b'aaa')

    for i in range(10, 15):
        m = await c.recv()
        assert (m.seq, m.msgid, m.data.tobytes()) == (i, 10, b'bbb')

    assert c.children[1].state == c.State.Closed

    for i in range(15, 20):
        m = await c.recv()
        assert (m.seq, m.msgid, m.data.tobytes()) == (i, 10, b'bbb')

    m = await c.recv()
    assert m.type == m.Type.Control
    assert (m.seq, c.unpack(m).SCHEME.name) == (19, 'Online')

    for i in range(20, 30):
        s.post(b'ccc', msgid=10, seq=i)

    for i in range(20, 30):
        m = await c.recv()
        assert (m.seq, m.msgid, m.data.tobytes()) == (i, 10, b'ccc')

@asyncloop_run
async def test_ring_autoclose(asyncloop, tmp_path):
    common = f'stream+pub+tcp:///{tmp_path}/stream.sock;request=tcp:///{tmp_path}/request.sock;dump=frame;pub.dump=frame;request.dump=frame;storage.dump=frame'
    s = asyncloop.Channel(f'{common};storage=file:///{tmp_path}/storage.dat;name=server;mode=server;storage.autoclose=yes')
    c = asyncloop.Channel(f'{common};name=client;mode=client;peer=test')

    s.open()
    assert s.state == s.State.Active # No need to wait

    for i in range(10):
        s.post(b'aaa', msgid=10, seq=i)

    c.open(seq='0', mode='seq')
    assert await c.recv_state() == c.State.Active

    m = await s.recv()
    assert m.type == m.Type.Control
    assert s.unpack(m).SCHEME.name == 'Connect'

    m = await c.recv()
    assert (m.seq, m.msgid, m.data.tobytes()) == (0, 10, b'aaa')

    assert [x.name for x in s.children] == ['server/stream', 'server/request', 'server/storage', 'server/storage/client']

    assert s.dcaps == s.DCaps.Zero

    file = s.children[-1]
    for _ in range(10):
        file.process()

    assert file.state == file.State.Closed
    file = None

    assert s.dcaps == s.DCaps.Process | s.DCaps.Pending
    s.process()
    assert s.dcaps == s.DCaps.Zero
    assert [x.name for x in s.children] == ['server/stream', 'server/request', 'server/storage']

    for i in range(10, 20):
        s.post(b'bbb', msgid=10, seq=i)

    for i in range(1, 10):
        m = await c.recv()
        assert (m.seq, m.msgid, m.data.tobytes()) == (i, 10, b'aaa')

    assert await c.recv_state() == c.State.Error

@asyncloop_run
async def test_export_client(asyncloop, tmp_path):
    scheme = 'yamls://[{name: Test, id: 10}]'
    server = asyncloop.Channel(f'stream+pub+tcp:///{tmp_path}/stream.sock;request=tcp:///{tmp_path}/request.sock;dump=frame;pub.dump=frame;request.dump=frame;storage.dump=frame;storage=file:///{tmp_path}/storage.dat;name=server;mode=server', scheme=scheme)
    server.open()
    server.post(b'xxx', seq=100)

    assert [m.name for m in server.scheme.messages] == ['Test']

    url = server.config.get_url('client.init')
    assert url.proto == 'stream+pub+tcp'

    c = asyncloop.Channel(url, name='client')
    c.open(mode='seq', seq='100')
    assert await c.recv_state() == c.State.Active
    assert (await c.recv()).seq == 100

    assert [m.name for m in c.scheme.messages] == ['Test']

@asyncloop_run
async def test_export_client_wildcard(asyncloop, tmp_path):
    scheme = 'yamls://[{name: Test, id: 10}]'
    server = asyncloop.Channel(f'stream+pub+tcp://*:0;request=tcp://*:0;dump=frame;storage=file:///{tmp_path}/storage.dat;name=server;mode=server', scheme=scheme)
    server.open()
    server.post(b'xxx', seq=100)

    assert [m.name for m in server.scheme.messages] == ['Test']

    url = server.config.get_url('client.init').copy()
    assert url.proto == 'stream+pub+tcp'

    assert [k for k, _ in server.config.sub('client.replace', False).browse('**')] == sorted(['host.init.tll.host.host', 'host.init.request.tll.host.host'])
    for k, _ in server.config.browse('client.replace.host.init.**'):
        url[k.split('.', 4)[-1]] = '::1'

    c = asyncloop.Channel(url, name='client')
    c.open(mode='seq', seq='100')
    assert await c.recv_state() == c.State.Active
    assert (await c.recv()).seq == 100

    assert [m.name for m in c.scheme.messages] == ['Test']

@asyncloop_run
@pytest.mark.parametrize("mode,rseq,oseq", [
        ("request-only", list(range(10, 15)), []),
        ("request+online", list(range(10, 25)), [30]),
        ])
async def test_request_close(asyncloop, tmp_path, path_srcdir, mode, rseq, oseq):
    request = asyncloop.Channel('direct://', name='request', scheme=f'yaml://{path_srcdir / "src/channel/stream-scheme.yaml"}')
    request.open()

    client = asyncloop.Channel(f'stream+direct://;request=direct://', dump='frame', name='client', mode='client', protocol='new',
                               **{'request.master': 'request', 'request.dump': 'frame', 'direct.dump': 'frame'})
    client.open(mode='seq', seq='10')

    assert client.children[0].name == 'client/stream'
    assert client.children[1].name == 'client/request'

    online = asyncloop.Channel('direct://', master=client.children[0], name='online')
    online.open()

    m = request.unpack(await request.recv())
    assert m.SCHEME.name == 'Request'
    assert m.as_dict() == {'version': m.version.Current, 'client': '', 'attributes': [], 'data': {'seq': 10}}

    request.post({'last_seq': 20, 'requested_seq': 10, 'block_seq': -1}, name='Reply')

    for s in oseq:
        online.post(b'online', seq=s)
    for i in rseq:
        request.post(b'xxx', seq=i)

    for i in rseq:
        m = await client.recv()
        assert (m.seq, m.data.tobytes()) == (i, b'xxx')

    client.children[1].close()

    assert client.state == client.State.Error

@asyncloop_run
async def test_client(asyncloop, tmp_path):
    s = asyncloop.Channel(f'stream+pub+tcp://::1:{ports.TCP6};request=tcp://127.0.0.1:{ports()}', storage=f'file://{tmp_path}/file.dat', name='server', mode='server')
    s.open()
    s.post(b'xxx', seq=10)

    c0 = asyncloop.Channel(s.config.sub('client.init'), name='c0')
    cfg = s.config.sub('client.children.online.init').copy()
    cfg['tll.proto'] = 'stream+' + cfg['tll.proto']
    cfg['request'] = s.config.sub('client.children.request.init').copy()
    c1 = asyncloop.Channel(cfg, name='c1')

    for c in (c0, c1):
        c.open(mode='seq', seq='0')

        m = await c.recv()
        assert (m.type, m.seq) == (m.Type.Data, 10)
        m = await c.recv()
        assert (m.type, m.seq) == (m.Type.Control, 10)

@asyncloop_run
async def test_block_in_future(asyncloop, tmp_path):
    common = f'stream+pub+tcp://{tmp_path}/stream.sock;request=tcp://{tmp_path}/request.sock;dump=frame;storage.dump=frame'
    s = asyncloop.Channel(f'{common};storage=file://{tmp_path}/storage.dat;name=server;mode=server;blocks=blocks://{tmp_path}/blocks.yaml')
    c = asyncloop.Channel(f'{common};name=client;mode=client;peer=test')

    assert [x.name for x in s.children] == ['server/stream', 'server/request', 'server/storage', 'server/blocks']
    blocks = s.children[-1]

    s.open()
    assert s.state == s.State.Active # No need to wait

    for i in range(10):
        s.post(b'xxx', msgid=10, seq=i)
    s.post({'type': 'default'}, name='Block', type=s.Type.Control)

    assert yaml.safe_load(open(tmp_path / 'blocks.yaml')) == [{'seq': 9, 'type':'default'}]
    s.close()
    s.open()

    c.open(mode='block', block='0')

    m = await c.recv()
    assert m.seq == 9

    c.close()

    blocks.post(b'', seq=15)
    blocks.post({'type': 'default'}, name='Block', type=s.Type.Control)

    assert blocks.config['info.seq'] == '15'

    c.open(mode='block', block='0')

    assert (await c.recv_state()) == c.State.Error

    c.close()
    s.close()

    with pytest.raises(TLLError): s.open()

@asyncloop_run
async def test_online_gap(asyncloop, tmp_path):
    common = Url.parse(f'stream+pub+tcp://{tmp_path}/stream.sock;request=tcp://{tmp_path}/request.sock;dump=frame;storage.dump=frame')
    s = asyncloop.Channel(common, storage=f'file://{tmp_path}/storage.dat', name='server', mode='server', **{'tll.proto': 'stream+null'})
    spub = asyncloop.Channel(common, name='online', mode='server', **{'tll.proto': 'pub+tcp'})
    c = asyncloop.Channel(common, name='client')

    s.open()
    spub.open()
    for i in range(5):
        s.post(b'xxx', seq=i)
    spub.post(b'yyy', seq=10)

    c.open(mode='seq', seq='0')

    for i in range(5):
        m = await c.recv()
        assert (m.type, m.seq) == (m.Type.Data, i)

    s.post(b'yyy', seq=10)

    m = await c.recv()
    assert (m.type, m.seq) == (m.Type.Data, 10)

    m = await c.recv()
    assert (m.type, m.msgid, m.seq) == (m.Type.Control, c.scheme_control['Online'].msgid, 10)

@asyncloop_run
async def test_online_last_seq(asyncloop, tmp_path):
    common = f'stream+pub+tcp://{tmp_path}/stream.sock;request=tcp://{tmp_path}/request.sock;dump=frame;storage.dump=frame'
    s = asyncloop.Channel(f'{common};storage=file://{tmp_path}/storage.dat;name=server;mode=server')
    c = asyncloop.Channel(f'pub+tcp://{tmp_path}/stream.sock;name=client;mode=client;peer=test')

    s.open()

    c.open()
    assert (await c.recv_state()) == c.State.Active
    assert c.config['info.seq'] == '-1'
    c.close()

    s.post(b'xxx', seq=10)

    c.open()
    assert (await c.recv_state()) == c.State.Active
    assert c.config['info.seq'] == '10'
    c.close()

    s.close()
    s.open()

    c.open()
    assert (await c.recv_state()) == c.State.Active
    assert c.config['info.seq'] == '10'
    c.close()

@asyncloop_run
async def test_close_in_block(asyncloop, tmp_path):
    common = f'stream+pub+tcp://{tmp_path}/stream.sock;request=tcp://{tmp_path}/request.sock;dump=frame;storage.dump=frame'
    s = asyncloop.Channel(f'{common};storage=file://{tmp_path}/storage.dat;name=server;mode=server;blocks=blocks://{tmp_path}/blocks.yaml')
    c = asyncloop.Channel(f'{common};name=client;mode=client;peer=test')

    s.open()
    s.post(b'xxx', seq=10)
    s.post({}, name='Block', type=s.Type.Control)

    c.open(mode='block', block='0')
    scheme = c.scheme_control
    def cb(ch, m):
        if m.msgid == scheme['EndOfBlock'].msgid:
            c.close()
    c.callback_add(cb, mask=c.MsgMask.Control)

    assert (await c.recv_state()) == c.State.Active
    assert (await c.recv_state()) == c.State.Closed

    assert [(m.type, m.msgid) for m in c.result] == [(c.Type.Control, scheme['BeginOfBlock'].msgid), (c.Type.Control, scheme['EndOfBlock'].msgid)]
