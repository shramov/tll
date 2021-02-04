#!/usr/bin/env python3
# vim: sts=4 sw=4 et

from tll import asynctll
import tll.channel as C
from tll.error import TLLError
from tll.test_util import ports

import common

import http.server
import os
import pytest
import socket
import socketserver

class EchoHandler(http.server.BaseHTTPRequestHandler):
    def version_string(self): return 'EchoServer/1.0'
    def date_time_string(self, timestamp=None): return 'today'

    def _reply(self, code, body, headers={}):
        self.send_response(code)
        if 'Content-Type' not in headers:
            self.send_header("Content-Type", "text/plain")
        for k,v in sorted(headers.items()):
            self.send_header(k, v)
        self.end_headers()

        self.wfile.write(body)

    def do_GET(self): return self._reply(200, f'GET {self.path}'.encode('ascii'))
    def do_POST(self):
        for k,v in self.headers.items():
            print(f'{k}: {v}')
        size = int(self.headers.get('Content-Length', '-1'))
        if size == -1:
            size = 0
        self.rfile.raw._sock.setblocking(False)
        data = f'POST {self.path} :'.encode('ascii') + (self.rfile.read(size) or b'')

        headers = {'Content-Length':str(len(data))}
        if 'X-Test-Header' in self.headers:
            headers['X-Test-Header'] = self.headers['X-Test-Header']

        return self._reply(500, data, headers)

HEADERS = [
    {'header': 'content-type', 'value': 'text/plain'},
    {'header': 'date', 'value': 'today'},
    {'header': 'server', 'value': 'EchoServer/1.0'}
]

class HTTPServer(socketserver.TCPServer):
    timeout = 0.1
    address_family = socket.AF_INET6
    allow_reuse_address = True

    def __init__(self, *a, **kw):
        super().__init__(*a, **kw)

@pytest.mark.skipif(not C.Context().has_impl('curl'), reason="curl:// channels not supported")
class Test:
    def setup(self):
        self.ctx = C.Context()
        self.loop = asynctll.Loop(context=self.ctx)

    def teardown(self):
        self.loop.stop = 1
        self.loop = None

        self.ctx = None

    async def async_test_autoclose(self):
        with HTTPServer(('::1', ports.TCP6), EchoHandler) as httpd:
            c = self.loop.Channel('curl+http://[::1]:{}/some/path'.format(ports.TCP6), autoclose='yes', dump='text', name='http')
            c.open()

            await self.loop.sleep(0.01)

            httpd.handle_request()

            m = await c.recv()
            assert m.type == m.Type.Control
            assert c.scheme_control.unpack(m).as_dict() == {'code': 200, 'method': -1, 'headers': HEADERS, 'path': f'http://[::1]:{ports.TCP6}/some/path', 'size': -1}

            m = await c.recv()
            assert m.data.tobytes() == b'GET /some/path'

            await self.loop.sleep(0.001)
            assert c.state == c.State.Closed

    def test_autoclose(self):
        self.loop.run(self.async_test_autoclose())

    async def async_test_autoclose_many(self):
        with HTTPServer(('::1', ports.TCP6), EchoHandler) as httpd:
            multi = self.loop.Channel('curl://', name='multi')
            multi.open()

            c0 = self.loop.Channel('curl+http://[::1]:{}/c0'.format(ports.TCP6), autoclose='yes', dump='text', name='c0', master=multi)
            c0.open()

            c1 = self.loop.Channel('curl+http://[::1]:{}/c1'.format(ports.TCP6), autoclose='yes', dump='text', name='c1', master=multi, method='POST')
            c1.open()

            await self.loop.sleep(0.01)

            httpd.handle_request()

            m = await c0.recv()
            assert m.type == m.Type.Control
            assert c0.scheme_control.unpack(m).as_dict() == {'code': 200, 'method': -1, 'headers': HEADERS, 'path': f'http://[::1]:{ports.TCP6}/c0', 'size': -1}

            m = await c0.recv(0.11)
            assert m.data.tobytes() == b'GET /c0'

            httpd.handle_request()

            m = await c1.recv()
            assert m.type == m.Type.Control
            assert c1.scheme_control.unpack(m).as_dict() == {
                'code': 500,
                'method': -1,
                'size': 10,
                'headers': [{'header': 'content-length', 'value': '10'}] + HEADERS,
                'path': f'http://[::1]:{ports.TCP6}/c1',
            }

            m = await c1.recv(0.12)
            assert m.data.tobytes() == b'POST /c1 :'

            await self.loop.sleep(0.001)
            assert c0.state == c0.State.Closed
            assert c1.state == c1.State.Closed

    def test_autoclose_many(self):
        self.loop.run(self.async_test_autoclose_many())

    async def async_test_data(self):
        with HTTPServer(('::1', ports.TCP6), EchoHandler) as httpd:
            c = self.loop.Channel('curl+http://[::1]:{}/post'.format(ports.TCP6), dump='text', name='post', transfer='data', method='POST', **{'expect-timeout': '1000ms', 'header.Expect':'', 'header.X-Test-Header': 'value'})
            c.open()

            await self.loop.sleep(0.01)

            httpd.handle_request()

            with pytest.raises(TimeoutError): await c.recv(0.01)

            for data in [b'xxx', b'zzzz']:
                c.post(data)

                await self.loop.sleep(0.01)

                httpd.handle_request()

                m = await c.recv(0.01)
                assert m.type == m.Type.Control
                assert m.addr == 0
                assert c.scheme_control.unpack(m).as_dict() == {
                    'code': 500,
                    'method': -1,
                    'size': 12 + len(data),
                    'headers': [{'header': 'content-length', 'value': str(12 + len(data))}] + HEADERS + [{'header': 'x-test-header', 'value': 'value'}],
                    'path': f'http://[::1]:{ports.TCP6}/post',
                }

                m = await c.recv(0.12)
                assert m.addr == 0
                assert m.data.tobytes() == b'POST /post :' + data

                await self.loop.sleep(0.001)

                assert c.state == c.State.Active

            for addr, data in enumerate([b'xxx', b'zzzz']):
                c.post(data, addr=addr)

            for addr, data in enumerate([b'xxx', b'zzzz']):
                await self.loop.sleep(0.01)

                httpd.handle_request()

                m = await c.recv(0.01)
                assert m.type == m.Type.Control
                assert m.addr == addr
                assert c.scheme_control.unpack(m).as_dict() == {
                    'code': 500,
                    'method': -1,
                    'size': 12 + len(data),
                    'headers': [{'header': 'content-length', 'value': str(12 + len(data))}] + HEADERS + [{'header': 'x-test-header', 'value': 'value'}],
                    'path': f'http://[::1]:{ports.TCP6}/post',
                }

                m = await c.recv(0.12)
                assert m.addr == addr
                assert m.data.tobytes() == b'POST /post :' + data

                await self.loop.sleep(0.001)

                assert c.state == c.State.Active

    def test_test_data(self):
        self.loop.run(self.async_test_data())
