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
from decimal import Decimal
import enum
from .error import TLLError
from .buffer cimport *
from . import chrono
from . import bits as B
from . import decimal128

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
    r.klass = enum.Enum(r.name, r)
    return r

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
    r.klass = namedtuple('Union', ['type', 'value'])
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

def memoryview_check(o):
    if not PyMemoryView_Check(o):
        raise TLLError("Need memoryview to pack, got {}".format(type(o)))

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

cdef int pack_fused(primitive_t v, object dest):
    if not PyMemoryView_Check(dest): return EINVAL
    cdef Py_buffer * buf = PyMemoryView_GET_BUFFER(dest)
    if buf.len < <ssize_t>(sizeof(v)): return EMSGSIZE
#        raise TLLError("Dest buffer too small: {} < {}".format(buf.len, sizeof(typeof(v))))
    memcpy(buf.buf, &v, sizeof(v))
    #(<typeof(v) *>buf.buf)[0] = v;

cdef primitive_t unpack_fused(primitive_t v, object src):
    if not PyMemoryView_Check(src): return 0 #EINVAL
    cdef Py_buffer * buf = PyMemoryView_GET_BUFFER(src)
    if buf.len < <ssize_t>(sizeof(v)): return 0 #EMSGSIZE
#        raise TLLError("Dest buffer too small: {} < {}".format(buf.len, sizeof(typeof(v))))
    memcpy(&v, buf.buf, sizeof(v))
    return v
    #(<typeof(v) *>buf.buf)[0] = v;

cdef offset_ptr_t read_optr_default(Py_buffer * buf, unsigned entity):
    if buf.len < <ssize_t>(sizeof(tll_scheme_offset_ptr_t)): return OFFSET_PTR_INVALID
    cdef tll_scheme_offset_ptr_t * ptr = <tll_scheme_offset_ptr_t *>buf.buf
    cdef offset_ptr_t r
    r.offset = ptr.offset
    r.size = ptr.size
    r.entity = ptr.entity
    return r

cdef offset_ptr_t read_optr_legacy_short(Py_buffer * buf, unsigned entity):
    if buf.len < <ssize_t>(sizeof(tll_scheme_offset_ptr_legacy_short_t)): return OFFSET_PTR_INVALID
    cdef tll_scheme_offset_ptr_legacy_short_t * ptr = <tll_scheme_offset_ptr_legacy_short_t *>buf.buf
    cdef offset_ptr_t r
    r.offset = ptr.offset
    r.size = ptr.size
    r.entity = entity
    return r

cdef offset_ptr_t read_optr_legacy_long(Py_buffer * buf, unsigned entity):
    if buf.len < <ssize_t>(sizeof(tll_scheme_offset_ptr_legacy_long_t)): return OFFSET_PTR_INVALID
    cdef tll_scheme_offset_ptr_legacy_long_t * ptr = <tll_scheme_offset_ptr_legacy_long_t *>buf.buf
    cdef offset_ptr_t r
    r.offset = ptr.offset
    r.size = ptr.size
    r.entity = ptr.entity
    return r

cdef offset_ptr_t read_optr(object src, int optr_type, unsigned entity):
    if not PyMemoryView_Check(src): return OFFSET_PTR_INVALID
    cdef Py_buffer * buf = PyMemoryView_GET_BUFFER(src)
    if optr_type == TLL_SCHEME_OFFSET_PTR_DEFAULT:
        return read_optr_default(buf, entity)
    elif optr_type == TLL_SCHEME_OFFSET_PTR_LEGACY_SHORT:
        return read_optr_legacy_short(buf, entity)
    elif optr_type == TLL_SCHEME_OFFSET_PTR_LEGACY_LONG:
        return read_optr_legacy_long(buf, entity)

cdef int write_optr_default(Py_buffer * buf, offset_ptr_t * ptr):
    if buf.len < <ssize_t>(sizeof(tll_scheme_offset_ptr_t)): return EMSGSIZE
    cdef tll_scheme_offset_ptr_t * r = <tll_scheme_offset_ptr_t *>buf.buf
    r.offset = ptr.offset
    r.size = ptr.size
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

cdef int write_optr(object src, int optr_type,  offset_ptr_t * ptr):
    if not PyMemoryView_Check(src): return EINVAL
    cdef Py_buffer * buf = PyMemoryView_GET_BUFFER(src)
    if optr_type == TLL_SCHEME_OFFSET_PTR_DEFAULT:
        return write_optr_default(buf, ptr)
    elif optr_type == TLL_SCHEME_OFFSET_PTR_LEGACY_SHORT:
        return write_optr_legacy_short(buf, ptr)
    elif optr_type == TLL_SCHEME_OFFSET_PTR_LEGACY_LONG:
        return write_optr_legacy_long(buf, ptr)

