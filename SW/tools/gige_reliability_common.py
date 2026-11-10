"""Portable helpers for volatile reliability trials; no persistent writes."""
import json
from pathlib import Path
from configure_fpga_scaler import apply_working
from stream_led_camera_common import read, write, mode


def save(path, value):
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_text(json.dumps(value, indent=2) + '\n')
    temporary.replace(path)


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
