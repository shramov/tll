#!/usr/bin/env python3
# vim: sts=4 sw=4 et

from errno import EINVAL

class TLLError(OSError):
    def __init__(self, text, code=EINVAL):
        OSError.__init__(self, code, text)
