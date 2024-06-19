# vim: sts=4 sw=4 et

import pathlib
from .s2b cimport *
from cpython.version cimport PY_MAJOR_VERSION
from cpython.object cimport Py_EQ, Py_NE, Py_GE, Py_GT, Py_LE, Py_LT

cpdef object s2b(object s):
    if PY_MAJOR_VERSION == 2:
        if isinstance(s, unicode):
            return s.encode('utf-8', errors='replace')
        return s
    if isinstance(s, pathlib.Path):
        s = str(s)
    if isinstance(s, str):
        return s.encode('utf-8', errors='replace')
    return s

cpdef object b2s(object b):
    if PY_MAJOR_VERSION == 2:
        return b
    if isinstance(b, str):
        return b
    return b.decode('utf-8', errors='replace')

cpdef int richcmp(intptr_t l, intptr_t r, int op):
    if op == Py_LT: return l < r
    elif op == Py_GT: return l > r
    elif op == Py_LE: return l <= r
    elif op == Py_GE: return l >= r
    elif op == Py_EQ: return l == r
    elif op == Py_NE: return l != r
    return 0
