#!/usr/bin/env python
# vim: sts=4 sw=4 et

from .channel cimport *
from ..config cimport Config, Url
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
from ..chrono import TimePoint, Resolution
from os import strerror
from ..error import TLLError
from .. import error
from collections import namedtuple
import weakref
from .. import scheme
from ..logger import Logger
from .context cimport Context
from .context import Context
from .common cimport *
from . import common

import enum
if not hasattr(enum, 'IntFlag'):
    enum.IntFlag = int

State = common.State
_Type = Type = common.Type

class MsgChannel(enum.Enum):
    Update = TLL_MESSAGE_CHANNEL_UPDATE
    Add = TLL_MESSAGE_CHANNEL_ADD
    Delete = TLL_MESSAGE_CHANNEL_DELETE
    UpdateFd = TLL_MESSAGE_CHANNEL_UPDATE_FD
    def __int__(self): return self.value

class MsgMask(enum.IntFlag):
    All = <unsigned>(TLL_MESSAGE_MASK_ALL) & (~TLL_MESSAGE_MASK_CHANNEL)
    Data = TLL_MESSAGE_MASK_DATA
    Control = TLL_MESSAGE_MASK_CONTROL
    State = TLL_MESSAGE_MASK_STATE
    Channel = TLL_MESSAGE_MASK_CHANNEL
_MsgMask = MsgMask

class Caps(enum.IntFlag):
    Zero = 0
    Input = TLL_CAPS_INPUT
    Output = TLL_CAPS_OUTPUT
    InOut = TLL_CAPS_INOUT

    Custom = TLL_CAPS_CUSTOM
    Parent = TLL_CAPS_PARENT
    Proxy = TLL_CAPS_PROXY
    LongClose = TLL_CAPS_LONG_CLOSE
_Caps = Caps

class DCaps(enum.IntFlag):
    Zero = 0
    PollIn = TLL_DCAPS_POLLIN
    PollOut = TLL_DCAPS_POLLOUT
    PollMask = TLL_DCAPS_POLLMASK

    Process = TLL_DCAPS_PROCESS
    Pending = TLL_DCAPS_PENDING
    Suspend = TLL_DCAPS_SUSPEND
    SuspendPermanent = TLL_DCAPS_SUSPEND_PERMANENT
_DCaps = DCaps

class PostFlags(enum.IntFlag):
    Zero = 0
    More = TLL_POST_MORE
    Urgent = TLL_POST_URGENT
_PostFlags = PostFlags

class RunGuard:
    def __init__(self, active=True):
        self._active = active

    def disable(self):
        self._active = False

    def enable(self):
        self._active = True

    @property
    def active(self):
        return self._active

cdef int ccallback_destroy_cb(const tll_channel_t * c, const tll_msg_t *msg, void * user) noexcept with gil:
    if msg.type != TLL_MESSAGE_STATE or msg.msgid != TLL_STATE_DESTROY:
        return 0
    o = <object>user
    try:
        o.destroy()
    except:
        pass
    return 0

cdef int ccallback_cb(const tll_channel_t * c, const tll_msg_t *msg, void * user) noexcept with gil:
    cb = <CCallback>user
    func = None
    cdef CMessage cmsg
    try:
        cmsg = CMessage.wrap(msg)
        func = cb._func_ref()
        if func is None:
            return 0
        chan = cb._chan_ref()
        if chan is None:
            chan = Channel.wrap(<tll_channel_t *>c)
        r = func(chan, cmsg)
        cmsg._ptr = NULL
        if r is not None:
            return r
    except:
        log = Logger('tll.python')
        log.exception("Exception in callback {}", func)
        return EINVAL
    return 0

cdef class CCallback:
    cdef tll_channel_t * _channel
    cdef void * _func_ptr
    cdef object _func_ref
    cdef object _chan_ref
    cdef unsigned int _mask
    cdef unsigned int _mask_state

    def __cinit__(self):
        self._reset()

    def __dealloc__(self):
        self._destroy()

    cdef _reset(self):
        self._channel = NULL
        self._func_ptr = NULL
        self._chan_ref = self._func_ref = None

    cdef _destroy(self):
        if self._channel == NULL:
            return

        cdef int r0 = tll_channel_callback_del(self._channel, ccallback_cb, <void *>self, self._mask)
        cdef int r1 = tll_channel_callback_del(self._channel, ccallback_destroy_cb, <void *>self, self._mask_state)

        self._reset()

        if r0 or r1:
            raise TLLError("Callback del failed", r0 if r0 else r1)

    def __init__(self, channel : Channel, func, mask = MsgMask.All):
        self._mask = mask
        self._mask_state = MsgMask.State
        self._chan_ref = weakref.ref(channel)

        if isinstance(func, weakref.ref):
            func = func()
        if func is None:
            return

        self._func_ptr = <void *>func
        self._func_ref = weakref.ref(func, lambda ref: self.destroy())

    def add(self):
        cdef Channel c = <Channel>(self._chan_ref())
        self._channel = c._ptr

        cdef int r

        try:
            r = tll_channel_callback_add(self._channel, ccallback_cb, <void *>self, self._mask)
            if r:
                raise TLLError("Callback add failed", r)

            r = tll_channel_callback_add(self._channel, ccallback_destroy_cb, <void *>self, MsgMask.State)
            if r:
                raise TLLError("Callback add failed", r)
        except:
            self._reset()
            raise

    def destroy(self):
        self._destroy()

    def __hash__(self):
        return hash((<intptr_t>self._func_ptr, self._mask))

