#!/usr/bin/env python3
# vim: sts=4 sw=4 et

from nose.tools import *

import logging, logging.handlers
import tll.logger as L

def test():
    L.init()
    try:
        buf = logging.handlers.BufferingHandler(1000)
        logging.getLogger('tll').addHandler(buf)
        l = L.Logger('tll.xxx')
        l.trace("trace")
        l.info("info")
        assert_equals([(r.levelname, r.name, r.msg) for r in buf.buffer], [('INFO', 'tll.xxx', 'info')])
    finally:
        logging.getLogger('tll').removeHandler(buf)
