#!/usr/bin/env python
# vim: sts=4 sw=4 et

from .scheme cimport *
from .s2b cimport *
from libc.stdint cimport int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t, uint32_t, uint64_t
from libc.string cimport memcpy, memset
from libc.errno cimport EINVAL, EMSGSIZE
from cython cimport typeof
from cpython.version cimport PY_MAJOR_VERSION

from collections import OrderedDict, namedtuple
import copy
from decimal import Decimal, DecimalException
import enum
from .error import TLLError, wrap as error_wrap
from .buffer cimport *
from . import chrono
from . import bits as B
from . import decimal128
from . cimport decimal128

class Type(enum.Enum):
    Int8 = TLL_SCHEME_FIELD_INT8
    Int16 = TLL_SCHEME_FIELD_INT16
    Int32 = TLL_SCHEME_FIELD_INT32
    Int64 = TLL_SCHEME_FIELD_INT64
    UInt8 = TLL_SCHEME_FIELD_UINT8
    UInt16 = TLL_SCHEME_FIELD_UINT16
    UInt32 = TLL_SCHEME_FIELD_UINT32
    UInt64 = TLL_SCHEME_FIELD_UINT64
    Double = TLL_SCHEME_FIELD_DOUBLE
    Decimal128 = TLL_SCHEME_FIELD_DECIMAL128
    Bytes = TLL_SCHEME_FIELD_BYTES
    Message = TLL_SCHEME_FIELD_MESSAGE
    Array = TLL_SCHEME_FIELD_ARRAY
    Pointer = TLL_SCHEME_FIELD_POINTER
    Union = TLL_SCHEME_FIELD_UNION
_Type = Type

class SubType(enum.Enum):
    NONE = TLL_SCHEME_SUB_NONE
    Enum = TLL_SCHEME_SUB_ENUM
    ByteString = TLL_SCHEME_SUB_BYTE_STRING
    FixedPoint = TLL_SCHEME_SUB_FIXED_POINT
    TimePoint = TLL_SCHEME_SUB_TIME_POINT
    Duration = TLL_SCHEME_SUB_DURATION
    Bits = TLL_SCHEME_SUB_BITS

_time_resolution_map = {
    TLL_SCHEME_TIME_NS: chrono.Resolution.ns,
    TLL_SCHEME_TIME_US: chrono.Resolution.us,
    TLL_SCHEME_TIME_MS: chrono.Resolution.ms,
    TLL_SCHEME_TIME_SECOND: chrono.Resolution.second,
    TLL_SCHEME_TIME_MINUTE: chrono.Resolution.minute,
    TLL_SCHEME_TIME_HOUR: chrono.Resolution.hour,
    TLL_SCHEME_TIME_DAY: chrono.Resolution.day,
}

class OffsetPtrVersion(enum.Enum):
    Default = TLL_SCHEME_OFFSET_PTR_DEFAULT
    LegacyShort = TLL_SCHEME_OFFSET_PTR_LEGACY_SHORT
    LegacyLong = TLL_SCHEME_OFFSET_PTR_LEGACY_LONG

cdef class Options(dict):
    @staticmethod
    cdef Options wrap(tll_scheme_option_t * ptr):
        r = Options()
        while ptr != NULL:
            r[b2s(ptr.name)] = b2s(ptr.value)
            ptr = ptr.next
        return r

class Enum(OrderedDict):
    pass

class EqEnum(enum.Enum):
    def __eq__(self, rhs):
        if type(self) == type(rhs):
            return self.value == rhs.value
        if isinstance(rhs, enum.Enum):
            return self.__class__.__name__ == rhs.__class__.__name__ and self.name == rhs.name
        return False

    def __ne__(self, rhs):
        return not self.__eq__(rhs)

cdef enum_wrap(tll_scheme_enum_t * ptr):
    r = Enum()
    r.options = Options.wrap(ptr.options)
    r.type = Type(ptr.type)
    r.name = b2s(ptr.name)
    r.name_bytes = ptr.name
    #r.values = {}
    cdef tll_scheme_enum_value_t * e = ptr.values
    while e != NULL:
        r[b2s(e.name)] = e.value
        e = e.next
    r.klass = EqEnum(r.name, r)
    return r

class UnionBase:
    __slots__ = ['type', 'value']

    def __init__(self, t, v):
        self.type = t
        self.value = v

    def __repr__(self):
        return f"<Union {self.__class__.__name__} type={self.type} value={self.value!r}>"

    def __getitem__(self, key):
        if key == 0:
            return self.type
        elif key == 1:
            return self.value
        raise KeyError(f"Invalid key: {key}")

    def __getattr__(self, key):
        if key == self.type:
            return self.value
        raise AttributeError(f"Invalid attribute: {key}")

class Union(OrderedDict):
    pass

cdef union_wrap(Scheme s, object m, tll_scheme_union_t * ptr):
    r = Union()
    r.options = Options.wrap(ptr.options)
    r.name = b2s(ptr.name)
    r.name_bytes = ptr.name
    r.type_ptr = field_wrap(s, m, ptr.type_ptr)
    r.union_size = ptr.union_size
    r.fields = []
    for i in range(ptr.fields_size):
        f = field_wrap(s, m, &ptr.fields[i])
        r[f.name] = f
        f.union_index = i
        r.fields.append(f)
    class K(UnionBase):
        pass
    K.__name__ = r.name
    r.klass = K
    return r

class Bits(OrderedDict):
    pass

cdef bits_wrap(tll_scheme_bits_t * ptr):
    r = Bits()
    r.options = Options.wrap(ptr.options)
    r.name = b2s(ptr.name)
    r.name_bytes = ptr.name
    r.type = Type(ptr.type)
    r.size = ptr.size
    r.fields = []
    cdef tll_scheme_bit_field_t * bf = ptr.values
    while bf != NULL:
        name = b2s(bf.name)
        v = B.BitField(name, bf.size, bf.offset)
        r[name] = v
        r.fields.append(v)
        bf = bf.next
    @B.fill_properties
    class Klass(B.Bits):
        BITS = r
    Klass.__name__ = r.name
    r.klass = Klass
    return r

class Import:
    pass

cdef import_wrap(tll_scheme_import_t * ptr):
    r = Import()
    r.options = Options.wrap(ptr.options)
    r.name = b2s(ptr.name)
    r.url = b2s(ptr.url)
    if ptr.filename:
        r.filename = b2s(ptr.filename)
    else:
        r.filename = None
    return r

def memoryview_check(o):
    if not PyMemoryView_Check(o):
        raise TLLError("Need memoryview to pack, got {}".format(type(o)))

def decimal_value_check(f, value):
    try:
        return f(value)
    except DecimalException as e:
        raise ValueError(f"Invalid decimal value: '{value}'") from e

