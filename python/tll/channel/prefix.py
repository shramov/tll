#!/usr/bin/env python
# vim: sts=4 sw=4 et

from .base import Base

import sys
import weakref

class Prefix(Base):
    PREFIX = True
    OPEN_POLICY = Base.OpenPolicy.Manual
    CLOSE_POLICY = Base.ClosePolicy.Long
    CHILD_POLICY = Base.ChildPolicy.Single
    PROCESS_POLICY = Base.ProcessPolicy.Never

    def _init(self, url, master = None):
        if '+' not in url.proto:
            raise ValueError("Invalid proto: no '+' found in '{}'".format(url.proto))

        curl = url.copy()
        pproto, curl.proto = url.proto.split('+', 1)
        curl['tll.internal'] = 'yes'
        curl['name'] = '{}/{}'.format(self.name, pproto)

        self._child = self.context.Channel(curl, master=master)
        self._child.callback_add(weakref.ref(self))

        self._child_add(self._child)

    def _free(self):
        if self._child:
            self._child.callback_del(weakref.ref(self))
        self._child = None

    def _open(self, props):
        self._child.open(props)

    def _close(self, force : bool = False):
        self._child.close()

    def _post(self, msg, flags):
        self._child.post(msg, flags)

    def prefix_data(self, msg):
        self._callback(msg)

    def prefix_other(self, msg):
        self._callback(msg)

    def prefix_state(self, msg):
        s = self.State(msg.msgid)
        if s == self.State.Active:
            try:
                self._on_active()
            except:
                self.log.exception("Failed to set Active state")
                self.state = self.State.Error
        elif s == self.State.Error:
            self._on_error()
        elif s == self.State.Closing:
            self._on_closing()
        elif s == self.State.Closed:
            self._on_closed()
        else:
            pass

    def _on_active(self):
        self.state = self.State.Active

    def _on_closing(self):
        self.log.debug("Child channel is closing")
        if self.state in (self.State.Opening, self.State.Active):
            self.state = self.State.Closing

    def _on_closed(self):
        self.log.debug("Child channel closed")
        if self.state == self.State.Closing:
            Base._close(self)

    def _on_error(self):
        self.log.info("Child channel failed")
        self.state = self.State.Error

    def __call__(self, c, msg):
        if msg.type == msg.Type.Data:
            self.prefix_data(msg)
        elif msg.type == msg.Type.State:
            self.prefix_state(msg)
        else:
            self.prefix_other(msg)
