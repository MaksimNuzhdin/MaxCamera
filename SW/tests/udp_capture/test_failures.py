"""Offline failure propagation using the real capture orchestration."""
import importlib.util
import json
from pathlib import Path
import subprocess
import unittest
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("capture_under_test", ROOT / "tools/gige_udp_capture.py")
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

class CaptureFailureTests(unittest.TestCase):
    def setUp(self):
        self.camera = Mock()
        self.device = self.camera.get_device.return_value
        self.camera.get_region.return_value = (0, 0, 640, 480)
        self.camera.get_pixel_format.return_value = 0x02100032
        self.camera.get_payload.return_value = 614400
        self.device.get_boolean_feature_value.return_value = False
        self.device.get_integer_feature_value.return_value = 0
        self.device.get_float_feature_value.return_value = 30.0
        self.device.get_string_feature_value.return_value = "YUV422_8"
        self.device.read_register.return_value = 0
        self.process = Mock()
        self.process.stdout.readline.return_value = '{"port":50000}\n'
        self.process.returncode = 0
        self.process.poll.side_effect = lambda: self.process.returncode
        self.process.kill.side_effect = lambda: setattr(self.process, 'returncode', -9)
        self.process.communicate.return_value = ('{"good":90}', '')
        self.sock = Mock()
        self.sock.getsockname.return_value = ('10.0.0.7', 12345)

    def capture(self, callback=None):
        with patch.object(m.subprocess, 'Popen', return_value=self.process), \
             patch.object(m.subprocess, 'run'), \
             patch.object(m.socket, 'socket', return_value=self.sock), \
             patch.object(m.Path, 'exists', return_value=False), \
             patch.object(m.Path, 'mkdir'):
            return m.capture(self.camera, '10.0.0.3', '/tmp/unused-capture.bin', 3, on_started=callback)

    def assert_commands_unchanged(self):
        self.camera.start_acquisition.assert_called_once_with()
        self.camera.stop_acquisition.assert_called_once_with()
        self.assertEqual(self.device.write_register.call_count, 3)

    def test_success_keeps_result_and_timeout(self):
        result = self.capture()
        self.assertTrue(result['passed'])
        self.assertEqual(result['good'], 90)
        self.process.communicate.assert_called_once_with(timeout=28)
        self.process.kill.assert_not_called()
        self.assert_commands_unchanged()

    def test_receiver_failure_stays_failed(self):
        self.process.returncode = 1
        self.assertFalse(self.capture()['passed'])
        self.assert_commands_unchanged()

    def test_stop_failure_preserves_completed_receiver_output(self):
        self.process.returncode = 1
        self.process.communicate.return_value = ('{"good":0,"missing_packets":7}', 'receiver failed')
        stop = RuntimeError('ACCESS_DENIED')
        self.camera.stop_acquisition.side_effect = stop
        with self.assertRaises(m.CaptureFailure) as caught:
            self.capture()
        error = caught.exception
        self.assertIs(error.stop_error, stop)
        self.assertEqual(error.receiver_stderr, 'receiver failed')
        self.assertEqual(json.loads(error.receiver_stdout)['missing_packets'], 7)
        self.assertEqual(error.receiver_returncode, 1)
        self.assertIn('receiver failed', str(error))
        self.assert_commands_unchanged()

    def test_timeout_and_stop_failure_both_survive(self):
        primary = subprocess.TimeoutExpired(['receiver'], 28)
        stop = RuntimeError('Stop timeout')
        self.process.returncode = None
        self.process.communicate.side_effect = [primary, ('partial receiver result', 'no frame received')]
        self.camera.stop_acquisition.side_effect = stop
        with self.assertRaises(m.CaptureFailure) as caught:
            self.capture()
        error = caught.exception
        self.assertIs(error.primary_error, primary)
        self.assertIs(error.__cause__, primary)
        self.assertIs(error.stop_error, stop)
        self.assertEqual(error.receiver_stdout, 'partial receiver result')
        self.assertEqual(error.receiver_stderr, 'no frame received')
        self.process.kill.assert_called_once_with()
        self.assert_commands_unchanged()

    def test_timeout_partial_output_survives_drain_failure(self):
        primary = subprocess.TimeoutExpired(['receiver'], 28, output=b'partial JSON', stderr=b'receiver diagnostic')
        self.process.returncode = None
        self.process.communicate.side_effect = [primary, OSError('pipe read failed')]
        with self.assertRaises(m.CaptureFailure) as caught:
            self.capture()
        self.assertIs(caught.exception.primary_error, primary)
        self.assertEqual(caught.exception.receiver_stdout, 'partial JSON')
        self.assertIn('receiver diagnostic', caught.exception.receiver_stderr)
        self.assertIn('pipe read failed', caught.exception.receiver_stderr)
        self.assert_commands_unchanged()

    def test_callback_failure_remains_primary(self):
        primary = ValueError('cache sample failed')
        self.process.returncode = None
        with self.assertRaises(m.CaptureFailure) as caught:
            self.capture(Mock(side_effect=primary))
        self.assertIs(caught.exception.primary_error, primary)
        self.assertIsNone(caught.exception.stop_error)
        self.process.kill.assert_called_once_with()
        self.assert_commands_unchanged()

    def test_interrupt_still_stops_acquisition(self):
        self.process.returncode = None
        with self.assertRaises(KeyboardInterrupt):
            self.capture(Mock(side_effect=KeyboardInterrupt))
        self.assert_commands_unchanged()
        self.process.kill.assert_called_once_with()

if __name__ == '__main__':
    unittest.main()
