#!/usr/bin/env python
# vim: sts=4 sw=4 et

from libc.string cimport memset
from libc.errno cimport EINVAL
from cpython.ref cimport Py_INCREF, Py_DECREF
from .impl cimport *
from .channel cimport *
from .common import State
from ..config cimport Config
from ..error import TLLError
from ..s2b cimport *
from ..logger import Logger
from ..url import Url

cdef class Impl:
    cdef tll_channel_impl_t impl
    cdef object name_bytes

    def __cinit__(self, object ctype):
        memset(&self.impl, 0, sizeof(tll_channel_impl_t))
        self.impl.init = &_py_init
        self.impl.free = &_py_free
        self.impl.open = &_py_open
        self.impl.close = &_py_close
        self.impl.process = &_py_process
        self.impl.post = &_py_post
        if getattr(ctype, 'PREFIX', 0):
            self.impl.prefix = 1
        self.impl.data = <void *>ctype
        self.name_bytes = s2b(getattr(ctype, "PROTO", ctype.__name__))
        self.impl.name = self.name_bytes

cdef class Internal:
    def __cinit__(self):
        tll_channel_internal_init(&self.internal)
        self.internal.fd = -1
        self.name = ''
        self.config = Config()
        self.internal.config = self.config._ptr

    def __dealloc__(self):
        tll_channel_internal_clear(&self.internal)

    @property
    def state(self): return State(self.internal.state)

    @state.setter
    def state(self, v): self.internal.state = v

    @property
    def name(self): return self.name_str

    @name.setter
    def name(self, name):
        if self.name_bytes == s2b(name):
            return
        self.name_bytes = s2b(name)
        self.name_str = b2s(self.name_bytes)
        self.internal.name = self.name_bytes

    @property
    def config(self): return self.config

    cdef callback(self, const tll_msg_t * msg):
        return tll_channel_callback(&self.internal, msg)

cdef class Context:
    def __cinit__(self):
        self._ptr = NULL

    def __dealloc__(self):
        if self._ptr:
            tll_channel_context_free(self._ptr)

    def __init__(self, cfg=None, __wrap=False):
        if __wrap: return
        cdef tll_config_t * cptr = NULL
        if cfg is not None:
            if not isinstance(cfg, Config):
                raise RuntimeError("cfg must be None or Config object, got {}".format(type(cfg)))
            cptr = (<Config>(cfg))._ptr
        self._ptr = tll_channel_context_new(cptr)

    def __richcmp__(Context self, Context other, int op):
        return bool(richcmp(<intptr_t>self._ptr, <intptr_t>other._ptr, op))

    def Channel(self, *a, **kw):
        kw['context'] = self
        return Channel(*a, **kw)

    @staticmethod
    cdef Context wrap(tll_channel_context_t * ptr):
        r = Context(__wrap=True)
        r._ptr = tll_channel_context_ref(ptr)
        return r

    @property
    def config_defaults(self):
        cdef tll_config_t * cfg = tll_channel_context_config_defaults(self._ptr)
        return Config.wrap(cfg)

    def get(self, name):
        n = s2b(name)
        cdef tll_channel_t * c = tll_channel_get(self._ptr, n, len(n))
        if c is NULL:
            return None
        return Channel.wrap(c)

    def load(self, path, symbol='module'):
        p = s2b(path)
        s = s2b(symbol)
        r = tll_channel_module_load(self._ptr, p, s)
        if r:
            raise TLLError("Failed to load {}:{}".format(path, symbol), r)

    def scheme_load(self, url, cache=True):
        b = s2b(url)
        cdef const tll_scheme_t * s = tll_channel_context_scheme_load(self._ptr, b, len(b), 1 if cache else 0)
        if s == NULL:
            raise TLLError("Failed to load scheme from '{}'".format(url))
        return Scheme.wrap(s)

    def register(self, obj, proto=None):
        if proto is None:
            proto = obj.PROTO
        proto = s2b(proto)
        impl = getattr(obj, '_TLL_IMPL', None)
        if impl is None:
            impl = Impl(obj)
            obj._TLL_IMPL = impl
        if not isinstance(impl, Impl):
            raise TypeError("Invalid _TLL_IMPL in {}: {}".format(obj, type(impl)))
        r = tll_channel_impl_register(self._ptr, &(<Impl>impl).impl, proto)
        if r:
            raise TLLError("Register {} failed".format(obj), r)

    def unregister(self, obj, proto=None):
        if proto is None:
            proto = obj.PROTO
        impl = getattr(obj, '_TLL_IMPL', None)
        if impl is None:
            raise ValueError("No _TLL_IMPL in {}".format(obj))
        if not isinstance(impl, Impl):
            raise TypeError("Invalid _TLL_IMPL in {}: {}".format(obj, type(impl)))
        proto = s2b(proto)
        r = tll_channel_impl_unregister(self._ptr, &(<Impl>impl).impl, proto)
        if r:
            raise TLLError("Unregister {} failed".format(obj), r)

