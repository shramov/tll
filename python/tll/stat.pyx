#!/usr/bin/env python
# vim: sts=4 sw=4 et

from .stat cimport *

cimport cython
from cpython.array cimport array
from libc.string cimport memcmp
from .s2b cimport s2b, b2s
from .error import TLLError

import enum
import time
from collections import namedtuple

cdef extern from "string.h":
    cdef size_t strnlen(const char *s, size_t maxlen)

class Method(enum.Enum):
    Sum = TLL_STAT_SUM
    Min = TLL_STAT_MIN
    Max = TLL_STAT_MAX
    Last = TLL_STAT_LAST

class Type(enum.Enum):
    Int = TLL_STAT_INT
    Float = TLL_STAT_FLOAT

class Unit(enum.Enum):
    Unknown = TLL_STAT_UNIT_UNKNOWN
    Bytes = TLL_STAT_UNIT_BYTES
    NS = TLL_STAT_UNIT_NS

cdef class Field:
    cdef tll_stat_field_t * ptr

    def __cinit__(self):
        self.ptr = NULL

    @staticmethod
    cdef wrap(tll_stat_field_t * ptr):
        f = Field()
        f.ptr = ptr
        return f

    @property
    def method(self): return Method(self.ptr.method)

    @property
    def type(self): return Type(self.ptr.type)

    @property
    def unit(self): return Unit(self.ptr.unit)

    @property
    def name(self): return b2s(self.ptr.name[:strnlen(self.ptr.name, sizeof(self.ptr.name))])

    @property
    def value(self): return self.ptr.value if self.ptr.type == TLL_STAT_INT else self.ptr.fvalue

    def __repr__(self):
        return f"<Field {self.name} {self.method.name} {self.unit.name} {self.value}>"

cdef class FieldGroup:
    cdef tll_stat_field_t * ptr
    cdef object fields

    def __cinit__(self):
        self.ptr = NULL

    @staticmethod
    cdef wrap(tll_stat_field_t * ptr):
        if memcmp(ptr.name, b'_tllgrp', 7) != 0:
            raise ValueError("Not a stat group")
        f = FieldGroup()
        f.ptr = ptr
        f.fields = [Field.wrap(ptr + i) for i in range(4)]
        return f

    @property
    def type(self): return self.fields[1].type

    @property
    def unit(self): return self.fields[1].unit

    @property
    def name(self): return self.fields[1].name

    @property
    def count(self): return self.fields[0].value

    @property
    def sum(self): return self.fields[1].value

    @property
    def min(self): return self.fields[2].value

    @property
    def max(self): return self.fields[3].value

cdef class Page:
    cdef tll_stat_page_t * ptr

    def __cinit__(self):
        self.ptr = NULL

    @staticmethod
    cdef wrap(tll_stat_page_t * ptr):
        p = Page()
        p.ptr = ptr
        return p

    def values(self):
        if self.ptr is NULL: raise ValueError("NULL pointer")
        cdef int i = 0
        r = []
        while i < self.ptr.size:
            if memcmp(self.ptr.fields[i].name, b'_tllgrp', 7) == 0:
                r.append(FieldGroup.wrap(self.ptr.fields + i))
                i += 4
            else:
                r.append(Field.wrap(self.ptr.fields + i))
                i += 1
        return r

cdef class Block:
    cdef tll_stat_block_t * ptr

    def __cinit__(self):
        self.ptr = NULL

    @staticmethod
    cdef wrap(tll_stat_block_t * ptr):
        b = Block()
        b.ptr = ptr
        return b

    def __enter__(self):
        return self.acquire()

    def __exit__(self):
        return self.release()

    def acquire(self):
        return Page.wrap(tll_stat_page_acquire(self.ptr))

    def release(self):
        tll_stat_page_release(self.ptr, NULL)

def Integer(name, method=Method.Sum, unit=Unit.Unknown):
    return {'name':name, 'method':method, 'unit':unit, 'type':int}

def Float(name, method=Method.Sum, unit=Unit.Unknown):
    return {'name':name, 'method':method, 'unit':unit, 'type':float}

class Group:
    def __init__(self, name, unit=Unit.Unknown, type=int):
        self.name, self.unit, self.type = name, unit, type

    def fields(self):
        return [
            {'name':'_tllgrp', 'method':Method.Sum, 'unit':Unit.Unknown, 'type':int},
            {'name':self.name, 'method':Method.Sum, 'unit':self.unit, 'type':self.type},
            {'name':self.name, 'method':Method.Min, 'unit':self.unit, 'type':self.type},
            {'name':self.name, 'method':Method.Max, 'unit':self.unit, 'type':self.type},
        ]

