"""Verify that the actual HTTP load loop releases all readers on cancellation."""
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
import importlib.util
import json
from pathlib import Path
import threading
import time
import unittest

spec=importlib.util.spec_from_file_location('http_load',Path(__file__).resolve().parents[2]/'tools/gige_http_concurrency.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)


class ConcurrentLoadTests(unittest.TestCase):
    def test_cancelled_hour_load_stops_all_four_readers(self):
        class Handler(BaseHTTPRequestHandler):
            def log_message(self,*args):pass
            def do_GET(self):
                body=json.dumps({'transport':'ethernet'}).encode()
                self.send_response(200);self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
        server=ThreadingHTTPServer(('127.0.0.1',0),Handler)
        thread=threading.Thread(target=server.serve_forever);thread.start()
        stop=threading.Event();timer=threading.Timer(.15,stop.set);timer.start()
        try:
            start=time.monotonic();summary,records=module.load('127.0.0.1:'+str(server.server_port),3600,4,stop=stop)
            self.assertLess(time.monotonic()-start,5)
            self.assertTrue(summary['passed'],summary)
            self.assertGreaterEqual(len(records),4)
            self.assertEqual({r['client'] for r in records},{0,1,2,3})
        finally:
            stop.set();timer.cancel();server.shutdown();server.server_close();thread.join()

    def test_already_cancelled_load_has_no_request_or_latency(self):
        stop=threading.Event();stop.set()
        summary,records=module.load('127.0.0.1:1',3600,4,stop=stop)
        self.assertEqual(records,[])
        self.assertFalse(summary['passed'])
        self.assertIsNone(summary['latency_ms']['maximum'])


if __name__=='__main__':unittest.main()
