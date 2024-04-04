#!/usr/bin/env python
# vim: sts=4 sw=4 et

from .config cimport Config
from .logger cimport *
from .s2b cimport *
from .error import TLLError

from cpython.ref cimport Py_INCREF, Py_DECREF
from libc.errno cimport EINVAL
from libc.string cimport memset

import enum
import logging
import traceback
import weakref

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

    @property
    def name_bytes(self): return tll_logger_name(self.ptr)

    @property
    def name(self): return b2s(self.name)

tll2logging = { TLL_LOGGER_TRACE: logging.DEBUG
              , TLL_LOGGER_DEBUG: logging.DEBUG
              , TLL_LOGGER_INFO:  logging.INFO
              , TLL_LOGGER_WARNING: logging.WARNING
              , TLL_LOGGER_ERROR: logging.ERROR
              , TLL_LOGGER_CRITICAL: logging.CRITICAL
              }

logging2tll = {v:k for k,v in tll2logging.items()}

class TLLLogRecord(logging.LogRecord):
    def __init__(self, name, level, msg):
        super().__init__(name, level, "TLL", 0, msg, None, None)

cdef class PyLog:
    cdef tll_logger_impl_t impl

    def __cinit__(self):
        memset(&self.impl, 0, sizeof(tll_logger_impl_t))
        self.impl.log = self.pylog
        self.impl.log_new = self.pylog_new
        self.impl.log_free = self.pylog_free
        self.impl.configure = self.pylog_configure
        self.impl.release = self.pylog_release
        self.impl.user = <void *>self

    def reg(self):
        tll_logger_register(&self.impl)

    def unreg(self):
        tll_logger_register(NULL)

    @property
    def registered(self):
        return &self.impl == tll_logger_impl_get()

    @staticmethod
    cdef int pylog(long long ts, const char * category, tll_logger_level_t level, const char * data, size_t size, void * obj) with gil:
        o = <object>obj
        o.handle(TLLLogRecord(o.name, tll2logging.get(level, logging.INFO), b2s(data[:size])))

    @staticmethod
    cdef void * pylog_new(tll_logger_impl_t * impl, const char * category) with gil:
        o = <PyLog>(impl.user)
        l = logging.getLogger(b2s(category))
        Py_INCREF(l)
        return <void *>l

    @staticmethod
    cdef void pylog_free(tll_logger_impl_t * impl, const char * category, void * obj) with gil:
        if obj == NULL: return
        o = <object>obj
        Py_DECREF(o)

    @staticmethod
    cdef void pylog_release(tll_logger_impl_t * impl) with gil:
        pass

    @staticmethod
    cdef int pylog_configure(tll_logger_impl_t * impl, const tll_config_t * _cfg) with gil:
        cdef Config config = Config.wrap_const(_cfg, ref=True)
        if config.sub('python', throw=False) is None:
            return 0

        try:
            lcfg = config.sub('python').as_dict()
            lcfg['version'] = int(lcfg.get('version', '1'))
            print(lcfg)
            logging.config.dictConfig(lcfg)
        except Exception as e:
            Logger("tll.logger.python").exception("Failed to configure python logging")
            return EINVAL

pylog = PyLog()

class TLLHandler(logging.Handler):
    def __init__(self, *a, **kw):
        self._cache = {}
        super().__init__(*a, **kw)

    def emit(self, record):
        if isinstance(record, TLLLogRecord):
            return
        try:
            log = self._cache.get(record.name)
            if log is not None:
                log = log()
            if log is None:
                log = logging.getLogger(record.name)
                log._tll_logger = Logger(record.name)
                self._cache[record.name] = weakref.ref(log)
            log._tll_logger.log(logging2tll.get(record.levelno, Level.Info), record.getMessage())
        except:
            self.handleError(record)
def init():
    for h in logging.root.handlers:
        if isinstance(h, TLLHandler):
            raise RuntimeError("Python logs are redirect to TLL, can not install TLL -> Python redirection")
    pylog.reg()

def configure(config):
    if config is None:
        return
    if isinstance(config, dict):
        config = Config.from_dict(config)

    tll_logger_config((<Config>config)._ptr);

def basicConfig():
    if pylog.registered:
        raise RuntimeError("TLL logs are redirected to Python, can not install Python -> TLL redirection")
    logging.basicConfig(level=logging.DEBUG, handlers=[TLLHandler(logging.DEBUG)])
