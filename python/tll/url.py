#!/usr/bin/env python3
# vim: sts=4 sw=4 et

from .conv import GetT

class Params(dict, GetT):
    @staticmethod
    def from_string(s):
        r = Params()
        for x in s.split(';'):
            if not x: continue
            idx = x.find('=')
            if idx == -1:
                raise ValueError("Invalid key=value pair: '{}'".format(x))
            if x[:idx] in r:
                raise ValueError("Duplicate key '{}'".format(x[:idx]))
            r[x[:idx]] = x[idx + 1:]
        return r

    def __str__(self):
        return ';'.join(['{}={}'.format(k,v) for k,v in self.items()])

class Url(Params):
    def __init__(self, *a, **kw):
        self.proto = None
        self.host = None
        Params.__init__(self, *a, **kw)

    @staticmethod
    def from_string(s):
        r = Url()
        idx = s.find('://')
        if idx == -1:
            raise ValueError("No :// separator in url")
        r.proto = s[:idx]
        s = s[idx + 3:]
        idx = s.find(';')
        if idx == -1:
            r.host = s
            return r
        r.host = s[:idx]
        for k,v in Params.from_string(s[idx+1:]).items():
            r[k] = v
        return r
