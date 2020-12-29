#!/usr/bin/env python
# vim: sts=4 sw=4 et

from .config cimport *
from .error import TLLError
from .s2b cimport *
from libc.errno cimport EAGAIN
from libc.stdlib cimport malloc, free
from .conv import getT

__default_tag = object()

cdef class Config:
    def __init__(self, bare=False):
        pass

    def __cinit__(self, bare=False):
        self._ptr = NULL
        if not bare:
            self._ptr = tll_config_new()

    def __dealloc__(self):
        if self._ptr != NULL:
            tll_config_unref(self._ptr)
        self._ptr = NULL

    @staticmethod
    cdef Config wrap(tll_config_t * ptr, int ref = False):
        r = Config(bare=True)
        if ref:
            tll_config_ref(ptr)
        r._ptr = ptr
        return r

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
        return Config.wrap(tll_config_copy(self._ptr))

    __copy__ = copy
    def __deepcopy__(self, memo):
        return self.copy()

    def sub(self, path, create=False, throw=True):
        p = s2b(path)
        cdef tll_config_t * cfg = tll_config_sub(self._ptr, p, len(p), 1 if create else 0)
        if cfg == NULL:
            if throw:
                raise KeyError("Sub-config {} not found".format(path))
            return
        return Config.wrap(cfg)

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
        if isinstance(value, Config):
            return self.set_config(key, value)
        elif callable(value):
            return self.set_callback(key, value)
        k = s2b(key)
        v = s2b(value)
        r = tll_config_set(self._ptr, k, len(k), v, len(v))
        if r:
            raise TLLError("Failed to set key {}".format(key), r)

    def set_config(self, key, value):
        k = s2b(key)
        r = tll_config_set_config(self._ptr, k, len(k), (<Config>value)._ptr, 0)
        if r:
            raise TLLError("Failed to set sub config {}".format(key), r)

    def set_callback(self, key, value):
        raise NotImplemented()

    def _get(self):
        if tll_config_value(self._ptr) == 0: return None
        cdef int len = 0;
        cdef char * buf = tll_config_get_copy(self._ptr, NULL, 0, &len)
        if buf == NULL:
            return None
        try:
            return b2s(buf[:len])
        finally:
            tll_config_value_free(buf)

    def get(self, key=None, default=__default_tag):
        if key is None: return self._get()
        k = s2b(key)
        cdef tll_config_t * cfg = tll_config_sub(self._ptr, k, len(k), 0)
        if cfg == NULL:
            if default == __default_tag:
                raise KeyError("Key {} not found".format(key))
            return default
        return Config.wrap(cfg).get()

    def getT(self, key, default):
        return getT(self, key, default)

    def remove(self, key, recursive=False):
        k = s2b(key)
        tll_config_del(self._ptr, k, len(k), 1 if recursive else 0)

    def has(self, key):
        k = s2b(key)
        return tll_config_has(self._ptr, k, len(k))

    def browse(self, mask, subpath=False, cb=None):
        m = s2b(mask)
        class appender(list):
            def __init__(self, sub):
                self.sub = sub
            def __call__(self, k, v):
                if v.value() or self.sub:
                    self.append((k, v.get()))

        _cb = appender(subpath) if cb is None else cb
        tll_config_browse(self._ptr, m, len(m), browse_cb, <void *>_cb)
        if cb is None:
            return list(_cb)

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

cdef int browse_cb(const char * key, int klen, const tll_config_t *value, void * data):
    cb = <object>data
    cfg = Config.wrap(<tll_config_t *>value, ref=True)
    cb(b2s(key[:klen]), cfg)
    return 0

cdef class Url(Config):
    def __init__(self, cfg = None):
        if not isinstance(cfg, Config):
            Config.__init__(self)
            return
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
