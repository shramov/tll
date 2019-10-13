# vim: sts=4 sw=4 et
# cython: language_level=3

cdef extern from "tll/channel.h":
    ctypedef enum tll_state_t:
        TLL_STATE_CLOSED
        TLL_STATE_OPENING
        TLL_STATE_ACTIVE
        TLL_STATE_CLOSING
        TLL_STATE_ERROR
        TLL_STATE_DESTROY
