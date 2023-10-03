#!/usr/bin/env python3
# vim: sts=4 sw=4 et

from .processor import Processor
from ..channel import Context
from ..config import Config

class Mock:
    State = Processor.State

    def __init__(self, loop, config, context=None):
        if not isinstance(config, Config):
            config = Config.load(config)
        self._config = config
        self._context = context or loop.context
        self._channels = {}

        self._mock(loop)

        Processor.load_modules(self._context, self._config)

        self._processor = Processor(self._config, context=self._context)
        loop._loop.add(self._processor)

        name = self._config['name']
        self._control = loop.Channel(f'ipc://;mode=client;dump=yes;name=processor-ipc-client;scheme=channel://{name}', master=self._processor, context=self._context)

    def _mock(self, loop):
        if 'name' not in self._config:
            self._config['name'] = 'processor'

        for path,_ in self._config.browse("mock.*", subpath=True):
            name = path[len('mock.'):]

            url = self._config.get_url(path)

            murl = url.copy()
            if 'master' in murl:
                del murl['master']
            murl['name'] = f'_mock_master_{name}'

            url['tll.processor.ignore-master-dependency'] = 'yes'

            if url.proto != 'null' and not url.proto.endswith('+null'):
                self._channels[name] = loop.Channel(murl)
                url['master'] = murl['name']

            self._config[path] = url

    def open(self):
        for c in self._channels.values():
            c.open()

        self._processor.open()

        self._control.open()

        for w in self._processor.workers:
            w.open()

    @property
    def processor(self):
        return self._processor

    @property
    def control(self):
        return self._control

    def io(self, *names):
        if len(names) == 1:
            return self._channels[names[0]]
        return [self._channels[n] for n in names]

    @staticmethod
    def _normalize_state(state):
        if isinstance(state, str):
            return Mock.State[state]
        return state

    async def wait(self, name, state):
        state = self._normalize_state(state)
        c = self._context.get(name)
        if c.state == state:
            return
        for _ in range(100):
            m = await self._control.recv()
            m = self._control.unpack(m)
            if m.channel != name:
                continue
            if m.state.name == state.name:
                return

    async def wait_many(self, **objects):
        objects = {n: self._normalize_state(s) for n, s in objects.items()}
        states = {n: self._context.get(n).state for n in objects.keys()}

        if sorted(objects.items()) == sorted(states.items()):
            return

        for _ in range(100):
            m = await self._control.recv()
            m = self._control.unpack(m)
            if m.channel not in states:
                continue
            states[m.channel] = self.State[m.state.name]

            if sorted(objects.items()) == sorted(states.items()):
                return