ctypedef fused primitive_t:
    int8_t
    int16_t
    int32_t
    int64_t
    uint8_t
    uint16_t
    uint32_t
    uint64_t
    double

cdef struct offset_ptr_t:
    unsigned offset
    int size
    unsigned entity

cdef offset_ptr_t OFFSET_PTR_INVALID
OFFSET_PTR_INVALID.offset = 0
OFFSET_PTR_INVALID.size = -1
OFFSET_PTR_INVALID.entity = 0

cdef Py_buffer pybuf_copy(const Py_buffer * buf):
    cdef Py_buffer r
    memcpy(&r, buf, sizeof(r))
    return r

cdef Py_buffer * pybuf_shift(Py_buffer * buf, size_t offset):
    if buf.len < offset:
        buf.len = 0
    else:
        buf.len -= offset
        buf.buf = offset + <char *>buf.buf
    return buf

cdef Py_buffer * pybuf_crop(Py_buffer * buf, size_t size):
    buf.len = size
    return buf

cdef int pack_fused(primitive_t v, object dest):
    if not PyMemoryView_Check(dest): return EINVAL
    cdef Py_buffer * buf = PyMemoryView_GET_BUFFER(dest)
    if buf.len < <ssize_t>(sizeof(v)): return EMSGSIZE
#        raise TLLError("Dest buffer too small: {} < {}".format(buf.len, sizeof(typeof(v))))
    memcpy(buf.buf, &v, sizeof(v))
    #(<typeof(v) *>buf.buf)[0] = v;

cdef primitive_t unpack_fused(primitive_t v, const Py_buffer * buf):
    if buf.len < <ssize_t>(sizeof(v)): return 0 #EMSGSIZE
#        raise TLLError("Dest buffer too small: {} < {}".format(buf.len, sizeof(typeof(v))))
    memcpy(&v, buf.buf, sizeof(v))
    return v
    #(<typeof(v) *>buf.buf)[0] = v;

cdef offset_ptr_t read_optr_default(const Py_buffer * buf, unsigned entity):
    if buf.len < <ssize_t>(sizeof(tll_scheme_offset_ptr_t)): return OFFSET_PTR_INVALID
    cdef tll_scheme_offset_ptr_t * ptr = <tll_scheme_offset_ptr_t *>buf.buf
    cdef offset_ptr_t r
    cdef unsigned offset = ptr.offset
    r.size = ptr.size
    r.entity = ptr.entity
    if ptr.entity == 0xff:
        r.entity = (<uint32_t *>((<char *> ptr) + offset))[0]
        offset += 4
    r.offset = offset
    return r

cdef offset_ptr_t read_optr_legacy_short(const Py_buffer * buf, unsigned entity):
    if buf.len < <ssize_t>(sizeof(tll_scheme_offset_ptr_legacy_short_t)): return OFFSET_PTR_INVALID
    cdef tll_scheme_offset_ptr_legacy_short_t * ptr = <tll_scheme_offset_ptr_legacy_short_t *>buf.buf
    cdef offset_ptr_t r
    r.offset = ptr.offset
    r.size = ptr.size
    r.entity = entity
    return r

cdef offset_ptr_t read_optr_legacy_long(const Py_buffer * buf, unsigned entity):
    if buf.len < <ssize_t>(sizeof(tll_scheme_offset_ptr_legacy_long_t)): return OFFSET_PTR_INVALID
    cdef tll_scheme_offset_ptr_legacy_long_t * ptr = <tll_scheme_offset_ptr_legacy_long_t *>buf.buf
    cdef offset_ptr_t r
    r.offset = ptr.offset
    r.size = ptr.size
    r.entity = ptr.entity
    return r

cdef offset_ptr_t read_optr(const Py_buffer * buf, int optr_type, unsigned entity):
    if optr_type == TLL_SCHEME_OFFSET_PTR_DEFAULT:
        return read_optr_default(buf, entity)
    elif optr_type == TLL_SCHEME_OFFSET_PTR_LEGACY_SHORT:
        return read_optr_legacy_short(buf, entity)
    elif optr_type == TLL_SCHEME_OFFSET_PTR_LEGACY_LONG:
        return read_optr_legacy_long(buf, entity)

cdef int write_optr_default(Py_buffer * buf, offset_ptr_t * ptr, object tail):
    if buf.len < <ssize_t>(sizeof(tll_scheme_offset_ptr_t)): return EMSGSIZE
    cdef tll_scheme_offset_ptr_t * r = <tll_scheme_offset_ptr_t *>buf.buf
    cdef unsigned char entity[4]
    r.offset = ptr.offset
    r.size = ptr.size
    if ptr.entity >= 0xff:
        r.entity = 0xff
        (<uint32_t *>entity)[0] = ptr.entity
        tail += entity[:4]
    else:
        r.entity = ptr.entity
    return 0

cdef int write_optr_legacy_short(Py_buffer * buf, offset_ptr_t * ptr):
    if buf.len < <ssize_t>(sizeof(tll_scheme_offset_ptr_legacy_short_t)): return EMSGSIZE
    cdef tll_scheme_offset_ptr_legacy_short_t * r = <tll_scheme_offset_ptr_legacy_short_t *>buf.buf
    r.offset = ptr.offset
    r.size = ptr.size
    return 0

cdef int write_optr_legacy_long(Py_buffer * buf, offset_ptr_t * ptr):
    if buf.len < <ssize_t>(sizeof(tll_scheme_offset_ptr_legacy_long_t)): return EMSGSIZE
    cdef tll_scheme_offset_ptr_legacy_long_t * r = <tll_scheme_offset_ptr_legacy_long_t *>buf.buf
    r.offset = ptr.offset
    r.size = ptr.size
    r.entity = ptr.entity
    return 0

cdef int write_optr(object src, int optr_type, offset_ptr_t * ptr, object tail):
    if not PyMemoryView_Check(src): return EINVAL
    cdef Py_buffer * buf = PyMemoryView_GET_BUFFER(src)
    if optr_type == TLL_SCHEME_OFFSET_PTR_DEFAULT:
        return write_optr_default(buf, ptr, tail)
    elif optr_type == TLL_SCHEME_OFFSET_PTR_LEGACY_SHORT:
        return write_optr_legacy_short(buf, ptr)
    elif optr_type == TLL_SCHEME_OFFSET_PTR_LEGACY_LONG:
        return write_optr_legacy_long(buf, ptr)

cdef class FBase:
    def __init__(self, f): pass

    cdef pack(FBase self, v, dest, tail, int tail_offset):
        pass
    cdef unpack(FBase self, const Py_buffer * src):
        pass
    cdef convert(FBase self, v):
        return v
    cdef from_string(FBase self, str s):
        raise NotImplementedError("Conversion from string not implemented")
    cdef reflection(FBase self, const Py_buffer * src):
        return self.unpack(src)
    cdef as_dict(self, v):
        return v

