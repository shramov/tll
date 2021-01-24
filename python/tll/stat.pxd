# vim: sts=4 sw=4 et
# cython: language_level=3

from libc.stdint cimport int64_t

cdef extern from "tll/stat.h" nogil:
    ctypedef enum tll_stat_method_t:
        TLL_STAT_SUM
        TLL_STAT_MIN
        TLL_STAT_MAX
        TLL_STAT_LAST

    ctypedef enum tll_stat_type_t:
        TLL_STAT_INT
        TLL_STAT_FLOAT

    ctypedef enum tll_stat_unit_t:
        TLL_STAT_UNIT_UNKNOWN
        TLL_STAT_UNIT_BYTES
        TLL_STAT_UNIT_NS

    ctypedef int64_t tll_stat_int_t
    ctypedef double tll_stat_float_t

    ctypedef struct tll_stat_field_t:
        unsigned char method
        unsigned char type
        unsigned char unit
        char name[7]
        tll_stat_int_t value
        tll_stat_float_t fvalue

    ctypedef struct tll_stat_page_t:
        tll_stat_field_t * fields
        size_t size

    ctypedef struct tll_stat_block_t:
        tll_stat_page_t * lock
        tll_stat_page_t * active
        tll_stat_page_t * inactive
        const char * name

    ctypedef struct tll_stat_list_t
    ctypedef struct tll_stat_iter_t

    cdef tll_stat_int_t tll_stat_default_int(tll_stat_method_t)
    cdef tll_stat_float_t tll_stat_default_float(tll_stat_method_t)
    cdef void tll_stat_field_reset(tll_stat_field_t *)
    cdef void tll_stat_field_update_int(tll_stat_field_t *, tll_stat_int_t)
    cdef void tll_stat_field_update_float(tll_stat_field_t *, tll_stat_float_t)

    cdef tll_stat_page_t * tll_stat_page_acquire(tll_stat_block_t *)
    cdef void tll_stat_page_release(tll_stat_block_t *, tll_stat_page_t *)
    cdef tll_stat_page_t * tll_stat_page_swap(tll_stat_block_t *)

    cdef tll_stat_list_t * tll_stat_list_new()
    cdef void tll_stat_list_free(tll_stat_list_t *)

    cdef int tll_stat_list_add(tll_stat_list_t *, tll_stat_block_t *)
    cdef int tll_stat_list_remove(tll_stat_list_t *, tll_stat_block_t *)

    cdef tll_stat_iter_t * tll_stat_list_begin(tll_stat_list_t *)

    cdef int tll_stat_iter_empty(const tll_stat_iter_t *)
    cdef const char * tll_stat_iter_name(const tll_stat_iter_t *)

    cdef tll_stat_iter_t * tll_stat_iter_next(tll_stat_iter_t *)
    cdef tll_stat_page_t * tll_stat_iter_swap(tll_stat_iter_t *)

cdef class List:
    cdef tll_stat_list_t * ptr
    cdef int owner

    @staticmethod
    cdef List wrap(tll_stat_list_t * cfg)

    @staticmethod
    cdef tll_stat_block_t * unwrap(object obj)
