#!/usr/bin/env python3
# vim: sts=4 sw=4 et

'''TLL library version information'''

from .version cimport *
from .s2b import b2s

'''Library version string'''
version = b2s(tll_version_string())

'''Library version tuple, (major, minor, patch)'''
version_tuple = (TLL_VERSION_MAJOR, TLL_VERSION_MINOR, TLL_VERSION_PATCH)