_TYPES = {}
cdef class FInt8(FBase):
    default = int
    cdef pack(FInt8 self, v, dest, tail, int tail_offset): return pack_fused(<int8_t>v, dest)
    cdef unpack(FInt8 self, const Py_buffer * src): return unpack_fused(<int8_t>0, src)
    cdef convert(FInt8 self, v): return <int8_t>v
    cdef from_string(FInt8 self, str s): return int(s, 0)
_TYPES[Type.Int8] = FInt8

cdef class FInt16(FBase):
    default = int
    cdef pack(FInt16 self, v, dest, tail, int tail_offset): return pack_fused(<int16_t>v, dest)
    cdef unpack(FInt16 self, const Py_buffer * src): return unpack_fused(<int16_t>0, src)
    cdef convert(FInt16 self, v): return <int16_t>v
    cdef from_string(FInt16 self, str s): return int(s, 0)
_TYPES[Type.Int16] = FInt16

cdef class FInt32(FBase):
    default = int
    cdef pack(FInt32 self, v, dest, tail, int tail_offset): return pack_fused(<int32_t>v, dest)
    cdef unpack(FInt32 self, const Py_buffer * src): return unpack_fused(<int32_t>0, src)
    cdef convert(FInt32 self, v): return <int32_t>v
    cdef from_string(FInt32 self, str s): return int(s, 0)
_TYPES[Type.Int32] = FInt32

cdef class FInt64(FBase):
    default = int
    cdef pack(FInt64 self, v, dest, tail, int tail_offset): return pack_fused(<int64_t>v, dest)
    cdef unpack(FInt64 self, const Py_buffer * src): return unpack_fused(<int64_t>0, src)
    cdef convert(FInt64 self, v): return <int64_t>v
    cdef from_string(FInt64 self, str s): return int(s, 0)
_TYPES[Type.Int64] = FInt64

cdef class FUInt8(FBase):
    default = int
    cdef pack(FUInt8 self, v, dest, tail, int tail_offset): return pack_fused(<uint8_t>v, dest)
    cdef unpack(FUInt8 self, const Py_buffer * src): return unpack_fused(<uint8_t>0, src)
    cdef convert(FUInt8 self, v): return <uint8_t>v
    cdef from_string(FUInt8 self, str s): return int(s, 0)
_TYPES[Type.UInt8] = FUInt8

cdef class FUInt16(FBase):
    default = int
    cdef pack(FUInt16 self, v, dest, tail, int tail_offset): return pack_fused(<uint16_t>v, dest)
    cdef unpack(FUInt16 self, const Py_buffer * src): return unpack_fused(<uint16_t>0, src)
    cdef convert(FUInt16 self, v): return <uint16_t>v
    cdef from_string(FUInt16 self, str s): return int(s, 0)
_TYPES[Type.UInt16] = FUInt16

cdef class FUInt32(FBase):
    default = int
    cdef pack(FUInt32 self, v, dest, tail, int tail_offset): return pack_fused(<uint32_t>v, dest)
    cdef unpack(FUInt32 self, const Py_buffer * src): return unpack_fused(<uint32_t>0, src)
    cdef convert(FUInt32 self, v): return <uint32_t>v
    cdef from_string(FUInt32 self, str s): return int(s, 0)
_TYPES[Type.UInt32] = FUInt32

cdef class FUInt64(FBase):
    default = int
    cdef pack(FUInt64 self, v, dest, tail, int tail_offset): return pack_fused(<uint64_t>v, dest)
    cdef unpack(FUInt64 self, const Py_buffer * src): return unpack_fused(<uint64_t>0, src)
    cdef convert(FUInt64 self, v): return <uint64_t>v
    cdef from_string(FUInt64 self, str s): return int(s, 0)
_TYPES[Type.UInt64] = FUInt64

cdef class FDouble(FBase):
    default = float
    cdef pack(FDouble self, v, dest, tail, int tail_offset): return pack_fused(<double>v, dest)
    cdef unpack(FDouble self, const Py_buffer * src): return unpack_fused(<double>0, src)
    cdef convert(FDouble self, v): return float(v)
    cdef from_string(FDouble self, str s): return float(s)
_TYPES[Type.Double] = FDouble

cdef class FDecimal128(FBase):
    default = lambda: decimal128.context.create_decimal(0)

    cdef pack(FDecimal128 self, v, dest, tail, int tail_offset):
        b = decimal128.pack(v)
        return pack_bytes(b, dest, tail, tail_offset)
    cdef unpack(FDecimal128 self, const Py_buffer * src):
        return decimal128.unpack_buf(src)
    cdef convert(FDecimal128 self, v):
        return decimal_value_check(decimal128.context.create_decimal, v)
    cdef from_string(FDecimal128 self, str s):
        return decimal_value_check(decimal128.context.create_decimal, s)
_TYPES[Type.Decimal128] = FDecimal128

cdef class FBytes(FBase):
    default = bytes

    cdef int size

    def __init__(self, f):
        self.size = f.size

    cdef pack(FBytes self, v, dest, tail, int tail_offset): return pack_bytes(v, dest, tail, tail_offset)
    cdef unpack(FBytes self, const Py_buffer * src):
        cdef Py_buffer buf = pybuf_copy(src)
        return unpack_bytes(pybuf_crop(&buf, self.size))
    cdef convert(FBytes self, v):
        if isinstance(v, str):
            v = v.encode('utf-8')
        elif not isinstance(v, bytes):
            raise TypeError(f"Expected bytes, got {type(v)}: {v}")
        if len(v) > self.size:
            raise ValueError(f"Too many bytes: {len(v)} > {self.size}")
        return v
    cdef from_string(FBytes self, str s): return s.encode('utf-8')
_TYPES[Type.Bytes] = FBytes

