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
#from cpython.ref cimport Py_INCREF

class StateMessage:
    def __init__(self, state):
        self.type = C.Type.State
        self.msgid = state
        self.data = None

cdef class Base:
    PROTO = None
    PREFIX = False

    cdef Internal internal
    cdef Context context

    def __init__(self, context, internal):
        self.context = context
        self.internal = internal

    def __dealloc__(self):
        #self.log.info("Deallocate")
        self.dealloc()

    def dealloc(self):
        if self.internal is None or self.internal.state == C.State.Destroy:
            return
        self.internal.state = C.State.Destroy

    def _callback(self, msg):
        cdef Internal intr = <Internal>self.internal
        if isinstance(msg, C.Message):
            return intr.callback((<C.Message>msg)._ptr)
        cdef tll_msg_t cmsg
        memset(&cmsg, 0, sizeof(tll_msg_t))

        cmsg.type = int(msg.type)
        cmsg.msgid = int(msg.msgid)
        if hasattr(msg, 'addr'): cmsg.addr = msg.addr
        if msg.data is not None:
            cmsg.size = len(msg.data)

        intr.callback(&cmsg)

    @property
    def config(self): return self.internal.config

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
    def fd(self, v): self.internal.internal.fd = v

    @property
    def caps(self): return C.Caps(self.internal.caps)

    @property
    def dcaps(self): return C.DCaps(self.internal.dcaps)

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
        self._open(props)

    def close(self):
        self.log.info("Close channel")
        self.state = C.State.Closing
        try:
            self._close()
        finally:
            self.state = C.State.Closed

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
