# vim: sts=4 sw=4 et
# cython: language_level=3

from .channel cimport *
from .common cimport tll_state_t

cdef extern from "tll/channel/impl.h":
    ctypedef struct tll_channel_impl_t:
        int (*init)(tll_channel_t *, const tll_config_t * url, tll_channel_t * parent, tll_channel_context_t * ctx)
        void (*free)(tll_channel_t *)
        int (*open)(tll_channel_t *, const char * str, size_t len)
        int (*close)(tll_channel_t *, int)

        int (*process)(tll_channel_t *, long timeout, int flags)
        int (*post)(tll_channel_t *, const tll_msg_t *msg, int flags)

        const tll_scheme_t * (*scheme)(const tll_channel_t *, int)

        const char * name

        void * data

    ctypedef struct tll_channel_list_t
    ctypedef struct tll_channel_callback_pair_t

    ctypedef struct tll_channel_internal_t:
        tll_state_t state
        tll_channel_t * self

        const char * name
        unsigned caps
        unsigned dcaps
        int fd
        tll_config_t * config
        tll_channel_list_t * children

        tll_channel_callback_pair_t * data_cb
        tll_channel_callback_pair_t * cb

    cdef void tll_channel_internal_init(tll_channel_internal_t *ptr)
    cdef void tll_channel_internal_clear(tll_channel_internal_t *ptr)
    cdef int tll_channel_internal_child_add(tll_channel_internal_t *ptr, tll_channel_t *c, const char * tag, int len)
    cdef int tll_channel_internal_child_del(tll_channel_internal_t *ptr, const tll_channel_t *c, const char * tag, int len)

    cdef void tll_channel_list_free(tll_channel_list_t *l)

    cdef int tll_channel_list_add(tll_channel_list_t **l, tll_channel_t *c)
    cdef int tll_channel_list_del(tll_channel_list_t **l, const tll_channel_t *c)

    cdef int tll_channel_callback_data(const tll_channel_internal_t *, const tll_msg_t *msg)
    cdef int tll_channel_callback(const tll_channel_internal_t *, const tll_msg_t *msg)
