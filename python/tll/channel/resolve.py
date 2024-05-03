#!/usr/bin/env python3
# vim: sts=4 sw=4 et

from .logic import Logic

class Service:
    def __init__(self, addr, service, host, tags=[]):
        self.addr = addr
        self.service = service
        self.host = host
        self.tags = tags
        self.channels = {}

    def match(self, request):
        return request == self.service or request == '*' or request in self.tags

    def lookup(self, request):
        if request == '*':
            return self.channels.values()
        r = self.channels.get(request, None)
        if r is not None:
            return [r]
        return []

class Channel:
    def __init__(self, service, name, config):
        self.service = service
        self.name = name
        self.config = config

class Subscription:
    def __init__(self, service):
        self.service = service
        self._subs = {}

    def add(self, channel, addr):
        v = self._subs.setdefault(channel, {})
        v[addr] = v.get(addr, 0) + 1

    def remove(self, channel, addr):
        v = self._subs.get(channel)
        if not v:
            raise KeyError(f"No subscription for {self.service}/{channel}")
        count = v.get(addr, None)
        if count is None:
            raise KeyError(f"No subscription for {self.service}/{channel} from addr {addr[0].name}:{addr[1]}")
        if count > 1:
            v[addr] = count - 1
        else:
            del v[addr]

    def disconnect_channel(self, channel):
        drop = []
        for k,v in self._subs.items():
            for x in [x for x in v.keys() if x[0] == channel]:
                del v[x]
            if not v:
                drop.append(k)
        for k in drop:
            del self._subs[k]
        return drop

    def disconnect(self, addr):
        drop = []
        for k,v in self._subs.items():
            if addr in v:
                del v[addr]
            if not v:
                drop.append(k)
        for k in drop:
            del self._subs[k]
        return drop

    def get(self, channel):
        return self._subs.get(channel, {}).keys()

