# vim: sts=4 sw=4 et
# cython: language_level=3

from ..config cimport tll_config_t
from ..scheme cimport tll_scheme_t, Scheme
from .impl cimport tll_channel_internal_t, tll_channel_impl_t
from .common cimport tll_state_t
from libc.stdint cimport int64_t

cdef extern from "tll/channel.h":
    #ctypedef struct tll_channel_t
    #ctypedef struct tll_channel_impl_t
    ctypedef struct tll_channel_context_t

    ctypedef struct tll_channel_t:
        tll_channel_impl_t * impl
        void * data
        tll_channel_internal_t * internal
        tll_channel_context_t * context
        tll_channel_t * parent

    ctypedef struct tll_channel_list_t:
        tll_channel_t * channel
        tll_channel_list_t * next


    ctypedef enum tll_msg_type_t:
        TLL_MESSAGE_DATA
        TLL_MESSAGE_CONTROL
        TLL_MESSAGE_STATE
        TLL_MESSAGE_CHANNEL

    ctypedef enum tll_message_mask_t:
        TLL_MESSAGE_MASK_ALL
        TLL_MESSAGE_MASK_DATA
        TLL_MESSAGE_MASK_CONTROL
        TLL_MESSAGE_MASK_STATE
        TLL_MESSAGE_MASK_CHANNEL

    ctypedef enum tll_msg_channel_t:
        TLL_MESSAGE_CHANNEL_UPDATE
        TLL_MESSAGE_CHANNEL_ADD
        TLL_MESSAGE_CHANNEL_DELETE
        TLL_MESSAGE_CHANNEL_UPDATE_FD

    ctypedef enum tll_channel_cap_t:
        TLL_CAPS_INPUT
        TLL_CAPS_OUTPUT
        TLL_CAPS_INOUT

        TLL_CAPS_EX_BIT
        TLL_CAPS_PROXY
        TLL_CAPS_CUSTOM

    ctypedef enum tll_channel_dcap_t:
        TLL_DCAPS_POLLIN
        TLL_DCAPS_POLLOUT
        TLL_DCAPS_POLLMASK

        TLL_DCAPS_PROCESS
        TLL_DCAPS_PENDING
        TLL_DCAPS_SUSPEND
        TLL_DCAPS_SUSPEND_PERMANENT

    ctypedef struct tll_msg_t:
        short type
        int msgid
        long long seq
        const void * data
        int64_t addr
        size_t size

    ctypedef int (*tll_channel_callback_t)(const tll_channel_t *, const tll_msg_t * msg, void * user);

    cdef tll_channel_t * tll_channel_new(tll_channel_context_t *ctx, const char *str, size_t len, tll_channel_t *parent, const tll_channel_impl_t * impl)
    cdef void tll_channel_free(tll_channel_t * cfg)

    cdef int tll_channel_open(tll_channel_t *, const char * props, int len)
    cdef int tll_channel_close(tll_channel_t *)

    cdef int tll_channel_callback_add(tll_channel_t *, tll_channel_callback_t cb, void * user, unsigned mask)
    cdef int tll_channel_callback_del(tll_channel_t *, tll_channel_callback_t cb, void * user, unsigned mask)

    cdef int tll_channel_process(tll_channel_t *c, long timeout, int flags)
    cdef int tll_channel_post(tll_channel_t *c, const tll_msg_t *msg, int flags)

    cdef tll_state_t tll_channel_state(const tll_channel_t *c)
    cdef const char * tll_channel_name(const tll_channel_t *c)
    cdef unsigned tll_channel_caps(const tll_channel_t *c)
    cdef unsigned tll_channel_dcaps(const tll_channel_t *c)
    cdef int tll_channel_fd(const tll_channel_t *c)
    cdef tll_channel_context_t * tll_channel_context(const tll_channel_t *c)
    cdef tll_config_t * tll_channel_config(tll_channel_t *c)
    cdef tll_scheme_t * tll_channel_scheme(tll_channel_t *c, int)
    cdef tll_channel_list_t * tll_channel_children(tll_channel_t *c)

    cdef tll_channel_t * tll_channel_get(const tll_channel_context_t *ctx, const char *name, int len)

    cdef tll_channel_context_t * tll_channel_context_new(tll_config_t *)
    cdef tll_channel_context_t * tll_channel_context_ref(tll_channel_context_t *)
    cdef void tll_channel_context_free(tll_channel_context_t *)

    cdef tll_config_t * tll_channel_context_config(tll_channel_context_t *)
    cdef tll_config_t * tll_channel_context_config_defaults(tll_channel_context_t *)
    cdef tll_scheme_t * tll_channel_context_scheme_load(tll_channel_context_t *, const char *url, int len, int cache)

    cdef int tll_channel_impl_register(tll_channel_context_t *ctx, tll_channel_impl_t *impl, const char *name)
    cdef int tll_channel_impl_unregister(tll_channel_context_t *ctx, tll_channel_impl_t *impl, const char *name)
    cdef int tll_channel_impl_get(tll_channel_context_t *ctx, const char *name)

    cdef int tll_channel_module_load(tll_channel_context_t *ctx, const char *module, const char * symbol)
    cdef int tll_channel_module_unload(tll_channel_context_t *ctx, const char *module)

cdef class Message:
    cdef const tll_msg_t * _ptr

    @staticmethod
    cdef Message wrap(const tll_msg_t * ptr)

"""
cdef class Context:
    cdef tll_channel_context_t * _ptr

    @staticmethod
    cdef Context wrap(tll_channel_context_t * ptr)
"""

cdef class Channel:
    cdef tll_channel_t * _ptr
    cdef int _own
    cdef object _callbacks
    cdef Scheme _scheme_cache
    cdef object __weakref__

    cdef _post(self, const tll_msg_t * msg, int flags)

    @staticmethod
    cdef Channel wrap(tll_channel_t * ptr)
