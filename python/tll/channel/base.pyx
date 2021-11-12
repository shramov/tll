#!/usr/bin/env python
# cython: language_level=3
# vim: sts=4 sw=4 et

from .impl cimport *
from . cimport channel as C
from . import channel as C
from ..config cimport Config
from ..scheme cimport Scheme
from .context cimport Context, Internal
from libc.string cimport memset
from libc.errno cimport EINVAL
from ..url import *
from ..s2b import b2s, s2b
from ..logger import Logger
from ..logger cimport TLL_LOGGER_INFO
from .. import error
from .. import scheme
from .. import stat
from .. cimport stat
#from cpython.ref cimport Py_INCREF
from cpython cimport Py_buffer
from ..buffer cimport PyMemoryView_GET_BUFFER
import enum

def StateMessage(state):
    return C.Message(type=C.Type.State, msgid = state)

class OpenPolicy(enum.Enum):
    Auto = enum.auto()
    Manual = enum.auto()
_OpenPolicy = OpenPolicy

class ClosePolicy(enum.Enum):
    Normal = enum.auto()
    Long = enum.auto()
_ClosePolicy = ClosePolicy

class ChildPolicy(enum.Enum):
    Never = enum.auto()
    Single = enum.auto()
    Many = enum.auto()
_ChildPolicy = ChildPolicy

class ProcessPolicy(enum.Enum):
    Normal = enum.auto()
    Never = enum.auto()
    Always = enum.auto()
    Custom = enum.auto()
_ProcessPolicy = ProcessPolicy

class MessageLogFormat(enum.Enum):
    Disable = TLL_MESSAGE_LOG_DISABLE
    Frame = TLL_MESSAGE_LOG_FRAME
    Text = TLL_MESSAGE_LOG_TEXT
    TextHex = TLL_MESSAGE_LOG_TEXT_HEX
    Scheme = TLL_MESSAGE_LOG_SCHEME
    def __int__(self): return self.value
_MessageLogFormat = MessageLogFormat

MessageLogFormatMap = {
    '': MessageLogFormat.Disable,
    'no': MessageLogFormat.Disable,
    'yes': MessageLogFormat.Text, # Keep in sync with tll/channel/base.h
    'frame': MessageLogFormat.Frame,
    'text': MessageLogFormat.Text,
    'text+hex': MessageLogFormat.TextHex,
    'scheme': MessageLogFormat.Scheme,
}

class Stat(stat.Base):
    FIELDS = [stat.Integer('rx', stat.Method.Sum)
             ,stat.Integer('rx', stat.Method.Sum, stat.Unit.Bytes)
             ,stat.Integer('tx', stat.Method.Sum)
             ,stat.Integer('tx', stat.Method.Sum, stat.Unit.Bytes)
             ]
_Stat = Stat

