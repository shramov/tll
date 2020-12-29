#!/usr/bin/env python
# vim: sts=4 sw=4 et

from .config cimport Config
from .logger cimport *
from .s2b cimport *
from .error import TLLError

from cpython.ref cimport Py_INCREF, Py_DECREF
from libc.string cimport memset

import enum
import logging
import traceback

class Level(enum.Enum):
    Trace = TLL_LOGGER_TRACE
    Debug = TLL_LOGGER_DEBUG
    Info = TLL_LOGGER_INFO
    Warning = TLL_LOGGER_WARNING
    Error = TLL_LOGGER_ERROR
    Critical = TLL_LOGGER_CRITICAL
_Level = Level # Rename needed for python3.6

cdef class Logger:
    Level = _Level

    cdef tll_logger_t * ptr
    cdef char style

    def __cinit__(self, name, style='{'):
        n = s2b(name)
        self.ptr = tll_logger_new(n, len(n))
        if self.ptr == NULL:
            raise TLLError("Failed to create logger {}".format(name))
        if style not in ('{', '%'):
            raise TLLError("Invalid style: '{}', expected one of '{{', '%'".format(style))
        self.style = ord(style[0])

    def __dealloc__(self):
        if self.ptr:
            tll_logger_free(self.ptr)
        self.ptr = NULL

    def log(self, level, msg, *a, **kw):
        if a or kw:
            if self.style == b'{':
                msg = msg.format(*a, **kw)
            elif kw:
                msg = msg % kw
            else:
                msg = msg % a

        m = s2b(msg)
        if isinstance(level, Level):
            level = level.value
        r = tll_logger_log(self.ptr, level, m, len(m))
        if r:
            raise TLLError("Failed to log message '{}'".format(m), r)

    def trace(self, m, *a, **kw): return self.log(Level.Trace, m, *a, **kw)
    def debug(self, m, *a, **kw): return self.log(Level.Debug, m, *a, **kw)
    def info(self, m, *a, **kw): return self.log(Level.Info, m, *a, **kw)
    def warning(self, m, *a, **kw): return self.log(Level.Warning, m, *a, **kw)
    def error(self, m, *a, **kw): return self.log(Level.Error, m, *a, **kw)
    def critical(self, m, *a, **kw): return self.log(Level.Critical, m, *a, **kw)

    def fail(self, r, m, *a, **kw):
        self.log(Level.Error, m, *a, **kw)
        return r

    def exception(self, m, *a, **kw):
        msg = m.format(*a, **kw)
        msg += "\n" + traceback.format_exc()
        self.error(msg)

    warn = warning
    crit = critical

    @property
    def level(self): return Level(self.ptr.level)

    @level.setter
    def level(self, l): self.ptr.level = Level(l).value

tll2logging = { TLL_LOGGER_TRACE: logging.DEBUG
              , TLL_LOGGER_DEBUG: logging.DEBUG
              , TLL_LOGGER_INFO:  logging.INFO
              , TLL_LOGGER_WARNING: logging.WARNING
              , TLL_LOGGER_ERROR: logging.ERROR
              , TLL_LOGGER_CRITICAL: logging.CRITICAL
              }

cdef class PyLog:
    cdef tll_logger_impl_t impl

    def __cinit__(self):
        memset(&self.impl, 0, sizeof(tll_logger_impl_t))
        self.impl.log = self.pylog
        self.impl.log_new = self.pylog_new
        self.impl.log_free = self.pylog_free
        self.impl.user = <void *>self

    def reg(self):
        tll_logger_register(&self.impl)

    def unreg(self):
        tll_logger_register(NULL)

    @staticmethod
    cdef int pylog(long long ts, const char * category, tll_logger_level_t level, const char * data, size_t size, void * obj) with gil:
        o = <object>obj
        o.log(tll2logging.get(level, logging.INFO), b2s(data[:size]))

    @staticmethod
    cdef void * pylog_new(const char * category, tll_logger_impl_t * impl) with gil:
        o = <PyLog>(impl.user)
        l = logging.getLogger(b2s(category))
        Py_INCREF(l)
        return <void *>l

    @staticmethod
    cdef void pylog_free(const char * category, void * obj, tll_logger_impl_t * impl) with gil:
        if obj == NULL: return
        o = <object>obj
        Py_DECREF(o)

pylog = PyLog()

def init():
    pylog.reg()

def pyconfigure(config):
    levels = config.sub("levels", throw=False)
    if levels:
        tll_logger_config((<Config>levels)._ptr);

    if config is None or config.sub('python', throw=False) is None: return

    lcfg = config.sub('python').as_dict()
    lcfg['version'] = int(lcfg.get('version', '1'))
    print(lcfg)
    logging.config.dictConfig(lcfg)
