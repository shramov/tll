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
    def __cinit__(self, bare=False):
        self._ptr = NULL
        if not bare:
            self._ptr = tll_config_new()

    def __dealloc__(self):
        if self._ptr != NULL:
            tll_config_unref(self._ptr)
        self._ptr = NULL

    @staticmethod
    cdef Config wrap(tll_config_t * ptr):
        r = Config(bare=True)
        r._ptr = ptr
        return r

    @classmethod
    def load(self, path):
        p = s2b(path)
        cdef tll_config_t * cfg = tll_config_load(p, len(p))
        if cfg == NULL:
            raise TLLError("Failed to load {}".format(path))
        return Config.wrap(cfg)

    def sub(self, path, create=False):
        p = s2b(path)
        cdef tll_config_t * cfg = tll_config_sub(self._ptr, p, len(p), 1 if create else 0)
        if cfg == NULL:
            raise KeyError("Sub-config {} not found".format(path))
        return Config.wrap(cfg)

    def merge(self, cfg):
        if not isinstance(cfg, Config):
            raise TypeError("Merge argument must be Config object, got {}".format(Config))
        r = tll_config_merge(self._ptr, (<Config>cfg)._ptr)
        if r:
            raise TLLError("Failed to merge config", r)

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
        r = tll_config_set_config(self._ptr, k, len(k), (<Config>value)._ptr)
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

    def browse(self, mask, subpath=False):
        r = ([], subpath)
        m = s2b(mask)
        tll_config_browse(self._ptr, m, len(m), list_cb, <void *>r)
        return r[0]

    def __contains__(self, key): return self.has(key)
    def __getitem__(self, key): return self.get(key)
    def __setitem__(self, key, value): self.set(key, value)
    def __delitem__(self, key): self.remove(key)

cdef int list_cb(const char * key, int klen, const tll_config_t *value, void * data):
    l = <object>data
    tll_config_ref(value)
    cfg = Config.wrap(<tll_config_t *>value)
    if cfg.value() or l[1]:
        l[0].append((b2s(key[:klen]), cfg.get()))
    return 0
