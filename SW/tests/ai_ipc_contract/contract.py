"""Offline proposal oracle; not firmware, an IPC driver, or a cache implementation."""
from dataclasses import dataclass
import struct

# Wire data is explicitly little-endian, fixed-width and independent of C padding.
WIRE = struct.Struct('<4sHHIIQQIIIIII')
RGB8 = 1

@dataclass(frozen=True)
class Descriptor:
    session: int
    config: int
    sequence: int
    timestamp_us: int
    width: int
    height: int
    stride: int
    offset: int
    length: int
    format: int = RGB8

    def encode(self):
        return WIRE.pack(b'MAI1', 1, WIRE.size, self.session, self.config,
                         self.sequence, self.timestamp_us, self.width, self.height,
                         self.stride, self.offset, self.length, self.format)

    @classmethod
    def decode(cls, data, reserved_bytes):
        if len(data) != WIRE.size:
            raise ValueError('Descriptor length')
        magic, version, size, *fields = WIRE.unpack(data)
        if (magic, version, size) != (b'MAI1', 1, WIRE.size):
            raise ValueError('Descriptor protocol')
        d = cls(*fields)
        if not d.session or not d.config or not d.sequence:
            raise ValueError('Reserved generation')
        if d.format != RGB8 or not 1 <= d.width <= 2592 or not 1 <= d.height <= 1944:
            raise ValueError('Unsupported input profile')
        if d.offset % 64 or d.stride < d.width * 3:
            raise ValueError('Alignment or stride')
        required = (d.height - 1) * d.stride + d.width * 3
        # Subtraction avoids accepting wrapped offset+length in a C implementation.
        if d.offset > reserved_bytes or d.length > reserved_bytes - d.offset or d.length < required:
            raise ValueError('Reserved-memory bounds')
        return d

RESULT = struct.Struct('<4sHHIIQQI')
OBJECT = struct.Struct('<IIiiii')  # class, Q16 score, x, y, width, height
MAX_OBJECTS = 16

def encode_result(session, config, sequence, timestamp_us, objects):
    if len(objects) > MAX_OBJECTS:
        raise ValueError('Object count')
    return RESULT.pack(b'MAR1', 1, RESULT.size, session, config, sequence,
                       timestamp_us, len(objects)) + b''.join(OBJECT.pack(*o) for o in objects)

def decode_result(data):
    if len(data) < RESULT.size:
        raise ValueError('Result length')
    magic, version, size, session, config, seq, stamp, count = RESULT.unpack_from(data)
    if (magic, version, size) != (b'MAR1', 1, RESULT.size) or count > MAX_OBJECTS:
        raise ValueError('Result protocol')
    if len(data) != RESULT.size + count * OBJECT.size:
        raise ValueError('Result payload')
    objects = tuple(OBJECT.unpack_from(data, RESULT.size + i * OBJECT.size) for i in range(count))
    if any(score > 65536 or min(x,y) < 0 or min(w,h) <= 0 for _,score,x,y,w,h in objects):
        raise ValueError('Object fields')
    return session, config, seq, stamp, objects

class Controller:
    """Reference state machine: no blocking operations or physical memory access."""
    def __init__(self):
        self.session = self.config = 1
        self.enabled = False
        self.lease = None
        self.result = self.snapshot = None
        self.fault = False

    def configure(self, enabled):
        if self.config == 0xffffffff:
            raise ValueError('New quiesced session required before generation wrap')
        self.config += 1
        self.enabled = enabled
        self.result = self.snapshot = None

    def offer(self, sequence):
        if not self.enabled or self.fault or self.lease is not None:
            return None
        self.lease = (self.session, self.config, sequence)
        return self.lease

    def release(self, token):
        if token != self.lease:
            raise ValueError('Lease identity')
        self.lease = None

    def publish(self, wire):
        s,c,seq,stamp,objects = decode_result(wire)
        if not self.enabled or self.fault or (s,c) != (self.session,self.config):
            return False
        if self.result and seq <= self.result[2]:
            return False
        self.result = (s,c,seq,stamp,objects)
        return True

    def latch(self):
        self.snapshot = self.result  # Immutable tuple; indexed reads stay on this version.
        return self.snapshot

    def timeout(self):
        self.fault = True
        self.result = self.snapshot = None
        # A timeout cannot revoke a worker's read lease.

    def restart(self, quiesced):
        if not quiesced or self.session == 0xffffffff:
            raise ValueError('Confirmed quiescence and fresh session required')
        self.session += 1
        self.config = 1
        self.enabled = self.fault = False
        self.lease = self.result = self.snapshot = None