cdef class FArray(FBase):
    cdef int count
    cdef CField type_array
    cdef CField count_ptr
    cdef object default

    def __init__(self, f):
        self.count = f.count
        self.count_ptr = f.count_ptr
        self.type_array = f.type_array
        self.default = f.list

    cdef pack(FArray self, v, dest, tail, int tail_offset):
        self.count_ptr.pack_data(len(v), dest[:self.count_ptr.size], b'', 0)
        off = self.type_array.offset
        for e in v[:self.count]:
            self.type_array.pack_data(e, dest[off:off + self.type_array.size], tail, tail_offset - off)
            off += self.type_array.size

    cdef _unpack(FArray self, const Py_buffer * src, object (*f)(CField, const Py_buffer *)):
        r = self.default()
        cdef int i = 0
        off = self.type_array.offset
        cdef int size = self.count_ptr.unpack_data(src)
        cdef Py_buffer buf = pybuf_copy(src)
        pybuf_shift(&buf, self.type_array.offset)
        while i < size:
            list.append(r, f(self.type_array, &buf))
            pybuf_shift(&buf, self.type_array.size)
            i += 1
        return r

    cdef unpack(FArray self, const Py_buffer * src): return self._unpack(src, self.type_array.unpack_data)
    cdef reflection(FArray self, const Py_buffer * src): return self._unpack(src, self.type_array._unpack_reflection)

    cdef convert(FArray self, l):
        if not isinstance(l, (tuple, list)):
            raise TypeError("Invalid type for list: {}".format(type(l)))
        if len(l) > self.count:
            raise ValueError("List too large: {} > {}".format(len(l), self.count))
        return self.default(l)

    cdef as_dict(self, v):
        return [self.type_array.as_dict(x) for x in v]

_TYPES[Type.Array] = FArray

cdef class FPointer(FBase):
    cdef int version
    cdef int size
    cdef CField type_ptr
    cdef object default

    def __init__(self, f):
        self.version = f.offset_ptr_version.value
        self.size = f.type_ptr.size
        self.type_ptr = f.type_ptr
        self.default = f.list

    cdef pack(FPointer self, v, dest, tail, int tail_offset):
        cdef offset_ptr_t ptr
        ptr.offset = tail_offset + len(tail)
        ptr.size = len(v)
        ptr.entity = self.size
        if write_optr(dest, self.version, &ptr, tail):
            return None
        b = bytearray(len(v) * ptr.entity)
        view = memoryview(b)
        tnew = bytearray()
        off = 0
        for i in v:
            self.type_ptr.pack_data(i, view[off:off + ptr.entity], tnew, len(b) - off)
            off += ptr.entity
        tail.extend(b)
        tail.extend(tnew)

    cdef _unpack(FPointer self, const Py_buffer * src, object (*f)(CField, const Py_buffer *buf)):
        cdef offset_ptr_t ptr = read_optr(src, self.version, self.size)
        if ptr.size < 0: return None
        r = self.default()
        if ptr.size == -1:
            raise ValueError("Truncated offset ptr")
        if ptr.size == 0:
            return r
        if ptr.offset + ptr.entity * ptr.size > src.len: # len(src):
            raise ValueError("Truncated list (size {}): end {} out of buffer size {}".format(ptr.size, ptr.offset + ptr.entity * ptr.size, src.len))
        cdef int i = 0
        cdef Py_buffer buf = pybuf_copy(src)
        pybuf_shift(&buf, ptr.offset)
        while i < ptr.size:
            list.append(r, f(self.type_ptr, &buf))
            pybuf_shift(&buf, ptr.entity)
            i += 1
        return r

    cdef unpack(FPointer self, const Py_buffer * src): return self._unpack(src, self.type_ptr.unpack_data)
    cdef reflection(FPointer self, const Py_buffer * src): return self._unpack(src, self.type_ptr._unpack_reflection)

    cdef convert(FPointer self, l):
        if not isinstance(l, (tuple, list)):
            raise TypeError("Invalid type for list: {}".format(type(l)))
        return self.default(l)

    cdef as_dict(self, v):
        return [self.type_ptr.as_dict(x) for x in v]
_TYPES[Type.Pointer] = FPointer

cdef class FMessage(FBase):
    cdef object type_msg
    cdef object default

    def __init__(self, f):
        self.type_msg = f.type_msg
        self.default = self.type_msg.klass

    cdef pack(FMessage self, v, dest, tail, int tail_offset):
        return self.type_msg.pack(v, dest, tail, tail_offset)

    cdef unpack(FMessage self, const Py_buffer * src):
        return unpack_message(self.type_msg, src, self.default())
    cdef reflection(FMessage self, const Py_buffer * src):
        return self.type_msg.reflection(PyMemoryView_FromBuffer(src))

    cdef convert(FMessage self, v):
        if isinstance(v, dict):
            return self.type_msg.object(**v)
        if isinstance(v, Data):
            if v.SCHEME != self.type_msg:
                raise TypeError("Can not convert message {} to {}".format(v.SCHEME.name, self.type_msg.name))
            return v
        raise TypeError("Can not convert {} to message".format(type(v)))

    cdef as_dict(self, v):
        return _as_dict_msg(self.type_msg, v)
_TYPES[Type.Message] = FMessage

cdef class FUnion(FBase):
    cdef object type_union
    cdef CField type_ptr
    cdef object default

    def __init__(self, f):
        self.type_union = f.type_union
        self.type_ptr = f.type_union.type_ptr
        self.default = lambda: None

    cdef pack(FUnion self, v, dest, tail, int tail_offset):
        f = self.type_union[v[0]]
        self.type_ptr.pack_data(f.union_index, dest[:self.type_ptr.size], b'', 0)
        return f.pack_data(v[1], dest[f.offset:f.offset + f.size], tail, tail_offset - f.offset)

    cdef unpack(FUnion self, const Py_buffer * src):
        idx = self.type_ptr.unpack_data(src)
        if idx < 0 or idx > len(self.type_union.fields):
            raise ValueError(f"Invalid union index: {idx}")
        cdef CField f = self.type_union.fields[idx]
        cdef Py_buffer buf = pybuf_copy(src)
        return self.type_union.klass(f.name, f.unpack_data(pybuf_shift(&buf, f.offset)))
    """
    cdef reflection(FUnion self, const Py_buffer * src):
        return self.type_msg.reflection(src)
    """

    cdef convert(FUnion self, v):
        if isinstance(v, dict):
            items = v.items()
            if len(items) != 1:
                raise TypeError(f"Invalid union dict: {v}")
            k, v = list(items)[0]
        elif isinstance(v, tuple):
            if len(v) != 2:
                raise TypeError(f"Invalid union tuple: {v}")
            k, v = v
        else:
            raise TypeError(f"Can not convert {type(v)} to union")
        f = self.type_union.get(k, None)
        if f is None:
            raise TypeError(f"Invalid union type value: {k}")
        return self.type_union.klass(k, f.convert(v))

    cdef as_dict(self, v):
        f = self.type_union[v[0]]
        return {v.type: f.as_dict(v.value)}
_TYPES[Type.Union] = FUnion

_SUBTYPES = {}

