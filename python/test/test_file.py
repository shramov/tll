#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import collections
import os
import random
import struct

import pytest

from tll.channel import Context
from tll.config import Config
from tll.error import TLLError
from tll.test_util import Accum

EXTRA_SIZE = 4 + 12 + 1 # Size + frame + tail marker
META_SIZE = EXTRA_SIZE + 32 # Extra + meta size

@pytest.fixture(params=['v0', 'v1'])
def context(request):
    cfg = Config.from_dict({"file.io": "posix", "file.version": request.param[1:]})
    return Context(cfg)

@pytest.fixture
def filename(tmp_path):
    return tmp_path / 'file.dat'

@pytest.fixture
def writer(context, filename):
    return context.Channel(f'file://{filename}', name='writer', dump='frame', dir='w', block='1kb')

@pytest.fixture
def reader(context, filename):
    return Accum(f'file://{filename}', name='reader', dump='frame', context=context, autoclose='no')

Frame = collections.namedtuple('Frame', ('size', 'msgid', 'seq'))

def frame(data):
    return Frame(*struct.unpack('Iiq', data[:16]))

def test_basic(writer, reader, filename):
    w = writer
    w.open()
    mask = 0 if w.config['info.version'] == '0' else 0x80000000
    assert w.dcaps == w.DCaps.Zero

    assert w.scheme_control is None

    assert w.config['info.seq-begin'] == '-1'
    assert w.config['info.seq'] == '-1'

    with pytest.raises(TLLError): w.post(b'x' * 1024 * 1024)
    with pytest.raises(TLLError): w.post(b'x' * (1024 - EXTRA_SIZE + 1))

    assert w.config['info.seq-begin'] == '-1'
    assert w.config['info.seq'] == '-1'

    assert filename.stat().st_size == META_SIZE
    fp = filename.open('rb')

    w.post(b'a' * 128, seq=0, msgid=0)
    assert filename.stat().st_size == META_SIZE + (128 + EXTRA_SIZE) * 1

    assert w.config['info.seq-begin'] == '0'
    assert w.config['info.seq'] == '0'

    with pytest.raises(TLLError): w.post(b'x', seq=0)

    data = fp.read(16)
    assert data == bytes([META_SIZE]) + b'\0\0\0Meta\0\0\0\0\0\0\0\0'

    data = fp.read(META_SIZE - 16) # Skip meta

    data = fp.read(128 + EXTRA_SIZE)
    assert frame(data) == Frame(mask | (128 + EXTRA_SIZE), 0, 0)
    assert data[16:] == b'a' * 128 + b'\x80'

    w.post(b'b' * 128, seq=1, msgid=10)
    assert filename.stat().st_size == META_SIZE + (128 + EXTRA_SIZE) * 2

    assert w.config['info.seq-begin'] == '0'
    assert w.config['info.seq'] == '1'

    with pytest.raises(TLLError): w.post(b'x', seq=1)

    data = fp.read(128 + EXTRA_SIZE)
    assert frame(data) == Frame(mask | (128 + EXTRA_SIZE), 10, 1)
    assert data[16:] == b'b' * 128 + b'\x80'

    reader.open()
    assert reader.dcaps == reader.DCaps.Process | reader.DCaps.Pending

    assert reader.scheme_control is not None
    assert [m.name for m in reader.scheme_control.messages] == ['Seek', 'EndOfData']

    assert reader.config['info.seq-begin'] == '0'
    assert reader.config['info.seq'] == '1'

    reader.process()
    assert [(x.seq, x.msgid, len(x.data), x.data.tobytes()) for x in reader.result] == [(0, 0, 128, b'a' * 128)]
    reader.result = []

    reader.process()
    assert [(x.seq, x.msgid, len(x.data), x.data.tobytes()) for x in reader.result] == [(1, 10, 128, b'b' * 128)]
    reader.result = []

    reader.process()
    assert [(x.type, x.seq, x.msgid) for x in reader.result] == [(reader.Type.Control, 0, reader.scheme_control.messages.EndOfData.msgid)]
    reader.result = []

    reader.process()
    assert [(x.seq, x.msgid, len(x.data), x.data.tobytes()) for x in reader.result] == []
    assert reader.dcaps == reader.DCaps.Process

    w.post(b'c' * 128, seq=2, msgid=20)
    assert filename.stat().st_size == META_SIZE + (128 + EXTRA_SIZE) * 3

    data = fp.read(128 + EXTRA_SIZE)
    assert frame(data) == Frame(mask | (128 + EXTRA_SIZE), 20, 2)
    assert data[16:] == b'c' * 128 + b'\x80'

    reader.process()
    assert [(x.seq, x.msgid, len(x.data), x.data.tobytes()) for x in reader.result] == [(2, 20, 128, b'c' * 128)]
    reader.result = []

