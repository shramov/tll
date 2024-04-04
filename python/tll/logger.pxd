# vim: sts=4 sw=4 et
# cython: language_level=3

from .config cimport tll_config_t

cdef extern from "tll/logger.h":
    ctypedef enum tll_logger_level_t:
        TLL_LOGGER_TRACE
        TLL_LOGGER_DEBUG
        TLL_LOGGER_INFO
        TLL_LOGGER_WARNING
        TLL_LOGGER_ERROR
        TLL_LOGGER_CRITICAL

    ctypedef struct tll_logger_t:
        tll_logger_level_t level

    cdef tll_logger_t * tll_logger_new(const char *name, int len)
    cdef tll_logger_t * tll_logger_copy(const tll_logger_t * log)
    cdef void tll_logger_free(tll_logger_t * cfg)
    cdef void tll_logger_set(const char * name, int len, tll_logger_level_t level)

    cdef const char * tll_logger_name(const tll_logger_t * log)
    cdef int tll_logger_log(tll_logger_t * log, tll_logger_level_t lvl, const char * buf, size_t size)

    cdef int tll_logger_config(const tll_config_t *)

cdef class Logger:
    cdef tll_logger_t * ptr
    cdef char style

cdef extern from "tll/logger/impl.h":
    ctypedef struct tll_logger_impl_t:
        int (*log)(long long ts, const char * category, tll_logger_level_t level, const char * data, size_t size, void * obj);
        void * (*log_new)(tll_logger_impl_t *, const char * category)
        void (*log_free)(tll_logger_impl_t *, const char * category, void *)
        int (*configure)(tll_logger_impl_t *, const tll_config_t *)
        void (*release)(tll_logger_impl_t *)
        void * user

    cdef int tll_logger_register(tll_logger_impl_t *)
    cdef const tll_logger_impl_t * tll_logger_impl_get()
