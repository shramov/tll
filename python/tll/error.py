#!/usr/bin/env python3
# vim: sts=4 sw=4 et

from errno import EINVAL

class TLLError(OSError):
    def __init__(self, text, code=EINVAL):
        OSError.__init__(self, code, text)

def wrap(r, fmt, *a, **kw):
    if r in (0, None): return r
    raise TLLError(fmt.format(*a, **kw), code=r)
