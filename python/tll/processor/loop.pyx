#!/usr/bin/env python
# vim: sts=4 sw=4 et

from .loop cimport *
from ..channel.channel cimport Channel
from ..config cimport Config
from ..s2b cimport s2b

from .. import error

from libc.stdlib cimport malloc, free
import signal

cdef class Loop:
    cdef tll_processor_loop_t * _ptr

    def __cinit__(self, name=None, bare=False, config={}):
        name = name or ''
        name = s2b(name)
        if bare:
            self._ptr = NULL

        if isinstance(config, dict):
            config = Config.from_dict(config)
        elif not isinstance(config, Config):
            raise TypeError("Invalid config argument: {}".format(config))
        self._ptr = tll_processor_loop_new_cfg((<Config>config)._ptr)
        if self._ptr == NULL:
            raise error.TLLError("Failed to init processor loop")

    def __dealloc__(self):
        if self._ptr != NULL:
            tll_processor_loop_free(self._ptr)
        self._ptr = NULL

    @property
    def fd(self):
        return tll_processor_loop_get_fd(self._ptr)

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

    def step(self, timeout : float = 0):
        error.wrap(tll_processor_loop_step(self._ptr, int(timeout * 1000)), "tll_processor_loop_step failed")

    def run(self, timeout):
        error.wrap(tll_processor_loop_run(self._ptr, int(timeout * 1000)), "tll_processor_loop_run failed")

    def run_signal(self, timeout, signals=[signal.SIGINT]):
        cdef size_t sigsize = len(signals)
        if sigsize == 0:
            return self.run(timeout)
        cdef int * array = <int *>malloc(sizeof(int) * sigsize)
        for i in range(sigsize):
            array[i] = signals[i]
        try:
            error.wrap(tll_processor_loop_run_signal(self._ptr, int(timeout * 1000), array, sigsize), "tll_processor_loop_run failed")
        finally:
            free(array)

    @property
    def pending(self):
        return tll_processor_loop_pending(self._ptr) != 0

    @property
    def stop(self):
        return tll_processor_loop_stop_get(self._ptr)

    @stop.setter
    def stop(self, v):
        tll_processor_loop_stop_set(self._ptr, v)
