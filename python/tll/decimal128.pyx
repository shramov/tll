#!/usr/bin/env python
# vim: sts=4 sw=4 et

from .decimal128 cimport *
from .buffer cimport *

import decimal

context_options = dict(prec = 34, Emin = -6143, Emax = 6144, rounding = decimal.ROUND_HALF_EVEN)
context = decimal.Context(traps = [decimal.InvalidOperation, decimal.Overflow, decimal.Inexact], **context_options)

_EXPONENTS = {'F': TLL_DECIMAL128_INF, 'n': TLL_DECIMAL128_NAN, 'N': TLL_DECIMAL128_SNAN}

def unpack(b):
    m = memoryview(b)
    if len(m) != 16:
        raise ValueError(f"Invalid size of packed decimal128 'f{b}': {len(m)}")
    return unpack_buf(PyMemoryView_GET_BUFFER(m))

cdef object unpack_buf(const Py_buffer * buf):
    cdef tll_decimal128_unpacked_t u
    tll_decimal128_unpack(&u, <const tll_decimal128_t *>(buf.buf))
    if u.exponent >= TLL_DECIMAL128_INF:
        if u.exponent == TLL_DECIMAL128_INF:
            if u.sign:
                return context.create_decimal('-Inf')
            else:
                return context.create_decimal('Inf')
        elif u.exponent == TLL_DECIMAL128_NAN:
            return context.create_decimal('NaN')
        elif u.exponent == TLL_DECIMAL128_SNAN:
            return context.create_decimal('sNaN')
    mantissa = u.mantissa.hi
    mantissa = (mantissa << 64) + u.mantissa.lo
    if u.sign:
        mantissa = -mantissa
    r = context.create_decimal(mantissa)
    return context.create_decimal(mantissa).scaleb(u.exponent, context=context)

def pack(d):
    cdef tll_decimal128_unpacked_t u

    b = bytearray(16) #sizeof(tll_decimal128_t))
    m = memoryview(b)

    sign, digits, exponent = d.as_tuple()
    mantissa = 0
    for d in digits:
        mantissa = mantissa * 10 + d

    u.sign = sign
    u.mantissa.lo = mantissa & 0xffffffffffffffff
    u.mantissa.hi = mantissa >> 64
    u.exponent = _EXPONENTS.get(exponent, exponent)

    if len(m) != 16:
        raise ValueError(f"Invalid size of packed decimal128 'f{b}': {len(m)}")
    tll_decimal128_pack(<tll_decimal128_t *>(PyMemoryView_GET_BUFFER(m).buf), &u)
    return b
