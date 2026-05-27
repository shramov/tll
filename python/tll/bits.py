#!/usr/bin/env python
# vim: sts=4 sw=4 et

from operator import attrgetter

class BitField:
    __slots__ = ['name', 'size', 'offset', 'mask']
    def __init__(self, name, size, offset):
        self.name, self.size, self.offset = name, size, offset
        self.mask = (1 << size) - 1

    def get(self, obj):
        if self.mask == 1:
            return bool(obj._value & (1 << self.offset))
        return (obj._value >> self.offset) & self.mask

    def set(self, obj, v):
        if self.mask == 1:
            v = 1 if v else 0
        else:
            v = v & self.mask
        old = (obj._value & (self.mask << self.offset))
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
                self.BITS[k].set(self, v)
        elif isinstance(value, Bits):
            self._value = value._value
        else:
            self._value = int(value)

    def __str__(self):
        r = []
        for b in self.BITS.values():
            if b.get(self):
                r.append(b.name)
        return '{' + ', '.join(r) + '}'

def from_str(obj, s):
    r = obj()
    s = s.strip()
    try:
        v = int(s, 0)
        r._value = v
        return r
    except:
        pass
    if s[:1] == '{':
        if s[-1] != '}':
            raise ValueError("Missing closing '}'")
        s = s[1:-1]
    for n in s.split(','):
        n = n.strip()
        b = obj.BITS.get(n)
        if b is None:
            raise ValueError(f"Unknown bit name {n}")
        b.set(r, b.mask)
    return r

def fill_properties(cls):
    for b in cls.BITS.values():
        setattr(cls, b.name, property(b.get, b.set))
    setattr(cls, 'from_str', lambda s: from_str(cls, s))
    return cls
