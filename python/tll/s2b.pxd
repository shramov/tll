# vim: sts=4 sw=4 et
# cython: language_level=3str

from libc.stdint cimport intptr_t

cpdef object s2b(object)
cpdef object b2s(object)
cpdef int richcmp(intptr_t l, intptr_t r, int op)