cdef class FEnum(FBase):
    cdef object default
    cdef FBase base
    cdef object enum_class

    def __init__(self, f):
        self.base = f.impl
        self.enum_class = f.type_enum.klass
        self.default = lambda: f.type_enum.klass(0) # XXX: What is default value for enum?

    cdef pack(FEnum self, v, dest, tail, int tail_offset):
        return self.base.pack(v.value, dest, tail, tail_offset)
    cdef unpack(FEnum self, const Py_buffer * src):
        return self.enum_class(self.base.unpack(src))
    cdef convert(FEnum self, v):
        if isinstance(v, str):
            return self.enum_class.__members__[v]
        elif isinstance(v, enum.Enum):
            if isinstance(v, self.enum_class):
                return v
            if self.enum_class.__name__ != v.__class__.__name__:
                raise ValueError(f"Enum name of '{v}' does not match field enum name '{self.enum_class.__name__}'")
            r = self.enum_class.__members__.get(v.name, None)
            if r is None:
                raise ValueError(f"Enum {self.enum_class} has no field '{v.name}'")
            return r
        return self.enum_class(v)
    cdef from_string(FEnum self, str s):
        v = self.enum_class.__members__.get(s, None)
        if v is not None:
            return v
        try:
            return self.enum_class(int(s))
        except:
            raise ValueError(f"Value {s} is not {self.enum_class} name or int value")
_SUBTYPES[SubType.Enum] = FEnum

cdef class FFixed(FBase):
    default = Decimal
    cdef FBase base
    cdef int fixed_precision
    cdef object limits_max
    cdef object limits_min

    def __init__(self, f):
        self.base = f.impl
        self.fixed_precision = f.fixed_precision
        self.limits_max = f.limits_max
        self.limits_min = f.limits_min

    cdef pack(FFixed self, v, dest, tail, int tail_offset):
        return self.base.pack(v.scaleb(self.fixed_precision).to_integral_value(), dest, tail, tail_offset)
    cdef unpack(FFixed self, const Py_buffer * src):
        return Decimal(self.base.unpack(src)) * Decimal((0, (1,), -self.fixed_precision))
    cdef convert(FFixed self, v):
        if isinstance(v, int):
            v = Decimal(v)
        elif isinstance(v, str):
            v = decimal_value_check(Decimal, v)
        elif isinstance(v, float):
            v = Decimal(round(v * (10 ** self.fixed_precision))).scaleb(-self.fixed_precision)
        elif not isinstance(v, Decimal):
            raise TypeError("Expected str, float or int, got {}: {}".format(type(v), v))
        if v > self.limits_max:
            raise ValueError(f"Value {v} > max {self.limits_max}")
        elif v < self.limits_min:
            raise ValueError(f"Value {v} < min {self.limits_min}")
        return v
    cdef from_string(FFixed self, str s): return decimal_value_check(Decimal, s)
_SUBTYPES[SubType.FixedPoint] = FFixed

cdef class FTimePoint(FBase):
    default = chrono.TimePoint
    cdef FBase base
    cdef object time_resolution

    def __init__(self, f):
        self.base = f.impl
        self.time_resolution = f.time_resolution

    cdef pack(FTimePoint self, v, dest, tail, int tail_offset):
        return self.base.pack(chrono.TimePoint(v, self.time_resolution, type=self.base.default).value, dest, tail, tail_offset)
    cdef unpack(FTimePoint self, const Py_buffer * src):
        return chrono.TimePoint(self.base.unpack(src), self.time_resolution, type=self.base.default)
    cdef convert(FTimePoint self, v):
        if isinstance(v, str):
            v = chrono.TimePoint.from_str(v)
        if isinstance(v, chrono.TimePoint):
            return v.convert(self.time_resolution, self.base.default)
        return chrono.TimePoint(chrono.TimePoint(v), self.time_resolution, type=self.base.default)
    cdef from_string(FTimePoint self, str s):
        return self.convert(chrono.TimePoint.from_str(s))
_SUBTYPES[SubType.TimePoint] = FTimePoint

cdef class FDuration(FBase):
    default = chrono.Duration
    cdef FBase base
    cdef object time_resolution

    def __init__(self, f):
        self.base = f.impl
        self.time_resolution = f.time_resolution

    cdef pack(FDuration self, v, dest, tail, int tail_offset):
        return self.base.pack(chrono.Duration(v, self.time_resolution, type=self.base.default).value, dest, tail, tail_offset)
    cdef unpack(FDuration self, const Py_buffer * src):
        return chrono.Duration(self.base.unpack(src), self.time_resolution, type=self.base.default)
    cdef convert(FDuration self, v):
        if isinstance(v, str):
            v = chrono.Duration.from_str(v)
        else:
            v = chrono.Duration(v)
        return chrono.Duration(v, self.time_resolution, type=self.base.default)
    cdef from_string(FDuration self, str s):
        return self.convert(chrono.Duration.from_str(s))
_SUBTYPES[SubType.Duration] = FDuration

cdef class FBits(FBase):
    cdef FBase base
    cdef object default

    def __init__(self, f):
        self.base = f.impl
        self.default = f.type_bits.klass

    cdef pack(FBits self, v, dest, tail, int tail_offset):
        return self.base.pack(v._value, dest, tail, tail_offset)
    cdef unpack(FBits self, const Py_buffer * src):
        return self.default(self.base.unpack(src))
    cdef convert(FBits self, v):
        if isinstance(v, str):
            return B.from_str(self.default, v)
        return self.default(v)
    cdef from_string(FBits self, str s):
        return B.from_str(self.default, s)
    cdef as_dict(self, v):
        return {f.name: getattr(v, f.name) for f in v.BITS.values()}

_SUBTYPES[SubType.Bits] = FBits

cdef class FFixedString(FBase):
    default = str

    cdef FBase base
    cdef int size

    def __init__(self, f):
        self.base = f.impl
        self.size = f.size

    cdef pack(FFixedString self, v, dest, tail, int tail_offset): return pack_bytes(v, dest, tail, tail_offset)
    cdef unpack(FFixedString self, const Py_buffer * src):
        cdef Py_buffer buf = pybuf_copy(src)
        return unpack_str(pybuf_crop(&buf, self.size))
    cdef convert(FFixedString self, v):
        v = convert_str(v)
        if len(v) > self.size:
            raise ValueError(f"String too long: {len(v)} > {self.size}")
        return v
    cdef from_string(FFixedString self, str s): return s
_SUBTYPES[(Type.Bytes, SubType.ByteString)] = FFixedString

cdef class FVString(FBase):
    default = str

    cdef FBase base
    cdef int version

    def __init__(self, f):
        self.base = f.impl
        self.version = f.offset_ptr_version.value

    cdef pack(FVString self, v, dest, tail, int tail_offset):
        b = s2b(v)
        cdef offset_ptr_t ptr
        ptr.offset = tail_offset + len(tail)
        ptr.size = len(b) + 1
        ptr.entity = 1
        if write_optr(dest, self.version, &ptr, None):
            return None
        tail.extend(b + b'\0')

    cdef unpack(FVString self, const Py_buffer * src):
        return unpack_vstring(src, self.version)
    cdef convert(FVString self, v): return convert_str(v)
    cdef from_string(FVString self, str s): return s
