import collections
import decorator
import weakref

import tll.channel as C
from tll.logger import Logger
from tll.processor import Loop as ProcessorLoop

class Entry:
    def __init__(self, *a, **kw):
        self.ref = 1
        self.queue = collections.deque()
        self.future = None

    def pop(self):
        if self.queue:
            return self.queue.popleft()

    def feed(self, msg):
        self.queue.append(msg)

        if self.future and (m := self.pop()) is not None:
            self.future.set_result(m)
            self.future = None

    def reset_future(self, f = None):
        self.future = None

    async def recv(self, timeout):
        if self.future:
            raise RuntimeError("Previous recv is not yet finished")
        if (m := self.pop()) is not None:
            return m

        return await self._recv(timeout)

class StateEntry(Entry):
    def __init__(self, *a, **kw):
        super().__init__(*a, **kw)
        self.ignore = set()

    def reset_future(self, f = None):
        super().reset_future(f)
        self.ignore = set()

    def pop(self):
        while self.queue:
            m = self.queue.popleft()
            if m not in self.ignore:
                return m

class AsyncChannel(C.Channel):
    LOOP_KEY = '_pytll_async_loop'
    MASK = C.MsgMask.All ^ C.MsgMask.State

    def __init__(self, *a, async_mask=None, **kw):
        loop = kw.pop(self.LOOP_KEY, None)
        if loop is None:
            raise ValueError("Need {} parameter".format(self.LOOP_KEY))
        self.MASK = self.MASK if async_mask is None else async_mask

        C.Channel.__init__(self, *a, **kw)
        self._loop = weakref.ref(loop)
        self._result = self.Entry(loop)
        self._result_state = self.StateEntry(loop)
        self.callback_add(weakref.ref(self), mask=self.MASK | C.MsgMask.State)

    def __call__(self, c, msg):
        if msg.type == msg.Type.State:
            state = C.State(msg.msgid)
            self._result_state.feed(state)
            if state in (C.State.Opening, C.State.Active):
                # Force cache scheme
                C.Channel._scheme(self, self.Type.Data)
                C.Channel._scheme(self, self.Type.Control)

            if self.MASK & C.MsgMask.State:
                self._result.feed(msg.clone())
        else:
            self._result.feed(msg.clone())
        self.tick()

    def tick(self): pass

    @property
    def scheme(self):
        return self._scheme(self.Type.Data)

    @property
    def scheme_control(self):
        return self._scheme(self.Type.Control)

    def _scheme(self, t):
        if self.state in (self.State.Opening, self.State.Active):
            return C.Channel._scheme(self, t)
        if t is None:
            t = self.Type.Data
        if t not in (self.Type.Data, self.Type.Control):
            raise ValueError(f"No scheme defined for message type {t}")
        return self._scheme_cache[int(t)]

    def open(self, *a, **kw):
        self._result.queue.clear()
        self._result_state.queue.clear()
        return C.Channel.open(self, *a, **kw)

    @property
    def result(self):
        return self._result.queue

    async def recv(self, timeout=1.):
        return await self._result.recv(timeout)

    async def recv_state(self, timeout=1., ignore={C.State.Opening, C.State.Closing}):
        if ignore is None:
            ignore = set()
        elif isinstance(ignore, (int, C.State)):
            ignore = {C.State(ignore)}
        self._result_state.ignore = ignore
        return await self._result_state.recv(timeout)

class Loop:
    def __init__(self, context = None, tick_interval = 0.1, config=None):
        self.context = context or C.Context()
        self.channels = weakref.WeakKeyDictionary()
        self.asyncchannels = weakref.WeakSet()
        self.log = Logger("tll.python.asynctll")
        self.tick = tick_interval
        self._state = C.State.Closed

        config = dict(config or {})
        config.setdefault('name', 'tll.python.asynctll/loop')
        self._loop = ProcessorLoop(config=config)

    def __del__(self):
        self.destroy()

    def destroy(self):
        if self._state == C.State.Destroy:
            return
        self._state = C.State.Destroy
        self.log.debug("Destroy async helper")
        for c in self.channels.keys():
            c.callback_del(self._callback, mask=C.MsgMask.All)
        self.channels = {}
        for c in self.asyncchannels:
            c.close()
        self.asyncchannels = set()

    def Channel(self, *a, **kw):
        if self.context is None:
            raise RuntimeError("Can not create channel without loop context")
        kw = dict(kw)
        kw.setdefault('context', self.context)
        kw[AsyncChannel.LOOP_KEY] = self
        c = self.AsyncChannel(*a, **kw)
        self.asyncchannels.add(c)
        self._loop.add(c)
        return c

    def channel_add(self, c):
        self.log.debug("Add channel {}", c.name)
        if c in self.asyncchannels:
            return
        if c not in self.channels:
            self._loop.add(c)
            self.channels[c] = self.AsyncChannel.Entry(self)
            c.callback_add(self._callback, mask=C.MsgMask.All)
        else:
            self.channels[c].ref += 1

    def channel_del(self, c, force=False):
        self.log.debug("Del channel {}", c.name)
        if c in self.asyncchannels:
            return
        entry  = self.channels.get(c, None)
        if entry is None:
            raise KeyError("Channel {} not processed by loop".format(c.name))
        entry.ref -= 1
        if force or entry.ref == 0:
            c.callback_del(self._callback, mask=C.MsgMask.All)
            self._loop.remove(c)
            del self.channels[c]

    async def recv(self, c, timeout=1.):
        if c in self.asyncchannels:
            return await c.recv(timeout)
        entry = self.channels.get(c, None)
        if entry is None:
            raise KeyError("Channel {} not processed by loop".format(c.name))
        return await entry.recv(timeout)

    def step(self, timeout: float = 0):
        self._loop.step(timeout)

    def _callback(self, channel, msg):
        self.log.debug("Got message for {}", channel.name)
        if msg.type == msg.Type.State:
            if msg.msgid == channel.State.Destroy:
                self.log.debug("Removing channel {}", channel.name)
                self.channel_del(channel, force=True)
        if msg.type not in (msg.Type.Data, msg.Type.Control):
            return
        if entry := self.channels.get(channel, None):
            entry.feed(msg.clone())

@decorator.decorator
def asyncloop_run(f, asyncloop, *a, **kw):
    asyncloop.run(f(asyncloop, *a, **kw))
