"""Exercise console routes without requiring a camera or network privileges."""
from email.message import Message
from io import BytesIO
import json
from pathlib import Path
import struct
import tempfile
from types import SimpleNamespace
import unittest

from ai.image_limits import validate_reference
from ai.web_service import DemoBridge, Handler, safe_name, validate_config

CONFIG = dict(mask=17, score=500000, period_us=500000, face_threshold=450000)


class ConsoleTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        storage = Path(self.temp.name)
        self.server = SimpleNamespace(bridge=DemoBridge(), storage=storage,
            profiles=storage/'web-profiles.json', preview=True, token='test-token',
            server_address=('127.0.0.1', 18765), server_port=18765)

    def tearDown(self):
        self.temp.cleanup()

    def request(self, method, path, data=None, **headers):
        handler = object.__new__(Handler)
        handler.server, handler.path = self.server, path
        handler.headers = Message()
        for name, value in {'Host': '127.0.0.1:18765', 'X-Camera-Token': 'test-token', **headers}.items():
            handler.headers[name] = value
        payload = json.dumps(data).encode() if isinstance(data, dict) else data or b''
        handler.headers['Content-Length'] = str(len(payload))
        handler.rfile, handler.wfile = BytesIO(payload), BytesIO()
        statuses = []
        handler.send_response = statuses.append
        handler.send_header = lambda *args: None
        handler.end_headers = lambda: None
        getattr(handler, 'do_'+method)()
        return statuses[0], handler.wfile.getvalue()

    def test_profiles_persist_without_applying_to_camera(self):
        status, _ = self.request('POST', '/api/profiles', dict(name='Entrance', config=CONFIG))
        self.assertEqual(status, 200)
        self.assertEqual(self.server.bridge.config['mask'], 0)
        saved = json.loads(self.server.profiles.read_text())
        self.assertEqual(saved['Entrance'], CONFIG)
        self.assertEqual(self.request('POST', '/api/config', CONFIG)[0], 200)
        self.assertEqual(self.server.bridge.config, CONFIG)

    def test_token_origin_and_host_validation(self):
        for headers in ({'X-Camera-Token': 'wrong'}, {'Origin':'https://evil.example'}, {'Host':'evil.example'}):
            self.assertEqual(self.request('POST', '/api/config', CONFIG, **headers)[0], 403)
        self.assertEqual(self.request('GET', '/api/status', **{'Host':'evil.example'})[0], 403)
        self.assertEqual(self.server.bridge.config['mask'], 0)

    def test_invalid_config_does_not_apply(self):
        for bad in (dict(CONFIG, mask=128), dict(CONFIG, score=True), dict(CONFIG, period_us=99999), dict(CONFIG, extra=1)):
            self.assertEqual(self.request('POST', '/api/config', bad)[0], 400)
        self.assertEqual(self.server.bridge.config['mask'], 0)

    def test_image_dimension_limits_and_gallery_write(self):
        png = b'\x89PNG\r\n\x1a\n'+b'\0\0\0\rIHDR'+struct.pack('>II',320,240)
        self.assertEqual(validate_reference(png), (320,240))
        self.assertEqual(self.request('POST', '/api/gallery', png, **{'X-Identity':'Reference'})[0], 200)
        self.assertTrue((self.server.storage/'gallery-reload.json').exists())
        self.assertEqual(len(list((self.server.storage/'faces/Reference').glob('*.png'))), 1)
        oversized = png[:16]+struct.pack('>II',9000,9000)
        self.assertEqual(self.request('POST', '/api/gallery', oversized, **{'X-Identity':'Reference'})[0], 400)
        self.assertEqual(self.request('POST', '/api/gallery', png, **{'X-Identity':'../escape'})[0], 400)
        # Header acceptance is staging only; the worker must decode and find one face.

    def test_status_and_static_assets(self):
        status, body = self.request('GET', '/api/status')
        self.assertEqual(status, 200)
        state = json.loads(body)
        self.assertTrue(state['preview'])
        self.assertFalse(state['available'])
        self.assertEqual(len(state['features']), 8)
        self.assertEqual(self.request('GET', '/')[0], 200)
        self.assertEqual(self.request('GET', '/../runtime.py')[0], 404)

    def test_name_and_type_validation(self):
        for name in ('../outside', '', 'has spaces', '<script>', 'x'*49):
            with self.assertRaises(ValueError):
                safe_name(name)
        with self.assertRaises(ValueError):
            validate_config([])

if __name__ == '__main__':
    unittest.main()