cdef class Base:
    PROTO = None

    OpenPolicy = _OpenPolicy
    ClosePolicy = _ClosePolicy
    ChildPolicy = _ChildPolicy
    ProcessPolicy = _ProcessPolicy
    MessageLogFormat = _MessageLogFormat

    OPEN_POLICY = OpenPolicy.Auto
    CLOSE_POLICY = ClosePolicy.Normal
    CHILD_POLICY = ChildPolicy.Never
    PROCESS_POLICY = ProcessPolicy.Normal

    Caps = C.Caps
    DCaps = C.DCaps
    State = C.State

    Stat = _Stat

    cdef Internal internal
    cdef Context context
    cdef Scheme scheme
    cdef object _children
    cdef object log
    cdef object _dump_mode

    def __init__(self, context, internal):
        self.context = context
        self.internal = internal
        self._children = set()

    def __dealloc__(self):
        #self.log.info("Deallocate")
        self.dealloc()

    def dealloc(self):
        for c in list(self._children or []):
            try:
                self._child_del(c)
            except:
                self.log.debug("Failed to del child '{}' in destroy", c)
        c = None
        self._children = set()
        if self.internal is None or self.internal.state == C.State.Destroy:
            return
        self.scheme = None
        self.state = C.State.Destroy

    def _callback(self, msg):
        if isinstance(msg, C.CMessage):
            return self.internal.callback((<C.CMessage>msg)._ptr)

        cdef tll_msg_t cmsg
        memset(&cmsg, 0, sizeof(tll_msg_t))

        if isinstance(msg, scheme.Data):
            cmsg.type = TLL_MESSAGE_DATA
            cmsg.msgid = msg.SCHEME.msgid

            data = msg.pack()
        else:
            cmsg.type = int(msg.type)
            cmsg.msgid = int(msg.msgid)

            addr = getattr(msg, 'addr', None)
            if addr: cmsg.addr.i64 = addr

            seq = getattr(msg, 'seq', None)
            if seq: cmsg.seq = int(seq)

            data = getattr(msg, 'data', None)

        cdef Py_buffer * buf = NULL
        if data is not None:
            data = memoryview(data)
            buf = PyMemoryView_GET_BUFFER(data)
            cmsg.size = buf.len
            cmsg.data = buf.buf

        self._log_msg("Recv", &cmsg)
        self.internal.callback(&cmsg)

    @property
    def config(self): return self.internal.config

    @property
    def context(self): return self.context

    @property
    def log(self): return self.log

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

    @property
    def scheme(self): return self.scheme

    @scheme.setter
    def scheme(self, v):
        self.scheme = v

    @property
    def stat(self): return self.internal.stat

    def init(self, url, master=None):
        Logger('tll.python').info('Init channel {}', url)
        self.internal.state = C.State.Closed
        self.config["state"] = "Closed"
        self.config['url'] = url.copy()

        self.internal.name = str(url.get("name", "noname"))
        self.log = Logger("tll.channel.{}".format(self.name))

        if self.CHILD_POLICY == ChildPolicy.Never:
            pass
        elif self.CHILD_POLICY == ChildPolicy.Single:
            self.caps |= self.Caps.Parent | self.Caps.Proxy;
        elif self.CHILD_POLICY == ChildPolicy.Many:
            self.caps |= self.Caps.Parent

        if self.CLOSE_POLICY == ClosePolicy.Long:
            self.caps |= self.Caps.LongClose

        self._scheme_url = url.get('scheme', None)

        dump = url.get('dump', '')
        self._dump_mode = MessageLogFormatMap.get(dump, None)
        if self._dump_mode is None:
            raise ValueError(f"Invalid 'dump' parameter: '{dump}'")

        stat = url.getT('stat', False)
        if stat:
            self.log.debug("Initialize stat with {}", self.Stat)
            self.internal.stat = self.Stat(self.name)

        self.log.info("Init channel")
        self._init(url, master)

    def free(self):
        self.log.info("Destroy channel")
        self.close(force=True)
        try:
            self._free()
        except:
            pass
        self.dealloc()

    def open(self, props):
        self.log.info("Open channel")
        self.state = C.State.Opening

        if self._scheme_url:
            self.scheme = self.context.scheme_load(self._scheme_url)

        if self.PROCESS_POLICY != self.ProcessPolicy.Never:
            self._update_dcaps(C.DCaps.Process)

        try:
            self._open(props)
        except:
            self.log.exception("Failed to open channel")
            self.state = C.State.Error
            return EINVAL

        if self.OPEN_POLICY == OpenPolicy.Auto:
            self.state = C.State.Active

    def close(self, force : bool = False):
        if self.state == C.State.Closed:
            return
        elif self.state == C.State.Closing and not force:
            return
        self.log.info("Close channel")
        self.state = C.State.Closing
        try:
            if self.CLOSE_POLICY == ClosePolicy.Long:
                self._close(force)
            else:
                self._close()
        finally:
            if force or self.CLOSE_POLICY == ClosePolicy.Normal:
                Base._close(self)

    def process(self, timeout, flags):
        try:
            self._process(timeout, flags)
        except Exception as e:
            self.log.exception("Process failed")
            self.state = C.State.Error
            return EINVAL

    def post(self, msg, flags):
        if self._dump_mode != self.MessageLogFormat.Disable and isinstance(msg, CMessage):
            self._log_msg("Post", (<CMessage>msg)._ptr)
        self._post(msg, flags)

    def _init(self, props, master=None): pass
    def _free(self): pass

    def _open(self, props): pass
    def _close(self, force : bool = False):
            self._update_dcaps(0, C.DCaps.Pending | C.DCaps.Process | C.DCaps.PollMask)
            self.state = C.State.Closed
            self.scheme = None

    def _process(self, timeout, flags): pass
    def _post(self, msg, flags): pass

    def _update_dcaps(self, caps, mask=None):
        if mask is None:
            mask = caps
        caps = C.DCaps(caps & mask)
        old = self.dcaps
        if old & mask == caps:
            return

        cdef unsigned ccaps = self.dcaps
        self.log.debug("Update caps: {!s} -> {!s}", old, caps)
        self.internal.internal.dcaps = (self.dcaps & ~mask) | caps

        cdef tll_msg_t msg
        memset(&msg, 0, sizeof(tll_msg_t))

        msg.type = C.Type.Channel
        msg.msgid = C.MsgChannel.Update
        msg.data = &ccaps
        msg.size = sizeof(ccaps)

        self.internal.callback(&msg)

    def _update_pending(self, pending : bool = True):
        return self._update_dcaps(self.dcaps.Pending if pending else 0, self.dcaps.Pending)

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

    cdef _log_msg(self, text, const tll_msg_t * msg):
        if self._dump_mode == self.MessageLogFormat.Disable: return
        ctext = s2b(text)
        tll_channel_log_msg(self.internal.internal.self, self.log.name_bytes, TLL_LOGGER_INFO, int(self._dump_mode), msg, ctext, len(ctext))
