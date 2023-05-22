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

    ctypedef enum tll_msg_type_t:
        TLL_MESSAGE_DATA
        TLL_MESSAGE_CONTROL
        TLL_MESSAGE_STATE
        TLL_MESSAGE_CHANNEL
