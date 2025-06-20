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
        b.set(r, 1)
    return r

def fill_properties(cls):
    for b in cls.BITS.values():
        setattr(cls, b.name, property(b.get, b.set))
    setattr(cls, 'from_str', lambda s: from_str(cls, s))
    return cls
