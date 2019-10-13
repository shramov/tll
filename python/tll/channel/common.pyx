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