cdef class FBase:
    def __init__(self, f): pass

    cdef pack(FBase self, v, dest, tail, int tail_offset):
        pass
    cdef unpack(FBase self, src):
        pass
    cdef convert(FBase self, v):
        return v
    cdef from_string(FBase self, str s):
        raise NotImplementedError("Conversion from string not implemented")
    cdef reflection(FBase self, src):
        return self.unpack(src)
    cdef as_dict(self, v):
        return v

_TYPES = {}
cdef class FInt8(FBase):
    default = int
    cdef pack(FInt8 self, v, dest, tail, int tail_offset): return pack_fused(<int8_t>v, dest)
    cdef unpack(FInt8 self, src): return unpack_fused(<int8_t>0, src)
    cdef convert(FInt8 self, v): return <int8_t>v
    cdef from_string(FInt8 self, str s): return int(s, 0)
_TYPES[Type.Int8] = FInt8

cdef class FInt16(FBase):
    default = int
    cdef pack(FInt16 self, v, dest, tail, int tail_offset): return pack_fused(<int16_t>v, dest)
    cdef unpack(FInt16 self, src): return unpack_fused(<int16_t>0, src)
    cdef convert(FInt16 self, v): return <int16_t>v
    cdef from_string(FInt16 self, str s): return int(s, 0)
_TYPES[Type.Int16] = FInt16

cdef class FInt32(FBase):
    default = int
    cdef pack(FInt32 self, v, dest, tail, int tail_offset): return pack_fused(<int32_t>v, dest)
    cdef unpack(FInt32 self, src): return unpack_fused(<int32_t>0, src)
    cdef convert(FInt32 self, v): return <int32_t>v
    cdef from_string(FInt32 self, str s): return int(s, 0)
_TYPES[Type.Int32] = FInt32

cdef class FInt64(FBase):
    default = int
    cdef pack(FInt64 self, v, dest, tail, int tail_offset): return pack_fused(<int64_t>v, dest)
    cdef unpack(FInt64 self, src): return unpack_fused(<int64_t>0, src)
    cdef convert(FInt64 self, v): return <int64_t>v
    cdef from_string(FInt64 self, str s): return int(s, 0)
_TYPES[Type.Int64] = FInt64

cdef class FUInt8(FBase):
    default = int
    cdef pack(FUInt8 self, v, dest, tail, int tail_offset): return pack_fused(<uint8_t>v, dest)
    cdef unpack(FUInt8 self, src): return unpack_fused(<uint8_t>0, src)
    cdef convert(FUInt8 self, v): return <uint8_t>v
    cdef from_string(FUInt8 self, str s): return int(s, 0)
_TYPES[Type.UInt8] = FUInt8

cdef class FUInt16(FBase):
    default = int
    cdef pack(FUInt16 self, v, dest, tail, int tail_offset): return pack_fused(<uint16_t>v, dest)
    cdef unpack(FUInt16 self, src): return unpack_fused(<uint16_t>0, src)
    cdef convert(FUInt16 self, v): return <uint16_t>v
    cdef from_string(FUInt16 self, str s): return int(s, 0)
_TYPES[Type.UInt16] = FUInt16

cdef class FUInt32(FBase):
    default = int
    cdef pack(FUInt32 self, v, dest, tail, int tail_offset): return pack_fused(<uint32_t>v, dest)
    cdef unpack(FUInt32 self, src): return unpack_fused(<uint32_t>0, src)
    cdef convert(FUInt32 self, v): return <uint32_t>v
    cdef from_string(FUInt32 self, str s): return int(s, 0)
_TYPES[Type.UInt32] = FUInt32

cdef class FUInt64(FBase):
    default = int
    cdef pack(FUInt64 self, v, dest, tail, int tail_offset): return pack_fused(<uint64_t>v, dest)
    cdef unpack(FUInt64 self, src): return unpack_fused(<uint64_t>0, src)
    cdef convert(FUInt64 self, v): return <uint64_t>v
    cdef from_string(FUInt64 self, str s): return int(s, 0)
_TYPES[Type.UInt64] = FUInt64

cdef class FDouble(FBase):
    default = float
    cdef pack(FDouble self, v, dest, tail, int tail_offset): return pack_fused(<double>v, dest)
    cdef unpack(FDouble self, src): return unpack_fused(<double>0, src)
    cdef convert(FDouble self, v): return float(v)
    cdef from_string(FDouble self, str s): return float(s)
