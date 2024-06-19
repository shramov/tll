# vim: sts=4 sw=4 et
# cython: language_level=3

cdef extern from "tll/scheme.h":
    ctypedef enum tll_scheme_field_type_t:
        TLL_SCHEME_FIELD_INT8
        TLL_SCHEME_FIELD_INT16
        TLL_SCHEME_FIELD_INT32
        TLL_SCHEME_FIELD_INT64
        TLL_SCHEME_FIELD_UINT8
        TLL_SCHEME_FIELD_UINT16
        TLL_SCHEME_FIELD_UINT32
        TLL_SCHEME_FIELD_UINT64
        TLL_SCHEME_FIELD_DOUBLE
        TLL_SCHEME_FIELD_DECIMAL128
        TLL_SCHEME_FIELD_BYTES
        TLL_SCHEME_FIELD_MESSAGE
        TLL_SCHEME_FIELD_ARRAY
        TLL_SCHEME_FIELD_POINTER
        TLL_SCHEME_FIELD_UNION

    ctypedef enum tll_scheme_sub_type_t:
        TLL_SCHEME_SUB_NONE
        TLL_SCHEME_SUB_ENUM
        TLL_SCHEME_SUB_BYTE_STRING
        TLL_SCHEME_SUB_FIXED_POINT
        TLL_SCHEME_SUB_TIME_POINT
        TLL_SCHEME_SUB_DURATION
        TLL_SCHEME_SUB_BITS

    ctypedef enum tll_scheme_offset_ptr_version_t:
        TLL_SCHEME_OFFSET_PTR_DEFAULT
        TLL_SCHEME_OFFSET_PTR_LEGACY_SHORT
        TLL_SCHEME_OFFSET_PTR_LEGACY_LONG

    ctypedef enum tll_scheme_time_resolution_t:
        TLL_SCHEME_TIME_NS
        TLL_SCHEME_TIME_US
        TLL_SCHEME_TIME_MS
        TLL_SCHEME_TIME_SECOND
        TLL_SCHEME_TIME_MINUTE
        TLL_SCHEME_TIME_HOUR
        TLL_SCHEME_TIME_DAY

    ctypedef struct tll_scheme_bit_field_t:
        tll_scheme_bit_field_t * next
        const char * name
        unsigned size
        unsigned offset

    ctypedef struct tll_scheme_bits_t:
        tll_scheme_bits_t * next
        const char * name
        tll_scheme_field_type_t type
        size_t size
        tll_scheme_bit_field_t * values
        tll_scheme_option_t * options

    ctypedef struct tll_scheme_option_t:
        tll_scheme_option_t * next
        const char * name
        const char * value

    ctypedef struct tll_scheme_enum_value_t:
        tll_scheme_enum_value_t * next
        const char * name
        long long value

    ctypedef struct tll_scheme_enum_t:
        tll_scheme_enum_t * next
        const char * name
        tll_scheme_field_type_t type
        tll_scheme_enum_value_t * values
        tll_scheme_option_t * options

    ctypedef struct tll_scheme_union_t:
        tll_scheme_union_t * next
        const char * name
        tll_scheme_field_t * type_ptr
        tll_scheme_field_t * fields
        size_t fields_size
        size_t union_size
        tll_scheme_option_t * options

    ctypedef struct tll_scheme_field_t:
        tll_scheme_field_t * next
        tll_scheme_option_t * options
        const char * name
        size_t offset
        tll_scheme_field_type_t type
        tll_scheme_sub_type_t sub_type
        size_t size
        int index

	# union
        tll_scheme_message_t * type_msg
        tll_scheme_enum_t * type_enum

        tll_scheme_offset_ptr_version_t offset_ptr_version
        tll_scheme_field_t * type_ptr

        size_t count
        tll_scheme_field_t * type_array
        tll_scheme_field_t * count_ptr

        unsigned fixed_precision
        tll_scheme_time_resolution_t time_resolution

        tll_scheme_bit_field_t * bitfields
        tll_scheme_bits_t * type_bits

        tll_scheme_union_t * type_union

    ctypedef struct tll_scheme_message_t:
        tll_scheme_message_t * next
        tll_scheme_option_t * options

        const char * name
        int msgid
        size_t size

        tll_scheme_field_t * fields
        tll_scheme_enum_t * enums
        tll_scheme_union_t * unions
        tll_scheme_bits_t * bits

        tll_scheme_field_t * pmap

    ctypedef struct tll_scheme_t:
        tll_scheme_option_t * options
        tll_scheme_message_t * messages
        tll_scheme_enum_t * enums
        tll_scheme_union_t * unions
        tll_scheme_bits_t * bits
        tll_scheme_field_t * aliases

    cdef tll_scheme_t * tll_scheme_load(const char *str, int len)
    cdef tll_scheme_t * tll_scheme_copy(const tll_scheme_t *src)
    cdef const tll_scheme_t * tll_scheme_ref(const tll_scheme_t * ptr)
    cdef void tll_scheme_unref(const tll_scheme_t * ptr)

    cdef char * tll_scheme_dump(const tll_scheme_t * ptr, const char * format)

    cdef enum tll_scheme_path_mode_t:
        TLL_SCHEME_PATH_USER
        TLL_SCHEME_PATH_ENV
        TLL_SCHEME_PATH_DEFAULT

    cdef int tll_scheme_path_add(const char * path, int plen, tll_scheme_path_mode_t mode)
    cdef int tll_scheme_path_remove(const char * path, int plen, tll_scheme_path_mode_t mode)

cdef extern from "tll/scheme/types.h":
    ctypedef struct tll_scheme_offset_ptr_t:
        unsigned offset
        unsigned size
        unsigned entity

    ctypedef struct tll_scheme_offset_ptr_legacy_short_t:
        unsigned offset
        unsigned size

    ctypedef struct tll_scheme_offset_ptr_legacy_long_t:
        unsigned offset
        unsigned size
        unsigned entity

cdef class Scheme:
    cdef tll_scheme_t * _ptr
    cdef int _own
    cdef object messages
    cdef object enums
    cdef object unions
    cdef object aliases
    cdef object options
    cdef object bits

    cdef fill(Scheme self, const tll_scheme_t * cfg)
    cdef same(Scheme self, const tll_scheme_t * cfg)

    @staticmethod
    cdef Scheme wrap(const tll_scheme_t * cfg, int ref = *)
