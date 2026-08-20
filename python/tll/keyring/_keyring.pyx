#!/usr/bin/env python
# vim: sts=4 sw=4 et

from ._keyring cimport *
from ..s2b import *
from ..error import TLLError

from libc.stdlib cimport free
from libc.string cimport strerror

from pathlib import Path
import enum

class KeyringId(enum.IntEnum):
    Thread = TLL_KEYRING_THREAD
    Process = TLL_KEYRING_PROCESS
    Session = TLL_KEYRING_SESSION
    User = TLL_KEYRING_USER

def read(name: str, keyring: int | KeyringId = 0) -> bytes:
    n = s2b(name)
    cdef char * buf = NULL
    cdef int r = tll_keyring_read(n, &buf, int(keyring))
    if r < 0:
        raise TLLError(f"Failed to read key '{name}': {strerror(-r)}")
    try:
        return buf[:r]
    finally:
        free(buf)

def write(name: str, body: bytes, keyring: int | KeyringId = KeyringId.Process) -> int:
    n = s2b(name)
    b = s2b(body)
    cdef int r = tll_keyring_write(int(keyring), n, b, len(b))
    if r < 0:
        raise TLLError(f"Failed to write key '{name}': {strerror(-r)}")
    return r

def read_ref(ref: str, compat: bool = False) -> bytes:
    n = s2b(ref)
    cdef char * buf = NULL
    cdef int r = tll_keyring_read_ref(n, len(n), &buf, 1 if compat else 0)
    if r < 0:
        raise TLLError(f"Failed to read key ref '{ref}': {strerror(-r)}")
    try:
        return buf[:r]
    finally:
        free(buf)

def load_file(filename: str | Path, keyring: int | KeyringId) -> None:
    n = s2b(filename)
    cdef int r = tll_keyring_load(int(keyring), n)
    if r < 0:
        raise TLLError(f"Failed to load keyring file '{filename}': {strerror(-r)}")

def keyring_new(name: str, parent: int | KeyringId) -> int:
    n = s2b(name)
    cdef int r = tll_keyring_new(n, int(parent))
    if r < 0:
        raise TLLError(f"Failed to create keyring '{name}': {strerror(-r)}")
    return r

def unlink(key: int, parent: int | KeyringId) -> None:
    cdef int r = tll_keyring_unlink(key, int(parent))
    if r < 0:
        raise TLLError(f"Failed to unlink key {key} from {parent}: {strerror(-r)}")