_TYPES[Type.Double] = FDouble

cdef class FDecimal128(FBase):
    default = lambda: decimal128.context.create_decimal(0)

    cdef pack(FDecimal128 self, v, dest, tail, int tail_offset):
        b = decimal128.pack(v)
        return pack_bytes(b, dest, tail, tail_offset)
    cdef unpack(FDecimal128 self, src):
        return decimal128.unpack(src[:16])
    cdef convert(FDecimal128 self, v):
        return decimal128.context.create_decimal(v)
    cdef from_string(FDecimal128 self, str s):
        return decimal128.context.create_decimal(s)
_TYPES[Type.Decimal128] = FDecimal128

cdef class FBytes(FBase):
    default = bytes

    cdef int size

    def __init__(self, f):
        self.size = f.size

    cdef pack(FBytes self, v, dest, tail, int tail_offset): return pack_bytes(v, dest, tail, tail_offset)
    cdef unpack(FBytes self, src): return unpack_bytes(src[:self.size])
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
    cdef object type_array
    cdef object count_ptr
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

    cdef _unpack(FArray self, src, f):
        r = self.default()
        cdef int i = 0
        off = self.type_array.offset
        cdef int size = self.count_ptr.unpack_data(src)
        while i < size:
            list.append(r, f(src[off:]))
            off += self.type_array.size
            i += 1
        return r

    cdef unpack(FArray self, src): return self._unpack(src, self.type_array.unpack_data)
    cdef reflection(FArray self, src): return self._unpack(src, self.type_array.unpack_reflection)

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
    cdef object type_ptr
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
        if write_optr(dest, self.version, &ptr):
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

    cdef _unpack(FPointer self, src, f):
        cdef offset_ptr_t ptr = read_optr(src, self.version, self.size)
        if ptr.size < 0: return None
        r = self.default()
        if ptr.size == -1:
            raise ValueError("Truncated offset ptr")
        if ptr.size == 0:
            return r
        if ptr.offset + ptr.entity * ptr.size > len(src):
            raise ValueError("Truncated list (size {}): end {} out of buffer size {}".format(ptr.size, ptr.offset + ptr.entity * ptr.size, len(src)))
        off = ptr.offset
        cdef int i = 0
        while i < ptr.size:
            list.append(r, f(src[off:]))
            off += ptr.entity
            i += 1
        return r

    cdef unpack(FPointer self, src): return self._unpack(src, self.type_ptr.unpack_data)
    cdef reflection(FPointer self, src): return self._unpack(src, self.type_ptr.unpack_reflection)

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
        self.default = self.type_msg.object

    cdef pack(FMessage self, v, dest, tail, int tail_offset):
        return self.type_msg.pack(v, dest, tail, tail_offset)

    cdef unpack(FMessage self, src):
        return self.type_msg.unpack(src)
    cdef reflection(FMessage self, src):
        return self.type_msg.reflection(src)

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
    cdef object type_ptr
    cdef object default

    def __init__(self, f):
        self.type_union = f.type_union
        self.type_ptr = f.type_union.type_ptr
        self.default = lambda: None

    cdef pack(FUnion self, v, dest, tail, int tail_offset):
        f = self.type_union[v[0]]
        self.type_ptr.pack_data(f.union_index, dest[:self.type_ptr.size], b'', 0)
        return f.pack_data(v[1], dest[f.offset:f.offset + f.size], tail, tail_offset - f.offset)

    cdef unpack(FUnion self, src):
        idx = self.type_ptr.unpack_data(src)
        if idx < 0 or idx > len(self.type_union.fields):
            raise ValueError(f"Invalid union index: {idx}")
        f = self.type_union.fields[idx]
        return self.type_union.klass(f.name, f.unpack_data(src[f.offset:]))
    """
    cdef reflection(FUnion self, src):
        return self.type_msg.reflection(src)
    """

    cdef convert(FUnion self, v):
        if isinstance(v, dict):
            items = v.items()
            if len(items) != 1:
                raise TypeError(f"Invalid union dict: {v}")
            k, v = items[0]
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
        return self.type_union.klass(v[0], f.as_dict(v[1]))
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
    cdef unpack(FEnum self, src):
        return self.enum_class(self.base.unpack(src))
    cdef convert(FEnum self, v):
        if isinstance(v, str):
            return self.enum_class.__members__[v]
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

    def __init__(self, f):
        self.base = f.impl
        self.fixed_precision = f.fixed_precision

    cdef pack(FFixed self, v, dest, tail, int tail_offset):
        return self.base.pack(v.scaleb(self.fixed_precision).to_integral_value(), dest, tail, tail_offset)
    cdef unpack(FFixed self, src):
        return Decimal(self.base.unpack(src)) * Decimal((0, (1,), -self.fixed_precision))
    cdef convert(FFixed self, v):
        if isinstance(v, (str, int)):
            v = Decimal(v)
        elif isinstance(v, float):
            v = Decimal(round(v * (10 ** self.fixed_precision))).scaleb(-self.fixed_precision)
        elif not isinstance(v, Decimal):
            raise TypeError("Expected str, float or int, got {}: {}".format(type(v), v))
        return v
    cdef from_string(FFixed self, str s): return Decimal(s)
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
    cdef unpack(FTimePoint self, src):
        return chrono.TimePoint(self.base.unpack(src), self.time_resolution, type=self.base.default)
    cdef convert(FTimePoint self, v):
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
    cdef unpack(FDuration self, src):
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
    cdef unpack(FBits self, src):
        return self.default(self.base.unpack(src))
    cdef convert(FBits self, v):
        return self.default(v)
    cdef from_string(FBits self, str s):
        return self.default.from_str(s)
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
    cdef unpack(FFixedString self, src): return unpack_str(src[:self.size])
    cdef convert(FFixedString self, v): return convert_str(v)
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
        if write_optr(dest, self.version, &ptr):
            return None
        tail.extend(b + b'\0')

    cdef unpack(FVString self, src):
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
    if buf.len < inbuf.len: return EMSGSIZE