cdef class Base:
    FIELDS = []

    def __cinit__(self, name):
        self.name = s2b(name)
        self.normalized = []
        for f in self.FIELDS:
            if isinstance(f, Group):
                self.normalized += f.fields()
            else:
                self.normalized += [f]

        self.block.name = self.name
        self.block.lock = &self.page0
        self.block.active = &self.page0
        self.block.inactive = &self.page1

        self.page0.size = self.page1.size = size = len(self.normalized)
        self.fields0 = array('b', b'\0' * sizeof(tll_stat_field_t) * size)
        self.fields1 = array('b', b'\0' * sizeof(tll_stat_field_t) * size)
        self.page0.fields = <tll_stat_field_t *>self.fields0.data.as_voidptr
        self.page1.fields = <tll_stat_field_t *>self.fields1.data.as_voidptr

        self.offsets = {}
        self.groups = {}
        for i,f in enumerate(self.normalized):
            name = s2b(f['name'])[:7]
            name += b'\0' * (7 - len(name))
            self.page0.fields[i].name = name
            self.page1.fields[i].name = name
            self.page0.fields[i].type = TLL_STAT_FLOAT if f.get('type', int) == float else TLL_STAT_INT
            self.page1.fields[i].type = TLL_STAT_FLOAT if f.get('type', int) == float else TLL_STAT_INT
            self.page0.fields[i].method = f.get('method', Method.Sum).value
            self.page1.fields[i].method = f.get('method', Method.Sum).value
            self.page0.fields[i].unit = f.get('unit', Unit.Unknown).value
            self.page1.fields[i].unit = f.get('unit', Unit.Unknown).value
            tll_stat_field_reset(&self.page0.fields[i])
            tll_stat_field_reset(&self.page1.fields[i])
            self.offsets[f['name']] = i
            if f['name'] == '_tllgrp':
                self.groups[self.normalized[i+1]['name']] = i

    @cython.boundscheck(False)
    def update(self, **kw):
        cdef ssize_t size = len(kw)
        cdef unsigned int[:] offsets = array('I', [0] * size)
        cdef long long [:] updates_i = array('q', [0] * size)
        cdef double [:] updates_f = array('d', [0] * size)
        cdef unsigned i
        for i,(name, value) in enumerate(kw.items()):
            o = self.groups.get(name, None)
            if o is None:
                o = self.offsets.get(name, None)
                if o is None:
                    raise KeyError("Unknown field {}".format(name))
                tp = self.page0.fields[o].type
            else:
                tp = self.page0.fields[o + 1].type
            offsets[i] = o
            if tp == TLL_STAT_INT:
                updates_i[i] = value
            else:
                updates_f[i] = value

        cdef tll_stat_page_t * page = NULL
        cdef tll_stat_field_t * field = NULL
        with nogil:
            page = tll_stat_page_acquire(&self.block)
            if page == NULL:
                with gil:
                    raise TLLError("Failed to acquire page, another writer locked")
            try:
                i = 0
                while i < size:
                    field = page.fields + offsets[i]
                    if memcmp(field.name, b'_tllgrp', 7) == 0:
                        tll_stat_field_update_int(field, 1)
                        if field[1].type == TLL_STAT_INT:
                            tll_stat_field_update_int(field + 1, updates_i[i])
                            tll_stat_field_update_int(field + 2, updates_i[i])
                            tll_stat_field_update_int(field + 3, updates_i[i])
                        else:
                            tll_stat_field_update_float(field + 1, updates_f[i])
                            tll_stat_field_update_float(field + 2, updates_f[i])
                            tll_stat_field_update_float(field + 3, updates_f[i])
                    else:
                        if field.type == TLL_STAT_INT:
                            tll_stat_field_update_int(field, updates_i[i])
                        else:
                            tll_stat_field_update_float(field, updates_f[i])
                    i += 1
            finally:
                tll_stat_page_release(&self.block, page)

cdef class Iter:
    cdef tll_stat_iter_t * ptr

    def __cinit__(self):
        self.ptr = NULL

    @staticmethod
    cdef wrap(tll_stat_iter_t * ptr):
        i = Iter()
        i.ptr = ptr
        return i

    def empty(self):
        return tll_stat_iter_empty(self.ptr)

    @property
    def name(self):
        cdef const char * name = tll_stat_iter_name(self.ptr)
        if name == NULL: return
        return b2s(name[:])

    def swap(self, timeout=0.001):
        if self.ptr == NULL: return
        timeout = time.time() + timeout
        cdef tll_stat_page_t * page = tll_stat_iter_swap(self.ptr)
        while page == NULL:
            if tll_stat_iter_empty(self.ptr) or timeout < time.time():
                return
            page = tll_stat_iter_swap(self.ptr)
        return Page.wrap(page).values()

    def __iter__(self): return self

    def __next__(self):
        if self.ptr == NULL:
            raise StopIteration()
        tmp = Iter.wrap(self.ptr)
        self.ptr = tll_stat_iter_next(self.ptr)
        return tmp

cdef class List:
    def __cinit__(self, new=True):
        self.ptr = NULL
        self.owner = new
        if new:
            self.ptr = tll_stat_list_new()

    def __dealloc__(self):
        if self.owner:
            tll_stat_list_free(self.ptr)

    @staticmethod
    cdef List wrap(tll_stat_list_t * ptr):
        l = List(new=False)
        l.ptr = ptr
        return l

    @staticmethod
    cdef tll_stat_block_t * unwrap(object obj):
        if isinstance(obj, Base):
            return &(<Base>obj).block
        elif isinstance(obj, Block):
            return (<Block>obj).ptr
        else:
            raise ValueError("Need either tll.stat.Block or tll.stat.Base class")

    def __iter__(self):
        return Iter.wrap(tll_stat_list_begin(self.ptr))

    def add(self, obj):
        cdef tll_stat_block_t * ptr = List.unwrap(obj)
        r = tll_stat_list_add(self.ptr, ptr)
        if r:
            raise TLLError("Failed to add object {}: {}".format(obj, r), r)

    def remove(self, obj):
        cdef tll_stat_block_t * ptr = List.unwrap(obj)
        r = tll_stat_list_remove(self.ptr, ptr)
        if r:
            raise TLLError("Failed to add object {}: {}".format(obj, r), r)
