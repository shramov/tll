#!/usr/bin/env python
# cython: language_level=3
# vim: sts=4 sw=4 et

from ..config cimport Config
from ..logger cimport Logger
from .channel cimport Channel
from .reopen cimport *

from ..chrono import TimePoint
from ..error import TLLError
from .common import State
from .channel import Callback, Type

import enum
import weakref

class Action(enum.Enum):
    Close = TLL_CHANNEL_REOPEN_CLOSE
    Open = TLL_CHANNEL_REOPEN_OPEN
    Nothing = TLL_CHANNEL_REOPEN_NONE

cdef class ReopenData:
    cdef object __weakref__
    cdef object _cb
    cdef tll_channel_reopen_t * _ptr
    cdef Logger _log

    def __cinit__(self, log, cfg=None):
        if isinstance(cfg, dict):
            cfg = Config.from_dict(cfg)
        cdef tll_config_t * cptr = NULL
        if cfg is None:
            pass
        elif isinstance(cfg, Config):
            cptr = (<Config>cfg)._ptr
        else:
            raise ValueError(f"Invalid config parameter {cfg}")
        self._ptr = tll_channel_reopen_new(cptr)
        if self._ptr == NULL:
            raise TLLError("Invalid reopen parameters")

    def __init__(self, log : Logger, cfg=None):
        self._log = log
        self._cb = Callback(self, '_on_state')

    def __dealloc__(self):
        tll_channel_reopen_free(self._ptr)
        self._log = None

    def on_state(self, state: State) -> None:
        tll_channel_reopen_on_state(self._ptr, int(state))

    def on_timer(self, ts: TimePoint | float):
        if isinstance(ts, float):
            ts = TimePoint(ts)
        return Action(tll_channel_reopen_on_timer(self._ptr, self._log.ptr, int(ts.convert('ns').value)))

    def open(self):
        return tll_channel_reopen_open(self._ptr)

    def close(self):
        tll_channel_reopen_close(self._ptr)

    @property
    def next(self) -> float:
        return tll_channel_reopen_next(self._ptr) / 1000000000.

    def reset(self):
        self._set_channel(NULL)
        tll_channel_reopen_set_open_config(self._ptr, NULL)

    cdef object _set_channel(ReopenData self, tll_channel_t * ptr):
        cdef tll_channel_t * r = tll_channel_reopen_set_channel(self._ptr, ptr)
        if r != NULL:
            c = Channel.wrap(r)
            c.callback_del(self._cb, mask=c.MsgMask.State)
            return c

    def set_channel(self, c: Channel) -> Channel | None:
        c.callback_add(self._cb, mask=c.MsgMask.State)
        return self._set_channel(c._ptr)

    def set_open_config(self, cfg: Config):
        tll_channel_reopen_set_open_config(self._ptr, cfg._ptr)

    def _on_state(self, c, msg):
        self.on_state(c.State(msg.msgid))
        f = getattr(self, 'on_timer_update', None)
        if f:
            f(self.next)

class Reopen(ReopenData):
    def __init__(self, base, cfg):
        super().__init__(base.log, cfg=cfg)
        self._timer = base.context.Channel(base._child_url_parse('timer://;clock=realtime', 'reopen-timer'))
        self._timer.callback_add(weakref.ref(self), self._timer.MsgMask.Data)
        base._child_add(self._timer)

    def open(self):
        self._timer.open()
        super().open()

    def close(self):
        self._timer.close()
        super().close()

    def __call__(self, c, m):
        a = self.on_timer(c.unpack(m).ts)
        if a == Action.Close:
            super().close()
        elif a == Action.Open:
            super().open()

    def on_timer_update(self, ts : float):
        if self._timer and self._timer.state == self._timer.State.Active:
            self._timer.post({'ts': ts}, name='absolute')
