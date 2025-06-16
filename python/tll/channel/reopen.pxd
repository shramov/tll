# vim: sts=4 sw=4 et
# cython: language_level=3

from ..config cimport tll_config_t
from ..logger cimport tll_logger_t
from .channel cimport tll_channel_t
from .common cimport tll_state_t

cdef extern from "tll/channel/reopen.h" nogil:
    ctypedef struct tll_channel_reopen_t

    ctypedef enum tll_channel_reopen_action_t:
        TLL_CHANNEL_REOPEN_NONE
        TLL_CHANNEL_REOPEN_CLOSE
        TLL_CHANNEL_REOPEN_OPEN

    cdef tll_channel_reopen_t * tll_channel_reopen_new(const tll_config_t *)
    cdef void tll_channel_reopen_free(tll_channel_reopen_t *)

    cdef long long tll_channel_reopen_next(tll_channel_reopen_t *)

    cdef void tll_channel_reopen_on_state(tll_channel_reopen_t *, tll_state_t)
    cdef tll_channel_reopen_action_t tll_channel_reopen_on_timer(tll_channel_reopen_t *, tll_logger_t *, long long)
    cdef tll_channel_t * tll_channel_reopen_set_channel(tll_channel_reopen_t *, tll_channel_t *)
    cdef void tll_channel_reopen_set_open_config(tll_channel_reopen_t *, const tll_config_t *)
    cdef int tll_channel_reopen_open(tll_channel_reopen_t *)
    cdef void tll_channel_reopen_close(tll_channel_reopen_t *)
