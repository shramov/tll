# vim: sts=4 sw=4 et
# cython: language_level=3

from .channel cimport tll_channel_context_t, tll_msg_t
from .impl cimport tll_channel_internal_t
from ..config cimport Config

cdef class Context:
    cdef tll_channel_context_t * _ptr

    @staticmethod
    cdef Context wrap(tll_channel_context_t * ptr)

cdef class Internal:
    cdef tll_channel_internal_t internal
    cdef object name_bytes
    cdef object name_str
    cdef Config config

    cdef callback(self, const tll_msg_t * msg)
