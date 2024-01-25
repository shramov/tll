#!/usr/bin/env python3
# vim: sts=4 sw=4 et

from tll.channel.base import Base

class Null(Base):
    pass

class Echo(Base):
    def _post(self, msg, flags):
        self._callback(msg)
