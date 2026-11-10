#!/usr/bin/env python3
"""Camera-local web console; no shell execution and no direct camera MMIO."""
import argparse
import json
import os
from pathlib import Path
import re
import secrets
import threading
import time
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import urlsplit

ASSETS = Path(__file__).with_name('web')
FEATURES = [
    (1, 'Face detection', 'YuNet face bounding boxes'),
    (2, 'Face recognition', 'SFace matching against your reference gallery'),
    (4, 'Object detection', 'YOLOX · 80 COCO classes'),
    (8, 'Plate candidates & OCR', 'Plate cascade + English CRNN; validate for your region'),
    (16, 'Motion regions', 'Sampled-frame difference; sensitive to lighting and camera movement'),
    (32, 'Pedestrian detection', 'HOG/SVM pedestrian detector'),
    (64, 'QR decoding', 'QR payloads; result labels limited to 31 UTF-8 bytes'),
    (128, 'Tracking', 'Nearest-center association; requires a detector'),
]


def validate_config(data):
    limits = {'mask': (0, 255), 'score': (1, 1000000),
              'period_us': (100000, 60000000), 'face_threshold': (1, 1000000)}
    if not isinstance(data, dict) or set(data) != set(limits):
        raise ValueError('Provide exactly mask, score, period_us and face_threshold')
    for key, (low, high) in limits.items():
        if type(data[key]) is not int or not low <= data[key] <= high:
            raise ValueError(f'{key} must be an integer from {low} to {high}')
    if data['mask'] == 128:
        raise ValueError('Tracking requires another detector')
    return data


def safe_name(value):
    if not isinstance(value, str) or not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_-]{0,47}', value):
        raise ValueError('Use 1–48 English letters, digits, underscores or hyphens')
    return value


def atomic_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix('.tmp')
    with temporary.open('w') as stream:
        json.dump(value, stream)
        stream.flush()
        os.fsync(stream.fileno())
    temporary.replace(path)


class Bridge:
    def __init__(self):
        from .worker import Mailbox
        self.device = os.open('/dev/mem', os.O_RDWR | os.O_SYNC)
        self.box = Mailbox(self.device)
        if self.box.read(0) != 0x3149414D or self.box.read(14) != 0x31424557:
            raise RuntimeError('R5 firmware does not support the web configuration channel')
        self.lock = threading.Lock()
        self.sequence = self.box.read(644)
        threading.Thread(target=self.heartbeat, daemon=True).start()

    def heartbeat(self):
        value = self.box.read(651)
        while True:
            value = (value + 1) & 0xffffffff or 1
            self.box.commit(651, value)
            time.sleep(.25)

    def status(self):
        with self.lock:
            for _ in range(10):
                generation = self.box.read(8)
                mask, score, period = self.box.read(9, 3)
                face = self.box.read(13)
                if not generation & 1 and self.box.read(8) == generation:
                    break
            else:
                raise RuntimeError('R5 configuration is changing; retry')
            state = self.box.read(640)
            return dict(available=bool(state & 1), owned=bool(state & 2),
                        web_active=bool(state & 4), supported=self.box.read(7),
                        generation=generation, error=self.box.read(652),
                        inference_us=self.box.read(653),
                        config=dict(mask=mask, score=score, period_us=period, face_threshold=face))

    def apply(self, config):
        with self.lock:
            if self.box.read(644) != self.box.read(645):
                raise RuntimeError('A previous configuration command is still pending; reboot if R5 stopped')
            if self.sequence == 0xffffffff:
                raise RuntimeError('Command sequence exhausted; reboot the camera')
            self.sequence += 1
            self.box.write(647, [config[k] for k in ('mask', 'score', 'period_us', 'face_threshold')])
            self.box.commit(644, self.sequence)
            deadline = time.monotonic() + 2
            while self.box.read(645) != self.sequence:
                if time.monotonic() > deadline:
                    raise RuntimeError('R5 acknowledgement timed out; command outcome is unknown')
                time.sleep(.01)
            error = self.box.read(646)
            if error:
                raise ValueError({0x8006: 'Camera is controlled by a GenICam client',
                                  0x8007: 'AI worker or web lease is unavailable',
                                  0x8002: 'Unsupported feature or invalid configuration'}.get(error, f'R5 error {error}'))


