# vim: sts=4 sw=4 et
# cython: language_level=3

cdef extern from "tll/config.h":

    ctypedef struct tll_config_t

    ctypedef int (*tll_config_callback_t)(const char * key, int klen, const tll_config_t *value, void * data)
    ctypedef char * (*tll_config_value_callback_t)(int * len, void * data)
    ctypedef void (*tll_config_value_callback_free_t)(tll_config_value_callback_t cb, void * data)

    cdef tll_config_t * tll_config_new()
    cdef tll_config_t * tll_config_copy(const tll_config_t *)
    cdef tll_config_t * tll_config_load(const char *path, int len)
    cdef tll_config_t * tll_config_load_data(const char *proto, int plen, const char * data, int dlen)
    cdef const tll_config_t * tll_config_ref(const tll_config_t * cfg)
    cdef void tll_config_unref(const tll_config_t * cfg)

    cdef tll_config_t * tll_config_sub(tll_config_t *, const char * path, int len, int create)


    cdef int tll_config_has(const tll_config_t *, const char * path, int plen)
    cdef int tll_config_remove(tll_config_t *, const char * path, int plen)
    cdef int tll_config_unlink(tll_config_t *, const char * path, int plen)
    cdef int tll_config_unset(tll_config_t *, const char * path, int plen)
    cdef int tll_config_get(const tll_config_t *, const char * path, int plen, char * value, int * vlen)
    cdef char * tll_config_get_copy(const tll_config_t *, const char * path, int plen, int * vlen)
    cdef void tll_config_value_free(char *value)

    cdef int tll_config_merge(tll_config_t *, tll_config_t *, int overwrite)
    cdef int tll_config_process_imports(tll_config_t *, const char * path, int plen)

    cdef int tll_config_set(tll_config_t *, const char * path, int plen, const char * value, int vlen)
    cdef int tll_config_set_link(tll_config_t *, const char * path, int plen, const char * value, int vlen)
    cdef int tll_config_set_config(tll_config_t *, const char * path, int plen, tll_config_t *, int consume)
    cdef int tll_config_set_callback(tll_config_t *, const char * path, int plen, tll_config_value_callback_t cb, void * user, tll_config_value_callback_free_t deleter)

    cdef int tll_config_value(const tll_config_t *)

    cdef int tll_config_list(const tll_config_t *, tll_config_callback_t cb, void * data)
    cdef int tll_config_browse(const tll_config_t *, const char * mask, int mlen, tll_config_callback_t cb, void * data)

cdef class Config:
    cdef tll_config_t * _ptr
    cdef int _const

    @staticmethod
    cdef Config wrap(tll_config_t * cfg, int ref = *, int _const = *)
    @staticmethod
    cdef Config wrap_const(const tll_config_t * cfg, int ref = *)

cdef class Url(Config):
    pass
