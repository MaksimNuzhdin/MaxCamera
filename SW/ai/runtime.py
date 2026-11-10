#!/usr/bin/env python3
"""Bounded CPU analytics. Run on A53; host execution is offline validation only."""
import argparse
import json
import os
import time
from pathlib import Path
import cv2 as cv
import numpy as np

ROOT = Path(__file__).resolve().parent
COCO = ('person bicycle car motorcycle airplane bus train truck boat traffic_light fire_hydrant stop_sign parking_meter bench bird cat dog horse sheep cow elephant bear zebra giraffe backpack umbrella handbag tie suitcase frisbee skis snowboard sports_ball kite baseball_bat baseball_glove skateboard surfboard tennis_racket bottle wine_glass cup fork knife spoon bowl banana apple sandwich orange broccoli carrot hot_dog pizza donut cake chair couch potted_plant bed dining_table toilet tv laptop mouse remote keyboard cell_phone microwave oven toaster sink refrigerator book clock vase scissors teddy_bear hair_drier toothbrush').split()
FEATURES = dict(faces=1, recognition=2, objects=4, plates=8, motion=16, people=32, qr=64, tracking=128)


def box_result(feature, box, score=1., cls=0, text=''):
    return dict(feature=feature, class_id=int(cls), score=float(np.clip(score, 0, 1)),
                box=[int(v) for v in box], track_id=0, text=text)


def clip_box(box, width, height):
    x, y, w, h = box
    x1, y1 = max(0, min(width, int(x))), max(0, min(height, int(y)))
    x2, y2 = max(0, min(width, int(x+w))), max(0, min(height, int(y+h)))
    return [x1, y1, x2-x1, y2-y1]


def letterbox(image, size):
    h, w = image.shape[:2]
    ratio = min(size/w, size/h)
    resized = cv.resize(image, (max(1, round(w*ratio)), max(1, round(h*ratio))))
    out = np.full((size, size, 3), 114, np.uint8)
    out[:resized.shape[0], :resized.shape[1]] = resized
    return out, ratio


def ctc_decode(logits):
    """CRNN EN: time, batch, blank+digits+lowercase. Greedy CTC decoding."""
    a = np.asarray(logits)
    if a.ndim != 3 or a.shape[1] != 1 or a.shape[2] != 37:
        raise ValueError('Unexpected CRNN output shape')
    ids = a[:, 0].argmax(axis=1)
    alphabet = '0123456789abcdefghijklmnopqrstuvwxyz'
    chars, previous = [], -1
    for idx in ids:
        if idx and idx != previous:
            chars.append(alphabet[idx-1])
        previous = idx
    return ''.join(chars).upper()


