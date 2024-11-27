#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import collections
import os
import random
import struct

import pytest

from tll.channel import Context
from tll.error import TLLError
from tll.test_util import Accum

EXTRA_SIZE = 4 + 12 + 1 # Size + frame + tail marker
META_SIZE = EXTRA_SIZE + 32 # Extra + meta size

@pytest.fixture
def context():
    return Context()

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
    return Frame(*struct.unpack('iiq', data[:16]))

def test_basic(writer, reader, filename):
    w = writer
    w.open()
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
    assert frame(data) == Frame(128 + EXTRA_SIZE, 0, 0)
    assert data[16:] == b'a' * 128 + b'\x80'

    w.post(b'b' * 128, seq=1, msgid=10)
    assert filename.stat().st_size == META_SIZE + (128 + EXTRA_SIZE) * 2

    assert w.config['info.seq-begin'] == '0'
    assert w.config['info.seq'] == '1'

    with pytest.raises(TLLError): w.post(b'x', seq=1)

    data = fp.read(128 + EXTRA_SIZE)
    assert frame(data) == Frame(128 + EXTRA_SIZE, 10, 1)
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
    assert frame(data) == Frame(128 + EXTRA_SIZE, 20, 2)
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

    assert filename.stat().st_size == META_SIZE
    fp = filename.open('rb')

    w.post(b'a' * 512, seq=0, msgid=0)
    assert filename.stat().st_size == META_SIZE + (512 + EXTRA_SIZE) * 1

    data = fp.read(16)
    assert data == bytes([META_SIZE]) + b'\0\0\0Meta\0\0\0\0\0\0\0\0'

    data = fp.read(META_SIZE - 16) # Skip meta

    data = fp.read(512 + EXTRA_SIZE)
    assert frame(data) == Frame(512 + EXTRA_SIZE, 0, 0)
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
    assert frame(data) == Frame(-1, 0, 0)
    fp.seek(1024)

    data = fp.read(5)
    assert data == b'\x05\0\0\0\x80'

    data = fp.read(512 + EXTRA_SIZE)
    assert frame(data) == Frame(512 + EXTRA_SIZE, 10, 1)
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
