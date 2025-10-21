# vim: sts=4 sw=4 et
# cython: language_level=3

from libc.stdint cimport uint64_t

cdef extern from "tll/util/decimal128.h":
    ctypedef struct tll_decimal128_t:
        uint64_t lo
        uint64_t hi

    ctypedef struct tll_uint128_t:
        uint64_t lo
        uint64_t hi

    ctypedef struct tll_decimal128_unpacked_t:
        tll_uint128_t mantissa
        int exponent
        int sign

    cdef short TLL_DECIMAL128_INF
    cdef short TLL_DECIMAL128_NAN
    cdef short TLL_DECIMAL128_SNAN

    cdef int tll_decimal128_pack(tll_decimal128_t *, const tll_decimal128_unpacked_t *)
    cdef int tll_decimal128_unpack(tll_decimal128_unpacked_t *, const tll_decimal128_t *)

cdef object unpack_buf(const Py_buffer * buf)