_SUBTYPES[(Type.Pointer, SubType.ByteString)] = FVString

cdef pack_bytes(v, dest, tail, tail_offset):
    if not PyMemoryView_Check(dest): return EINVAL
    cdef Py_buffer * buf = PyMemoryView_GET_BUFFER(dest)
    if not isinstance(v, memoryview):
        v = memoryview(s2b(v))
    cdef Py_buffer * inbuf = PyMemoryView_GET_BUFFER(v)
    if buf.len < inbuf.len:
        raise ValueError(f"Dest buffer too small: {buf.len} < {inbuf.len}")
    memcpy(buf.buf, inbuf.buf, inbuf.len)
    #if inbuf.len < buf.len:
    #    memset(buf.buf + inbuf.len, 0, buf.len - inbuf.len)

cdef unpack_bytes(const Py_buffer * buf):
    return bytearray((<char *>buf.buf)[:buf.len]) #<char *>buf.buf, buf.len)

cdef unpack_str(const Py_buffer * buf):
    cdef int l = strnlen(<char *>buf.buf, buf.len)
    return b2s((<char *>buf.buf)[:l])

cdef unpack_vstring(const Py_buffer * src, int optr_version):
    cdef offset_ptr_t ptr = read_optr(src, optr_version, 1)
    if ptr.size < 0: return None
    if ptr.size == 0:
        return "" #src[sizeof(tll_scheme_offset_ptr_t):sizeof(tll_scheme_offset_ptr_t)]
    if ptr.offset + ptr.size > src.len:
        raise ValueError(f"Offset string at {ptr.offset}+{ptr.size} out of tail size {src.len}")
    cdef char * r = <char *>src.buf
    return str(r[ptr.offset:ptr.offset + ptr.size - 1], encoding='utf-8')

def convert_str(v):
    if isinstance(v, bytes):
        return v.decode('utf-8')
    elif not isinstance(v, str):
        raise TypeError(f"Expected str, got {type(v)}: {v}")
    return v

def from_string_int(v): return int(v, 0)

def _as_dict_list(field, v):
    return [field.as_dict(x) for x in v]

def _as_dict_msg(msg, v, only=set(), ignore=set()):
    r = {}
    for f in msg.fields:
        if only and f.name not in only:
            continue
        if f.name in ignore:
            continue
        x = getattr(v, f.name, None)
        if x is None: continue
        r[f.name] = f.as_dict(x)
    return r

cdef class CField:
    cdef public size_t offset
    cdef public size_t size
    cdef public int index
    cdef public FBase impl
    cdef public str name
    cdef public bytes name_bytes
    cdef public CField type_ptr
    cdef public CField type_array
    cdef dict __dict__

    Type = _Type
    Sub = SubType
    def init(self, name, type, sub_type):
        impl = _TYPES.get(type)
        if impl is None:
            raise NotImplementedError(f"Type {type} is not implemented")
        self.impl = impl(self)

        if sub_type != SubType.NONE:
            impl = _SUBTYPES.get((type, sub_type))
            if impl is None:
                impl = _SUBTYPES.get(sub_type)
            if impl is not None:
                #raise NotImplementedError(f"Sub-type {self.sub_type} for type {type} is not implemented")
                self.impl = impl(self)

    def __repr__(self):
        return "<Field {0.name}, type: {0.type}, size: {0.size}, offset: {0.offset}>".format(self)

    def default(self):
        return self.impl.default()

    def convert(self, v):
        return self.impl.convert(v)

    def pack_data(self, v, dest, tail, tail_offset):
        return self.impl.pack(v, dest, tail, tail_offset)

    cdef unpack_data(self, const Py_buffer * src):
        return self.impl.unpack(src)

    def unpack_reflection(self, src):
        return self._unpack_reflection(PyMemoryView_GET_BUFFER(memoryview(src)))

    cdef _unpack_reflection(self, const Py_buffer * src):
        return self.impl.reflection(src)

    def pack(self, v, dest, tail, tail_offset = 0):
        memoryview_check(dest)
        self.pack_data(v, dest[self.offset:self.offset + self.size])

    def unpack(self, src):
        memoryview_check(src)
        return self.unpack_data(PyMemoryView_GET_BUFFER(src[self.offset:self.offset + self.size]))

    def from_string(self, v : str):
        try:
            return self.impl.convert(self.impl.from_string(v))
        except NotImplementedError:
            raise TypeError("Field {} with type {} can not be constructed from string".format(self.name, self.type))

    def as_dict(self, v):
        return self.impl.as_dict(v)

    @property
    def optional(self):
        return self.index >= 0

class Field(CField):
    pass

for t in Type:
    setattr(Field, t.name, t)

cdef object field_wrap(Scheme s, object m, tll_scheme_field_t * ptr):
    cdef tll_scheme_bit_field_t * bits

    r = Field()
    r.name = b2s(ptr.name)
    r.name_bytes = ptr.name
    r.options = Options.wrap(ptr.options)
    r.type = Type(ptr.type)
    r.sub_type = SubType(ptr.sub_type)
    r.index = ptr.index
    if r.type in (Type.Int8, Type.Int16, Type.Int32, Type.Int64):
        r.limits_max = 2 ** (8 * ptr.size - 1) - 1
        r.limits_min = -r.limits_max
    elif r.type in (Type.UInt8, Type.UInt16, Type.UInt32, Type.UInt64):
        r.limits_max = 2 ** (8 * ptr.size) - 1
        r.limits_min = 0
    if r.type == Type.Message:
        r.type_msg = s[ptr.type_msg.name]
    elif r.type == Type.Array:
        r.count = ptr.count
        r.count_ptr = field_wrap(s, m, ptr.count_ptr)
        r.type_array = field_wrap(s, m, ptr.type_array)

        class L(List):
            SCHEME = r.type_array
        L.__name__ = r.name
        r.list = L
    elif r.type == Type.Pointer:
        r.offset_ptr_version = OffsetPtrVersion(ptr.offset_ptr_version)
        r.type_ptr = field_wrap(s, m, ptr.type_ptr)

        class L(List):
            SCHEME = r.type_ptr
        L.__name__ = r.name
        r.list = L
    elif r.type == Type.Union:
        uname = b2s(ptr.type_union.name)
        r.type_union = m.unions.get(uname, s.unions.get(uname, None))
        if r.type_union is None:
            raise TLLError("Failed to build field {}: Union {} not found".format(r.name, uname))
    elif r.sub_type == r.Sub.Enum:
        ename = b2s(ptr.type_enum.name)
        r.type_enum = m.enums.get(ename, s.enums.get(ename, None))
        if r.type_enum is None:
            raise TLLError("Failed to build field {}: Enum {} not found".format(r.name, ename))
    elif r.sub_type == r.Sub.Bits:
        bname = b2s(ptr.type_bits.name)
        r.type_bits = m.bits.get(bname, s.bits.get(bname, None))
        if r.type_bits is None:
            print(m.bits, s.bits)
            raise TLLError("Failed to build field {}: Bits {} not found".format(r.name, bname))
        r.bitfields = r.type_bits # Legacy
    elif r.sub_type == r.Sub.FixedPoint:
        r.fixed_precision = ptr.fixed_precision
        r.limits_max = Decimal(r.limits_max).scaleb(-r.fixed_precision)
        r.limits_min = Decimal(r.limits_min).scaleb(-r.fixed_precision)
    elif r.sub_type == r.Sub.TimePoint or r.sub_type == r.Sub.Duration:
        r.time_resolution = _time_resolution_map[ptr.time_resolution]

    r.size = ptr.size
    r.offset = ptr.offset
    r.init(r.name, r.type, r.sub_type) #b2s(ptr.name), Type(ptr.type))
    return r

