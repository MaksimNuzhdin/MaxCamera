#!/usr/bin/env python3
"""Serial hardware QA with discovery, image evidence and verified per-case rollback.

Uses volatile controls only. Never saves UserSets or writes Flash, SD or OTP.
Run with GI_TYPELIB_PATH and LD_LIBRARY_PATH pointing at build/aravis/usr/lib/...
"""
import argparse
from collections import Counter
import csv
from datetime import datetime, timezone
import gc
import hashlib
import html
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import socket
import signal
import shutil
import subprocess
import sys
import threading
import time
import traceback
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'camera_validation'))
import numpy as np
from PIL import Image
from camera_validation.cases import Context, Skip, CleanupError
from camera_validation.functional import functional_cases
from camera_validation.functional import assert_images
from camera_validation.acquisition import ImageSession
from camera_validation.imaging import decode, validate_bars
from configure_fpga_scaler import apply_working
from gige_probe import Control
from gige_profile_backup import full_snapshot, full_restore
from gige_udp_capture import capture
from qa_image_checks import raw_guard_bars
import stream_led_camera_common as common
from stream_led_camera_common import Aravis, idle, mode, read, write


def save(path, value):
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_text(json.dumps(value, indent=2) + '\n')
    temporary.replace(path)


def report(output, document):
    rows = []
    for row in document['results']:
        name = html.escape(row['id'])
        detail = html.escape(row.get('error', row.get('restoration_error', '')))
        images = ''.join(f'<a href="{name}/{html.escape(p.name)}"><img width="240" src="{name}/{html.escape(p.name)}"></a>'
                         for p in sorted((output/row['id']).glob('*.png'))[:4])
        rows.append(f'<tr><td>{name}<br><small>{html.escape(row["purpose"])}</small></td>'
                    f'<td class="{row["status"]}">{row["status"]}<br>{detail}</td>'
                    f'<td>{row.get("restored", False)}</td><td>{images}</td></tr>')
    page = ('<!doctype html><html lang="en"><meta charset="utf-8"><title>MaxCamera QA results</title>'
            '<style>body{font:15px system-ui;margin:28px;background:#101826;color:#dbe7f5}'
            'table{border-collapse:collapse;width:100%}td,th{padding:10px;text-align:left;border-bottom:1px solid #445}'
            '.PASS{color:#7edec9}.FAIL,.ERROR{color:#ff9b9b}small{color:#9caec5}</style>'
            '<h1>MaxCamera QA results</h1><p>'+html.escape(document['status'])+'</p><p>'
            +html.escape(json.dumps(document.get('counts', {})))+'</p><p>'
            +html.escape('; '.join(document['limitations']))+'</p>'
            '<table><tr><th>Test and purpose</th><th>Status</th><th>Restored</th><th>Retained images</th></tr>'
            +''.join(rows)+'</table></html>')
    (output/'report.html').write_text(page)


def discover(ip, host):
    wire = Control(ip, host)
    try:
        # Request a unicast ACK: this host filters broadcast UDP replies.
        response, peer = wire.command(2, flags=1)
        assert len(response) == 248
        mac = ':'.join(f'{x:02x}' for x in response[10:16])
        assert mac == '02:00:00:00:00:01', 'Unexpected physical development unit'
        wire.ip = peer[0]
        assert wire.read(0xa00) == 0 and wire.read(0x10018) == 0, 'Camera owned or acquiring'
        return dict(ip=peer[0], mac=mac,
                    model=response[104:136].split(b'\0')[0].decode('ascii'))
    finally:
        wire.sock.close()


