#!/usr/bin/env python
# vim: sts=4 sw=4 et

from operator import attrgetter

class BitField:
    __slots__ = ['name', 'size', 'offset']
    def __init__(self, name, size, offset):
        self.name, self.size, self.offset = name, size, offset

    def get(self, obj):
        return bool(obj._value & (1 << self.offset))

    def set(self, obj, v):
        v = 1 if v else 0
        old = (obj._value & (1 << self.offset))
        obj._value ^= old ^ (v << self.offset)

class Bits(object):
    __slots__ = ['_value']
    BITS = {}

    def __init__(self, value = 0):
        if isinstance(value, (set, list, tuple)):
            self._value = 0
            for n in value:
                self.BITS[n].set(self, 1)
        elif isinstance(value, dict):
            self._value = 0
            for k,v in value.items():
                if v:
                    self.BITS[k].set(self, 1)
        else:
            self._value = int(value)

def fill_properties(cls):
    for b in cls.BITS.values():
        setattr(cls, b.name, property(b.get, b.set))
    return cls