#        raise TLLError("Dest buffer too small: {} < {}".format(buf.len, inbuf.len))
    memcpy(buf.buf, inbuf.buf, inbuf.len)
    #if inbuf.len < buf.len:
    #    memset(buf.buf + inbuf.len, 0, buf.len - inbuf.len)

cdef unpack_bytes(src):
    if not PyMemoryView_Check(src): return None #EINVAL
    #cdef Py_buffer * buf = PyMemoryView_GET_BUFFER(src)
    return bytearray(src) #<char *>buf.buf, buf.len)

cdef unpack_str(src):
    if not PyMemoryView_Check(src): return None #EINVAL
    cdef Py_buffer * buf = PyMemoryView_GET_BUFFER(src)
    cdef int l = strnlen(<char *>buf.buf, buf.len)
    return b2s(bytearray(src[:l])) #<char *>buf.buf, buf.len)

cdef unpack_vstring(object src, int optr_version):
    cdef offset_ptr_t ptr = read_optr(src, optr_version, 1)
    if ptr.size < 0: return None
    if ptr.size == 0:
        return "" #src[sizeof(tll_scheme_offset_ptr_t):sizeof(tll_scheme_offset_ptr_t)]
    r = src[ptr.offset:ptr.offset + ptr.size - 1]
    if PY_MAJOR_VERSION == 2:
        return r.tobytes()
    return str(r, encoding='utf-8')

def convert_str(v):
    if isinstance(v, bytes):
        return v.decode('utf-8')
    elif not isinstance(v, str):
        raise TypeError(f"Expected str, got {type(v)}: {v}")
    return v

def from_string_int(v): return int(v, 0)

def _as_dict_list(field, v):
    return [field.as_dict(x) for x in v]

def _as_dict_msg(msg, v):
    r = {}
    for f in msg.fields:
        x = getattr(v, f.name, None)
        if x is None: continue
        r[f.name] = f.as_dict(x)
    return r

