# vim: sts=4 sw=4 et
# cython: language_level=3

from cpython cimport Py_buffer

cdef extern from "Python.h":
    ctypedef struct PyMemoryViewObject:
        pass

    cdef Py_buffer * PyMemoryView_GET_BUFFER(object mview)
    cdef int PyMemoryView_Check(object obj)

cdef extern from "string.h":
    cdef size_t strnlen(const char *, size_t)
