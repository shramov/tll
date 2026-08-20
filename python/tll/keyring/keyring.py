#!/usr/bin/env python3

from __future__ import annotations

from . import _keyring

KeyringId = _keyring.KeyringId
read = _keyring.read
write = _keyring.write

class KeyRef:
    _ref: str
    _compat: bool

    def __init__(self, ref: str, compat: bool = False):
        self._ref = ref
        self._compat = compat

    def read(self) -> bytes:
        return _keyring.read_ref(self._ref, self._compat)

class Keyring:
    _id: int
    _parent: int
    def __init__(self, name: str, parent: Keyring | KeyringId):
        self._parent = parent._id if isinstance(parent, Keyring) else int(parent)
        self._id = _keyring.keyring_new(name, self._parent)

    def unlink(self):
        if self._id == 0:
            return
        try:
            _keyring.unlink(self._id, self._parent)
        except:
            # Keyring can be destroyed already, ignore errors
            pass
        self._id = 0

    def __del__(self):
        self.unlink()

    def write(self, name: str, body: bytes) -> int:
        return _keyring.write(name, body, self._id)
