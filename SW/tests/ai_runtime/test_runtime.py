import tempfile
import unittest
from pathlib import Path
import cv2 as cv
import numpy as np
from ai.runtime import Engine, clip_box, ctc_decode, letterbox
from ai.worker import decode_frame, object_words


class AnalyticsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.engine = Engine()

    def test_all_models_forward(self):
        self.assertEqual(self.engine.supported, 255, self.engine.errors)
        image = np.zeros((240,320,3), np.uint8)
        self.assertEqual(self.engine.run(image)['objects'], [])
        recognizer = self.engine.loaded['recognizer']
        feature = recognizer.feature(np.zeros((112,112,3), np.uint8))
        self.assertEqual(feature.shape, (1,128))
        net = self.engine.loaded['ocr']
        net.setInput(cv.dnn.blobFromImage(np.zeros((32,100), np.uint8), 1/127.5, (100,32), (127.5,)))
        self.assertIsInstance(ctc_decode(net.forward()), str)

    def test_ctc_blank_and_repeat(self):
        logits = np.zeros((6,1,37), np.float32)
        for i, cls in enumerate((1,1,0,1,11,11)):
            logits[i,0,cls] = 10
        self.assertEqual(ctc_decode(logits), '00A')
        with self.assertRaises(ValueError):
            ctc_decode(np.zeros((2,37)))

    def test_decode_padding_and_channel_order(self):
        payload = bytes([255,0,0, 0,255,0, 99,99])*2
        image = decode_frame(payload,2,2,8,0x02180014)
        self.assertEqual(image[0,0].tolist(), [0,0,255])
        self.assertEqual(image.shape,(2,2,3))
        for args in ((b'',2,2,8,0x02180014),(bytes(12),3,2,6,0x02100032),(bytes(8),2,2,4,99)):
            with self.assertRaises(ValueError):
                decode_frame(*args)

    def test_coordinate_transform(self):
        padded, ratio = letterbox(np.zeros((120,320,3),np.uint8),640)
        self.assertEqual(padded.shape,(640,640,3))
        self.assertEqual(ratio,2)
        self.assertEqual(clip_box([-5,-5,20,30],10,15),[0,0,10,15])

    def test_motion_tracks_and_disable_reset(self):
        engine = self.engine
        engine.previous = None
        blank = np.zeros((240,320,3),np.uint8)
        engine.run(blank,144)
        moved = blank.copy();moved[80:150,100:160] = 255
        result = engine.run(moved,144)
        self.assertTrue(result['objects'])
        track = result['objects'][0]['track_id']
        self.assertGreater(track,0)
        engine.run(blank,0)
        self.assertIsNone(engine.previous)
        self.assertEqual(engine.tracks,{})

    def test_empty_enrollment_is_unknown_and_explicit_gallery_matches(self):
        fixture = Path('build/ai-downloads/lena.jpg')
        if not fixture.exists():
            self.skipTest('Standard OpenCV fixture is not downloaded')
        image = cv.imread(str(fixture))
        result = self.engine.run(image,3)
        self.assertTrue(result['objects'])
        self.assertEqual(result['objects'][0]['text'],'unknown')
        with tempfile.TemporaryDirectory() as directory:
            gallery = Path(directory)/'faces'/'fixture_identity';gallery.mkdir(parents=True)
            cv.imwrite(str(gallery/'reference.jpg'),image)
            cv.imwrite(str(gallery/'large.png'),cv.resize(image,(2048,2048)))
            engine = Engine(presets=Path(directory))
            self.assertEqual(len(engine.gallery),2)
            recognized = engine.run(image,3)['objects'][0]
            self.assertEqual(recognized['text'],'fixture_identity')
            self.assertGreater(recognized['score'],.9)
        # This self-match tests plumbing, not accuracy or false match performance.

    def test_qr_roundtrip(self):
        qr = cv.QRCodeEncoder_create().encode('MAXCAMERA-AI-TEST')
        qr = cv.copyMakeBorder(qr,4,4,4,4,cv.BORDER_CONSTANT,value=255)
        qr = cv.resize(qr,(264,264),interpolation=cv.INTER_NEAREST)
        result = self.engine.run(cv.cvtColor(qr,cv.COLOR_GRAY2BGR),64)
        self.assertEqual(result['objects'][0]['text'],'MAXCAMERA-AI-TEST')

    def test_object_fixture(self):
        fixture = Path('build/ai-downloads/basketball1.png')
        if not fixture.exists():
            self.skipTest('Standard OpenCV fixture is not downloaded')
        result = self.engine.run(cv.imread(str(fixture)),4)
        self.assertTrue(any(o['text']=='person' for o in result['objects']))

    def test_bounded_utf8_wire_label(self):
        words = object_words(dict(feature=0,class_id=0,score=.7,box=[1,2,3,4],track_id=5,text='x'*100))
        self.assertEqual(len(words),16)
        self.assertEqual(words[2],700000)
        self.assertEqual(words[-1] >> 24,0)

if __name__ == '__main__':
    unittest.main()