def start_web(args):
    """Local QA interface; camera access uses GigE Vision, never camera HTTP."""
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def do_GET(self):
            try:
                if self.path == '/api/camera':
                    payload = json.dumps(discover(args.ip, args.host)).encode()
                    kind = 'application/json'
                elif self.path == '/api/status':
                    payload = (args.output/'results.json').read_bytes()
                    kind = 'application/json'
                elif self.path == '/':
                    payload = WEB_PAGE.encode()
                    kind = 'text/html; charset=utf-8'
                else:
                    from urllib.parse import unquote
                    path = (args.output/unquote(self.path.lstrip('/'))).resolve()
                    if not path.is_relative_to(args.output.resolve()) or not path.is_file():
                        self.send_error(404)
                        return
                    payload = path.read_bytes()
                    kind = 'image/png' if path.suffix == '.png' else 'application/octet-stream'
                self.send_response(200)
                self.send_header('Content-Type', kind)
                self.send_header('Content-Length', str(len(payload)))
                self.send_header('Cache-Control', 'no-store')
                self.end_headers()
                self.wfile.write(payload)
            except Exception as exc:
                self.send_error(503, str(exc))
    server = ThreadingHTTPServer(('127.0.0.1', args.web_port), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    args.web_url = f'http://127.0.0.1:{server.server_port}'
    print(json.dumps(dict(event='web_ready', url=args.web_url)), flush=True)
    return server


WEB_PAGE = '''<!doctype html><html lang="en"><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1"><title>MaxCamera QA Cycle</title>
<style>body{font:16px system-ui;background:#101826;color:#dbe7f5;margin:32px}h1{color:#7edec9}
table{border-collapse:collapse;width:100%}td,th{text-align:left;padding:10px;border-bottom:1px solid #334155}
a{color:#8ccaff}.PASS{color:#7edec9}.FAIL,.ERROR{color:#ff9b9b}img{max-width:480px}small{color:#9caec5}</style>
<h1>MaxCamera QA Cycle</h1><p id="summary">Loading results...</p>
<small>Camera discovery: GigE Vision. Serial isolated tests with verified runtime restoration.</small>
<table><thead><tr><th>Test</th><th>Purpose</th><th>Result</th><th>Evidence</th></tr></thead><tbody id="rows"></tbody></table>
<script>async function refresh(){try{let s=await(await fetch('/api/status')).json();
document.getElementById('summary').textContent=(s.firmware||'Connecting')+' / '+s.status+' / '+s.results.filter(r=>r.status==='PASS').length+' passed of '+s.results.length;
let t=document.getElementById('rows');t.replaceChildren();for(let r of s.results){let tr=document.createElement('tr');
for(let text of [r.id,r.purpose,r.status]){let td=document.createElement('td');td.textContent=text;td.className=r.status;tr.append(td)}
let td=document.createElement('td');if(r.status!=='NOT_RUN'){let a=document.createElement('a');a.href='/'+encodeURIComponent(r.id)+'/result.json';a.textContent='Result JSON';td.append(a)}
tr.append(td);t.append(tr)}}catch(e){document.getElementById('summary').textContent=e.message}}refresh();setInterval(refresh,3000)</script></html>'''


class Adapter:
    """Use the existing Aravis owner for portable pixel cases."""
    def __init__(self, device, ip):
        self.device, self.ip, self.trace = device, ip, []

    def read(self, address):
        return self.device.read_register(address)

    def write(self, address, value):
        self.device.write_register(address, value & 0xffffffff)


def counters(device):
    return {key: read(device, key) for key in
            ['CSI_ERRORS', 'RECOVERIES', 'DROPPED', 'CAPTURED', 'LED_STATE',
             'TX_ERRORS', 'GEM_ERRORS', 'RX_DROPS', 'RX_PACKETS', 'TX_PACKETS']}


def host_network_counters():
    """Cumulative host diagnostics; unrelated traffic can affect these counters."""
    result = {}
    for directory in sorted(Path('/sys/class/net').glob('*/statistics')):
        for path in directory.iterdir():
            try:
                result['nic.'+directory.parent.name+'.'+path.name] = int(path.read_text())
            except (OSError, ValueError):
                pass
    try:
        lines = Path('/proc/net/snmp').read_text().splitlines()
        for header, values in zip(lines, lines[1:]):
            if header.startswith('Udp:') and values.startswith('Udp:'):
                result.update({'udp.'+key: int(value) for key, value in
                               zip(header.split()[1:], values.split()[1:])})
        rows = [line.split() for line in Path('/proc/net/softnet_stat').read_text().splitlines()]
        for name, column in [('dropped', 1), ('time_squeeze', 2)]:
            result['softnet.'+name] = sum(int(row[column], 16) for row in rows)
    except (OSError, ValueError):
        pass
    return result


def reject_write(device, address, value, *, volatile=False):
    original = device.read_register(address)
    error = None
    try:
        device.write_register(address, value)
    except Exception as exc:
        error = str(exc)
        assert 'timeout' not in error.lower(), 'Timeout is not proof of rejection: '+error
    actual = device.read_register(address)
    if actual != original and not volatile:
        device.write_register(address, original)
    assert error is not None, f'Invalid/protected write accepted at {address:#x}'
    if volatile:
        assert 'write-protect' in error.lower(), 'Volatile read-only rejection must report write protection'
    else:
        assert actual == original, f'Rejected write changed state at {address:#x}'
    return dict(address=hex(address), attempted=value, original=original, actual=actual,
                volatile_readback=volatile, error=error)


def native(camera, fmt='YUV422_8', width=640, height=480, bars=True):
    mode(camera, width, height, fmt)
    write(camera.get_device(), 'TEST_PATTERN', int(bars))
    # A full-array YUV/RAW stream cannot sustain nominal FPS with20us pacing.
    camera.get_device().write_register(0xd08, 0 if width*height > 1920*1080 else 20)


def fpga(camera):
    apply_working(camera)
    device = camera.get_device()
    for key, value in [('EXPOSURE_AUTO', 0), ('GAIN_AUTO', 0), ('EXPOSURE_LINES', 100),
                       ('GAIN', 16), ('TEST_PATTERN', 1), ('FPGA_AWB', 0),
                       ('FPGA_GAMMA', 0), ('FPGA_BPC', 0)]:
        write(device, key, value)
    device.write_register(0xd08, 20)


def stream(camera, args, folder, label='stream', seconds=4, bars=False, callback=None,
           allow_black=False, require_motion=False):
    device = camera.get_device()
    before = counters(device)
    path = folder / (label + '.bin')
    trace = folder / (label + '.csv')
    telemetry = []
    def monitor():
        deadline = time.monotonic()+seconds
        last_progress = time.monotonic()
        last_size = 0
        while time.monotonic() < deadline:
            sample = dict(elapsed_seconds=seconds-(deadline-time.monotonic()),
                          temperature_c=device.get_float_feature_value('DeviceTemperature'),
                          exposure_lines=read(device, 'EXPOSURE_LINES'), gain_raw=read(device, 'GAIN'),
                          **counters(device))
            telemetry.append(sample)
            assert -20 < sample['temperature_c'] < 95, 'Temperature outside QA operating bounds'
            assert sample['CSI_ERRORS'] == before['CSI_ERRORS'], 'CSI error during stream'
            assert sample['RECOVERIES'] == before['RECOVERIES'], 'Recovery during stream'
            assert sample['DROPPED'] == before['DROPPED'], 'Camera dropped frames during stream: '+json.dumps(sample)
            for key in ['GEM_ERRORS', 'RX_DROPS']:
                assert sample[key] == before[key], 'Camera Ethernet error during stream: '+json.dumps(sample)
            current_size = trace.stat().st_size if trace.exists() else 0
            if current_size > last_size:
                last_progress, last_size = time.monotonic(), current_size
            assert time.monotonic()-last_progress < 10, 'No complete frame for ten seconds'
            save(folder/(label+'-progress.json'), sample)
            time.sleep(min(2, max(0, deadline-time.monotonic())))
    try:
        result = capture(camera, args.ip, path, seconds, on_started=callback or monitor,
                         trace=trace, receiver=args.receiver, packet_trace=folder/(label+'-packet-failure-window.csv'))
    finally:
        save(folder/(label+'-telemetry.json'), telemetry)
    save(folder / (label + '-transport.json'), result)
    if path.is_file():
        image = decode(path.read_bytes(), camera.get_pixel_format_as_string(), *camera.get_region()[2:])
        preview = image
        if camera.get_pixel_format_as_string().startswith('Bayer') and camera.get_pixel_format_as_string() != 'BayerBG8':
            preview = (image / 4).clip(0, 255).astype(np.uint8)
        Image.fromarray(preview.astype(np.uint8)).save(folder / (label + '.png'))
    assert result['passed'] and result['good'] >= 2, result
    assert result['counters']['DroppedFrames'] == 0, result
    assert result['counters']['EthernetErrors'] == 0 and result['counters']['EthernetReceiveDrops'] == 0, result
    assert read(device, 'RECOVERIES') == before['RECOVERIES'], 'Camera recovered during test'
    assert read(device, 'CSI_ERRORS') == before['CSI_ERRORS'], 'CSI errors increased'
    rows = list(csv.DictReader(trace.open()))
    stamps = np.array([int(row['timestamp_ticks']) for row in rows], dtype=np.int64)
    assert len(rows) == result['good']
    assert np.all(np.diff(stamps) > 0), 'Non-monotonic capture timestamps'
    blocks = [int(row['block_id']) for row in rows]
    assert all((b-a) % 65535 == 1 for a, b in zip(blocks, blocks[1:])), 'GVSP BlockID gap'
    hosts = np.array([float(row['completed_host_seconds']) for row in rows])
    gaps = np.diff(hosts)
    nominal = device.get_float_feature_value('ResultingFrameRate')
    assert result['delivered_fps'] >= nominal * .90, 'Delivered FPS below nominal by >10%'
    assert float(gaps.max()) < max(.25, 2.0 / nominal), 'Stream interruption'
    steady_intervals = np.diff(stamps[3:])
    if len(steady_intervals):
        assert float(steady_intervals.max()) < 1.75e6/nominal, 'Missing sensor cadence despite contiguous BlockIDs'
    means = [float(row['byte_mean']) for row in rows[3:]]
    if require_motion:
        hashes = [row['fnv64'] for row in rows[3:]]
        assert len(set(hashes)) >= 3, 'Rolling pattern did not produce distinct frames'
        assert all(len(set(hashes[i:i+30])) > 1 for i in range(0, len(hashes)-29, 30)), 'Frozen rolling pattern'
    if means and not allow_black:
        assert all(0 < value < 255 for value in means), 'Blank/clipped frame byte mean'
    fmt = camera.get_pixel_format_as_string()
    width, height = camera.get_region()[2:]
    image = decode(path.read_bytes(), fmt, width, height)
    luminance = image.mean(axis=2) if image.ndim == 3 else image
    metrics = dict(mean=float(luminance.mean()), spatial_std=float(luminance.std()),
                   minimum=float(image.min()), maximum=float(image.max()),
                   luminance_percentiles=np.percentile(luminance, [1, 10, 50, 90, 99]).tolist())
    save(folder/(label+'-image-metrics.json'), metrics)
    result['image_metrics'] = metrics
    if not allow_black and not read(device, 'FPGA_ROI_ENABLE'):
        assert float(np.std(luminance)) > 1, 'No observable spatial image content'
    if bars:
        result['pixel_oracle'] = (raw_guard_bars(image, fmt, 50) if fmt.startswith('Bayer')
                                  else validate_bars(image, fmt, 50))
    preview = image
    if fmt.startswith('Bayer') and fmt != 'BayerBG8':
        preview = (image / 4).clip(0, 255).astype(np.uint8)
    Image.fromarray(preview.astype(np.uint8)).save(folder / (label + '.png'))
    result.update(max_gap_seconds=float(gaps.max()), timestamp_monotonic=True,
                  contiguous_blocks=True, sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                  image_scope='Every frame: transport, timing, byte mean/hash; final frame: decoder and optional bar oracle')
    save(folder / (label + '-result.json'), result)
    return result


def inventory(args):
    cases = []
    def xml(camera, folder):
        wire = Control(args.ip, args.host)
        try:
            url = wire.memory(0x200, 512).split(b'\0')[0].decode('ascii')
            assert url.startswith('Local:')
            _, address, length = url[6:].split(';')
            address, length = int(address, 16), int(length, 16)
            assert 0 < length < 4*1024*1024
            data = b''.join(wire.memory(address+off, min(512, ((length-off+3)//4)*4))
                            for off in range(0, length, 512))[:length]
            import xml.etree.ElementTree as ET
            root = ET.fromstring(data)
            names = {node.get('Name') for node in root.iter()}
            assert {'Width', 'Height', 'PixelFormat', 'AcquisitionStart', 'MaxAI'} <= names
            (folder/'camera.xml').write_bytes(data)
            return dict(bytes=length, nodes=len(names), sha256=hashlib.sha256(data).hexdigest())
        finally:
            wire.sock.close()
    cases.append(('protocol.xml', 'Download and parse camera-served GenICam XML', xml))
    def owner(camera, folder):
        wire = Control(args.ip, args.host)
        try:
            try:
                wire.write(0xa00, 2)
            except RuntimeError as exc:
                assert '8006' in str(exc)
                return dict(second_controller_rejected=True)
            raise AssertionError('Second controller acquired an owned camera')
        finally:
            wire.sock.close()
    cases.append(('protocol.exclusive_owner', 'Reject a second controller during the isolated test session', owner))
    for pattern in [2, 3, 4]:
        for fmt in ['Mono8', 'RGB8']:
            def run(camera, folder, pattern=pattern, fmt=fmt):
                native(camera, fmt)
                write(camera.get_device(), 'TEST_PATTERN', pattern)
                result = stream(camera, args, folder, allow_black=pattern==4,
                                require_motion=pattern==2)
                if pattern == 4:
                    assert result['image_metrics']['mean'] <= 32, 'Black pattern is too bright'
                return result
            cases.append((f'pattern.{pattern}.{fmt}', 'Pattern-specific content, decoding and stream continuity', run))
    def rolling(camera, folder):
        native(camera, 'RGB8')
        write(camera.get_device(), 'TEST_ROLL', 1)
        return stream(camera, args, folder, seconds=6, require_motion=True)
    cases.append(('pattern.rolling.native', 'Reject frozen native RGB rolling-pattern output', rolling))
    formats = json.loads((ROOT / 'camera_validation/camera_validation/formats.json').read_text())
    for entry in formats:
        fmt = entry['name']
        def run(camera, folder, fmt=fmt):
            native(camera, fmt)
            return stream(camera, args, folder, bars=True)
        cases.append(('native.bars.' + fmt, 'Independent bar pixels and transport after format selection', run))
    for repeat in range(1, 11):
        def run(camera, folder):
            native(camera, 'RGB444_16BE')
            return stream(camera, args, folder, bars=True)
        cases.append((f'stress.rgb444.repeat{repeat:02d}', 'Reproduce intermittent RGB444 packet loss in an isolated capture', run))
    for width, height in [(160, 120), (320, 240), (854, 480), (1280, 720), (1920, 1080), (2592, 1944)]:
        for fmt in ['Mono8', 'YUV422_8', 'RGB8', 'BayerBG10p']:
            if fmt == 'BayerBG10p' and width % 4:
                continue
            def run(camera, folder, fmt=fmt, width=width, height=height):
                native(camera, fmt, width, height)
                return stream(camera, args, folder, bars=True, seconds=5)
            cases.append((f'geometry.{fmt}.{width}x{height}', 'Resolution switch, decoded bars and uninterrupted delivery', run))
    def invalid_raw_width(camera, folder):
        native(camera, 'YUV422_8', 854, 480)
        try:
            camera.get_device().set_string_feature_value('PixelFormat', 'BayerBG10p')
        except Exception as exc:
            assert 'Value not found in <Enumeration>' in str(exc), str(exc)
            rejection = str(exc)
        else:
            raise AssertionError('XML accepted packed RAW10 width not divisible by four')
        assert camera.get_pixel_format_as_string() in ['YUV422_8', 'YUV422_YUYVPacked']
        return dict(rejection=rejection, valid_stream_after_rejection=stream(camera, args, folder, bars=True))
    cases.append(('negative.raw10p.width854', 'Reject unsupported packed-RAW width through XML and preserve valid streaming', invalid_raw_width))
    for repeat in range(1, 6):
        def run(camera, folder):
            native(camera, 'YUV422_8', 2592, 1944)
            device = camera.get_device()
            before = counters(device)
            measured = []
            def active():
                measured.append(read(device, 'TELEMETRY_DEFERRED'))
                time.sleep(2)
                measured.append(read(device, 'TELEMETRY_DEFERRED'))
            result = capture(camera, args.ip, folder/'frame.bin', 5,
                             on_started=active, trace=folder/'frames.csv', receiver=args.receiver)
            save(folder/'transport.json', result)
            after = counters(device)
            assert result['good'] >= 2, 'No valid complete frame before Stop'
            assert read(device, 'SENSOR_READY') and not read(device, 'SENSOR_BUSY')
            assert after['RECOVERIES'] == before['RECOVERIES'], 'Stop triggered automatic recovery'
            assert after['CSI_ERRORS'] == before['CSI_ERRORS'], 'CSI error before Stop'
            assert device.read_register(0xa00) == 2, 'Stop revoked control'
            device.write_register(0xd08, 0)
            return dict(before=before, after=after, transport=result,
                        telemetry_deferred=measured,
                        scope='SCCB drain and retained control after Stop; transport losses remain separately visible')
        cases.append((f'lifecycle.full_yuv_stop.repeat{repeat:02d}',
                      'Isolate full-array YUV Stop/SCCB recovery and retained ownership', run))
    def paced_full_yuv(camera, folder):
        native(camera, 'YUV422_8', 2592, 1944)
        camera.get_device().write_register(0xd08, 1)
        return stream(camera, args, folder, seconds=10, bars=True)
    cases.append(('transport.full_yuv.paced1', 'Full-array YUV with explicit1us pacing; require nominal FPS and complete pixels', paced_full_yuv))
    def paced_fpga_rgb(camera, folder):
        fpga(camera)
        write(camera.get_device(), 'TEST_ROLL', 1)
        camera.get_device().write_register(0xd08, 100)
        return stream(camera, args, folder, seconds=60, require_motion=True)
    cases.append(('transport.fpga_rgb.paced100', 'Bounded60s host-buffer mitigation with100us pacing; retain nominal FPS and moving pixels', paced_fpga_rgb))
    for name, address, value in [('width.zero', 0x10000, 0), ('width.overflow', 0x10000, 2593),
                                  ('height.zero', 0x10004, 0), ('height.overflow', 0x10004, 1945),
                                  ('format.unknown', 0x10008, 0xffffffff),
                                  ('packet.too_small', 0xd04, 1), ('packet.jumbo', 0xd04, 9000),
                                  ('packet_delay.overflow', 0xd08, 1000001),
                                  ('heartbeat.zero', 0x938, 0), ('transport_lock.invalid', 0x1001c, 2),
                                  ('stream_port.overflow', 0xd00, 65536),
                                  ('destination.loopback', 0xd18, 0x7f000001),
                                  ('destination.broadcast', 0xd18, 0xffffffff),
                                  ('payload.read_only', 0x1000c, 0),
                                  ('sensor_id.read_only', common.REG['SENSOR_ID'], 0),
                                  ('temperature.read_only', common.REG['TEMPERATURE'], 0)]:
        def run(camera, folder, address=address, value=value):
            native(camera, 'Mono8')
            rejection = reject_write(camera.get_device(), address, value,
                                     volatile=address == common.REG['TEMPERATURE'])
            return dict(rejection=rejection, valid_stream_after_rejection=stream(camera, args, folder, seconds=2, bars=True))
        cases.append(('negative.'+name, 'Reject invalid register write without state corruption, then validate pixels', run))
    for name, address in [('width.unaligned', 0x10001), ('unmapped', 0xf0000000),
                          ('acquisition.write_only', 0x10014)]:
        def run(camera, folder, address=address):
            native(camera, 'Mono8')
            try:
                camera.get_device().read_register(address)
            except Exception as exc:
                rejection = str(exc)
                assert 'timeout' not in rejection.lower(), 'Timeout does not establish read rejection'
            else:
                raise AssertionError(f'Invalid register read accepted at {address:#x}')
            return dict(address=hex(address), rejection=rejection,
                        valid_stream_after_rejection=stream(camera, args, folder, seconds=2, bars=True))
        cases.append(('negative.read.'+name, 'Reject invalid register reads and retain a valid following stream', run))
    for name, address, value in [('width', 0x10000, 320), ('packet_size', 0xd04, 1400),
                                  ('packet_delay', 0xd08, 21), ('destination_port', 0xd00, 1)]:
        def run(camera, folder, address=address, value=value):
            native(camera, 'Mono8')
            rejection = []
            def active():
                rejection.append(reject_write(camera.get_device(), address, value))
                time.sleep(1)
            result = stream(camera, args, folder, seconds=3, bars=True, callback=active)
            return dict(rejection=rejection, uninterrupted_stream=result)
        cases.append(('locked.transport.'+name, 'Reject transport/geometry mutation during acquisition without interrupting pixels', run))
    for key in ['FPGA_ENABLE', 'FPGA_SCALER', 'FPGA_GAMMA']:
        def run(camera, folder, key=key):
            fpga(camera)
            device = camera.get_device()
            rejected = []
            def during_stream():
                device.set_integer_feature_value('TLParamsLocked', 1)
                rejected.append(reject_write(device, common.REG[key], read(device, key)^1))
                time.sleep(1)
                device.set_integer_feature_value('TLParamsLocked', 0)
            return dict(rejections=rejected, capture=stream(camera, args, folder, callback=during_stream))
        cases.append(('locked.streaming.'+key, 'Reject structural ISP changes during acquisition and preserve output', run))
    for case in functional_cases():
        if case.id.startswith(('flash.', 'pixels.fixture.', 'pixels.bars.')):
            continue
        def run(camera, folder, case=case):
            native(camera, bars=False)
            context = Context(Adapter(camera.get_device(), args.ip), args)
            context.case_output, context.evidence, context.fixture = folder, {}, {}
            context.fixture_root = ROOT
            return case.action(context)
        cases.append((case.id, case.purpose, run))
    for key, value in [('FPGA_GAMMA', 1), ('FPGA_CCM', 1), ('FPGA_AWB', 2),
                       ('FPGA_MEDIAN', 1), ('FPGA_BPC', 1), ('FPGA_BILATERAL', 1),
                       ('FPGA_SHARPEN', 64), ('FPGA_LSC', 1), ('FPGA_CLAHE', 1),
                       ('FPGA_ADAPTIVE_TONE', 1), ('FPGA_BLACK', 32)]:
        def run(camera, folder, key=key, value=value):
            fpga(camera)
            baseline = stream(camera, args, folder, 'baseline')
            write(camera.get_device(), key, value)
            changed = stream(camera, args, folder, 'enabled')
            write(camera.get_device(), key, 0)
            restored = stream(camera, args, folder, 'disabled')
            return dict(baseline=baseline, changed=changed, restored=restored,
                        scope='Stream/decoded-image regression; no optical efficacy claim')
        cases.append(('fpga.toggle.' + key, 'Baseline/enable/disable FPGA feature with actual images', run))
    for factor in [1, 2, 4]:
        def run(camera, folder, factor=factor):
            fpga(camera)
            device = camera.get_device()
            for key, value in [('FPGA_ROI_WIDTH', 320), ('FPGA_ROI_HEIGHT', 240),
                               ('FPGA_ROI_X', 976), ('FPGA_ROI_Y', 732), ('FPGA_ROI_ENABLE', 1)]:
                write(device, key, value)
            device.set_integer_feature_value('Width', 320*factor)
            device.set_integer_feature_value('Height', 240*factor)
            return stream(camera, args, folder, seconds=8)
        cases.append((f'fpga.roi.upscale.{factor}x', 'ROI/scaler transport and decoded output', run))
    def live_black(camera, folder):
        fpga(camera)
        device = camera.get_device()
        baseline = stream(camera, args, folder, 'baseline')
        changes = []
        def update():
            device.set_integer_feature_value('TLParamsLocked', 1)
            assert read(device, 'FPGA_BLACK_LIVE_ACTIVE') == 1
            for value in [0, 64, 1023, 0]:
                write(device, 'FPGA_BLACK', value)
                changes.append(dict(black=value, at=time.monotonic()))
                time.sleep(.7)
            device.set_integer_feature_value('TLParamsLocked', 0)
        result = stream(camera, args, folder, 'live', seconds=6, callback=update, allow_black=True)
        rows = list(csv.DictReader((folder/'live.csv').open()))
        means = [float(row['byte_mean']) for row in rows]
        assert min(means) == 0 and max(means) > 10, 'Live black sweep did not affect received pixels'
        assert (folder/'live.bin').read_bytes() == (folder/'baseline.bin').read_bytes(), 'Live black image did not restore'
        return dict(baseline=baseline, live=result, changes=changes)
    cases.append(('live.black_sweep', 'Change live Black Level without restarting stream; verify black pixels and exact restoration', live_black))
    for fps in [1, 2, 5, 10, 15]:
        def run(camera, folder, fps=fps):
            native(camera, 'Mono8')
            device = camera.get_device()
            write(device, 'FRAME_RATE_ENABLE', 1)
            write(device, 'SENSOR_FPS', fps*1000)
            return stream(camera, args, folder, seconds=max(5, int(4/fps)), bars=True)
        cases.append((f'timing.fps.{fps}', 'Delivered timestamps and FPS after sensor rate change', run))
    def stress(camera, folder):
        records = []
        for index in range(args.stress_cycles):
            fmt = ['Mono8', 'RGB8', 'BayerBG10p', 'YUV422_8'][index % 4]
            width, height = [(640, 480), (320, 240), (1280, 720)][index % 3]
            native(camera, fmt, width, height)
            records.append(stream(camera, args, folder, f'cycle-{index:03d}', seconds=2, bars=True))
            print(json.dumps(dict(event='stress_cycle', cycle=index+1, total=args.stress_cycles)), flush=True)
        return dict(cycles=records)
    cases.append(('stress.mode_switch_restart', 'Repeated format/geometry/start/stop transitions with bar oracles', stress))
    def soak(camera, folder):
        fpga(camera)
        write(camera.get_device(), 'TEST_ROLL', 1)
        return stream(camera, args, folder, seconds=args.soak_seconds, require_motion=True)
    cases.append(('endurance.rgb.one_hour', 'Continuous rolling-pattern FPGA RGB stream; reject frozen content and transport gaps', soak))
    def scene(camera, folder):
        apply_working(camera)
        camera.get_device().write_register(0xd08, 20)
        return stream(camera, args, folder, seconds=min(args.soak_seconds, 60))
    cases.append(('endurance.scene.auto', 'Real scene with automatic exposure/gain and display gamma; require observable content', scene))
    def native_scene(camera, folder):
        native(camera, 'RGB8', bars=False)
        device = camera.get_device()
        for key in ['EXPOSURE_AUTO', 'GAIN_AUTO', 'AWB_AUTO']:
            write(device, key, 2)
        result = stream(camera, args, folder, seconds=8)
        result['exposure_lines'] = read(device, 'EXPOSURE_LINES')
        result['gain_raw'] = read(device, 'GAIN')
        return result
    cases.append(('scene.native.auto', 'Verify bright-scene pixels on native sensor ISP with automatic controls', native_scene))
    for exposure, gain in [(100, 16), (500, 16), (1500, 16), (1500, 64), (1500, 256), (1500, 512)]:
        def run(camera, folder, exposure=exposure, gain=gain):
            fpga(camera)
            device = camera.get_device()
            write(device, 'TEST_PATTERN', 0)
            write(device, 'EXPOSURE_LINES', min(exposure, read(device, 'EXPOSURE_MAX_LINES')))
            write(device, 'GAIN', gain)
            return dict(exposure_lines=read(device, 'EXPOSURE_LINES'), gain_raw=read(device, 'GAIN'),
                        capture=stream(camera, args, folder, seconds=6))
        cases.append((f'scene.fpga.exposure{exposure}.gain{gain}', 'Fixed exposure/gain real-scene observability and stable FPGA stream', run))
    def ai_available(camera, folder):
        if not camera.get_device().get_boolean_feature_value('MaxAIAvailable'):
            raise Skip('Camera-only QA boot: A53 Linux worker is not running')
        return dict(available=True, scope='Availability only; no inference or accuracy assertion')
    cases.append(('capability.ai_worker', 'Explicitly report Linux AI-worker availability for this boot', ai_available))
    def gain_aba(camera, folder):
        fpga(camera)
        device = camera.get_device()
        write(device, 'TEST_PATTERN', 0)
        write(device, 'EXPOSURE_LINES', min(1500, read(device, 'EXPOSURE_MAX_LINES')))
        records = []
        images = []
        for index, gain in enumerate([256, 512, 256]):
            write(device, 'GAIN', gain)
            records.append(stream(camera, args, folder, f'gain-{index}-{gain}', seconds=6))
            images.append(np.array(Image.open(folder/f'gain-{index}-{gain}.png')).astype(float))
        baseline, high, restored = [image.mean(axis=2) for image in images]
        mask = (baseline > 8) & (baseline < 120) & (high < 245)
        assert mask.mean() >= .1, 'Insufficient observable gain-response pixels'
        stability = float(np.mean(np.abs(baseline-restored)))
        ratio = float(np.median(high[mask]/baseline[mask]))
        metrics = dict(gain_sequence=[256, 512, 256], response_ratio=ratio,
                       restored_baseline_mae=stability, usable_fraction=float(mask.mean()))
        save(folder/'response.json', metrics)
        if stability > 4:
            raise Skip('A/B/A baseline changed; illumination or sensor state confounds gain response')
        assert ratio >= 1.15, 'Doubling gain did not increase luminance by15%: '+json.dumps(metrics)
        return dict(captures=records, metrics=metrics)
    cases.append(('response.fpga.gain_aba', 'Predict monotonic gain response on fixed real scene and verify A/B/A stability', gain_aba))
    for feature, low, high, gain in [('ExposureTime', 500, 1000, 256),
                                     ('GainFactor', 128, 256, 128)]:
        def run(camera, folder, feature=feature, low=low, high=high, gain=gain):
            native(camera, bars=False)
            context = Context(Adapter(camera.get_device(), args.ip), args)
            context.case_output, context.evidence = folder, {}
            with ImageSession(context) as session:
                session.setup('RGB8', 0)
                session.set('ExposureTime', min(1000, session.get('ExposureMaximumLines')))
                session.set('GainFactor', gain)
                session.set(feature, low)
                baseline, _ = session.capture('baseline')
                repeat, _ = session.capture('baseline-repeat')
                assert_images(context, 'noise', baseline, baseline, repeat)
                session.set(feature, high)
                actual, _ = session.capture('increased')
                session.set(feature, low)
                restored, _ = session.capture('restored')
                assert_images(context, 'restoration', baseline, baseline, restored)
                a, b = [image @ np.array([.299, .587, .114]) for image in [baseline, actual]]
                mask = (a > 8) & (a < 120) & (b < 245)
                if mask.mean() < .1:
                    raise Skip('Insufficient observable unclipped pixels for native response')
                ratio = float(np.median(b[mask]/a[mask]))
                metrics = dict(feature=feature, low=low, high=high, ratio=ratio,
                               usable_fraction=float(mask.mean()), minimum_ratio=1.15)
                save(folder/'response.json', metrics)
                assert ratio >= 1.15, 'Manual control response below minimum: '+json.dumps(metrics)
            return dict(metrics=metrics, images=context.evidence)
        cases.append(('response.native.'+feature+'.aba', 'Native real-scene response with usable manual gain/exposure and A/B/A restoration', run))
    for channel, index in [('Red', 0), ('Green', 1), ('Blue', 2)]:
        def run(camera, folder, channel=channel, index=index):
            native(camera, bars=False)
            context = Context(Adapter(camera.get_device(), args.ip), args)
            context.case_output, context.evidence = folder, {}
            with ImageSession(context) as session:
                session.setup('RGB8', 0)
                session.set('ExposureTime', min(1000, session.get('ExposureMaximumLines')))
                session.set('GainFactor', 256)
                baseline, _ = session.capture('baseline')
                repeat, _ = session.capture('baseline-repeat')
                assert_images(context, 'noise', baseline, baseline, repeat)
                session.set('WhiteBalance'+channel, 1536)
                actual, _ = session.capture('channel-increased')
                session.set('WhiteBalance'+channel, 1024)
                restored, _ = session.capture('restored')
                assert_images(context, 'restoration', baseline, baseline, restored)
                mask = np.all((baseline > 15) & (baseline < 150) & (actual < 230), axis=2)
                if mask.mean() < .1:
                    raise Skip('Insufficient observable unclipped pixels for color-channel response')
                ratios = np.median(actual[mask]/baseline[mask], axis=0)
                metrics = dict(channel=channel, channel_ratios=ratios.tolist(),
                               usable_fraction=float(mask.mean()))
                save(folder/'response.json', metrics)
                assert ratios[index] > 1.12, 'Selected color channel did not increase: '+json.dumps(metrics)
                assert ratios[index] > max(ratios[i] for i in range(3) if i != index)+.06, 'Wrong color channel changed: '+json.dumps(metrics)
            return dict(metrics=metrics, images=context.evidence)
        cases.append(('response.native.white_balance.'+channel, 'Selected color-channel response on bright manual recipe and A/B/A restoration', run))
    for axes in ['X', 'Y', 'XY']:
        def run(camera, folder, axes=axes):
            native(camera, bars=False)
            context = Context(Adapter(camera.get_device(), args.ip), args)
            context.case_output, context.evidence = folder, {}
            with ImageSession(context) as session:
                session.setup('RGB8', 0)
                session.set('ExposureTime', min(1000, session.get('ExposureMaximumLines')))
                session.set('GainFactor', 256)
                baseline, _ = session.capture('baseline')
                repeat, _ = session.capture('baseline-repeat')
                assert_images(context, 'noise', baseline, baseline, repeat)
                for axis in axes:
                    session.set('Reverse'+axis, 1)
                actual, _ = session.capture('flipped')
                expected = np.flip(baseline, axis=tuple(1 if axis=='X' else 0 for axis in axes))
                if float(np.abs(expected-baseline).mean()) < 5:
                    raise Skip('Scene is too symmetric to observe this orientation')
                assert_images(context, 'orientation', baseline, expected, actual)
                for axis in axes:
                    session.set('Reverse'+axis, 0)
                restored, _ = session.capture('restored')
                assert_images(context, 'restored', baseline, baseline, restored)
            return context.evidence
        cases.append(('orientation.bright.'+axes, 'Predict real-scene flip with a usable fixed exposure/gain and restore pixels', run))
    return cases


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ip', default='auto', help='Camera IPv4 address, or auto for GigE discovery')
    parser.add_argument('--host', default='10.0.0.7')
    parser.add_argument('--web-port', type=int, default=8787)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--select', action='append', default=[])
    parser.add_argument('--plan', action='store_true')
    parser.add_argument('--soak-seconds', type=int, default=3600)
    parser.add_argument('--stress-cycles', type=int, default=100)
    parser.add_argument('--image-packet-delay', type=int, default=20,
                        help='Packet delay in microseconds for short functional image captures')
    args = parser.parse_args()
    args.bind = args.host
    args.allow_reorder = True
    args.buffer_packets = False
    args.receiver = (args.output.resolve()/'source/qa-cycle-receiver')
    args.c_receiver = args.receiver
    args.acquisition_backend = 'Independent C process for image and JPEG capture'
    stop_requested = False
    cleaning_up = False
    def stop(_signum, _frame):
        nonlocal stop_requested
        stop_requested = True
        if cleaning_up:
            return
        raise KeyboardInterrupt('QA stop requested; restoring camera')
    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    assert 4 <= args.soak_seconds <= 3600 and 1 <= args.stress_cycles <= 1000
    assert 0 <= args.image_packet_delay <= 1000000
    # Parameters for the existing independent functional image tests.
    for key, value in dict(timeout=25, frame_timeout=20, frames=3, settle_frames=3,
                           packet_delay=args.image_packet_delay, bar_tolerance=50, black_max=32,
                           image_mae=8, pixel_error=24, bad_fraction=.02,
                           response_source='scene').items():
        setattr(args, key, value)
    from fnmatch import fnmatchcase
    cases = [row for row in inventory(args) if not args.select or any(fnmatchcase(row[0], pattern) for pattern in args.select)]
    assert cases, 'Empty selection'
    args.output.mkdir(parents=True, exist_ok=False)
    source_directory = args.output/'source'
    source_directory.mkdir()
    source_hashes = {}
    for source in [Path(__file__), ROOT/'tools/qa_image_checks.py', ROOT/'tools/qa_cycle_receiver.c',
                   ROOT/'tools/stream_led_camera_common.py', ROOT/'tools/gige_profile_backup.py',
                   ROOT/'tools/gige_udp_capture.py', ROOT/'tools/configure_fpga_scaler.py']:
        shutil.copy2(source, source_directory/source.name)
        source_hashes[str(source.relative_to(ROOT))] = hashlib.sha256(source.read_bytes()).hexdigest()
    for name in ['transport.py','acquisition.py','packet_queue.py','functional.py','relative_cases.py','imaging.py','relations.py']:
        source = ROOT/'camera_validation/camera_validation'/name
        shutil.copy2(source, source_directory/name)
        source_hashes[str(source.relative_to(ROOT))] = hashlib.sha256(source.read_bytes()).hexdigest()
    document = dict(created=datetime.now(timezone.utc).isoformat(), status='PLAN' if args.plan else 'RUNNING',
                    source_sha256=source_hashes,
                    parameters={k: str(v) if isinstance(v, Path) else v for k, v in vars(args).items()},
                    limitations=['No certification or optical accuracy claim',
                                 'No Flash/SD/UserSet writes or electrical power-cycle',
                                 'Static scenes cannot establish frozen-frame detection',
                                 'Expected-pixel assertions and transport-only feature regressions are reported separately'],
                    results=[dict(id=name, purpose=purpose, status='NOT_RUN') for name, purpose, _ in cases])
    save(args.output/'results.json', document)
    if args.plan:
        return 0
    if args.ip == 'auto':
        args.ip = discover('255.255.255.255', args.host)['ip']
        document['parameters']['resolved_ip'] = args.ip
    server = start_web(args)
    subprocess.run(['cc', '-O2', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    str(ROOT/'tools/qa_cycle_receiver.c'), '-o', str(args.receiver)], check=True)
    document['receiver_sha256'] = hashlib.sha256(args.receiver.read_bytes()).hexdigest()
    save(args.output/'results.json',document)
    common.Control = lambda _ip, _host: Control(args.ip, args.host)
    baseline = None
    for row, (_, _, action) in zip(document['results'], cases):
        camera = device = None
        folder = args.output / row['id']
        folder.mkdir()
        started = time.monotonic()
        host_before = host_network_counters()
        try:
            opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
            with opener.open(args.web_url+'/api/camera', timeout=5) as response:
                row['discovery'] = json.load(response)
            document['web_requirement'] = 'Local QA HTTP interface discovers camera using GigE Vision before every case'
            camera = Aravis.Camera.new(args.ip)
            device = camera.get_device()
            idle(device)
            if baseline is None:
                baseline = full_snapshot(camera)
                save(args.output/'runtime-before.json', baseline)
                document['firmware'] = device.get_string_feature_value('DeviceVersion')
                document['fpga_version'] = read(device, 'FPGA_VERSION')
            row['before'] = counters(device)
            row['status'] = 'RUNNING'
            save(args.output/'results.json', document)
            row['evidence'] = action(camera, folder)
            row['status'] = 'PASS'
        except Skip as exc:
            row.update(status='SKIP', error=str(exc))
        except BaseException as exc:
            row.update(status='FAIL', error=f'{type(exc).__name__}: {exc}', traceback=traceback.format_exc())
        finally:
            cleaning_up = True
            if camera is not None and baseline is not None:
                try:
                    try:
                        device.set_integer_feature_value('MaxAIEnabledFeatures', 0)
                        actual = full_restore(camera, baseline)
                    except Exception as first_error:
                        # A real firmware recovery revokes the failed lease. A new
                        # lease may restore state only while the camera is unowned.
                        row['first_restoration_error'] = str(first_error)
                        device = camera = None
                        gc.collect()
                        wire = Control(args.ip, args.host)
                        try:
                            assert wire.read(0xa00) == 0, 'Another controller owns the camera; cannot reconnect for cleanup'
                        finally:
                            wire.sock.close()
                        camera = Aravis.Camera.new(args.ip)
                        device = camera.get_device()
                        idle(device)
                        actual = full_restore(camera, baseline)
                        row['cleanup_reconnected'] = True
                        row['status'] = 'FAIL'
                        row.setdefault('error', 'Original control lease was lost during cleanup')
                    save(folder/'runtime-restored.json', actual)
                    row['restored'] = True
                    row['after'] = counters(device)
                    for key in ['CSI_ERRORS', 'RECOVERIES']:
                        if row['after'][key] != row['before'][key]:
                            row['status'] = 'FAIL'
                            row.setdefault('error', key+' increased')
                except BaseException as exc:
                    row.update(status='ERROR', restoration_error=str(exc), restoration_traceback=traceback.format_exc())
            device = camera = None
            gc.collect()
            time.sleep(.1)
            try:
                wire = Control(args.ip, args.host)
                try:
                    row['owner_after_release'] = wire.read(0xa00)
                    assert row['owner_after_release'] == 0, 'Owner not released'
                    row['transport_after_release'] = {hex(address): wire.read(address)
                                                      for address in [0xd00, 0xd18, 0x1001c]}
                    assert not any(row['transport_after_release'].values()), 'Released stream route/lock not cleared'
                finally:
                    wire.sock.close()
            except Exception as exc:
                row.update(status='ERROR', release_error=str(exc))
            row['duration_seconds'] = time.monotonic()-started
            host_after = host_network_counters()
            save(folder/'host-network.json', dict(before=host_before, after=host_after,
                 delta={key: value-host_before[key] for key, value in host_after.items() if key in host_before},
                 scope='Entire isolated case including cleanup; cumulative host counters can include unrelated traffic'))
            save(folder/'result.json', row)
            save(args.output/'results.json', document)
            print(json.dumps(dict(id=row['id'], status=row['status'], error=row.get('error'),
                                  restoration_error=row.get('restoration_error'))), flush=True)
            cleaning_up = False
        if stop_requested or row['status'] == 'ERROR' or not row.get('restored'):
            break
    document['counts'] = dict(Counter(row['status'] for row in document['results']))
    document['status'] = 'PASS' if all(row['status']=='PASS' for row in document['results']) else 'INCOMPLETE_OR_FAILED'
    save(args.output/'results.json', document)
    report(args.output, document)
    from camera_validation.report import write_manifest
    write_manifest(args.output)
    Aravis.shutdown()
    server.shutdown()
    return 0 if document['status']=='PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
