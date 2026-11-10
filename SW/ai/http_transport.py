#!/usr/bin/env python3
"""Serve the camera console over R5 HTTP IPC, without a Linux network device."""
import ctypes
from io import BytesIO
import mmap
import os
from pathlib import Path
import struct
import time

from .web_service import Bridge, Console, Handler

CONTROL = 0x1f004000
REQUEST = 0x1f010000
RESPONSE = 0x1f500000
REQUEST_MAX = 4*1024*1024+8192
RESPONSE_MAX = 128*1024
MAGIC = 0x31505448


class SharedHttp:
    def __init__(self, device='/dev/mem', library=None):
        self.fd = os.open(device, os.O_RDWR | os.O_SYNC)
        self.maps = []
        self.native = ctypes.CDLL(str(library or Path(__file__).with_name('libai_barrier.so')))
        self.native.ai_read32.argtypes = [ctypes.c_void_p]
        self.native.ai_read32.restype = ctypes.c_uint32
        self.native.ai_write32.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
        self.native.ai_http_read.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t]
        self.native.ai_http_write.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t]
        for address, size in ((CONTROL,4096), (REQUEST,REQUEST_MAX), (RESPONSE,RESPONSE_MAX)):
            mapping = mmap.mmap(self.fd, size, flags=mmap.MAP_SHARED,
                                prot=mmap.PROT_READ | mmap.PROT_WRITE, offset=address)
            self.maps.append(mapping)
        self.addresses = [ctypes.addressof(ctypes.c_char.from_buffer(m)) for m in self.maps]
        if self.read(0) != MAGIC or self.read(1) != 1:
            self.close()
            raise RuntimeError('R5 Ethernet HTTP transport is absent or incompatible')

    def read(self, word):
        if not 0 <= word < 16:
            raise ValueError('Invalid HTTP control word')
        return self.native.ai_read32(self.addresses[0]+4*word)

    def commit(self, word, value):
        self.native.ai_barrier()
        self.native.ai_write32(self.addresses[0]+4*word, value)
        self.native.ai_barrier()

    def request(self, length):
        if not 1 <= length <= REQUEST_MAX:
            raise ValueError('Invalid HTTP request length')
        data = ctypes.create_string_buffer(length)
        self.native.ai_http_read(self.addresses[1], data, length)
        return data.raw

    def respond(self, sequence, data):
        if not 1 <= len(data) <= RESPONSE_MAX:
            raise ValueError('Invalid HTTP response length')
        source = ctypes.create_string_buffer(data)
        self.native.ai_http_write(self.addresses[2], source, len(data))
        self.commit(5, len(data))
        self.commit(3, sequence)

    def close(self):
        for mapping in self.maps:
            mapping.close()
        self.maps.clear()
        os.close(self.fd)


def dispatch(console, request):
    """Use the same HTTP routes/security checks as the socket-based preview."""
    handler = object.__new__(Handler)
    handler.server = console
    handler.client_address = ('R5-bridge', 0)
    handler.rfile, handler.wfile = BytesIO(request), BytesIO()
    handler.close_connection = True
    handler.handle_one_request()
    return handler.wfile.getvalue()


def serve():
    transport = SharedHttp()
    console = object.__new__(Console)
    console.configure(Bridge(), Path(os.environ.get('MAXCAMERA_AI_PRESETS', '/run/maxcamera/presets')))
    serve_session(transport, console)


def serve_session(transport, console, stop=None):
    console.transport = 'ethernet'
    console.server_port = 80
    last = transport.read(3)
    beat = transport.read(8)
    while stop is None or not stop.is_set():
        if transport.read(0) != MAGIC:
            raise RuntimeError('HTTP transport reset; reboot both processors')
        beat = (beat+1) & 0xffffffff or 1
        transport.commit(8, beat)
        sequence = transport.read(2)
        if sequence and sequence != last:
            ip = transport.read(6)
            console.server_address = ('.'.join(str(v) for v in struct.pack('>I',ip)),80)
            console.streaming = bool(transport.read(7))
            try:
                result = dispatch(console, transport.request(transport.read(4)))
                if not result or len(result) > RESPONSE_MAX:
                    raise ValueError('Response exceeds HTTP transport capacity')
            except Exception:
                # Do not log uploaded identity images or request headers/tokens.
                result = b'HTTP/1.0 503 Service Unavailable\r\nContent-Length: 0\r\nConnection: close\r\n\r\n'
            transport.respond(sequence, result)
            last = sequence
        if stop is None:
            time.sleep(.01)
        else:
            stop.wait(.01)

if __name__ == '__main__':
    serve()
