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
print(json.dumps(dict(status='PASS', supported=engine.supported,
                     inference_us=result['inference_us'], embedding_shape=list(embedding.shape),
                     blank_ocr=ocr, max_rss_kib=next((line.split(':',1)[1].strip() for line in Path('/proc/self/status').read_text().splitlines() if line.startswith('VmHWM:')), 'unavailable'),
                     opencv=cv.__version__, numpy=np.__version__)), flush=True)
