#!/usr/bin/env python3
# vim: sts=4 sw=4 et

from cpython.exc cimport PyErr_CheckSignals

from .processor cimport *
from .loop cimport *
from ..channel.channel cimport Channel
from ..channel.context cimport Context
from ..config cimport Config
from .. import error
from libc.errno cimport ECONNRESET

cdef class Processor(Channel):
    cdef Context _context

    def __init__(self, config, context=None):
        if context is None:
            context = Context()
        elif not isinstance(context, Context):
            raise ValueError("Invalid context parameter")

        if not isinstance(config, Config):
            raise ValueError("Invalid config parameter")
        self._context = context

        tll_processor_init(self._context._ptr)
        if 'tll.proto' not in config:
            prefix = config.get('processor.format', '')
            config['tll.proto'] = 'processor' if not prefix else prefix + "+processor"
        Channel.__init__(self, config, context=self._context)

    def step(self, timeout=0.1):
        cdef int r = 0
        cdef long ms = int(timeout * 1000)
        with nogil:
            r = tll_processor_loop_step(tll_processor_loop(self._ptr), ms)
        error.wrap(r, "Processor step failed")

    def run(self, timeout=0.1):
        cdef int r = 0
        cdef long ms = int(timeout * 1000)
        cdef tll_processor_loop_t * loop = tll_processor_loop(self._ptr)
        with nogil:
            while tll_processor_loop_stop_get(loop) == 0:
                tll_processor_loop_step(loop, ms)
                with gil:
                    PyErr_CheckSignals()

    @property
    def workers(self):
        r = []
        cdef const tll_channel_list_t * ptr = tll_processor_workers(self._ptr)
        while ptr != NULL:
            r.append(Worker.wrap(ptr.channel))
            ptr = ptr.next
        return r

cdef class Worker(Channel):
    @staticmethod
    cdef Worker wrap(tll_processor_worker_t * ptr):
        r = Worker(None)
        r._ptr = ptr
        return r

    def step(self, timeout=0.1):
        cdef int r = 0
        cdef long ms = int(timeout * 1000)
        with nogil:
            r = tll_processor_loop_step(tll_processor_worker_loop(self._ptr), ms)
        error.wrap(r, "Processor worker step failed")

    def run(self, timeout=0.1):
        cdef int r = 0
        cdef long ms = int(timeout * 1000)
        with nogil:
            r = tll_processor_loop_run(tll_processor_worker_loop(self._ptr), ms)
        error.wrap(r, "Processor worker run failed")

def main(cfg):
    p = Processor(cfg)
    for w in p.workers():
        pass
    p.run()
