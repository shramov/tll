#!/usr/bin/env python3
# vim: sts=4 sw=4 et

'''TLL library version information'''

from .version cimport *
from .s2b import b2s

'''Library version string'''
version = b2s(tll_version_string())

''' Library version number, as single integer'''
version_number = tll_version_number()

'''Library version tuple, (major, minor, patch)'''
version_tuple = (version_number >> 16, (version_number >> 8) & 0xff, version_number & 0xff)
