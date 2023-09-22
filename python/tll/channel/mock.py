#!/usr/bin/env python3
# vim: sts=4 sw=4 et

from . import Context
from ..config import Config

from tll import asynctll

class Mock:
    def __init__(self, loop, config):
        if not isinstance(config, Config):
            config = Config.load(config)
        self._config = config
        self._inner = {}
        self._channels = {}

        for path,_ in self._config.browse("mock.*", subpath=True):
            name = path[len('mock.'):]

            url = self._config.get_url(path)

            if 'dump' not in url:
                url['dump'] = 'yes'

            murl = url.copy()
            if 'master' in murl:
                del murl['master']
            murl['name'] = f'_mock_master_{name}'

            url['name'] = name

            self._config[path] = url

            if url.proto != 'null' and not url.proto.endswith('+null'):
                self._channels[name] = loop.Channel(murl)
                url['master'] = murl['name']
            self._inner[name] = loop.Channel(url)

        url = self._config.get_url('channel')
        if 'dump' not in url:
            url['dump'] = 'yes'
        self._channel = loop.Channel(url)

    def open(self, inner : bool = True, skip = []):
        for c in self._channels.values():
            c.open()

        if inner:
            for c in self._inner.values():
                if c.name in skip:
                    continue
                c.open()

        self._channel.open()

    @property
    def channel(self):
        return self._channel

    def io(self, *names):
        if len(names) == 1:
            return self._channels[names[0]]
        return [self._channels[n] for n in names]

    def inner(self, *names):
        if len(names) == 1:
            return self._inner[names[0]]
        return [self._inner[n] for n in names]
