#!/usr/bin/env python
# vim: sts=4 sw=4 et

from .channel cimport *
from ..config cimport Config
from ..scheme cimport Scheme
from ..s2b cimport *
from libc.errno cimport EAGAIN, EINVAL
from libc.stdint cimport intptr_t
from libc.stdlib cimport malloc, free
from libc.string cimport memcpy, memset
from cpython cimport Py_buffer
from cpython.buffer cimport *
from cpython.ref cimport Py_INCREF, Py_DECREF
from ..buffer cimport *
from os import strerror
from ..error import TLLError
from collections import namedtuple
import weakref
from .. import scheme
from .context cimport Context
from .context import Context
from .common cimport *
from . import common

import enum
if not hasattr(enum, 'IntFlag'):
    enum.IntFlag = int

State = common.State

class Type(enum.Enum):
    Data = TLL_MESSAGE_DATA
    State = TLL_MESSAGE_STATE
    Channel = TLL_MESSAGE_CHANNEL
    Control = TLL_MESSAGE_CONTROL
    def __int__(self): return self.value
_Type = Type

class MsgChannel(enum.Enum):
    Update = TLL_MESSAGE_CHANNEL_UPDATE
    Add = TLL_MESSAGE_CHANNEL_ADD
    Delete = TLL_MESSAGE_CHANNEL_DELETE
    UpdateFd = TLL_MESSAGE_CHANNEL_UPDATE_FD
    def __int__(self): return self.value

class MsgMask(enum.IntFlag):
    All = TLL_MESSAGE_MASK_ALL
    Data = TLL_MESSAGE_MASK_DATA
    Control = TLL_MESSAGE_MASK_CONTROL
    State = TLL_MESSAGE_MASK_STATE
    Channel = TLL_MESSAGE_MASK_CHANNEL

class Caps(enum.IntFlag):
    Input = TLL_CAPS_INPUT
    Output = TLL_CAPS_OUTPUT
    InOut = TLL_CAPS_INOUT

    ExBit = TLL_CAPS_EX_BIT
    Proxy = TLL_CAPS_PROXY
    Custom = TLL_CAPS_CUSTOM
_Caps = Caps

class DCaps(enum.IntFlag):
    PollIn = TLL_DCAPS_POLLIN
    PollOut = TLL_DCAPS_POLLOUT
    PollMask = TLL_DCAPS_POLLMASK

    Process = TLL_DCAPS_PROCESS
    Pending = TLL_DCAPS_PENDING
    Suspend = TLL_DCAPS_SUSPEND
    SuspendPermanent = TLL_DCAPS_SUSPEND_PERMANENT
_DCaps = DCaps