def test_open_error(reader, writer, filename):
    with pytest.raises(TLLError): reader.open()
    filename.mkdir()
    with pytest.raises(TLLError): writer.open()

def test_block_boundary(writer, reader, filename):
    w = writer
    w.open()
    mask = 0 if w.config['info.version'] == '0' else 0x80000000

    assert filename.stat().st_size == META_SIZE
    fp = filename.open('rb')

    w.post(b'a' * 512, seq=0, msgid=0)
    assert filename.stat().st_size == META_SIZE + (512 + EXTRA_SIZE) * 1

    data = fp.read(16)
    assert data == bytes([META_SIZE]) + b'\0\0\0Meta\0\0\0\0\0\0\0\0'

    data = fp.read(META_SIZE - 16) # Skip meta

    data = fp.read(512 + EXTRA_SIZE)
    assert frame(data) == Frame(mask | (512 + EXTRA_SIZE), 0, 0)
    assert data[16:] == b'a' * 512 + b'\x80'

    reader.open()

    reader.process()
    assert [(x.seq, x.msgid, len(x.data), x.data.tobytes()) for x in reader.result] == [(0, 0, 512, b'a' * 512)]
    reader.result = []

    reader.process()
    assert [(x.type, x.seq, x.msgid) for x in reader.result] == [(reader.Type.Control, 0, reader.scheme_control.messages.EndOfData.msgid)]
    reader.result = []

    reader.process()
    assert [(x.seq, x.msgid, len(x.data), x.data.tobytes()) for x in reader.result] == []
    assert reader.dcaps == reader.DCaps.Process

    w.post(b'b' * 512, seq=1, msgid=10)
    assert filename.stat().st_size == 1024 + 5 + (512 + EXTRA_SIZE) * 1

    data = fp.read(16)
    assert frame(data) == Frame(0xffffffff, 0, 0)
    fp.seek(1024)

    data = fp.read(5)
    assert data == b'\x05\0\0\0\x80' if mask == 0 else b'\x05\0\0\x80\x80'

    data = fp.read(512 + EXTRA_SIZE)
    assert frame(data) == Frame(mask | (512 + EXTRA_SIZE), 10, 1)
    assert data[16:] == b'b' * 512 + b'\x80'

    reader.process()
    assert [(x.seq, x.msgid, len(x.data), x.data.tobytes()) for x in reader.result] == [(1, 10, 512, b'b' * 512)]
    reader.result = []

