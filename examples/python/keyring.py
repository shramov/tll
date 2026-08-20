#!/usr/bin/env python3

from tll.channel.base import Base
from tll.keyring import KeyRef

class KeyDump(Base):
    PROCESS_POLICY = Base.ProcessPolicy.Never

    def _init(self, cfg, master=None):
        self._ref = KeyRef(cfg.get('secret'), cfg.getT('compat', False))

    def _open(self, props):
        self.log.warning("Secret: {}", self._ref.read())
        super()._open(props)