class DemoBridge:
    """Explicit offline UI preview, never presented as a connected camera."""
    def __init__(self):
        self.config = dict(mask=0, score=500000, period_us=500000, face_threshold=450000)
    def status(self):
        return dict(available=False, owned=False, web_active=False, supported=255,
                    generation=0, error=0, inference_us=0, config=self.config)
    def apply(self, config):
        self.config = config


class Console(HTTPServer):
    def __init__(self, address, bridge, storage, preview=False):
        super().__init__(address, Handler)
        self.configure(bridge, storage, preview)

    def configure(self, bridge, storage, preview=False):
        self.bridge, self.storage, self.preview = bridge, storage, preview
        self.token = secrets.token_urlsafe(32)
        self.storage.mkdir(parents=True, exist_ok=True)
        self.profiles = self.storage/'web-profiles.json'


class Handler(BaseHTTPRequestHandler):
    def setup(self):
        super().setup()
        self.connection.settimeout(5)

    def allowed_host(self):
        host = self.headers.get('Host', '')
        expected = f'{self.server.server_address[0]}:{self.server.server_address[1]}'
        return host in (expected, self.server.server_address[0] if self.server.server_port == 80 else expected)

    def log_message(self, *args):
        pass  # Do not log identity names, QR payloads or reference images.

    def respond(self, value, status=200, content_type='application/json'):
        data = json.dumps(value).encode() if content_type == 'application/json' else value
        self.send_response(status)
        self.send_header('Content-Type', content_type)
        self.send_header('Content-Length', str(len(data)))
        self.send_header('Cache-Control', 'no-store')
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.send_header('Content-Security-Policy', "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' blob:; frame-ancestors 'none'; base-uri 'none'; form-action 'self'")
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        try:
            if not self.allowed_host():
                return self.respond({'error': 'Unrecognized camera address'}, 403)
            route = urlsplit(self.path).path
            if route == '/api/status':
                state = self.server.bridge.status()
                worker = Path('/run/maxcamera/worker.json')
                try:
                    details = json.loads(worker.read_text())
                    details['age_s'] = max(0, time.monotonic() - details['updated'])
                    if details.get('generation') != state['generation'] or details['age_s'] > 30:
                        details['objects'] = []
                        details['stale'] = True
                    state['worker'] = details
                except (OSError, ValueError):
                    state['worker'] = None
                state.update(transport=getattr(self.server, 'transport', 'usb'),
                             device_address=self.server.server_address[0],
                             streaming=getattr(self.server, 'streaming', False),
                             storage_persistent=str(self.server.storage.resolve()).startswith('/media/sd/'), preview=self.server.preview, token=self.server.token,
                             features=[dict(bit=b, name=n, description=d) for b, n, d in FEATURES],
                             gallery=self.gallery(), profiles=self.profiles())
                return self.respond(state)
            files = {'/': ('index.html', 'text/html; charset=utf-8'),
                     '/app.js': ('app.js', 'text/javascript'), '/style.css': ('style.css', 'text/css')}
            if route in files:
                name, mime = files[route]
                return self.respond((ASSETS/name).read_bytes(), content_type=mime)
            self.respond({'error': 'Not found'}, 404)
        except (OSError, RuntimeError, ValueError) as exc:
            self.respond({'error': str(exc)}, 503)

    def profiles(self):
        try:
            return json.loads(self.server.profiles.read_text())
        except FileNotFoundError:
            return {}

    def gallery(self):
        root = self.server.storage/'faces'
        return [dict(name=p.name, images=sum(1 for f in p.iterdir() if f.suffix.lower() in ('.jpg', '.jpeg', '.png')))
                for p in sorted(root.glob('*')) if p.is_dir() and not p.is_symlink()]

    def do_POST(self):
        try:
            # The random per-boot token is readable only by the same origin.
            # Reject cross-origin browser writes, including DNS-rebinding origins.
            if getattr(self.server, 'streaming', False):
                return self.respond({'error': 'Stop acquisition before modifying settings or reference data'}, 409)
            if not self.allowed_host():
                return self.respond({'error': 'Unrecognized camera address'}, 403)
            if self.headers.get('X-Camera-Token') != self.server.token:
                return self.respond({'error': 'Reload the console before making changes'}, 403)
            origin = self.headers.get('Origin')
            if origin and origin != 'http://' + self.headers.get('Host', ''):
                return self.respond({'error': 'Cross-origin request rejected'}, 403)
            length = int(self.headers.get('Content-Length', '0'))
            if length < 1 or length > 4*1024*1024:
                return self.respond({'error': 'Request must be between 1 byte and 4 MiB'}, 413)
            payload = self.rfile.read(length)
            route = urlsplit(self.path).path
            if route == '/api/gallery':
                identity = safe_name(self.headers.get('X-Identity'))
                if sum(1 for p in (self.server.storage/'faces').glob('*/*') if p.is_file()) >= 256:
                    raise ValueError('Maximum 256 gallery reference images')
                count = sum(p.stat().st_size for p in (self.server.storage/'faces').glob('*/*') if p.is_file())
                if count + length > 32*1024*1024:
                    raise ValueError('Gallery storage limit is 32 MiB')
                from .image_limits import validate_reference
                validate_reference(payload)
                if payload.startswith(b'\x89PNG\r\n\x1a\n'):
                    suffix = '.png'
                elif payload.startswith(b'\xff\xd8\xff'):
                    suffix = '.jpg'
                else:
                    raise ValueError('Upload a JPEG or PNG reference image')
                directory = self.server.storage/'faces'/identity
                directory.mkdir(parents=True, exist_ok=True)
                if directory.is_symlink():
                    raise ValueError('Invalid identity directory')
                target = directory/(secrets.token_hex(8) + suffix)
                with target.open('xb') as stream:
                    stream.write(payload)
                    stream.flush()
                    os.fsync(stream.fileno())
                atomic_json(self.server.storage/'gallery-reload.json', {'revision': secrets.token_hex(8)})
                return self.respond({'message': 'Reference saved. The worker validates exactly one face before enrollment.'})
            data = json.loads(payload)
            if route == '/api/config':
                self.server.bridge.apply(validate_config(data))
            elif route == '/api/profiles':
                name = safe_name(data['name'])
                config = validate_config(data['config'])
                profiles = self.profiles()
                if name not in profiles and len(profiles) >= 32:
                    raise ValueError('Maximum 32 profiles')
                profiles[name] = config
                atomic_json(self.server.profiles, profiles)
            else:
                return self.respond({'error': 'Not found'}, 404)
            self.respond({'message': 'Saved'})
        except (ValueError, KeyError, TypeError) as exc:
            self.respond({'error': str(exc)}, 400)
        except (OSError, RuntimeError) as exc:
            self.respond({'error': str(exc)}, 503)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bind', default='192.168.7.2')
    parser.add_argument('--port', type=int, default=80)
    parser.add_argument('--storage', type=Path, default=Path(os.environ.get('MAXCAMERA_AI_PRESETS', '/media/sd/presets')))
    parser.add_argument('--preview', action='store_true')
    args = parser.parse_args()
    if args.preview and args.bind not in ('127.0.0.1', '::1'):
        parser.error('Offline preview must bind to loopback')
    Console((args.bind, args.port), DemoBridge() if args.preview else Bridge(), args.storage, args.preview).serve_forever()

if __name__ == '__main__':
    main()