class List(list):
    SCHEME = None

    def __init__(self, v = []):
        list.__init__(self, [self.convert(x) for x in v])

    def convert(self, v):
        return self.SCHEME.convert(v)

    def as_dict(self):
        return [self.SCHEME.as_dict(x) for x in self]

    def __add__(self, v):
        return list.__add__(self, self.convert(v))

    def __iadd__(self, v):
        return list.__iadd__(self, [self.convert(x) for x in v])

    def __setitem__(self, i, v):
        return list.__setitem__(self, i, self.convert(v))

    def append(self, v):
        return list.append(self, self.convert(v))

    def extend(self, v):
        return list.extend(self, [self.convert(x) for x in v])

    def insert(self, i, v):
        return list.insert(self, i, self.convert(v))

class Data(object):
    SCHEME = None
    def __init__(self, *a, **kw):
        keys = set()
        if a:
            for k,v in zip(a, self.SCHEME.items()):
                setattr(self, k, v)
                keys.add(k)
        if kw:
            for k,v in kw.items():
                setattr(self, k, v)
                keys.add(k)
        for f in self.SCHEME.fields_init:
            if f.name in keys:
                continue
            if f.type == Type.Message:
                setattr(self, f.name, {})
            elif f.type in (Type.Array, Type.Pointer):
                setattr(self, f.name, [])

    def pack(self):
        tail = bytearray()
        dest = bytearray(self.SCHEME.size)
        self.SCHEME.pack(self, memoryview(dest), tail, len(dest))
        return dest + tail

    def unpack(self, data):
        return self.SCHEME.unpack(memoryview(data), self)

    def __setattr__(self, k, v):
        f = self.SCHEME.get(k, None)
        if f is None:
            raise AttributeError(f"No such field in {self.SCHEME.name}: {k}")
        if v is None:
            if hasattr(self, k):
                object.__delattr__(self, k)
            return
        object.__setattr__(self, k, f.convert(v))

    def __delattr__(self, k):
        f = self.SCHEME.get(k, None)
        if f is None:
            raise AttributeError(f"No such field in {self.SCHEME.name}: {k}")
        if hasattr(self, k):
            object.__delattr__(self, k)

    def __repr__(self):
        return "<{}.Data.{} object at {:x}>".format(self.__module__, self.__class__.__name__, id(self))

    def __str__(self):
        l = []
        for f in self.SCHEME.fields:
            r = getattr(self, f.name, None)
            if r is None: continue
            if isinstance(r, (tuple, list)):
                l.append('{}: [{}]'.format(f.name, ', '.join([str(x) for x in r])))
            elif isinstance(r, str):
                l.append('{}: {}'.format(f.name, repr(r)))
            else:
                l.append('{}: {}'.format(f.name, str(r)))
        return "<{} {}>".format(self.SCHEME.name, ", ".join(l))

    def __copy__(self):
        r = self.SCHEME.object()
        for f in self.SCHEME.fields:
            object.__setattr__(r, f.name, copy.deepcopy(getattr(self, f.name, None)))
        return r

    def __deepcopy__(self, memo):
        return self.__copy__()

    def as_dict(self, **kw):
        return _as_dict_msg(self.SCHEME, self, **kw)

class Reflection(object):
    SCHEME = None
    __slots__ = ['__data']
    def __init__(self, data):
        self.__data = memoryview(data)

    def __getattr__(self, k):
        f = self.SCHEME.get(k, None)
        if f is None:
            raise AttributeError("Field {} not found".format(k))
        return f.unpack_reflection(self.__data[f.offset:])

    def __repr__(self):
        return "<{}.Reflection.{} object at {:x}>".format(self.__module__, self.__class__.__name__, id(self))

    def as_dict(self, **kw):
        return _as_dict_msg(self.SCHEME, self, **kw)

