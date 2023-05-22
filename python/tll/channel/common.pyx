#!/usr/bin/env python
# vim: sts=4 sw=4 et

from .common cimport *

import enum

class State(enum.Enum):
    Closed = TLL_STATE_CLOSED
    Opening = TLL_STATE_OPENING
    Active = TLL_STATE_ACTIVE
    Closing = TLL_STATE_CLOSING
    Error = TLL_STATE_ERROR
    Destroy = TLL_STATE_DESTROY
    def __int__(self): return self.value

class Type(enum.Enum):
    Data = TLL_MESSAGE_DATA
    State = TLL_MESSAGE_STATE
    Channel = TLL_MESSAGE_CHANNEL
    Control = TLL_MESSAGE_CONTROL
    def __int__(self): return self.value
