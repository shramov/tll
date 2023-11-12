#!/usr/bin/env python
# vim: sts=4 sw=4 et

from .buffer cimport *
from .config cimport *
from .s2b cimport *

from cpython.ref cimport Py_INCREF, Py_DECREF
from libc.errno cimport ENOENT, EINVAL
from libc.stdlib cimport malloc
from libc.string cimport memcpy

from .conv import getT
from .error import TLLError
from .logger import Logger

DEFAULT_TAG = object()

cdef object _check_error(int r, object message):
    if r == ENOENT:
        raise KeyError(message)
    elif r:
        raise TLLError(message, r)
    return

cdef class Callback:
    cdef object _cb

    def __init__(self, cb):
        self._cb = cb

    @property
    def callback(self): return self._cb

    def __call__(self):
        return self._cb()

cdef char * pyvalue_callback(int * length, void * data) noexcept with gil:
    cdef object cb = <object>data
    cdef Py_buffer * buf
    cdef char * ptr
    try:
        v = cb()
        if v is None:
            return NULL
        elif isinstance(v, bytes):
            pass
        else:
            if not isinstance(v, str):
                v = str(v)
            v = v.encode('utf-8')
        v = memoryview(v)

        buf = PyMemoryView_GET_BUFFER(v)
        ptr = <char *>malloc(buf.len)
        memcpy(ptr, buf.buf, buf.len)
        length[0] = buf.len
        return ptr
    except:
        return NULL

cdef void pyvalue_callback_free(tll_config_value_callback_t f, void * data) noexcept with gil:
    if f != pyvalue_callback:
        return
    cdef cb = <object>data
    if not isinstance(cb, Callback):
        return
    Py_DECREF(cb)

