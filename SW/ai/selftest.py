"""Target CPU/runtime smoke test; no physical camera or identity accuracy claim."""
import json
from pathlib import Path
import cv2 as cv
import numpy as np
from .runtime import Engine, ctc_decode
from .worker import decode_frame, object_words

engine = Engine()
assert engine.supported == 255, engine.errors
blank = np.zeros((240, 320, 3), np.uint8)
result = engine.run(blank)
assert result['objects'] == [], result
# Forward the OCR and recognition networks even with empty enrollment.
net = engine.loaded['ocr']
net.setInput(cv.dnn.blobFromImage(np.zeros((32,100), np.uint8), 1/127.5, (100,32), (127.5,)))
ocr = ctc_decode(net.forward())
embedding = engine.loaded['recognizer'].feature(np.zeros((112,112,3), np.uint8))
assert embedding.shape == (1,128)
assert decode_frame(bytes(8*2), 2, 2, 8, 0x02180014).shape == (2,2,3)
assert len(object_words(dict(feature=0,class_id=0,score=.8,box=[0,0,1,1],track_id=1,text='test'))) == 16
# Exercise the new Ethernet backend dependencies and scalar-copy helper on the
# target A53 runtime without claiming physical shared-memory qualification.
import ctypes
import tempfile
from .http_transport import dispatch
from .web_service import Console, DemoBridge
with tempfile.TemporaryDirectory() as directory:
    console = object.__new__(Console)
    console.configure(DemoBridge(), Path(directory), preview=True)
    console.server_address = ('10.0.0.8',80)
    console.server_port = 80
    console.transport = 'ethernet'
    console.streaming = False
    response = dispatch(console,b'GET /api/status HTTP/1.1\r\nHost: 10.0.0.8\r\n\r\n')
    status = json.loads(response.split(b'\r\n\r\n',1)[1])
    assert status['transport'] == 'ethernet' and not status['available']
    native = ctypes.CDLL(str(Path(__file__).with_name('libai_barrier.so')))
    native.ai_http_write.argtypes = [ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t]
    native.ai_http_read.argtypes = [ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t]
    source = ctypes.create_string_buffer(b'HTTP1')
    shared = ctypes.create_string_buffer(8)
    copied = ctypes.create_string_buffer(5)
    native.ai_http_write(shared,source,5)
    native.ai_http_read(shared,copied,5)
    assert copied.raw == b'HTTP1'
print(json.dumps(dict(status='PASS', ethernet_http_smoke=True, supported=engine.supported,
                     inference_us=result['inference_us'], embedding_shape=list(embedding.shape),
                     blank_ocr=ocr, max_rss_kib=next((line.split(':',1)[1].strip() for line in Path('/proc/self/status').read_text().splitlines() if line.startswith('VmHWM:')), 'unavailable'),
                     opencv=cv.__version__, numpy=np.__version__)), flush=True)
