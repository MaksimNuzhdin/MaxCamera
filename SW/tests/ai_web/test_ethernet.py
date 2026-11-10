"""Run real native shared-memory copies and HTTP routes over a sparse IPC surrogate."""
import ctypes
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import threading
import time
import unittest

from ai.http_transport import SharedHttp, CONTROL, RESPONSE, RESPONSE_MAX, MAGIC, serve_session
from ai.web_service import Console, DemoBridge

ROOT = Path(__file__).resolve().parents[2]


class EthernetTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        library = self.root/'barrier.so'
        subprocess.run(['cc','-shared','-fPIC','-Wall','-Wextra','-Werror', str(ROOT/'ai/barrier.c'),'-o',str(library)],check=True)
        memory = self.root/'memory'
        with memory.open('wb') as stream:
            stream.truncate(RESPONSE+RESPONSE_MAX)
            stream.seek(CONTROL)
            stream.write(struct.pack('<II',MAGIC,1))
        self.transport = SharedHttp(memory,library)
        self.console = object.__new__(Console)
        self.console.configure(DemoBridge(),self.root/'presets',preview=True)
        self.stop = threading.Event()
        self.thread = threading.Thread(target=serve_session,args=(self.transport,self.console,self.stop))
        self.thread.start()
        self.sequence = 0

    def tearDown(self):
        self.stop.set()
        self.thread.join(3)
        self.assertFalse(self.thread.is_alive())
        self.transport.close()
        self.temp.cleanup()

    def request(self, path='/api/status', body=None, host='10.0.0.8', streaming=False, token=None):
        method = 'GET' if body is None else 'POST'
        payload = json.dumps(body).encode() if body is not None else b''
        headers = [f'{method} {path} HTTP/1.1',f'Host: {host}', f'Content-Length: {len(payload)}']
        if token is not None:
            headers.append('X-Camera-Token: '+token)
        data = ('\r\n'.join(headers)+'\r\n\r\n').encode()+payload
        source = ctypes.create_string_buffer(data)
        self.transport.native.ai_http_write(self.transport.addresses[1],source,len(data))
        self.transport.commit(6,0x0a000008)
        self.transport.commit(7,int(streaming))
        self.transport.commit(4,len(data))
        self.sequence += 1
        self.transport.commit(2,self.sequence)
        until = time.monotonic()+3
        while self.transport.read(3)!=self.sequence and time.monotonic()<until:
            time.sleep(.005)
        self.assertEqual(self.transport.read(3),self.sequence,'Matching response commit')
        length = self.transport.read(5)
        result = ctypes.create_string_buffer(length)
        self.transport.native.ai_http_read(self.transport.addresses[2],result,length)
        header, body = result.raw.split(b'\r\n\r\n',1)
        declared = next(line for line in header.split(b'\r\n') if line.lower().startswith(b'content-length:'))
        self.assertEqual(int(declared.split(b':',1)[1]),len(body))
        return int(header.split()[1]), json.loads(body)

    def test_status_has_ethernet_address_and_token(self):
        status, result = self.request()
        self.assertEqual(status,200)
        self.assertEqual(result['transport'],'ethernet')
        self.assertEqual(result['device_address'],'10.0.0.8')
        self.assertTrue(result['preview'])
        self.assertFalse(result['available'])
        self.assertTrue(result['token'])

    def test_configuration_ack_and_streaming_write_gate(self):
        _, result = self.request()
        config = dict(mask=17, score=600000, period_us=1000000, face_threshold=550000)
        self.assertEqual(self.request('/api/config',config,token=result['token'])[0],200)
        self.assertEqual(self.console.bridge.config,config)
        self.assertEqual(self.request('/api/config',dict(config,mask=4),streaming=True,token=result['token'])[0],409)
        self.assertEqual(self.console.bridge.config,config)
        self.assertEqual(self.request(streaming=True)[0],200)

    def test_profiles_persist_via_same_http_routes(self):
        _, result = self.request()
        config = self.console.bridge.config
        self.assertEqual(self.request('/api/profiles',dict(name='Entry',config=config),token=result['token'])[0],200)
        self.assertEqual(self.request()[1]['profiles']['Entry'],config)
        self.assertEqual(self.console.bridge.config['mask'],0)

    def test_host_and_token_checks_survive_bridge(self):
        self.assertEqual(self.request(host='evil.example')[0],403)
        config = self.console.bridge.config
        self.assertEqual(self.request('/api/config',config,token='wrong')[0],403)

    def test_payload_copy_preserves_unaligned_tail_and_bounds(self):
        for data in (b'x',b'abc',b'abcde',bytes(range(255))*100):
            self.transport.respond(0,data)
            destination=ctypes.create_string_buffer(len(data))
            self.transport.native.ai_http_read(self.transport.addresses[2],destination,len(data))
            self.assertEqual(destination.raw,data)
        with self.assertRaises(ValueError):
            self.transport.request(0)
        with self.assertRaises(ValueError):
            self.transport.respond(0,b'x'*(RESPONSE_MAX+1))

if __name__ == '__main__':
    unittest.main()