class Field:
    Type = _Type
    Sub = SubType
    def init(self, name, type):
        impl = _TYPES.get(type)
        if impl is None:
            raise NotImplementedError(f"Type {type} is not implemented")
        self.impl = impl(self)

        if self.sub_type != SubType.NONE:
            impl = _SUBTYPES.get((type, self.sub_type))
            if impl is None:
                impl = _SUBTYPES.get(self.sub_type)
            if impl is not None:
                #raise NotImplementedError(f"Sub-type {self.sub_type} for type {type} is not implemented")
                self.impl = impl(self)

    def __repr__(self):
        return "<Field {0.name}, type: {0.type}, size: {0.size}, offset: {0.offset}>".format(self)

    def default(self):
        cdef FBase impl = self.impl
        return impl.default()

    def convert(self, v):
        cdef FBase impl = self.impl
        return impl.convert(v)

    def pack_data(self, v, dest, tail, tail_offset):
        cdef FBase impl = self.impl
        return impl.pack(v, dest, tail, tail_offset)

    def unpack_data(self, src):
        cdef FBase impl = self.impl
        return impl.unpack(src)

    def unpack_reflection(self, src):
        cdef FBase impl = self.impl
        return impl.reflection(src)

    def pack(self, v, dest, tail, tail_offset = 0):
        memoryview_check(dest)
        self.pack_data(v, dest[self.offset:self.offset + self.size])

    def unpack(self, src):
        memoryview_check(src)
        return self.unpack_data(src[self.offset:self.offset + self.size])

    def from_string(self, v : str):
        cdef FBase impl = self.impl
        try:
            return impl.convert(impl.from_string(v))
        except NotImplementedError:
            raise TypeError("Field {} with type {} can not be constructed from string".format(self.name, self.type))

    def as_dict(self, v):
        cdef FBase impl = self.impl
        return impl.as_dict(v)

    @property
    def optional(self):
        return self.index >= 0

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
    if r.type == r.Message:
        r.type_msg = s[ptr.type_msg.name]
    elif r.type == r.Array:
        r.count = ptr.count
        r.count_ptr = field_wrap(s, m, ptr.count_ptr)
        r.type_array = field_wrap(s, m, ptr.type_array)

        class L(List):
            SCHEME = r.type_array
        L.__name__ = r.name
        r.list = L
    elif r.type == r.Pointer:
        r.offset_ptr_version = OffsetPtrVersion(ptr.offset_ptr_version)
        r.type_ptr = field_wrap(s, m, ptr.type_ptr)

        class L(List):
            SCHEME = r.type_ptr
        L.__name__ = r.name
        r.list = L
    elif r.type == r.Union:
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
    elif r.sub_type == r.Sub.TimePoint or r.sub_type == r.Sub.Duration:
        r.time_resolution = _time_resolution_map[ptr.time_resolution]

    r.size = ptr.size
    r.offset = ptr.offset
    r.init(r.name, r.type) #b2s(ptr.name), Type(ptr.type))
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
        if a:
            for k,v in zip(a, self.SCHEME.items()):
                setattr(self, k, v)
        if kw:
            for k,v in kw.items():
                setattr(self, k, v)

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
            raise KeyError("No such field in {}: {}".format(self.SCHEME.name, k))
        object.__setattr__(self, k, f.convert(v))

    def __delattr__(self, k):
        f = self.SCHEME.get(k, None)
        if f is None:
            raise KeyError("No such field in {}: {}".format(self.SCHEME.name, k))
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

    def as_dict(self):
        return _as_dict_msg(self.SCHEME, self)

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

    def as_dict(self):
        return _as_dict_msg(self.SCHEME, self)

class Message(OrderedDict):
    def object(self, *a, **kw):
        return self.klass(*a, **kw)

    def pack(self, v, dest, tail, tail_offset):
        memoryview_check(dest)
        pmap = dest[self.pmap.offset:self.pmap.offset + self.pmap.size] if self.pmap else None
        for f in self.fields:
            i = getattr(v, f.name, None)
            if i is None:
                continue
            if pmap and f.index >= 0:
                pmap[f.index // 8] |= (1 << (f.index % 8))
            off = f.offset
            f.pack_data(i, dest[off:off + f.size], tail, tail_offset - off)

    def unpack(self, src, v=None):
        memoryview_check(src)
        if len(src) < self.size:
            raise ValueError("Buffer size {} less then message size {}".format(len(src), self.size))
        pmap = src[self.pmap.offset:self.pmap.offset + self.pmap.size] if self.pmap else None
        v = self.object() if v is None else v
        for f in self.fields:
            if pmap:
                if f.index >= 0 and pmap[f.index // 8] & (1 << (f.index % 8)) == 0:
                    continue
                elif self.pmap.name == f.name:
                    continue
            r = f.unpack_data(src[f.offset:])
            object.__setattr__(v, f.name, r)
        return v

    @property
    def fields(s): return s.values()

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

    class D(Data):
        SCHEME = r
    D.__name__ = r.name
    r.klass = D

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

    if ptr.pmap:
        r.pmap = r[b2s(ptr.pmap.name)]
    else:
        r.pmap = None

    return r

cdef class Scheme:
    def __init__(self, uri):
        self._ptr = NULL
        self.messages = []
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

    def copy(self):
        if not self._ptr:
            raise RuntimeError("Uninitialized pointer")
        return Scheme.wrap(tll_scheme_ref(self._ptr))

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

    def find(self, k):
        if isinstance(k, int):
            for m in self.messages:
                if m.msgid == k: return m
        elif isinstance(k, bytes):
            for m in self.messages:
                if m.name_bytes == k: return m
        else:
            for m in self.messages:
                if m.name == k: return m
        raise KeyError("Message '{}' not found".format(k))

    def __getitem__(self, k): return self.find(k)

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
        self.messages = []

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