cdef class Config:
    def __init__(self, bare=False):
        pass

    def __cinit__(self, bare=False):
        self._ptr = NULL
        self._const = 0
        if not bare:
            self._ptr = tll_config_new()

    def __dealloc__(self):
        if self._ptr != NULL:
            tll_config_unref(self._ptr)
        self._ptr = NULL

    @staticmethod
    cdef Config wrap(tll_config_t * ptr, int ref = False, int _const = False):
        r = Config(bare=True)
        if ref:
            tll_config_ref(ptr)
        r._ptr = ptr
        r._const = _const
        return r

    @staticmethod
    cdef Config wrap_const(const tll_config_t * ptr, int ref = False):
        return Config.wrap(<tll_config_t *>(ptr), ref, True)

    @classmethod
    def load(self, path):
        p = s2b(path)
        cdef tll_config_t * cfg = tll_config_load(p, len(p))
        if cfg == NULL:
            raise TLLError("Failed to load {}".format(path))
        return Config.wrap(cfg)

    @classmethod
    def load_data(self, proto, data):
        p = s2b(proto)
        d = s2b(data)
        cdef tll_config_t * cfg = tll_config_load_data(p, len(p), d, len(d))
        if cfg == NULL:
            raise TLLError("Failed to load '{}' from '{}'".format(proto, data))
        return Config.wrap(cfg)

    def copy(self):
        return Config.wrap(tll_config_copy(self._ptr), False)

    __copy__ = copy
    def __deepcopy__(self, memo):
        return self.copy()

    @property
    def root(self):
        return Config.wrap(<tll_config_t *>tll_config_root(self._ptr), False, True)

    @property
    def parent(self):
        cdef const tll_config_t * ptr = tll_config_parent(self._ptr)
        if ptr:
            return Config.wrap(<tll_config_t *>ptr, False, True)

    def sub(self, path, create=False, throw=True):
        p = s2b(path)
        cdef tll_config_t * cfg = tll_config_sub(self._ptr, p, len(p), 1 if create else 0)
        if cfg == NULL:
            if throw:
                raise KeyError("Sub-config {} not found".format(path))
            return
        return Config.wrap(cfg, False, self._const)

    def merge(self, cfg, overwrite=True):
        if not isinstance(cfg, Config):
            raise TypeError("Merge argument must be Config object, got {}".format(Config))
        r = tll_config_merge(self._ptr, (<Config>cfg)._ptr, overwrite)
        if r:
            raise TLLError("Failed to merge config", r)

    def process_imports(self, key):
        k = s2b(key)
        r = tll_config_process_imports(self._ptr, k, len(k))
        if r:
            raise TLLError("Failed to process imports {}".format(key), r)

    def value(self):
        return bool(tll_config_value(self._ptr))

    def set(self, key, value):
        if self._const:
            raise RuntimeError("Can not modify const Config")
        if isinstance(value, Config):
            return self.set_config(key, value)
        elif callable(value):
            return self.set_callback(key, value)
        elif value is None:
            if self.has(key):
                self.remove(key)
            return
        k = s2b(key)
        v = s2b(value)
        r = tll_config_set(self._ptr, k, len(k), v, len(v))
        if r:
            raise TLLError("Failed to set key {}".format(key), r)

    def set_link(self, key, value):
        if self._const:
            raise RuntimeError("Can not modify const Config")
        k = s2b(key)
        v = s2b(value)
        r = tll_config_set_link(self._ptr, k, len(k), v, len(v))
        if r:
            raise TLLError("Failed to set link {} -> {}".format(key, value), r)

    def set_config(self, key, value):
        if self._const:
            raise RuntimeError("Can not modify const Config")
        k = s2b(key)
        r = tll_config_set_config(self._ptr, k, len(k), (<Config>value)._ptr, 0)
        if r:
            raise TLLError("Failed to set sub config {}".format(key), r)

    def set_callback(self, key, value):
        if self._const:
            raise RuntimeError("Can not modify const Config")
        k = s2b(key)
        cb = Callback(value)
        r = tll_config_set_callback(self._ptr, k, len(k), pyvalue_callback, <void *>cb, pyvalue_callback_free)
        if r:
            raise TLLError(f"Failed to set callback at {key}: {value}", r)
        Py_INCREF(cb)

    def _get(self, key=None, decode=True):
        cdef int len = 0;
        cdef const char * ckey = NULL
        if key is not None:
            key = s2b(key)
            ckey = key
        cdef char * buf = tll_config_get_copy(self._ptr, ckey, -1, &len)
        if buf == NULL:
            return None
        try:
            if decode:
                return b2s(buf[:len])
            else:
                return buf[:len]
        finally:
            tll_config_value_free(buf)

    def get(self, key=None, default=DEFAULT_TAG, decode=True):
        r = self._get(key, decode=decode)
        if r is None:
            if default == DEFAULT_TAG:
                raise KeyError("Key {} not found".format(key))
            return default
        return r

    def get_url(self, key=None, default=DEFAULT_TAG):
        cdef Config sub = self
        if key is not None:
            sub = self.sub(key, create=False, throw=False)
            if sub is None:
                if default == DEFAULT_TAG:
                    raise KeyError(f"Key {key} not found")
                return default
        r = Config.wrap(tll_config_get_url(sub._ptr, NULL, 0))
        error = r.get(default=None)
        if error is not None:
            raise ValueError(f"Invalid url at '{key}': {error}")
        return Url(r)

    def getT(self, key, default):
        return getT(self, key, default)

    def unlink(self, key):
        if self._const:
            raise RuntimeError("Can not modify const Config")
        k = s2b(key)
        _check_error(tll_config_unlink(self._ptr, k, len(k)), f'Failed to unlink "{key}"')

    def unset(self, key):
        if self._const:
            raise RuntimeError("Can not modify const Config")
        k = s2b(key)
        _check_error(tll_config_unset(self._ptr, k, len(k)), f'Failed to unset "{key}"')

    def remove(self, key):
        if self._const:
            raise RuntimeError("Can not modify const Config")
        k = s2b(key)
        _check_error(tll_config_remove(self._ptr, k, len(k)), f'Failed to remove "{key}"')

    def has(self, key):
        k = s2b(key)
        return tll_config_has(self._ptr, k, len(k))

    def browse(self, mask, subpath=False, cb=None):
        m = s2b(mask)
        class ExcWrapper:
            def __init__(self, cb):
                self.cb = cb
            def __call__(self, k, v):
                self.cb(k, v)

        class appender(list):
            def __init__(self, sub):
                self.sub = sub
            def __call__(self, k, v):
                if v.value() or self.sub:
                    self.append((k, v.get(default=None)))

        _cb = ExcWrapper(appender(subpath) if cb is None else cb)
        if tll_config_browse(self._ptr, m, len(m), browse_cb, <void *>_cb):
            if cb is None:
                raise RuntimeError(f"Browse failed on key '{_cb.exc[0]}'") from _cb.exc[1]
        if cb is None:
            return list(_cb.cb)

    @staticmethod
    def from_dict(d):
        r = Config()
        for k,v in d.items():
            if isinstance(v, dict):
                v = Config.from_dict(v)
            elif isinstance(v, (list, tuple)):
                v = Config.from_dict({f'{i:04d}':x for (i, x) in enumerate(v)})
            r.set(k, v)
        return r

    def as_dict(self):
        if self.value():
            return self.get()
        class cb:
            def __init__(self):
                self.r = {}

            def __call__(self, k, v):
                if self.r == {} and k == '0000':
                    self.r = []
                if isinstance(self.r, dict):
                    self.r[k] = v.as_dict()
                else:
                    self.r.append(v.as_dict())
        _cb = cb()
        self.browse('*', cb=_cb)
        return _cb.r

    def __contains__(self, key): return self.has(key)
    def __getitem__(self, key): return self.get(key)
    def __setitem__(self, key, value): self.set(key, value)
    def __delitem__(self, key): self.remove(key)

cdef int browse_cb(const char * key, int klen, const tll_config_t *value, void * data) noexcept:
    cb = <object>data
    pykey = None
    try:
        cfg = Config.wrap(<tll_config_t *>value, ref=True, _const=True)
        pykey = b2s(key[:klen])
        cb(pykey, cfg)
    except Exception as e:
        Logger('tll.python').exception("Exception in browse callback {}", cb)
        cb.exc = (pykey, e)
        return EINVAL
    return 0

cdef class Url(Config):
    def __init__(self, cfg = None):
        if cfg is None:
            Config.__init__(self)
            return
        elif not isinstance(cfg, Config):
            raise ValueError("Url can be constructed from Config, got {}".format(cfg))
        Config.__init__(self, bare=True)
        self._ptr = (<Config>cfg)._ptr
        tll_config_ref(self._ptr)

    def copy(self):
        return Url(Config.copy(self))

    __copy__ = copy
    def __deepcopy__(self, memo):
        return self.copy()

    @classmethod
    def parse(self, s):
        return Url(Config.load_data("url", s))

    @property
    def proto(self): return self.get('tll.proto', '')

    @proto.setter
    def proto(self, v): self['tll.proto'] = v

    @property
    def host(self): return self.get('tll.host', '')

    @host.setter
    def host(self, v): self['tll.host'] = v

    def __str__(self):
        return '{}://{};{}'.format(self.proto, self.host, ';'.join(['{}={}'.format(k,v) for k,v in self.browse('**') if k not in {'tll.proto', 'tll.host'}]))