cdef class Channel:
    Caps = _Caps
    DCaps = _DCaps
    State = common.State
    MsgMask = _MsgMask
    Type = _Type
    PostFlags = _PostFlags

    def __cinit__(self):
        self._ptr = NULL
        self._own = False
        self._callbacks = {}
        self._scheme_cache = [None, None]

    def __init__(self, url, master=None, context=None, **kw):
        if url is None:
            return

        cdef Url curl = None
        if isinstance(url, Config):
            curl = Url(url.copy())
        else:
            curl = Url.parse(url)

        cdef tll_channel_t * pptr = NULL
        if isinstance(master, Channel):
            pptr = (<Channel>master)._ptr
        elif master is not None:
            curl["master"] = master

        cdef tll_channel_context_t * cptr = NULL
        if isinstance(context, Context):
            cptr = (<Context>context)._ptr
        elif context is not None:
            kw['context'] = context

        for k,v in kw.items():
            curl[k] = v

        self._own = True
        self._ptr = tll_channel_new_url(cptr, curl._ptr, pptr, NULL)
        if self._ptr == NULL:
            raise TLLError("Init channel with url {} failed".format(url))

    def __dealloc__(self):
        Channel.free(self)

    def free(self):
        if self._ptr is NULL:
            return
        if self._callbacks != None:
            for o in self._callbacks.keys():
                o.destroy()
            self._callbacks = {}
        self._scheme_cache = None
        if self._own and self._ptr is not NULL:
            tll_channel_free(self._ptr)
        self._ptr = NULL
        self._own = False

    @staticmethod
    cdef Channel wrap(tll_channel_t * ptr):
        r = Channel(url=None)
        r._ptr = ptr
        return r

    def open(self, params='', **kw):
        cfg = None
        if isinstance(params, Config):
            if len(kw):
                cfg = params.copy()
            else:
                cfg = params
        elif isinstance(params, dict):
            cfg = Config()
            for k,v in params.items():
                cfg[k] = v
        else:
            cfg = Config()
            if params:
                for s in params.split(';'):
                    k,v = s.split('=', 1)
                    cfg[k] = v
        for k,v in kw.items():
            cfg[k] = v
        r = tll_channel_open_cfg(self._ptr, (<Config>cfg)._ptr)
        if r:
            raise TLLError("Open failed", r)

    def close(self, force : bool = False):
        tll_channel_close(self._ptr, force)

    def process(self):
        r = tll_channel_process(self._ptr, 0, 0)
        if r and r != EAGAIN:
            raise TLLError("Process failed", r)

    cdef _post(self, const tll_msg_t * msg, int flags):
        r = tll_channel_post(self._ptr, msg, flags)
        if r:
            raise TLLError("Post failed", r)
        return r

    def post(self, data, type=None, msgid=None, seq=None, name=None, addr=None, time=None, flags=None):
        if isinstance(data, CMessage):
            if (type is not None and type == data.type) and msgid is None and seq is None:
                return self._post((<CMessage>data)._ptr, flags or 0)

        cdef tll_msg_t msg
        memset(&msg, 0, sizeof(msg))
        msg.type = int(Type.Data)
        if not isinstance(data, scheme.Data) and hasattr(data, 'msgid'):
            msg.type = data.type
            msg.msgid = data.msgid
            msg.seq = data.seq
            msg.addr.i64 = data.addr
            msg.time = TimePoint(msg.time, Resolution.ns, type=int).value
            data = data.data
        if name is not None:
            msg.msgid = self._scheme(type)[name].msgid
        if type is not None: msg.type = int(type)
        if msgid is not None: msg.msgid = msgid
        if seq is not None: msg.seq = seq
        if addr is not None: msg.addr.i64 = addr
        if time is not None: msg.time = TimePoint(time, Resolution.ns, type=int).value
        mv = None
        if isinstance(data, dict):
            data = self._scheme(type)[name].object(**data)
        if isinstance(data, scheme.Data):
            if msgid is None:
                msg.msgid = data.SCHEME.msgid
            data = data.pack()
        if data is not None:
            mv = memoryview(data)
            msg.data = PyMemoryView_GET_BUFFER(mv).buf
            msg.size = len(mv)
        return self._post(&msg, flags or 0)

    def callback_add(self, cb, mask = MsgMask.All, store=True):
        obj = CCallback(self, cb, mask)
        if obj in self._callbacks:
            return
        obj.add()
        #if not skip_ref:
        if store:
            self._callbacks[obj] = (obj, cb)
        return obj

    def callback_del(self, cb, mask = MsgMask.All):
        obj = CCallback(self, cb, mask)
        obj, fn = self._callbacks.pop(obj, (None, None))
        if fn is None:
            return
        obj.destroy()

    def suspend(self):
        error.wrap(tll_channel_suspend(self._ptr), "Suspend failed")

    def resume(self):
        error.wrap(tll_channel_resume(self._ptr), "Resume failed")

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
        return Config.wrap_const(tll_channel_config(self._ptr))

    @property
    def children(self):
        cdef const tll_channel_list_t * l = tll_channel_children(self._ptr)
        r = []
        while l != NULL:
            r.append(Channel.wrap(l.channel))
            l = l.next
        return r

    @property
    def scheme(self):
        return self._scheme_load(Type.Data)

    @property
    def scheme_control(self):
        return self._scheme_load(Type.Control)

    def scheme_load(self, stype):
        return self._scheme_load(int(stype))

    @property
    def _scheme_cache(self):
        return self._scheme_cache

    cdef object _scheme_load(self, int t):
        cdef const tll_scheme_t * ptr = tll_channel_scheme(self._ptr, t)
        if ptr == NULL:
            self._scheme_cache[t] = None
            return None
        cdef Scheme scheme = <Scheme>self._scheme_cache[t]
        if scheme is None or not scheme.same(ptr):
            scheme = self._scheme_cache[t] = Scheme.wrap(ptr, ref=True)
        return scheme

    def _scheme(self, t):
        if t in (Type.Data, Type.Control):
            return self._scheme_load(int(t))
        elif t is None:
            return self._scheme_load(Type.Data)
        else:
            raise ValueError("No scheme defined for message type {}".format(t))

    def unpack(self, msg):
        s = self._scheme(msg.type)
        if s is None:
            raise ValueError("Channel has no scheme for message type {}".format(msg.type))
        return s.unpack(msg)

    def reflection(self, msg):
        s = self._scheme(msg.type)
        if s is None:
            raise ValueError("Channel has no scheme for message type {}".format(msg.type))
        return s.reflection(msg)

    def __hash__(self): return hash(<intptr_t>self._ptr)

    def __richcmp__(Channel self, Channel other, int op):
        return bool(richcmp(<intptr_t>self._ptr, <intptr_t>other._ptr, op))

    def __repr__(self):
        ptr = <intptr_t>self._ptr
        if self._ptr == NULL:
            return "<Channel ptr=NULL>"
        return f"<Channel ptr=0x{ptr:x} name={self.name}>"

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