class Engine:
    def __init__(self, models=ROOT/'models', presets=ROOT/'presets', threads=1):
        cv.setNumThreads(threads)
        self.models, self.presets = Path(models), Path(presets)
        self.loaded, self.gallery, self.previous = {}, [], None
        self.tracks, self.next_track, self.frame = {}, 1, 0
        self.errors = {}
        self.gallery_errors = {}
        self.supported = 16 | 32 | 64 | 128
        self.hog = cv.HOGDescriptor()
        self.hog.setSVMDetector(cv.HOGDescriptor_getDefaultPeopleDetector())
        self.qr = cv.QRCodeDetector()
        factories = {
            'face': (1, lambda: cv.FaceDetectorYN.create(str(self.models/'face_detection_yunet_2023mar.onnx'), '', (320, 320), 0.5)),
            'recognizer': (2, lambda: cv.FaceRecognizerSF.create(str(self.models/'face_recognition_sface_2021dec.onnx'), '')),
            'objects': (4, lambda: cv.dnn.readNet(str(self.models/'object_detection_yolox_2022nov.onnx'))),
            'ocr': (8, lambda: cv.dnn.readNet(str(self.models/'text_recognition_CRNN_EN_2021sep.onnx'))),
        }
        for name, (bit, factory) in factories.items():
            try:
                self.loaded[name] = factory()
                self.supported |= bit
                # Initramfs weights occupy RAM twice after network import. Release only
                # the explicitly disposable boot copy; never delete SD/user model files.
                if os.environ.get('MAXCAMERA_RELEASE_BOOT_MODELS') == '1' and self.models.resolve() == Path('/opt/maxcamera/ai/models'):
                    patterns = {'face':'face_detection_*.onnx', 'recognizer':'face_recognition_*.onnx', 'objects':'object_detection_*.onnx', 'ocr':'text_recognition_*.onnx'}
                    for path in self.models.glob(patterns[name]):
                        path.unlink()
            except (cv.error, OSError) as exc:
                self.errors[name] = str(exc)
        cascade = cv.CascadeClassifier(str(self.models/'haarcascade_russian_plate_number.xml'))
        self.plate = None if cascade.empty() else cascade
        if self.plate is None:
            self.supported &= ~8
        if 'face' not in self.loaded:
            self.supported &= ~2
        if self.supported & 2:
            self.load_gallery()

    def detect_faces(self, image, score):
        detector = self.loaded['face']
        detector.setInputSize((image.shape[1], image.shape[0]))
        detector.setScoreThreshold(score)
        return detector.detect(image)[1]

    def load_gallery(self):
        """Only explicit local identity directories are enrolled. Empty means unknown."""
        self.gallery = []
        self.gallery_errors = {}
        for identity in sorted((self.presets/'faces').glob('*')):
            if not identity.is_dir():
                continue
            for path in sorted(identity.iterdir()):
                if path.suffix.lower() not in ('.jpg', '.jpeg', '.png'):
                    continue
                try:
                    from .image_limits import validate_reference
                except ImportError:
                    from image_limits import validate_reference
                try:
                    if path.stat().st_size > 32*1024*1024:
                        raise ValueError('Reference exceeds local storage limit')
                    validate_reference(path.read_bytes(), max_bytes=32*1024*1024)
                except (OSError, ValueError):
                    self.gallery_errors[str(path)] = 'Invalid or oversized reference image'
                    continue
                try:
                    image = cv.imread(str(path))
                    if image is None:
                        self.gallery_errors[str(path)] = 'Invalid reference image'
                        continue
                    # Bound enrollment inference just like live frames; full-size
                    # phone photos must not allocate full-resolution DNN tensors.
                    h, w = image.shape[:2]
                    if max(w, h) > 640:
                        ratio = 640/max(w, h)
                        image = cv.resize(image, (max(1, round(w*ratio)), max(1, round(h*ratio))))
                    faces = self.detect_faces(image, .7)
                    if faces is None or len(faces) != 1:
                        self.gallery_errors[str(path)] = 'Enrollment requires exactly one face'
                        continue
                    r = self.loaded['recognizer']
                    self.gallery.append((identity.name, r.feature(r.alignCrop(image, faces[0]))))
                except (cv.error, ValueError):
                    self.gallery_errors[str(path)] = 'Reference decoding or enrollment failed'


    def detect_objects(self, image, score):
        padded, ratio = letterbox(image, 640)
        net = self.loaded['objects']
        net.setInput(cv.dnn.blobFromImage(padded, 1., (640, 640), swapRB=True))
        predictions = net.forward().reshape(-1, 85).copy()
        grids, strides = [], []
        for stride in (8, 16, 32):
            y, x = np.mgrid[:640//stride, :640//stride]
            grids.append(np.stack((x, y), axis=-1).reshape(-1, 2))
            strides.extend([stride]*x.size)
        grid, stride = np.concatenate(grids), np.array(strides)[:, None]
        if len(predictions) != len(grid):
            raise ValueError('Unexpected YOLOX output shape')
        predictions[:, :2] = (predictions[:, :2]+grid)*stride
        predictions[:, 2:4] = np.exp(np.clip(predictions[:, 2:4], -20, 20))*stride
        class_ids = predictions[:, 5:].argmax(axis=1)
        scores = predictions[:, 4]*predictions[np.arange(len(predictions)), class_ids+5]
        indices = np.flatnonzero(scores >= score)
        boxes = predictions[indices, :4]/ratio
        boxes[:, :2] -= boxes[:, 2:]/2
        results = []
        # Per-class NMS prevents overlapping objects of different types suppressing each other.
        for cls in np.unique(class_ids[indices]):
            subset = np.flatnonzero(class_ids[indices] == cls)
            keep = cv.dnn.NMSBoxes(boxes[subset].tolist(), scores[indices[subset]].tolist(), score, .45)
            for local in np.asarray(keep).reshape(-1):
                i = subset[int(local)]
                results.append(box_result(2, boxes[i], scores[indices[i]], cls, COCO[int(cls)]))
        return results

    def run(self, image, mask=255, score=.5, face_threshold=.45):
        if image is None or image.ndim != 3 or image.dtype != np.uint8 or image.shape[2] != 3:
            raise ValueError('Expected uint8 BGR image')
        if mask & ~self.supported:
            raise ValueError('Requested feature is unavailable')
        start = time.monotonic_ns()
        h, w = image.shape[:2]
        # Bound all inference inputs; returned coordinates refer to the original image.
        scale = min(1., 640/max(w, h))
        small = cv.resize(image, (max(1, round(w*scale)), max(1, round(h*scale))))
        out = []
        if mask & 3:
            faces = self.detect_faces(small, score)
            for face in [] if faces is None else faces[:32]:
                name, similarity = 'unknown', 0.
                if mask & 2 and self.gallery:
                    r = self.loaded['recognizer']
                    embedding = r.feature(r.alignCrop(small, face))
                    for label, reference in self.gallery:
                        match = float(r.match(embedding, reference, cv.FaceRecognizerSF_FR_COSINE))
                        if match >= face_threshold and match > similarity:
                            name, similarity = label, match
                feature = 1 if mask & 2 else 0
                out.append(box_result(feature, face[:4], similarity if name != 'unknown' else float(face[-1]), 1 if name != 'unknown' else 0, name if mask & 2 else 'face'))
        if mask & 4:
            out.extend(self.detect_objects(small, score))
        gray = cv.cvtColor(small, cv.COLOR_BGR2GRAY)
        if mask & 8:
            plates = self.plate.detectMultiScale(gray, 1.1, 3, minSize=(30, 10))
            for x, y, bw, bh in plates[:8]:
                crop = gray[y:y+bh, x:x+bw]
                net = self.loaded['ocr']
                net.setInput(cv.dnn.blobFromImage(crop, 1/127.5, (100, 32), (127.5,)))
                text = ctc_decode(net.forward())
                out.append(box_result(3, [x, y, bw, bh], 1., text=text))
        if mask & 16:
            if self.previous is not None and self.previous.shape == gray.shape:
                diff = cv.absdiff(gray, self.previous)
                binary = cv.threshold(diff, 25, 255, cv.THRESH_BINARY)[1]
                binary = cv.morphologyEx(binary, cv.MORPH_OPEN, np.ones((3, 3), np.uint8))
                contours = cv.findContours(binary, cv.RETR_EXTERNAL, cv.CHAIN_APPROX_SIMPLE)[0]
                for contour in sorted(contours, key=cv.contourArea, reverse=True)[:8]:
                    if cv.contourArea(contour) > max(25, gray.size*.001):
                        out.append(box_result(4, cv.boundingRect(contour), text='motion'))
            self.previous = gray.copy()
        else:
            self.previous = None
        if mask & 32 and small.shape[0] >= 128 and small.shape[1] >= 64:
            boxes, weights = self.hog.detectMultiScale(small, winStride=(8, 8), scale=1.1)
            for box, weight in zip(boxes, weights):
                if float(weight) >= score:
                    out.append(box_result(5, box, min(1., float(weight)), text='person'))
        if mask & 64:
            ok, labels, points, _ = self.qr.detectAndDecodeMulti(small)
            if ok:
                for label, polygon in zip(labels, points):
                    if label:
                        out.append(box_result(6, cv.boundingRect(polygon.astype(np.float32)), text=label))
        out = sorted(out, key=lambda o: o['score'], reverse=True)[:32]
        for obj in out:
            obj['box'] = clip_box([v/scale for v in obj['box']], w, h)
        out = [o for o in out if o['box'][2] and o['box'][3]]
        self.frame += 1
        if mask & 128:
            used = set()
            for obj in out:
                x, y, bw, bh = obj['box']
                center = np.array([x+bw/2, y+bh/2])
                best, distance = None, max(30, min(w, h)*.1)
                for tid, (feature, cls, old, frame) in self.tracks.items():
                    d = np.linalg.norm(center-old)
                    if tid not in used and feature == obj['feature'] and cls == obj['class_id'] and self.frame-frame <= 3 and d < distance:
                        best, distance = tid, d
                if best is None:
                    best, self.next_track = self.next_track, self.next_track+1
                obj['track_id'] = best
                self.tracks[best] = obj['feature'], obj['class_id'], center, self.frame
                used.add(best)
            self.tracks = {k: v for k, v in self.tracks.items() if self.frame-v[3] <= 3}
        else:
            self.tracks.clear()
        return dict(objects=out, inference_us=(time.monotonic_ns()-start)//1000, width=w, height=h)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('image')
    parser.add_argument('--mask', type=lambda x: int(x, 0), default=255)
    parser.add_argument('--models', type=Path, default=ROOT/'models')
    parser.add_argument('--presets', type=Path, default=ROOT/'presets')
    args = parser.parse_args()
    engine = Engine(args.models, args.presets)
    result = engine.run(cv.imread(args.image), args.mask)
    result.update(supported=engine.supported, gallery_entries=len(engine.gallery), errors=engine.errors)
    print(json.dumps(result, indent=2))

if __name__ == '__main__':
    main()
