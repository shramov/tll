# vim: sts=4 sw=4 et
# cython: language_level=3

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

    ctypedef struct tll_logger_impl_t:
        int (*log)(long long ts, const char * category, tll_logger_level_t level, const char * data, size_t size, void * obj);
        void * (*log_new)(const char * category, tll_logger_impl_t *)
        void (*log_free)(const char * category, void *, tll_logger_impl_t *)
        void * user

    cdef tll_logger_t * tll_logger_new(const char *name, int len)
    cdef void tll_logger_free(tll_logger_t * cfg)
    cdef void tll_logger_set(const char * name, int len, tll_logger_level_t level)

    cdef const char * tll_logger_name(const tll_logger_t * log)
    cdef int tll_logger_log(tll_logger_t * log, tll_logger_level_t lvl, const char * buf, size_t size)

    cdef int tll_logger_register(tll_logger_impl_t *)