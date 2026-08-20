#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import pytest
import random
import sys

from tll.error import TLLError
import tll.keyring as K

@pytest.mark.skipif(sys.platform != 'linux', reason='Keyring support is linux only')
def test():
    kname = f'tll:test:{random.randint(0, 0xffffffff):08x}'
    rname = f'tll:test:{random.randint(0, 0xffffffff):08x}'
    ref = K.KeyRef(f'key:{kname}')

    assert K.KeyRef('data:body').read() == b'body'

    with pytest.raises(TLLError): K.KeyRef('compat-body').read()
    assert K.KeyRef('compat-body', compat=True).read() == b'compat-body'

    with pytest.raises(TLLError): K.read(kname)
    with pytest.raises(TLLError): ref.read()

    kr = K.Keyring(rname, K.KeyringId.Process)
    kr.write(kname, b"test-body")

    assert K.read(kname) == b'test-body'
    assert ref.read() == b'test-body'

    kr.unlink()
    with pytest.raises(TLLError): K.read(kname)
    with pytest.raises(TLLError): ref.read()
