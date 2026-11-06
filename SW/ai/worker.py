#!/usr/bin/env python3
"""Single-slot A53 worker. /dev/mem control is reserved and uncached; no camera MMIO."""
import argparse
import mmap
import os
import struct
import threading
import time
from pathlib import Path
import cv2 as cv
import numpy as np
try:
    from .runtime import Engine, ROOT
except ImportError:
    from runtime import Engine, ROOT

MAGIC = 0x3149414D
BASE, SIZE = 0x1F000000, 8192


def decode_frame(payload, width, height, stride, pfnc):
    bpp = {0x02180014: 3, 0x02180015: 3, 0x01080001: 1, 0x02100032: 2}.get(pfnc)
    if not bpp or not 1 <= width <= 2592 or not 1 <= height <= 1944 or not width*bpp <= stride <= 7808:
        raise ValueError('Invalid frame descriptor')
    if pfnc == 0x02100032 and width % 2:
        raise ValueError('YUYV width must be even')
    if len(payload) != height*stride:
        raise ValueError('Invalid frame length')
    rows = np.frombuffer(payload, np.uint8).reshape(height, stride)[:, :width*bpp]
    image = rows.reshape(height, width, bpp).copy()
    if pfnc == 0x02180014:
        image = cv.cvtColor(image, cv.COLOR_RGB2BGR)
    elif pfnc == 0x01080001:
        image = cv.cvtColor(image, cv.COLOR_GRAY2BGR)
    elif pfnc == 0x02100032:
        image = cv.cvtColor(image, cv.COLOR_YUV2BGR_YUY2)
    return image


def object_words(obj):
    label = obj['text'].encode('utf-8')[:31]
    # Avoid emitting a truncated UTF-8 code point.
    label = label.decode('utf-8', 'ignore').encode('utf-8')
    return [obj['feature'], obj['class_id'], round(obj['score']*1000000),
            *obj['box'], obj['track_id'] & 0xffffffff,
            *struct.unpack('<8I', label.ljust(32, b'\0'))]


class Mailbox:
    def __init__(self, fd):
        self.fd = fd
        self.map = mmap.mmap(fd, SIZE, flags=mmap.MAP_SHARED,
                             prot=mmap.PROT_READ | mmap.PROT_WRITE, offset=BASE)
        # A real architectural barrier is mandatory across uncached device mappings.
        import ctypes
        self.ctypes = ctypes
        self.native = ctypes.CDLL(str(ROOT/'libai_barrier.so'), use_errno=True)
        self.barrier = self.native.ai_barrier
        self.barrier.restype = None
        self.native.ai_read32.argtypes = [ctypes.c_void_p]
        self.native.ai_read32.restype = ctypes.c_uint32
        self.native.ai_write32.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
        self.native.ai_write32.restype = None
        self.native.ai_copy_physical.argtypes = [ctypes.c_int, ctypes.c_uint64, ctypes.c_void_p, ctypes.c_size_t]
        self.native.ai_copy_physical.restype = ctypes.c_int
        self.base = ctypes.addressof(ctypes.c_char.from_buffer(self.map))

    def read(self, word, count=1):
        if word < 0 or count < 1 or word+count > SIZE//4:
            raise ValueError('Control read exceeds mailbox')
        values = tuple(self.native.ai_read32(self.base+4*(word+i)) for i in range(count))
        return values[0] if count == 1 else values

    def write(self, word, values):
        if isinstance(values, int):
            values = [values]
        if word < 0 or word+len(values) > SIZE//4:
            raise ValueError('Control write exceeds mailbox')
        for i, value in enumerate(values):
            self.native.ai_write32(self.base+4*(word+i), value)

    def commit(self, word, value):
        self.barrier()
        self.write(word, value)
        self.barrier()

    def copy_frame(self, descriptor):
        _, _, address, w, h, stride, fmt, *_ = descriptor
        length = h*stride
        if address < 0x100000 or address+length > 0x10000000 or length > 15200000:
            raise ValueError('Capture address outside R5 carveout')
        if not 1 <= w <= 2592 or not 1 <= h <= 1944 or not 1 <= stride <= 7808:
            raise ValueError('Invalid capture geometry')
        target = self.ctypes.create_string_buffer(length)
        if self.native.ai_copy_physical(self.fd, address, target, length):
            error = self.ctypes.get_errno()
            raise OSError(error, os.strerror(error))
        return target.raw


def serve():
    parser = argparse.ArgumentParser()
    parser.add_argument('--device', default='/dev/mem')
    args = parser.parse_args()
    engine = Engine(presets=Path(os.environ.get("MAXCAMERA_AI_PRESETS", ROOT/"presets")))
    with os.fdopen(os.open(args.device, os.O_RDWR | os.O_SYNC), 'r+b', buffering=0) as device:
        box = Mailbox(device.fileno())
        if box.read(0) != MAGIC or box.read(1) != 1:
            raise RuntimeError('R5 mailbox is absent or incompatible')
        box.commit(7, engine.supported)
        stopped = threading.Event()
        def heartbeat():
            value = 0
            while not stopped.is_set():
                value = (value+1) & 0xffffffff or 1
                box.commit(6, value)
                stopped.wait(.25)
        thread = threading.Thread(target=heartbeat, daemon=True)
        thread.start()
        last = 0
        try:
            while True:
                request = box.read(2)
                if not request or request == last or box.read(4) != box.read(5):
                    time.sleep(.01)
                    continue
                descriptor = box.read(32, 10)
                config = descriptor[1]
                generation, mask, score, _ = box.read(8, 4)
                face_threshold = box.read(13)
                output, error = dict(objects=[], inference_us=0), 0
                try:
                    if descriptor[0] != request or generation != config or config & 1 or box.read(8) != config:
                        raise ValueError('Stale descriptor/configuration')
                    payload = box.copy_frame(descriptor)
                    # Release only after every physical capture read has finished.
                    box.commit(3, request)
                    image = decode_frame(payload, *descriptor[3:7])
                    output = engine.run(image, mask, score/1e6, face_threshold/1e6)
                except Exception as exc:
                    # An exception unwinds the mapping before acknowledging. No retained reader.
                    box.commit(3, request)
                    print(f'AI request {request} failed: {exc}', flush=True)
                    error = 1
                if box.read(8) != config:
                    output, error = dict(objects=[], inference_us=0), 3
                for i, obj in enumerate(output['objects'][:32]):
                    box.write(96+i*16, object_words(obj))
                box.write(64, [request, config, min(32, len(output['objects'])),
                               min(0xffffffff, output['inference_us']), error])
                box.commit(4, request)
                last = request
        finally:
            stopped.set()
            thread.join()

if __name__ == '__main__':
    serve()
