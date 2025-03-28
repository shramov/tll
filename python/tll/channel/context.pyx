#!/usr/bin/env python
# vim: sts=4 sw=4 et

from libc.string cimport memset
from libc.errno cimport EINVAL, EAGAIN
from cpython.ref cimport Py_INCREF, Py_DECREF
from .impl cimport *
from .channel cimport *
from .common import State, Type
from ..config cimport Config, Url
from ..error import TLLError
from ..s2b cimport *
from ..stat cimport List as StatList
from ..logger cimport tll_logger_free, tll_logger_copy, Logger
from ..logger import Logger

import importlib
import pathlib
import sys
import warnings

cdef class Impl:
    def __cinit__(self, object ctype):
        memset(&self.impl, 0, sizeof(tll_channel_impl_t))
        self.impl.init = &_py_init
        self.impl.free = &_py_free
        self.impl.open = &_py_open
        self.impl.close = &_py_close
        self.impl.process = &_py_process
        self.impl.post = &_py_post
        self.impl.scheme = &_py_scheme
        self.impl.data = <void *>ctype
        self.name_bytes = s2b(getattr(ctype, "PROTO") or ctype.__name__)
        self.impl.name = self.name_bytes
        self.impl.version = TLL_CHANNEL_IMPL_V0

cdef class Internal:
    def __cinit__(self):
        memset(&self.internal, 0, sizeof(tll_channel_internal_t))
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

    @property
    def stat(self): return self.stat_obj

    @stat.setter
    def stat(self, stat):
        if self.stat_obj != None:
            raise RuntimeError("Can not change stat block")
        self.stat_obj = stat
        self.internal.stat = &self.stat_obj.block

    @property
    def logger(self):
        return None

    @logger.setter
    def logger(self, l):
        if not isinstance(l, Logger):
            raise ValueError(f'Invalid argument type, expected Logger got {type(l)}: {l}')
        cdef tll_logger_t * ptr = (<Logger> l).ptr
        if self.internal.logger == ptr:
            return
        if self.internal.logger:
            tll_logger_free(self.internal.logger)
        self.internal.logger = tll_logger_copy(ptr)

    @property
    def dump(self): return self.internal.dump

    @dump.setter
    def dump(self, v): self.internal.dump = v

    cdef callback(self, const tll_msg_t * msg):
        return tll_channel_callback(&self.internal, msg)

cdef class Context:
    def __cinit__(self):
        self._ptr = NULL

    def __dealloc__(self):
        if self._ptr:
            tll_channel_context_free(self._ptr)

    def __init__(self, cfg=None, _wrap=False):
        if _wrap: return
        cdef tll_config_t * cptr = NULL
        if cfg is not None:
            if not isinstance(cfg, Config):
                raise RuntimeError("cfg must be None or Config object, got {}".format(type(cfg)))
            cptr = (<Config>(cfg))._ptr
        self._ptr = tll_channel_context_new(cptr)
        if self._ptr == NULL:
            raise RuntimeError("Failed to create context")
        self.register_loader()

    def __richcmp__(Context self, Context other, int op):
        return bool(richcmp(<intptr_t>self._ptr, <intptr_t>other._ptr, op))

    def Channel(self, *a, **kw):
        kw['context'] = self
        return Channel(*a, **kw)

    @staticmethod
    cdef Context wrap(tll_channel_context_t * ptr):
        r = Context(_wrap=True)
        r._ptr = tll_channel_context_ref(ptr)
        return r

    @property
    def config(self):
        cdef tll_config_t * cfg = tll_channel_context_config(self._ptr)
        return Config.wrap(cfg)

    @property
    def config_defaults(self):
        cdef tll_config_t * cfg = tll_channel_context_config_defaults(self._ptr)
        return Config.wrap(cfg)

    @property
    def stat_list(self):
        cdef tll_stat_list_t * stat = tll_channel_context_stat_list(self._ptr)
        return StatList.wrap(stat)

    def get(self, name):
        n = s2b(name)
        cdef tll_channel_t * c = tll_channel_get(self._ptr, n, len(n))
        if c is NULL:
            return None
        return Channel.wrap(c)

    def load(self, path, symbol='', config=None):
        if isinstance(path, pathlib.Path):
            path = str(path)
        cdef const tll_config_t * cfg = NULL
        if config:
            cfg = (<Config>config)._ptr
        p = s2b(path)
        s = s2b(symbol)
        # XXX: Temporary warning
        if symbol == 'channel_module':
            warnings.warn('Passing "channel_module" to load() is deprecated, omit second argument', DeprecationWarning)
        r = tll_channel_module_load_cfg(self._ptr, p, s, cfg)
        if r:
            raise TLLError("Failed to load {}:{}".format(path, symbol), r)

    def scheme_load(self, url, cache=True):
        b = s2b(url)
        cdef const tll_scheme_t * s = tll_channel_context_scheme_load(self._ptr, b, len(b), 1 if cache else 0)
        if s == NULL:
            raise TLLError("Failed to load scheme from '{}'".format(url))
        return Scheme.wrap(s)

    def alias(self, name, alias):
        bname = s2b(name)
        if isinstance(alias, Config):
            r = tll_channel_alias_register_url(self._ptr, bname, (<Config>alias)._ptr)
        else:
            balias = s2b(alias)
            r = tll_channel_alias_register(self._ptr, bname, balias, len(balias))
        if r:
            raise TLLError(f"Failed to register alias {name}")

    def alias_unregister(self, name, alias):
        bname = s2b(name)
        if isinstance(alias, Config):
            r = tll_channel_alias_unregister_url(self._ptr, bname, (<Config>alias)._ptr)
        else:
            balias = s2b(alias)
            r = tll_channel_alias_unregister(self._ptr, bname, balias, len(balias))
        if r:
            raise TLLError(f"Failed to unregister alias {name}")

    def register(self, obj, proto=None):
        if proto is None:
            proto = obj.PROTO
        proto = s2b(proto)
        cdef Impl impl = getattr(obj, '_TLL_IMPL', None)
        if impl is None or impl.impl.data != <void *>obj:
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

    def register_loader(self):
        if self.has_impl('python'): return
        self.register(PyLoader)
        self.register(PyPrefixLoader)

    def has_impl(self, proto : str):
        p = s2b(proto)
        return tll_channel_impl_get(self._ptr, p) != NULL