cdef int _py_bad_channel(const tll_channel_t * channel):
    if channel == NULL or channel.data == NULL or channel.impl.free != &_py_free: return 1
    return 0

cdef int _py_check_return(object obj):
    if obj is None: return 0
    if isinstance(obj, int):
        return <int>obj
    cdef int r = <int> obj
    return r

cdef int _py_init(tll_channel_t * channel, const tll_config_t *curl, tll_channel_t * parent, tll_channel_context_t *ctx) with gil:
    if channel == NULL or channel.impl.free != &_py_free: return 0
    if channel.impl.data == NULL: return 0
    cdef Internal intr = Internal()
    try:
        cfg = Config.wrap(<tll_config_t *>curl, ref=True)
        url = Url()
        url.proto = cfg.get('tll.proto', '')
        url.host = cfg.get('tll.host', '')
        for k,v in cfg.browse('**'):
            url[k] = v or ''

        ctype = <object>(channel.impl.data)
        pyc = ctype(Context.wrap(ctx), intr)
        channel.internal = &intr.internal
        intr.internal.self = channel

        r = _py_check_return(pyc.init(url, master=Channel.wrap(parent)))
        if r:
            return r
        channel.data = <void *>pyc
        Py_INCREF(pyc)
        return 0
    except Exception as e:
        try:
            log = Logger("tll.channel.python")
            log.exception("Failed to init channel '{}'", str(url))
        except:
            pass
        return EINVAL

cdef void _py_free(tll_channel_t * channel) with gil:
    if _py_bad_channel(channel): return
    pyc = <object>(channel.data)
    try:
        pyc.free()
        if channel.internal != NULL:
            tll_channel_internal_clear(channel.internal)
        Py_DECREF(pyc)
    except:
        try:
            log = Logger("tll.channel.python")
            log.exception("Failed to free channel '{}'", pyc)
        except:
            pass

cdef int _py_open(tll_channel_t * channel, const char *str, size_t len) with gil:
    if _py_bad_channel(channel): return EINVAL
    pyc = <object>(channel.data)
    try:
        return _py_check_return(pyc.open(str[:len]))
    except:
        try:
            log = Logger("tll.channel.python")
            log.exception("Failed to open channel '{}'", pyc)
        except:
            pass
        return EINVAL

cdef int _py_close(tll_channel_t * channel) with gil:
    if _py_bad_channel(channel): return EINVAL
    pyc = <object>(channel.data)
    try:
        return _py_check_return(pyc.close())
    except:
        try:
            log = Logger("tll.channel.python")
            log.exception("Failed to close channel '{}'", pyc)
        except:
            pass
        return EINVAL

cdef int _py_post(tll_channel_t * channel, const tll_msg_t *msg, int flags) with gil:
    if _py_bad_channel(channel): return EINVAL
    pyc = <object>(channel.data)
    try:
        return _py_check_return(pyc.post(Message.wrap(msg), flags))
    except:
        try:
            log = Logger("tll.channel.python")
            log.exception("Failed to post to channel '{}'", pyc)
        except:
            pass
        return EINVAL

cdef int _py_process(tll_channel_t * channel, long timeout, int flags) with gil:
    if _py_bad_channel(channel): return EINVAL
    pyc = <object>(channel.data)
    try:
        return _py_check_return(pyc.process(timeout, flags))
    except:
        try:
            log = Logger("tll.channel.python")
            log.exception("Failed to process channel '{}'", pyc)
        except:
            pass
        return EINVAL
