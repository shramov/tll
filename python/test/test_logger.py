#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import logging, logging.handlers
import tll.logger as L

def test():
    L.init()
    try:
        buf = logging.handlers.BufferingHandler(1000)
        logging.getLogger('tll').addHandler(buf)
        l = L.Logger('tll.xxx')
        l.trace("trace")
        l.error("error")
        assert [(r.levelname, r.name, r.msg) for r in buf.buffer] == [('ERROR', 'tll.xxx', 'error')]
    finally:
        logging.getLogger('tll').removeHandler(buf)
