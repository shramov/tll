#!/usr/bin/env python
# vim: sts=4 sw=4 et

import datetime
import enum
import functools

__all__ = ['Resolution', 'Duration', 'TimePoint']

class Resolution(enum.Enum):
    nanosecond = (1, 1000000000)
    microsecond = (1, 1000000)
    millisecond = (1, 1000)
    second = (1, 1)
    minute = (60, 1)
    hour = (3600, 1)
    day = (86400, 1)

    ns = nanosecond
    us = microsecond
    ms = millisecond

_str2res = {
    'ns': (1, 1000000000),
    'us': (1, 1000000),
    'ms': (1, 1000),
    's': (1, 1),
    'm': (60, 1),
    'h': (3600, 1),
    'd': (86400, 1),
}

_res2str = dict([(v,k) for (k,v) in _str2res.items()])

@functools.total_ordering
class _Base:
    __slots__ = ['value', 'resolution']
    def __init__(self, value, resolution, type=float):
        if isinstance(resolution, Resolution):
            resolution = resolution.value
        elif isinstance(resolution, str):
            resolution = Resolution[resolution].value
        self.resolution = resolution

        if isinstance(value, _Base):
            if value.resolution == resolution:
                value = value.value
            else:
                value = value.seconds * self.resolution[1] / self.resolution[0]
        self.value = type(value)

    @property
    def seconds(self):
        return self.value * self.resolution[0] / self.resolution[1]

    def __repr__(self):
        return "<{} {} {}>".format(self.__class__.__name__, self.value, self.resolution)

    def __str__(self):
        suffix = _res2str.get(self.resolution, None)
        if suffix is not None:
            return '{}{}'.format(self.value, suffix)
        return '{}*({}/{})'.format(self.value, self.resolution[0], self.resolution[1])

    @classmethod
    def from_str(cls, s):
        if '*' in s:
            raise ValueError("Invalid string")
        body = s.rstrip('abcdefghijklmnopqrstuvwxyz')
        suffix = s[len(body):]
        if '.' in body or 'e' in body or 'E' in body:
            typ = float
        else:
            typ = int
        return cls(typ(body), _str2res[suffix], type=typ)

    def __eq__(self, other):
        if not isinstance(other, self.__class__): return False
        if self.resolution == other.resolution:
            return self.value == other.value
        return self.seconds == other.seconds

    def __lt__(self, other):
        if not isinstance(other, self.__class__): return False
        return self.seconds < v.seconds

class Duration(_Base):
    def __init__(self, value = 0, resolution = Resolution.second.value, type=float):
        if isinstance(value, datetime.timedelta):
            value = _Base(int(value.total_seconds()) * 1000000 + value.microseconds, Resolution.us.value)
        super().__init__(value, resolution, type)

    @property
    def timedelta(self):
        return datetime.timedelta(self.seconds)

class TimePoint(_Base):
    def __init__(self, value = 0, resolution = Resolution.second.value, type=float):
        if isinstance(value, datetime.datetime):
            value = _Base(int(value.timestamp()) * 1000000 + value.microsecond, Resolution.us.value)
        super().__init__(value, resolution, type)

    @property
    def datetime(self):
        return datetime.datetime.fromtimestamp(self.seconds)