def channel_cast(channel, klass=None):
    if not isinstance(channel, Channel):
        raise TypeError('Not a channel object')
    cdef tll_channel_t * ptr = (<Channel>channel)._ptr
    if _py_bad_channel(ptr):
        raise TypeError("Not a python channel object")
    if klass is None:
        return <object>ptr.data
    impl = getattr(klass, '_TLL_IMPL', None)
    if impl is None or not isinstance(impl, Impl):
        raise TypeError("Invalid or missing _TLL_IMPL")
    if ptr.impl != &(<Impl>impl).impl:
        raise TypeError("Expected type {}, got {}".format((<Impl>impl).name_bytes, ptr.impl.name))
    return <object>ptr.data

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
        cfg = Config.wrap_const(curl, ref=True)
        url = Url()
        url.proto = cfg.get('tll.proto', '')
        url.host = cfg.get('tll.host', '')
        for k,v in cfg.browse("**"):
            url[k] = v or ''

        ctype = <object>(channel.impl.data)
        pyc = ctype(Context.wrap(ctx), intr)
        channel.internal = &intr.internal
        intr.internal.self = channel

        r = pyc.init(url, master=Channel.wrap(parent))
        if isinstance(r, Impl):
            channel.internal = NULL
            intr.internal.self = NULL
            pyc = None
            channel.impl = &(<Impl>r).impl
            return EAGAIN
        else:
            r = _py_check_return(r)
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

cdef int _py_open(tll_channel_t * channel, const tll_config_t *cfg) with gil:
    if _py_bad_channel(channel): return EINVAL
    pyc = <object>(channel.data)
    try:
        return _py_check_return(pyc.open(Config.wrap(<tll_config_t *>cfg, ref=True)))
    except:
        try:
            log = Logger("tll.channel.python")
            log.exception("Failed to open channel '{}'", pyc)
        except:
            pass
        return EINVAL

cdef int _py_close(tll_channel_t * channel, int force) with gil:
    if _py_bad_channel(channel): return EINVAL
    pyc = <object>(channel.data)
    try:
        return _py_check_return(pyc.close(force != 0))
    except:
        try:
            log = Logger("tll.channel.python")
            log.exception("Failed to close channel '{}'", pyc)
        except:
            pass
        return EINVAL

cdef const tll_scheme_t * _py_scheme(const tll_channel_t * channel, int stype) with gil:
    if _py_bad_channel(channel): return NULL
    pyc = <object>(channel.data)
    try:
        s = pyc.scheme_get(Type(stype))
        if s is None:
            return NULL
        if not isinstance(s, Scheme):
            return NULL
        return (<Scheme>(s))._ptr
    except:
        try:
            log = Logger("tll.channel.python")
            log.exception("Failed to get scheme from channel '{}'", pyc)
        except:
            pass
        return NULL

cdef int _py_post(tll_channel_t * channel, const tll_msg_t *msg, int flags) with gil:
    if _py_bad_channel(channel): return EINVAL
    pyc = <object>(channel.data)
    try:
        return _py_check_return(pyc.post(CMessage.wrap(msg), flags))
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

cdef Impl pychannel_lookup(object module):
    log = Logger("tll.channel.python")
    path = None
    if not module:
        return log.fail(None, "Need python parameter")
    module = b2s(module)
    if '/' in module:
        path, module = module.rsplit('/', 1)
    if ':' not in module:
        return log.fail(None, "Invalid module parameter '{}': need 'module:Class'".format(module))
    module, klass = module.split(':', 1)
    if path and path not in sys.path:
        sys.path.insert(0, path)
    else:
        path = None
    cdef Impl impl = None
    try:
        m = importlib.import_module(module)
        obj = getattr(m, klass)
        impl = getattr(obj, '_TLL_IMPL', None)
        print(f"Impl: {impl}, object: {obj}\n")
        if impl is None or impl.impl.data != <void *>obj:
            impl = Impl(obj)
            obj._TLL_IMPL = impl
        return impl
    finally:
        if path:
            sys.path.remove(path)

cdef api:
    cdef tll_channel_impl_t * tll_pychannel_lookup(const char *s) with gil:
        cdef Impl impl = pychannel_lookup(b2s(s))
        if impl is None:
            return NULL
        return &impl.impl

class PyLoader:
    PROTO = 'python'

    def __init__(self, *a, **kw):
        pass

    def init(self, url, master=None):
        return pychannel_lookup(url.get('python', None))

class PyPrefixLoader(PyLoader):
    PROTO = 'python+'
