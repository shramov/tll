# vim: sts=4 sw=4 et
# cython: language_level=3

cdef extern from "tll/version.h":
    cdef const char * tll_version_string()

    cdef const int TLL_VERSION_MAJOR
    cdef const int TLL_VERSION_MINOR
    cdef const int TLL_VERSION_PATCH
