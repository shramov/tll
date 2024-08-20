#!/usr/bin/env python
# cython: language_level=3
# vim: sts=4 sw=4 et

from .impl cimport *
from . cimport channel as C
from . import channel as C
from ..config cimport Config, Url
from ..scheme cimport Scheme
from .context cimport Context, Internal
from libc.string cimport memset
from libc.errno cimport EINVAL
from ..s2b import b2s, s2b
from ..logger import Logger
from ..logger cimport TLL_LOGGER_INFO
from .. import chrono
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
    Never = enum.auto() # Channel has no child objects
    Proxy = enum.auto() # Channel has some child objects, first one can be casted with tll::channel_cast<SubType>
    Many = enum.auto() # Channel has some child objects, tll::channel_cast does not check children
    Single = Proxy # Old name, alias for Proxy
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
    Auto = TLL_MESSAGE_LOG_AUTO
    def __int__(self): return self.value
_MessageLogFormat = MessageLogFormat

MessageLogFormatMap = {
    '': MessageLogFormat.Disable,
    'no': MessageLogFormat.Disable,
    'yes': MessageLogFormat.Auto, # Keep in sync with tll/channel/base.h
    'frame': MessageLogFormat.Frame,
    'text': MessageLogFormat.Text,
    'text+hex': MessageLogFormat.TextHex,
    'scheme': MessageLogFormat.Scheme,
    'auto': MessageLogFormat.Auto,
}

class Stat(stat.Base):
    FIELDS = [stat.Integer('rx', stat.Method.Sum)
             ,stat.Integer('rx', stat.Method.Sum, stat.Unit.Bytes, alias='rxb')
             ,stat.Integer('tx', stat.Method.Sum)
             ,stat.Integer('tx', stat.Method.Sum, stat.Unit.Bytes, alias='txb')
             ]
_Stat = Stat

cdef class Base:
    PROTO = None

    OpenPolicy = _OpenPolicy
    ClosePolicy = _ClosePolicy
    ChildPolicy = _ChildPolicy
    ProcessPolicy = _ProcessPolicy

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
            cmsg.type = C.Type.Data
            cmsg.msgid = msg.SCHEME.msgid

            data = msg.pack()
        else:
            cmsg.type = int(msg.type)
            cmsg.msgid = int(msg.msgid)

            addr = getattr(msg, 'addr', None)
            if addr: cmsg.addr.i64 = addr

            seq = getattr(msg, 'seq', None)
            if seq: cmsg.seq = int(seq)

            time = getattr(msg, 'time', None)
            if time: cmsg.time = chrono.TimePoint(time, chrono.Resolution.ns, type=int).value

            data = getattr(msg, 'data', None)

        cdef Py_buffer * buf = NULL
        if data is not None:
            data = memoryview(data)
            buf = PyMemoryView_GET_BUFFER(data)
            cmsg.size = buf.len
            cmsg.data = buf.buf

        self.internal.callback(&cmsg)

    @property
    def config(self): return self.internal.config

    @property
    def config_info(self): return self.internal.config.sub("info", True)

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

    def scheme_get(self, tp):
        """ API function """
        if tp == C.Type.Data:
            return self.scheme
        elif tp == C.Type.Control:
            return getattr(self, 'scheme_control', None)

    @property
    def stat(self): return self.internal.stat

    def init(self, url, master=None):
        Logger('tll.python').info('Init channel {}', url)
        self.internal.state = C.State.Closed
        self.config["state"] = "Closed"
        self.config['init'] = url.copy()
        self.config.set_link('url', '../init')

        self.internal.name = str(url.get("name", "noname"))
        self.internal.logger = self.log = Logger("tll.channel.{}".format(self.name))

        if self.CHILD_POLICY == ChildPolicy.Never:
            pass
        elif self.CHILD_POLICY == ChildPolicy.Proxy:
            self.caps |= self.Caps.Parent | self.Caps.Proxy;
        elif self.CHILD_POLICY == ChildPolicy.Many:
            self.caps |= self.Caps.Parent

        if self.CLOSE_POLICY == ClosePolicy.Long:
            self.caps |= self.Caps.LongClose

        self._scheme_url = url.get('scheme', None)

        dir = url.get('dir', '')
        if dir in {'r', 'rw', 'in', 'inout'}:
            self.caps |= self.Caps.Input;
        if dir in {'w', 'rw', 'out', 'inout'}:
            self.caps |= self.Caps.Output

        dump = url.get('dump', '')
        dump_mode = MessageLogFormatMap.get(dump, None)
        if dump_mode is None:
            raise ValueError(f"Invalid 'dump' parameter: '{dump}'")
        self.internal.dump = dump_mode

        stat = url.getT('stat', False)
        if stat:
            self.log.debug("Initialize stat with {}", self.Stat)
            self.internal.stat = self.Stat(self.name)

        self.log.info("Init channel")
        self._init(url, master)

        self.internal.logger = self.log

    def free(self):
        self.log.info("Destroy channel")
        try:
            self._free()
        except:
            pass
        self.dealloc()

    def open(self, props : Config):
        pstr = ';'.join([f'{k}={v}' for (k, v) in props.browse('**')])
        self.log.info(f"Open channel: {pstr}")
        self.state = C.State.Opening

        if self._scheme_url:
            self.scheme = self.context.scheme_load(self._scheme_url)

        if self.PROCESS_POLICY != self.ProcessPolicy.Never:
            self._update_dcaps(C.DCaps.Process)

        try:
            self.config.unlink('open')
        except KeyError:
            pass
        self.config['open'] = props.copy()

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
        self._post(msg, flags)

    def _init(self, props, master=None): pass
    def _free(self): pass

    def _open(self, props): pass
    def _close(self, force : bool = False):
        self.scheme = None
        self._update_dcaps(0, C.DCaps.Pending | C.DCaps.Process | C.DCaps.PollMask)
        for c in list(self._children):
            if c.state != c.State.Closed:
                c.close(True)
        self.state = C.State.Closed

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
        self.log.trace("Update caps: {!s} -> {!s}", old, caps)
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

    def _child_url_parse(self, url, suffix):
        url = Url.parse(url);
        return self._child_url_fill(url, suffix);

    def _child_url_fill(self, url, suffix):
        url.set("name", f"{self.name}/{suffix}")
        url.set("tll.internal", "yes");
        #if self._with_fd and 'fd' not in url:
        #        url.set("fd", "no");
        return url

    def _child_add(self, c, tag=None):
        self.log.info("Add child channel '{}' (tag {})", c.name, tag)
        cdef tll_channel_t * ptr = (<Channel>c)._ptr

        if c in self._children:
            raise KeyError("Channel {} is a child already".format(c.name))

        tag = s2b(tag or "")

        error.wrap(tll_channel_internal_child_add(&self.internal.internal, ptr, tag, len(tag)), "Child '{}' add failed", c.name)
        self._children.add(c)

    def _child_del(self, c, tag=None):
        self.log.info("Delete child channel '{}' (tag {})", c.name, tag)
        cdef tll_channel_t * ptr = (<Channel>c)._ptr

        tag = s2b(tag or "")

        error.wrap(tll_channel_internal_child_del(&self.internal.internal, ptr, tag, len(tag)), "Child '{}' del failed", c.name)

        if c in self._children:
            self._children.remove(c)