class Message:
    Type = _Type
    __slots__ = ["type", "msgid", "seq", "addr", "data", "time"]

    def __init__(self, msgid, data=b'', type : _Type =_Type.Data, seq : int = 0, addr : int = 0, time : TimePoint = None):
        self.type, self.msgid, self.seq, self.addr, self.data, self.time = type, msgid, seq, addr, memoryview(data), time

    def __str__(self):
        return f"<Message type={self.type} msgid={self.msgid} seq={self.seq} addr={self.addr} size={len(self.data)}>"

cdef class CMessage:
    Type = _Type

    def __cinit__(self):
        self._ptr = NULL

    @staticmethod
    cdef CMessage wrap(const tll_msg_t * ptr):
        m = CMessage(None)
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
    def addr(self): return self._ptr.addr.i64 if self._ptr else None

    @property
    def time(self): return TimePoint(self._ptr.time, Resolution.ns, type=int) if self._ptr else None

    @property
    def data(self): return memoryview(self) if self._ptr else None

    def copy(self):
        if not self._ptr: raise RuntimeError("Message is uninitialized")
        return Message(type = Type(self._ptr.type), msgid = self._ptr.msgid, seq = self._ptr.seq, addr = self.addr, time = self.time, data = memoryview(memoryview(self).tobytes()))

    def clone(self): return self.copy()
