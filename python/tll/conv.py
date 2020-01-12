#!/usr/bin/env python3
# vim: sts=4 sw=4 et

REGISTRY = {}
REGISTRY[int] = lambda s: int(s, 0)

def conv_bool(s):
    l = str(s).lower()
    if l in ['yes', 'true', '1', 'on']:
        return True
    elif l in ['no', 'false', '0', 'off']:
        return False
    raise ValueError("Invalid bool string: {}".format(s))

REGISTRY[bool] = conv_bool

def from_string(t, s):
    f = REGISTRY.get(t, None)
    if f is None:
        f = getattr(t, 'from_string', t)
    return f(s)

_default_tag = object()

def getT(obj, key, default):
    s = obj.get(key, _default_tag)
    if s in (_default_tag, None, ''):
        return default
    if type(s) == type(default):
        return s
    return from_string(type(default), s)

class GetT:
    def getT(self, key, default):
        return getT(self, key, default)

class PrefixedDict(GetT):
    def __init__(self, prefix, data, separator='.'):
        self._data = data
        self._prefix = prefix or ''
        if self._prefix and not self._prefix.endswith('.'):
            self._prefix += separator

    def get(self, key, default = _default_tag):
        v = self._data.get(self._prefix + key, _default_tag)
        return default if v is _default_tag else v

    def has(self, key):
        return self._prefix + key in self._data

    def __contains__(self, key):
        return self.has(key)

    def __repr__(self):
        return "<PrefixedDict prefix: '{}', data: {}>".format(self._prefix, self._data)

class ChainedDict(GetT):
    def __init__(self, *a):
        self._chain = a

    def get(self, key, default = _default_tag):
        for d in self._chain:
            v = d.get(key, _default_tag)
            if v is not _default_tag:
                return v
        return default

    def has(self, key):
        for d in self._chain:
            if key in d: return True
        return False

    def __contains__(self, key): return self.has(key)

    def __repr__(self):
        return "<ChainedDict {}>".format(", ".join([repr(x) for x in self._chain]))