class Resolve(Logic):
    def _init(self, url, master=None):
        super()._init(url, master)
        uplink = self._channels.get('uplink', [])
        if len(uplink) > 1:
            raise RuntimeError(f"More then one uplink channel: {[x.name for x in uplink]}")
        elif uplink:
            self._uplink = uplink[0]
        else:
            self._uplink = None
        self._input = set(self._channels.get("input", []))

    def _close(self):
        self._exports = {}
        self._subs = {}
        super()._close()

    def _open(self, cfg):
        self._exports = {}
        self._subs = {}
        super()._open(cfg)

    def  _subs_get(self, service, channel, tags):
        r = set()
        if sub := self._subs.get(service):
            r.update(sub.get(channel))
            r.update(sub.get('*'))
        if sub := self._subs.get('*'):
            r.update(sub.get(channel))
            r.update(sub.get('*'))
        for t in tags:
            if sub := self._subs.get(t):
                r.update(sub.get(channel))
                r.update(sub.get('*'))
        return r

    def _logic(self, channel, msg):
        if channel == self._uplink:
            return self._on_uplink(channel, msg)
        elif channel in self._input:
            return self._on_input(channel, msg)

    def _on_uplink(self, channel, msg):
        if msg.type == msg.Type.State:
            if msg.msgid == int(self.State.Active):
                self.log.info(f"Uplink channel is active, syncronize with it")
                for service in self._exports.values():
                    self._uplink.post({'service': service.service, 'host': service.host, 'tags': service.tags}, name='ExportService')
                    for c in service.channels.values():
                        self._uplink.post({'service': service.service, 'channel': c.name, 'config': [{'key': k, 'value': v} for k,v in c.config.items()]}, name='ExportChannel')
                for sub in self._subs.values():
                    for c in sub._subs.keys():
                        self._uplink.post({'service': sub.service, 'channel': c}, name='Request')
            return
        elif msg.type != msg.Type.Data:
            return

        msg = channel.unpack(msg)
        if msg.SCHEME.name == 'ExportChannel':
            for addr in self._subs_get(msg.service, msg.channel, msg.tags):
                addr[0].post(msg, addr=addr[1])
        elif msg.SCHEME.name == 'DropChannel':
            for addr in self._subs_get(msg.service, msg.channel, msg.tags):
                addr[0].post(msg, addr=addr[1])

    def _on_input(self, channel, msg):
        addr = (channel, msg.addr)
        if msg.type == msg.Type.Control:
            sc = channel.scheme_control
            if sc and sc.find(msg.msgid).name == 'Disconnect':
                self.log.info(f"Input client {msg.addr:x} disconnected, clear exported services")
                for v in [x for x in self._exports.values() if x.addr == addr]:
                    self._drop_service(v.service)
                for sub in self._subs.values():
                    for c in sub.disconnect(addr):
                        if self._uplink and self._uplink.state == self._uplink.State.Active:
                            self._uplink.post({'service': sub.service, 'channel': c}, name='Unsubscribe')
            return
        elif msg.type == msg.Type.State:
            if msg.msgid == int(self.State.Closed):
                self.log.info(f"Input channel {channel.name} is closing, clear exported services")
                for v in [x for x in self._exports.values() if x.addr[0] == channel]:
                    self._drop_service(v.service)
                for c in self.disconnect_channel(channel):
                    if self._uplink and self._uplink.state == self._uplink.State.Active:
                        self._uplink.post({'service': sub.service, 'channel': c}, name='Unsubscribe')
            return
        elif msg.type != msg.Type.Data:
            return

        msg = channel.unpack(msg)
        if msg.SCHEME.name == 'ExportService':
            if msg.service in self._exports:
                raise RuntimeError(f"Can not register service '{msg.service}', already exists")
            self.log.info(f"Register service '{msg.service}', tags [{', '.join(msg.tags)}]")
            self._exports[msg.service] = Service(addr, msg.service, msg.host, msg.tags)
            if self._uplink and self._uplink.state == self._uplink.State.Active:
                self._uplink.post(msg)
        elif msg.SCHEME.name == 'DropService':
            if msg.service not in self._exports:
                raise KeyError(f"Can not drop service '{msg.service}', not registered")
            self._drop_service(msg.service)
        elif msg.SCHEME.name == 'ExportChannel':
            service = self._exports.get(msg.service, None)
            if service is None:
                raise KeyError(f"Service '{msg.service}' not found")
            if service.addr != addr:
                raise RuntimeError(f"Service '{msg.service}' registered from different address")
            self.log.info(f"Register channel {msg.service}/{msg.channel}")
            channel = Channel(service.service, msg.channel, {x.key: x.value for x in msg.config})
            service.channels[msg.channel] = channel
            if self._uplink and self._uplink.state == self._uplink.State.Active:
                self._uplink.post(msg)
                return

            for addr in self._subs_get(service.service, msg.channel, service.tags):
                self._reply(service, channel, addr)
        elif msg.SCHEME.name == 'DropChannel':
            service = self._exports.get(msg.service, None)
            if service is None:
                raise KeyError(f"Can not drop channel '{msg.service}/{msg.channel}', service not registered")
            if msg.channel not in service.channels:
                raise KeyError(f"Can not drop channel '{msg.service}/{msg.channel}', channel not registered")
            del service.channels[msg.channel]
            if self._uplink and self._uplink.state == self._uplink.State.Active:
                self._uplink.post(msg)
                return
            for addr in self._subs_get(service.service, msg.channel, service.tags):
                self._reply_drop(service, msg.channel, addr)
        elif msg.SCHEME.name == 'Request':
            self.log.debug(f"Request for {msg.service}/{msg.channel}")
            sub = self._subs.setdefault(msg.service, Subscription(msg.service))
            sub.add(msg.channel, addr)

            if self._uplink and self._uplink.state == self._uplink.State.Active:
                self.log.debug(f"Forward request for {msg.service}/{msg.channel} to uplink")
                self._uplink.post(msg)
                return

            for service in self._exports.values():
                if not service.match(msg.service):
                    continue

                for channel in service.lookup(msg.channel):
                    self._reply(service, channel, addr)

    def _drop_service(self, key):
        self.log.info(f"Drop service {key}")
        del self._exports[key]
        if self._uplink and self._uplink.state == self._uplink.State.Active:
            self._uplink.post({'service': key}, name='DropService')

    def _reply(self, service, channel, addr):
        self.log.info(f"Reply with channel {channel.service}/{channel.name} to {addr[0].name}:{addr[1]:x}")
        addr[0].post({'service': service.service, 'channel': channel.name, 'tags': service.tags, 'host': service.host, 'config': [{'key': k, 'value': v} for k,v in channel.config.items()]}, name='ExportChannel', addr=addr[1])

    def _reply_drop(self, service, channel, addr):
        self.log.info(f"Reply with drop {channel.service}/{channel.name} to {addr[0].name}:{addr[1]:x}")
        addr[0].post({'service': service.service, 'channel': channel, 'tags': service.tags}, name='DropChannel', addr=addr[1])
