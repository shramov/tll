# vim: sts=4 sw=4 et
# cython: language_level=3

from ..config cimport tll_config_t
from ..channel.channel cimport tll_channel_t, tll_channel_context_t, tll_channel_list_t
from .loop cimport tll_processor_loop_t

cdef extern from "tll/processor.h" nogil:
    ctypedef tll_channel_t tll_processor_t
    ctypedef tll_channel_t tll_processor_worker_t

    cdef int tll_processor_init(tll_channel_context_t *) nogil

    cdef tll_channel_list_t * tll_processor_workers(tll_processor_t *) nogil
    cdef tll_processor_loop_t * tll_processor_loop(tll_processor_t *)
    cdef tll_processor_loop_t * tll_processor_worker_loop(tll_processor_t *)
