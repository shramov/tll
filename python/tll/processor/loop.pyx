#!/usr/bin/env python
# vim: sts=4 sw=4 et

from .loop cimport *
from ..channel.channel cimport Channel

from .. import error

cdef class Loop:
    cdef tll_processor_loop_t * _ptr

    def __cinit__(self, bare=False):
        if bare:
            self._ptr = NULL
        self._ptr = tll_processor_loop_new()

    def __dealloc__(self):
        if self._ptr != NULL:
            tll_processor_loop_free(self._ptr)
        self._ptr = NULL

    def add(self, channel):
        if not isinstance(channel, Channel):
            raise TypeError("Invalid channel argument: {}".format(channel))
        cdef int r = tll_processor_loop_add(self._ptr, (<Channel>channel)._ptr)
        error.wrap(r, "Failed to add channel {} to loop", channel.name)

    def remove(self, channel):
        if not isinstance(channel, Channel):
            raise TypeError("Invalid channel argument: {}".format(channel))
        cdef int r = tll_processor_loop_del(self._ptr, (<Channel>channel)._ptr)
        error.wrap(r, "Failed to delete channel {} from loop", channel.name)

    def poll(self, timeout=0.001):
        cdef tll_channel_t * r = tll_processor_loop_poll(self._ptr, int(timeout * 1000))
        if r == NULL:
            return
        return Channel.wrap(r)

    def process(self):
        tll_processor_loop_process(self._ptr)
