#!/usr/bin/env python
# cython: language_level=3
# vim: sts=4 sw=4 et

from .impl cimport *
from . cimport channel as C
from . import channel as C
from ..config cimport Config
from .context cimport Context, Internal
from libc.string cimport memset
from libc.errno cimport EINVAL
from ..url import *
from ..s2b import b2s, s2b
from ..logger import Logger
from .. import error
#from cpython.ref cimport Py_INCREF

class StateMessage:
    def __init__(self, state):
        self.type = C.Type.State
        self.msgid = state
        self.data = None

cdef class Base:
    PROTO = None
    PREFIX = False

    Caps = C.Caps
    DCaps = C.DCaps
    State = C.State

    cdef Internal internal
    cdef Context context
    cdef object _children

    def __init__(self, context, internal):
        self.context = context
        self.internal = internal
        self._children = set()

    def __dealloc__(self):
        #self.log.info("Deallocate")
        self.dealloc()

    def dealloc(self):
        for c in list(self._children):
            try:
                self._child_del(c)
            except:
                self.log.debug("Failed to del child '{}' in destroy", c)
        self._children = set()
        if self.internal is None or self.internal.state == C.State.Destroy:
            return
        self.internal.state = C.State.Destroy

    def _callback(self, msg):
        if isinstance(msg, C.Message):
            return self.internal.callback((<C.Message>msg)._ptr)
        cdef tll_msg_t cmsg
        memset(&cmsg, 0, sizeof(tll_msg_t))

        cmsg.type = int(msg.type)
        cmsg.msgid = int(msg.msgid)
        if hasattr(msg, 'addr'): cmsg.addr = msg.addr
        if msg.data is not None:
            cmsg.size = len(msg.data)

        self.internal.callback(&cmsg)

    @property
    def config(self): return self.internal.config

    @property
    def context(self): return self.context

    @property
    def state(self): return C.State(self.internal.state)

    @state.setter
    def state(self, v):
        if not isinstance(v, C.State):
            raise ValueError("Expected tll.channel.State, got {} ({})".format(v, type(v)))
        if self.internal.state == v: return
        self.log.info("State {} -> {}", self.state.name, v.name)
        self.internal.state = v
        self._callback(StateMessage(v))
        self.config['state'] = v.name

    @property
    def fd(self): return self.internal.internal.fd

    @fd.setter
    def fd(self, v): self._update_fd(self, v)

    @property
    def caps(self): return C.Caps(self.internal.internal.caps)

    @caps.setter
    def caps(self, v): self.internal.internal.caps = v

    @property
    def dcaps(self): return C.DCaps(self.internal.internal.dcaps)

    @property
    def name(self): return self.internal.name

    def init(self, props, master=None):
        url = Url.from_string(b2s(props))
        self.internal.state = C.State.Closed
        self.config["state"] = "Closed"

        self.internal.name = str(url.get("name", "noname"))
        self.log = Logger("tll.channel.{}".format(self.name))
        self.log.info("Init channel")
        self._init(url, master)

    def free(self):
        self.log.info("Destroy channel")
        try:
            self._free()
        except:
            pass
        self.dealloc()

    def open(self, props):
        self.log.info("Open channel")
        self.state = C.State.Opening
        self._update_dcaps(C.DCaps.Process)
        self._open(props)

    def close(self):
        self.log.info("Close channel")
        self.state = C.State.Closing
        try:
            self._close()
        finally:
            self.state = C.State.Closed
            self._update_dcaps(0, C.DCaps.Process | C.DCaps.PollMask)

    def process(self, timeout, flags):
        try:
            self._process(timeout, flags)
        except Exception as e:
            self.log.exception("Process failed")
            self.state = C.State.Error
            return EINVAL

    def post(self, msg, flags):
        self._post(msg, flags)

    def _init(self, props, master=None): pass
    def _free(self): pass

    def _open(self, props):
        self.state = C.State.Active
    def _close(self): pass

    def _process(self, timeout, flags): pass
    def _post(self, msg, flags): pass

    def _update_dcaps(self, caps, mask=None):
        if mask is None:
            mask = caps
        caps = C.DCaps(caps & mask)
        old = self.dcaps
        if old & mask == caps:
            return
        self.log.debug("Update caps: {!s} -> {!s}", old, caps)
        self.internal.internal.dcaps = (self.dcaps & ~mask) | caps
        cdef unsigned ccaps = self.dcaps

        cdef tll_msg_t msg
        memset(&msg, 0, sizeof(tll_msg_t))

        msg.type = C.Type.Channel
        msg.msgid = C.MsgChannel.Update
        msg.data = &ccaps
        msg.size = sizeof(ccaps)

        self.internal.callback(&msg)

    def _update_fd(self, fd : int):
        cdef int old = self.internal.internal.fd
        if old == fd:
            return fd
        self.log.debug("Update fd: {} -> {}", old, fd)
        self.internal.internal.fd = fd

        cdef tll_msg_t msg
        memset(&msg, 0, sizeof(tll_msg_t))

        msg.type = C.Type.Channel
        msg.msgid = C.MsgChannel.UpdateFd
        msg.data = &old
        msg.size = sizeof(old)

        self.internal.callback(&msg)

    def _child_add(self, c):
        self.log.info("Add child channel '{}'", c.name)
        cdef tll_channel_t * ptr = (<Channel>c)._ptr

        if c in self._children:
            raise KeyError("Channel {} is a child already".format(c.name))

        error.wrap(tll_channel_internal_child_add(&self.internal.internal, ptr, NULL, 0), "Child '{}' add failed", c.name)
        self._children.add(c)

    def _child_del(self, c):
        self.log.info("Delete child channel '{}'", c.name)
        cdef tll_channel_t * ptr = (<Channel>c)._ptr

        error.wrap(tll_channel_internal_child_del(&self.internal.internal, ptr, NULL, 0), "Child '{}' del failed", c.name)

        if c in self._children:
            self._children.remove(c)