cdef class Channel:
    Caps = _Caps
    DCaps = _DCaps
    State = common.State

    def __init__(self, url, master=None, context=None, **kw):
        pass

    def __cinit__(self, url, master=None, context=None, **kw):
        self._ptr = NULL
        self._own = False
        self._callbacks = {}
        self._scheme_cache = None
        if url is None:
            return
        cdef tll_channel_t * pptr = NULL
        if isinstance(master, Channel):
            pptr = (<Channel>master)._ptr
        elif master is not None:
            url += ';master={}'.format(master)

        cdef tll_channel_context_t * cptr = NULL
        if isinstance(context, Context):
            cptr = (<Context>context)._ptr
        elif context is not None:
            kw['context'] = context

        for k,v in sorted(kw.items()):
            url += ';{}={}'.format(k,v)
        self._own = True
        b = s2b(url)
        self._ptr = tll_channel_new(cptr, b, len(b), pptr, NULL)
        if self._ptr == NULL:
            raise TLLError("Init channel with url {} failed".format(url))

    def __dealloc__(self):
        self.free()

    def free(self):
        if self._own and self._ptr is not NULL:
            tll_channel_free(self._ptr)
        self._ptr = NULL
        self._own = False
        self._callbacks = set()
        self._scheme_cache = None

    @staticmethod
    cdef Channel wrap(tll_channel_t * ptr):
        r = Channel(url=None)
        r._ptr = ptr
        return r

    def open(self, params='', **kw):
        for k,v in sorted(kw.items()):
            sep = ';' if params else ''
            params += sep + '{}={}'.format(k,v)
        p = s2b(params)
        r = tll_channel_open(self._ptr, p, len(p))
        if r:
            raise TLLError("Open failed", r)

    def close(self):
        tll_channel_close(self._ptr)

    def process(self):
        r = tll_channel_process(self._ptr, 0, 0)
        if r and r != EAGAIN:
            raise TLLError("Process failed", r)

    cdef _post(self, const tll_msg_t * msg, int flags):
        r = tll_channel_post(self._ptr, msg, flags)
        if r:
            raise TLLError("Post failed", r)
        return r


    def post(self, data, type=Type.Data, msgid=None, seq=None, name=None):
        if isinstance(data, Message):
            if type == data.type and msgid is None and seq is None:
                return self._post((<Message>data)._ptr, 0)

        cdef tll_msg_t msg
        memset(&msg, 0, sizeof(msg))
        if not isinstance(data, scheme.Data) and hasattr(data, 'msgid'):
            msg.type = data.type
            msg.msgid = data.msgid
            msg.seq = data.seq
            msg.addr = data.addr
        if name is not None:
            msg.msgid = self.scheme[name].msgid
        if type is not None: msg.type = int(type)
        if msgid is not None: msg.msgid = msgid
        if seq is not None: msg.seq = seq
        mv = None
        if isinstance(data, dict):
            data = self.scheme[name].object(**data)
        if isinstance(data, scheme.Data):
            if msgid is None:
                msg.msgid = data.SCHEME.msgid
            data = data.pack()
        if data is not None:
            mv = memoryview(data)
            msg.data = PyMemoryView_GET_BUFFER(mv).buf
            msg.size = len(mv)
        return self._post(&msg, 0)

    def callback_add(self, cb, mask = MsgMask.All, skip_ref=False):
        t = (cb, mask)
        if t in self._callbacks:
            return
        r = tll_channel_callback_add(self._ptr, msg_cb, <void *>t[0], mask)
        if r:
            raise TLLError("Callback add failed", r)
        #if not skip_ref:
        self._callbacks[t] = t

    def callback_del(self, cb, mask = MsgMask.All, skip_ref=False):
        t = self._callbacks.get((cb, mask), None)
        if t is None:
            return
        r = tll_channel_callback_del(self._ptr, msg_cb, <void *>t[0], mask)
        if r:
            raise TLLError("Callback del failed", r)
        #if not skip_ref:
        del self._callbacks[t]

    @property
    def state(self): return State(tll_channel_state(self._ptr))

    @property
    def name(self): return b2s(tll_channel_name(self._ptr) or b"")

    @property
    def caps(self): return Caps(tll_channel_caps(self._ptr))

    @property
    def dcaps(self): return DCaps(tll_channel_dcaps(self._ptr))

    @property
    def fd(self):
        fd = tll_channel_fd(self._ptr)
        return None if fd == -1 else fd

    @property
    def context(self): return Context.wrap(tll_channel_context(self._ptr))

    @property
    def config(self):
        return Config.wrap(tll_channel_config(self._ptr))

    @property
    def children(self):
        cdef tll_channel_list_t * l = tll_channel_children(self._ptr)
        r = []
        while l != NULL:
            r.append(Channel.wrap(l.channel))
            l = l.next
        return r

    @property
    def scheme(self):
        cdef const tll_scheme_t * ptr = tll_channel_scheme(self._ptr, TLL_MESSAGE_DATA)
        if ptr == NULL:
            self._scheme_cache = None
            return None
        if not self._scheme_cache or not self._scheme_cache.same(ptr):
            self._scheme_cache = Scheme.wrap(ptr, ref=True)
        return self._scheme_cache

    def __hash__(self): return hash(<intptr_t>self._ptr)

    def __richcmp__(Channel self, Channel other, int op):
        return bool(richcmp(<intptr_t>self._ptr, <intptr_t>other._ptr, op))

class Callback:
    def __init__(self, obj, func='__call__', weak=True):
        self.func = func
        if weak:
            self.ref = weakref.ref(obj)
        else:
            self.obj = obj
            self.ref = lambda s: s.obj

    def __call__(self, *a, **kw):
        o = self.ref()
        if o is None: return # Exception?
        return getattr(o, self.func)(*a, **kw)

"""
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
        """

MessageTuple = namedtuple('MessageTuple', ["type", "msgid", "seq", "addr", "data"])
MessageTuple.Type = Type

cdef class Message:
    Type = _Type

    def __cinit__(self):
        self._ptr = NULL

    @staticmethod
    cdef Message wrap(const tll_msg_t * ptr):
        m = Message(None)
        m._ptr = ptr
        return m

    def __getbuffer__(self, Py_buffer *buf, int flags):
        if self._ptr is NULL:
            raise RuntimeError("Message is uninitialized")
        PyBuffer_FillInfo(buf, self, <void *>self._ptr.data, self._ptr.size, 1, flags)

    def __releasebuffer__(self, Py_buffer *buf):
        pass

    @property
    def seq(self): return self._ptr.seq if self._ptr else None

    @property
    def msgid(self): return self._ptr.msgid if self._ptr else None

    @property
    def type(self): return Type(self._ptr.type) if self._ptr else None

    @property
    def addr(self): return self._ptr.addr if self._ptr else None

    @property
    def data(self): return memoryview(self) if self._ptr else None

    def copy(self):
        if not self._ptr: raise RuntimeError("Message is uninitialized")
        return MessageTuple(Type(self._ptr.type), self._ptr.msgid, self._ptr.seq, self._ptr.addr, memoryview(memoryview(self).tobytes()))

    def clone(self): return self.copy()

cdef int msg_cb(const tll_channel_t * c, const tll_msg_t *msg, void * user) with gil:
    o = <object>user
    try:
        if isinstance(o, weakref.ref):
            o = o()
        if o is None: return 0
        r = o(Channel.wrap(<tll_channel_t *>c), Message.wrap(msg))
        if r is not None:
            return r
        return 0
    except:
        import traceback
        traceback.print_exc()
        return EINVAL