@pytest.mark.parametrize("seq,r", [(None, 10), (0, 10), (5, 10), (100, 100), (105, 110), (1000, 1000)])
def test_open_seq(seq, r, writer, reader):
    writer.open()

    for i in range(100):
        writer.post(b'abc' * i, seq = 10 * (i + 1), msgid = i)

    reader.open(**({'seq': str(seq)} if seq is not None else {}))

    assert reader.config['info.seq-begin'] == '10'
    assert reader.config['info.seq'] == '1000'

    reader.process()
    assert [(x.seq, x.msgid, len(x.data)) for x in reader.result] == [(r, r // 10 - 1, 3 * (r // 10 - 1))]

    if seq is not None:
        reader.post(b'', type=reader.Type.Control, name='Seek', seq=seq)
        reader.result = []
        reader.process()
        assert [(x.seq, x.msgid, len(x.data)) for x in reader.result] == [(r, r // 10 - 1, 3 * (r // 10 - 1))]

def test_open_seq_border(writer, reader):
    writer.open()

    writer.post(b'a' * 512, seq = 0, msgid = 10)
    writer.post(b'b' * 512, seq = 10, msgid = 20)

    reader.open(seq='5')

    assert reader.config['info.seq-begin'] == '0'
    assert reader.config['info.seq'] == '10'

    reader.process()
    assert [(x.seq, x.msgid, len(x.data)) for x in reader.result] == [(10, 20, 512)]

def test_meta(context, filename):
    SCHEME = '''yamls://
- name: msg
  fields: [{name: f0, type: int32}]
'''
    w = context.Channel(f'file://{filename}', name='writer', dump='frame', dir='w', block='1kb', scheme=SCHEME)
    r = context.Channel(f'file://{filename}', name='reader', dump='frame', dir='r', block='4kb')

    w.open()
    assert os.path.exists(filename)
    r.open()
    assert r.scheme != None
    assert [m.name for m in r.scheme.messages] == ['msg']
    assert r.config.get('info.block', '') == '1kb'

def test_autoclose(context, filename, writer):
    writer.open()
    reader = Accum(f'file://{filename}', name='reader', dump='frame', dir='r', context=context, autoclose='yes')

    for i in range(10):
        writer.post(b'abc' * i, seq = 10 * (i + 1), msgid = i)

    reader.open(seq='50')
    reader.process()
    for _ in range(10):
        reader.process()
    assert [m.seq for m in reader.result] == list(range(50, 101, 10))
    assert reader.state == reader.State.Closed

@pytest.mark.fuzzy
@pytest.mark.parametrize("rio", ['posix', 'mmap'])
@pytest.mark.parametrize("wio", ['posix', 'mmap'])
def test_fuzzy(filename, rio, wio):
    writer = Accum(f'file://{filename}', name='writer', dump='frame', context=context, dir='w', block='4kb', io=wio)
    reader = Accum(f'file://{filename}', name='reader', dump='frame', context=context, autoclose='no', io=rio)
    data = []
    start = random.randrange(0, 10000)

    writer.open()

    for i in range(1000):
        size = random.randrange(0, 512)
        data.append(size)
        writer.post(bytes([size % 256] * size), msgid=size, seq=start + 2 * i)

    for j in range(2000 - 2):
        i = (j + 1) // 2
        reader.result = []
        reader.open(seq=f'{start + j}')
        assert reader.config['info.seq-begin'] == f'{start}'
        assert reader.config['info.seq'] == f'{start + 2 * 999}'

        for _ in range(5):
            reader.process()
        assert [(m.seq, m.msgid, len(m.data)) for m in reader.result if m.type == m.Type.Data] == [(start + 2 * k, data[k], data[k]) for k in range(i, min(i + 5, len(data)))]
        reader.close()

    reader.open()
    for j in reversed(range(2000 - 2)):
        i = (j + 1) // 2
        reader.result = []
        reader.post(b'', type=reader.Type.Control, name='Seek', seq=start + j)
        for _ in range(5):
            reader.process()
        assert [(m.seq, m.msgid, len(m.data)) for m in reader.result if m.type == m.Type.Data] == [(start + 2 * k, data[k], data[k]) for k in range(i, min(i + 5, len(data)))]

def test_open_filename(context, filename):
    writer = context.Channel('file://', name='writer', dump='frame', dir='w', block='1kb')

    writer.open(filename=str(filename))
    for i in range(10):
        writer.post(b'abc' * i, seq = 10 * (i + 1), msgid = i)

    writer.close()
    with pytest.raises(TLLError): writer.open()

    reader = Accum('file://', name='reader', dump='frame', dir='r', context=context, autoclose='yes')
    reader.open(filename=str(filename), seq='50')

    for _ in range(10):
        reader.process()

    assert [m.seq for m in reader.result] == list(range(50, 101, 10))
    assert reader.state == reader.State.Closed

def test_autoseq(context, filename):
    writer = context.Channel(f'file://{filename}', name='writer', dump='frame', dir='w', block='1kb', autoseq='yes')

    writer.open()
    for i in range(5):
        writer.post(b'abc' * i, seq = 100)
    assert writer.config['info.seq'] == '4'

    writer.close()

    writer.open()
    assert writer.config['info.seq'] == '4'
    for i in range(5):
        writer.post(b'abc' * i, seq = 100)
    assert writer.config['info.seq'] == '9'

    reader = Accum(f'file://{filename}', name='reader', dump='frame', dir='r', context=context, autoclose='yes')

    reader.open()
    for _ in range(20):
        reader.process()

    assert [m.seq for m in reader.result] == list(range(10))
    assert reader.state == reader.State.Closed

@pytest.mark.parametrize("extra", [0, 1, 2, 4])
def test_tail_extra(context, filename, extra):
    writer = context.Channel(f'file://{filename}', name='writer', dump='frame', dir='w', block='1kb', autoseq='yes', **{'extra-space': f'{extra}kb'})

    writer.open()
    if extra:
        assert filename.stat().st_size == (1 + extra) * 1024
    else:
        assert filename.stat().st_size < 1024

    for i in range(2):
        writer.post(b'a' * 256)

    size = META_SIZE + 2 * (16 + 256 + 1)
    if extra:
        size = (1 + extra) * 1024
    assert filename.stat().st_size == size

    writer.close()
    writer.open()

    assert filename.stat().st_size == size

    for i in range(3):
        writer.post(b'a' * 256)

    size = 1024 + 4 + 1 + 2 * (16 + 256 + 1)
    if extra == 1:
        size = (2 + extra) * 1024
    elif extra:
        size = (1 + extra) * 1024 # Not resized, extra space was at the end
    assert filename.stat().st_size == size

    r = Accum(f'file://{filename}', name='reader', dump='frame', dir='r', context=context, block='4kb')
    r.open()

    for _ in range(5):
        r.process()
    assert [(m.seq, len(m.data)) for m in r.result] == [(i, 256) for i in range(5)]

def test_mmap_read(context, filename):
    writer = context.Channel(f'file://{filename}', name='writer', dump='frame', dir='w', block='4kb', autoseq='yes', **{'extra-space': f'16kb'})
    writer.open()

    for i in range(4):
        writer.post(b'a' * 512)

    assert filename.stat().st_size == 20 * 1024

    r = Accum(f'file://{filename}', name='reader', dump='frame', dir='r', io='mmap', context=context)
    r.open()

    for _ in range(4):
        r.process()
    assert [(m.seq, len(m.data)) for m in r.result] == [(i, 512) for i in range(4)]

@pytest.mark.parametrize("io", ['posix', 'mmap'])
@pytest.mark.parametrize("compress", ['none', 'lz4'])
def test_compress(context, filename, io, compress):
    writer = Accum(f'file://{filename}', name='writer', dump='frame', context=context, dir='w', block='4kb', compression=compress, io=io)
    reader = Accum(f'file://{filename}', name='reader', dump='frame', context=context, autoclose='no', io=io)

    writer.open()
    assert writer.config['info.compression'] == compress

    data = bytes(range(0, 0x100)) + bytes(range(0, 0x100, 2)) + bytes(range(1, 0x100, 2))
    for i in range(100):
        if i % 17 == 0:
            writer.close()
            writer.open()
        writer.post(data * (i % 7 + 1), seq=i, msgid=i + 10)
    reader.open()
    assert reader.config['info.compression'] == compress
    for i in range(100):
        reader.process()
        assert [(m.msgid, m.seq) for m in reader.result[-1:]] == [(i + 10, i)]
        assert reader.result[-1].data.tobytes() == data * (i % 7 + 1)
    reader.process()
    assert reader.result[-1].type == reader.Type.Control

    for oseq in range(0, 100, 7):
        reader.post(b'', type=reader.Type.Control, name='Seek', seq=oseq)
        reader.result = []
        for i in range(oseq, 100):
            reader.process()
            assert [(m.msgid, m.seq) for m in reader.result[-1:]] == [(i + 10, i)]
            assert reader.result[-1].data.tobytes() == data * (i % 7 + 1)

def test_skip_frame_trim(context, filename):
    writer = Accum(f'file://{filename}', name='writer', dump='frame', context=context, dir='w', block='1kb', io='posix')
    writer.open()
    writer.post(b'x' * (1024 - (5 + 16 + 1) - 2), seq=0)
    assert filename.stat().st_size == 1024 + 1024 - 2
    writer.post(b'x' * 4, seq=1)
    assert filename.stat().st_size == 1024 * 2 + 5 + 16 + 4 + 1
    f = filename.open('rb')
    f.seek(2 * 1024 - 2)
    assert f.read(2) == b'\0\0'

def test_scheme_override(context, filename):
    writer = context.Channel(f'file://{filename}', name='writer', dir='w', scheme='yamls://[{name: Old}]')
    writer.open()
    writer.close()

    r0 = context.Channel(f'file://{filename}', name='r0')
    r1 = context.Channel(f'file://{filename}', name='r0', scheme='yamls://[{name: New}]')

    r0.open()
    r1.open()

    assert r0.scheme != None
    assert r1.scheme != None

    assert [m.name for m in r0.scheme.messages] == ['Old']
    assert [m.name for m in r1.scheme.messages] == ['New']

def test_lz4_repeated(context, filename):
    writer = context.Channel(f'file://{filename}', name='writer', dir='w', compression='lz4', dump='frame')
    writer.open()

    data = bytes(range(256))

    for i in range(20):
        writer.post(data, msgid=i // 10, seq=i)

    reader = Accum(f'file://{filename}', name='reader', dump='frame', context=context)
    reader.open()
    for i in range(20):
        reader.process()
        m = reader.result.pop(0)
        assert (m.seq, m.msgid) == (i, i // 10)
        assert m.data.tobytes() == data

def test_lz4_init(context, filename):
    w0 = context.Channel(f'file://{filename}', name='writer', dir='w', compression='lz4', dump='frame')
    w0.open()
    w0.post(b'xxx', seq=10)
    w0.close()

    w1 = context.Channel(f'file://{filename}', name='writer', dir='w', dump='frame')
    w1.open()
    assert w1.config['info.seq'] == '10'
    w1.close()

@pytest.mark.parametrize("prefix", ["timeit", "rate"])
def test_file_prefix(context, filename, prefix):
    w = context.Channel(f'{prefix}+file://{filename}', speed='1kb', name='writer', dir='w', compression='lz4', dump='frame')
    w.open()
    w.post(b'xxx', seq=10)
    assert w.config['info.seq'] == '10'
    assert w.config.sub('info').as_dict() == w.config.sub('child.info').as_dict()

@pytest.mark.parametrize("io", ['posix', 'mmap'])
def test_reopen_lz4_meta(context, filename, io):
    SCHEME = '''yamls://
- name: Frame
  fields:
    - {name: msgid, type: int32}
    - {name: seq, type: int64}
- name: Data
  id: 12345678
  fields:
    - {name: list, type: 'Frame[2]'}
'''
    w = context.Channel(f'file://{filename}', name='writer', dir='w', compression='lz4', io=io, dump='yes', autoseq='yes', scheme=SCHEME)
    w.open()
    msgid = 12345678
    for i in range(4):
        # Feed lz4 dict with frame-like structures
        w.post({'list': [{'msgid': msgid, 'seq': 1}] * 2}, name='Data')
    w.close()

    w.open()
    w.post({'list': [{'msgid': msgid, 'seq': 3}] * 2}, name='Data')
    w.close()

    r = Accum(f'file://{filename}', name='reader', dump='yes', context=context)
    r.open()
    for _ in range(5):
        r.process()
    assert r.unpack(r.result[-1]).as_dict() == {'list': [{'msgid': msgid, 'seq': 3}] * 2}

@pytest.mark.parametrize("count", [0, 3, 4])
@pytest.mark.parametrize("mode", ['last', 'end'])
def test_open_mode(context, filename, count, mode):
    w = context.Channel(f'file://{filename}', name='writer', dir='w', dump='frame', block='4kb')
    r = Accum(f'file://{filename}', name='reader', dump='frame', autoclose='no', context=context)
    w.open()

    for i in range(count):
        w.post(b'x' * 256, seq=i)

    r.open(mode=mode)
    r.process()

    initial = [] if mode == 'end' or count == 0 else [count - 1]
    assert [m.seq for m in r.result if m.type == m.Type.Data] == initial

    w.post(b'x' * 1024, seq=count)
    r.process()
    assert [m.seq for m in r.result if m.type == m.Type.Data] == initial + [count]

def test_write_lock(context, filename):
    w0 = context.Channel(f'file://{filename}', name='w0', dir='w')
    w1 = context.Channel(f'file://{filename}', name='w1', dir='w')

    w0.open()
    with pytest.raises(TLLError): w1.open()

    w1.close()
    w0.close()

    w1.open()
    with pytest.raises(TLLError): w0.open()

def test_meta_page_boundary(context, filename):
    '''
    Metadata has to be 4093 bytes (4096 - 3, frame overlaps with page boundary)
    '''
    body = ''.join([
        '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'
    ])
    scheme = f'''yamls://
- name: Data
  options.doc: {body}
  fields:
    - {{name: f0, type: int32}}
'''
    f = context.Channel(f'file://{filename}', name='writer', dir='w', io='mmap', scheme=scheme)
    f.open(overwrite='yes')

@pytest.mark.parametrize("eod", ["", "once", "many", "before-close"])
@pytest.mark.parametrize("autoclose", ["autoclose", "no-autoclose"])
def test_eod(context, filename, writer, eod, autoclose):
    autoclose = autoclose == "autoclose"
    writer.open()

    reader = Accum(f'file://{filename}', name='reader', dump='frame', context=context, autoclose=str(autoclose).lower(), **{'end-of-data': eod})
    reader.open()
    assert reader.state == reader.State.Active
    reader.process()
    if autoclose:
        assert reader.state == reader.State.Closed
        if eod == 'before-close':
            assert [(m.type, m.msgid) for m in reader.result] == [(reader.Type.Control, reader.scheme_control['EndOfData'].msgid)]
        return
    assert [(m.type, m.msgid) for m in reader.result] == [(reader.Type.Control, reader.scheme_control['EndOfData'].msgid)]
    writer.post(b'xxx', msgid=100, seq=10)
    reader.result = []
    reader.process()
    assert [(m.type, m.msgid) for m in reader.result] == [(reader.Type.Data, 100)]
    reader.result = []
    reader.process()
    if eod == 'many':
        assert [(m.type, m.msgid) for m in reader.result] == [(reader.Type.Control, reader.scheme_control['EndOfData'].msgid)]
    else:
        assert [(m.type, m.msgid) for m in reader.result] == []

    writer.post(b'xxx', msgid=200, seq=20)
    reader.result = []
    reader.process()
    assert [(m.type, m.msgid) for m in reader.result] == [(reader.Type.Data, 200)]
    reader.result = []
    reader.process()
    if eod == 'many':
        assert [(m.type, m.msgid) for m in reader.result] == [(reader.Type.Control, reader.scheme_control['EndOfData'].msgid)]
    else:
        assert [(m.type, m.msgid) for m in reader.result] == []