class Message(OrderedDict):
    def __call__(self, *a, **kw):
        return self.klass(*a, **kw)

    def object(self, *a, **kw):
        return self.klass(*a, **kw)

    def pack(self, v, dest, tail, tail_offset):
        memoryview_check(dest)
        pmap = dest[self.pmap.offset:self.pmap.offset + self.pmap.size] if self.pmap else None
        for f in self.fields:
            if f == self.pmap:
                continue
            i = getattr(v, f.name, None)
            if i is None:
                continue
            if pmap and f.index >= 0:
                pmap[f.index // 8] |= (1 << (f.index % 8))
            off = f.offset
            f.pack_data(i, dest[off:off + f.size], tail, tail_offset - off)

    def unpack(self, src, v=None):
        memoryview_check(src)
        v = self.object() if v is None else v
        return unpack_message(self, PyMemoryView_GET_BUFFER(src), v)

cdef unpack_message(object self, const Py_buffer * src, v):
    if src.len < self.size:
        raise ValueError("Buffer size {} less then message size {}".format(src.len, self.size))
    cdef const unsigned char * pmap = NULL
    if self.pmap:
        pmap = <const unsigned char *>src.buf + <size_t>(self.pmap.offset)
    cdef CField f
    cdef Py_buffer buf = pybuf_copy(src)
    for f in self.fields:
        if pmap:
            if f.index >= 0 and pmap[f.index // 8] & (1 << (f.index % 8)) == 0:
                continue
            elif self.pmap.name == f.name:
                continue
        buf.buf = src.buf
        buf.len = src.len
        pybuf_shift(&buf, f.offset)
        r = f.impl.unpack(&buf)
        object.__setattr__(v, f.name, r)
    return v

@staticmethod
cdef object message_wrap(Scheme s, tll_scheme_message_t * ptr):
    r = Message()
    r.name = b2s(ptr.name)
    r.name_bytes = ptr.name
    r.msgid = ptr.msgid
    r.size = ptr.size
    r.options = Options.wrap(ptr.options)
    r.enums = OrderedDict()
    r.unions = OrderedDict()
    r.bits = OrderedDict()
    r.fields_init = []

    class R(Reflection):
        SCHEME = r
    R.__name__ = r.name
    r.reflection = R

    cdef tll_scheme_enum_t * e = ptr.enums
    while e != NULL:
        tmp = enum_wrap(e)
        r.enums[tmp.name] = tmp
        e = e.next

    cdef tll_scheme_union_t * u = ptr.unions
    while u != NULL:
        tmp = union_wrap(s, r, u)
        r.unions[tmp.name] = tmp
        u = u.next

    cdef tll_scheme_bits_t * b = ptr.bits
    while b != NULL:
        tmp = bits_wrap(b)
        r.bits[tmp.name] = tmp
        b = b.next

    cdef tll_scheme_field_t * f = ptr.fields
    while f != NULL:
        tmp = field_wrap(s, r, f)
        r[tmp.name] = tmp
        f = f.next
        if tmp.optional:
            pass
        elif tmp.type == Type.Message:
            r.fields_init.append(tmp)
        elif tmp.type in (Type.Array, Type.Pointer):
            if tmp.sub_type != SubType.ByteString:
                r.fields_init.append(tmp)
    r.fields = list(r.values())

    class D(Data):
        __slots__ = list([i.name for i in r.fields])
        SCHEME = r
    D.__name__ = r.name
    r.klass = D

    if ptr.pmap:
        r.pmap = r[b2s(ptr.pmap.name)]
    else:
        r.pmap = None

    return r

class MessageList(list):
    def __getattr__(self, name):
        for m in self:
            if m.name == name: return m
        raise KeyError(f"Message '{name}' not found")

cdef class Scheme:
    def __init__(self, uri):
        self._ptr = NULL
        self.messages = MessageList()
        if uri is None:
            return
        u = s2b(uri)
        self._ptr = tll_scheme_load(u, len(u))
        if self._ptr == NULL:
            raise RuntimeError("Failed to load scheme")
        self.fill(self._ptr)

    def __dealloc__(self):
        tll_scheme_unref(self._ptr)
        self._ptr = NULL

    def ref(self) -> Scheme:
        """ Create new Scheme object that reference same C API pointer """
        if not self._ptr:
            raise RuntimeError("Uninitialized pointer")
        return Scheme.wrap(tll_scheme_ref(self._ptr))

    def copy(self) -> Scheme:
        """ Create new Scheme object with copy of C API pointer """
        if not self._ptr:
            raise RuntimeError("Uninitialized pointer")
        return Scheme.wrap(tll_scheme_copy(self._ptr))

    @property
    def options(self): return self.options

    @property
    def enums(self): return self.enums

    @property
    def unions(self): return self.unions

    @property
    def bits(self): return self.bits

    @property
    def aliases(self): return self.aliases

    @property
    def imports(self): return self.imports

    @property
    def messages(self): return self.messages

    def dump(self, fmt=None):
        if self._ptr == NULL:
            raise ValueError("Unbound scheme, null pointer")
        bfmt = s2b(fmt) if fmt else None
        cdef const char * f = NULL
        if bfmt:
            f = bfmt
        cdef char * str = tll_scheme_dump(self._ptr, f)
        if str == NULL:
            raise TLLError("Failed to dump scheme")
        b = bytes(str[:])
        return b.decode('utf-8')

    def _find(self, k):
        if isinstance(k, int):
            for m in self.messages:
                if m.msgid == k: return m
        elif isinstance(k, bytes):
            for m in self.messages:
                if m.name_bytes == k: return m
        else:
            for m in self.messages:
                if m.name == k: return m

    def find(self, k):
        r = self._find(k)
        if r is None:
            raise KeyError(f"Message '{k}' not found")
        return r

    def __getitem__(self, k): return self.find(k)
    def __contains__(self, k): return self._find(k) is not None

    @staticmethod
    cdef Scheme wrap(const tll_scheme_t * ptr, int ref = 0):
        s = Scheme(None)
        if ref:
            tll_scheme_ref(ptr)
        s._ptr = <tll_scheme_t *>ptr
        s.fill(ptr)
        return s

    cdef same(Scheme self, const tll_scheme_t *ptr):
        return self._ptr == ptr

    cdef fill(Scheme self, const tll_scheme_t *ptr):
        self.options = Options.wrap(ptr.options)
        self.enums = OrderedDict()
        self.unions = OrderedDict()
        self.bits = OrderedDict()
        self.aliases = OrderedDict()
        self.messages = MessageList()
        self.imports = {}

        cdef tll_scheme_enum_t * e = ptr.enums
        while e != NULL:
            tmp = enum_wrap(e)
            self.enums[tmp.name] = tmp
            e = e.next

        cdef tll_scheme_union_t * u = ptr.unions
        while u != NULL:
            tmp = union_wrap(self, None, u)
            self.unions[tmp.name] = tmp
            u = u.next

        cdef tll_scheme_bits_t * b = ptr.bits
        while b != NULL:
            tmp = bits_wrap(b)
            self.bits[tmp.name] = tmp
            b = b.next

        cdef tll_scheme_field_t * f = ptr.aliases
        while f != NULL:
            tmp = field_wrap(self, Message(), f)
            self.aliases[tmp.name] = tmp
            f = f.next

        cdef tll_scheme_import_t * imp = ptr.imports
        while imp != NULL:
            tmp = import_wrap(imp)
            self.imports[tmp.name] = tmp
            imp = imp.next

        cdef tll_scheme_message_t * m = ptr.messages
        while m != NULL:
            self.messages.append(message_wrap(self, m))
            m = m.next

    def unpack(self, msg):
        if not msg.msgid:
            raise ValueError("No msgid in message")
        m = self.find(msg.msgid)
        return m.unpack(msg.data)

    def reflection(self, msg):
        if not msg.msgid:
            raise ValueError("No msgid in message")
        m = self.find(msg.msgid)
        return m.reflection(msg.data)

class PathMode(enum.Enum):
    User = TLL_SCHEME_PATH_USER
    Env = TLL_SCHEME_PATH_ENV
    Default = TLL_SCHEME_PATH_DEFAULT

def path_add(path, mode = PathMode.User):
    p = s2b(path)
    error_wrap(tll_scheme_path_add(p, len(p), mode.value), f"Failed to add search path {path} with mode {mode}")

def path_remove(path, mode = PathMode.User):
    p = s2b(path)
    error_wrap(tll_scheme_path_remove(p, len(p), mode.value), f"Failed to remove search path {path} with mode {mode}")
