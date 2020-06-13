# vim: sts=4 sw=4 et
# cython: language_level=3

from ..channel.channel cimport tll_channel_t

cdef extern from "tll/processor/loop.h":
    ctypedef struct tll_processor_loop_t

    cdef tll_processor_loop_t * tll_processor_loop_new()
    cdef void tll_processor_loop_free(tll_processor_loop_t *)

    cdef int tll_processor_loop_add(tll_processor_loop_t *, tll_channel_t *)
    cdef int tll_processor_loop_del(tll_processor_loop_t *, const tll_channel_t *)
    cdef tll_channel_t * tll_processor_loop_poll(tll_processor_loop_t *, long)
    cdef int tll_processor_loop_process(tll_processor_loop_t *)
    cdef int tll_processor_loop_pending(tll_processor_loop_t *)
