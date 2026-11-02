#!/usr/bin/env python3
"""Check native-format diagnostic evidence and VGA sensor color bars.

Requires a stopped coherent frame (dump_video_frame.tcl) and g_video_result
(read_video_result.tcl), both from the same native-y8 firmware execution.
"""
import argparse
import json
import struct
from pathlib import Path


def jpeg_end(data):
    """Locate EOI by JPEG syntax, ignoring marker-like bytes inside segments."""
    if not data.startswith(b'\xff\xd8'):
        raise ValueError('JPEG SOI is missing at frame start')
    pos=2;entropy=False
    while pos<len(data):
        if entropy:
            pos=data.find(b'\xff',pos)
            if pos<0:break
            if pos+1>=len(data):break
            code=data[pos+1]
            if code==0 or 0xd0<=code<=0xd7:
                pos+=2;continue
            if code==0xff:
                pos+=1;continue
            entropy=False
        if data[pos]!=0xff:raise ValueError('Invalid JPEG marker boundary')
        while pos<len(data) and data[pos]==0xff:pos+=1
        if pos>=len(data):break
        code=data[pos];pos+=1
        if code==0xd9:return pos
        if code in (0,0xd8):raise ValueError('Invalid nested JPEG marker')
        if code==1 or 0xd0<=code<=0xd7:continue
        if pos+2>len(data):break
        size=(data[pos]<<8)|data[pos+1]
        if size<2 or pos+size>len(data):raise ValueError('Invalid JPEG segment length')
        pos+=size
        if code==0xda:entropy=True
    raise ValueError('Complete JPEG EOI is missing')


def check(result_path, frame_path, sensor_format='y8', expected_fps=30, pattern='bars', expected_seconds=10, layout='native', rgb_order=3, width=640, height=480, rgb_source='native'):
    if sensor_format not in ('y8', 'raw8', 'raw10', 'rgb888', 'rgb565', 'rgb555', 'rgb444', 'yuv444', 'yuv420','jpeg','yuyv','yvyu','uyvy','vyuy'):
        raise ValueError('Unknown native byte format')
    if rgb_order not in (0,1,2,3,4,5,15):
        raise ValueError('Invalid RGB888 sequence')
    if rgb_source not in ('native','yuv444-csc'):
        raise ValueError('Unknown RGB source')
    if rgb_source!='native' and (sensor_format!='rgb888' or layout!='native' or rgb_order!=3):
        raise ValueError('CSC profile requires RGB888 memory layout and default RGB order')
    if layout not in ('native', 'csi-bytes'):
        raise ValueError('Unknown memory layout')
    if pattern not in ('bars', 'off'):
        raise ValueError('Unknown pattern mode')
    if not 1 <= expected_seconds <= 1800:
        raise ValueError('Expected duration must be in [1,1800]')
    if (width,height) not in ((640,480),(2592,1944)):
        raise ValueError('Unqualified diagnostic dimensions')
    if (width,height)!=(640,480) and sensor_format not in ('raw8','raw10'):
        raise ValueError('Full sensor checker currently supports RAW only')
    names = ('stage bytes frames dma_done errors elapsed_us coherent changed hash '
             'receiver_type formatter mux result sensor_error_register sensor_state '
             'receiver_line_info').split()
    result = Path(result_path).read_bytes()
    if len(result) != 4 * len(names):
        raise ValueError('Wrong diagnostic result size')
    stats = dict(zip(names, struct.unpack('<16I', result)))
    if stats['result'] & 0x80000000:
        stats['result'] -= 1 << 32
    frame = Path(frame_path).read_bytes()
    traits = {
        'y8': (640, 0x10, 0, 0x2a),
        'raw8': (width, 0, 3, 0x2a),
        'raw10': (((width*5//4 if layout == 'csi-bytes' else ((width+2)//3)*4)+15)&~15, 0, 3, 0x2b),
        'rgb888': (1920, 0x20|rgb_order, 1, 0x24),
        'rgb565': (1280, 0x61, 1, 0x22),
        'rgb555': (1280, 0x81, 1, 0x2a),
        'rgb444': (1280, 0xa1, 1, 0x2a),
        'yuv444': (1920, 0x20, 0, 0x24),
        'yuv420': (960, 0x5f, 0, 0x1a),
        'jpeg': (1280,0x30,0,0x1e),
        **{name:(1280,0x30+n,0,0x1e) for n,name in enumerate(('yuyv','yvyu','uyvy','vyuy'))},
    }
    stride, formatter, mux, datatype = traits[sensor_format]
    if rgb_source=='yuv444-csc':formatter,mux=0x20,0
    line_bytes=width*5//4 if sensor_format=='raw10' else stride
    stats.update(width=width,height=height,memory_stride=stride,active_line_bytes=line_bytes)
    failures = []
    if sensor_format in ('rgb565','rgb555','rgb444','yuv444','yuv420','jpeg','yuyv','yvyu','uyvy','vyuy') and layout != 'csi-bytes':
        failures.append('Format requires the no-VFB CSI memory-byte layout')
    for valid, reason in [
        (stats['stage'] == 4 and stats['result'] == 0, 'firmware capture failed'),
        (stats['bytes'] == len(frame) == stride * height, 'wrong native payload size'),
        (stats['formatter'] == formatter and stats['mux'] & 7 == mux, 'wrong native sensor formatter/mux'),
        (stats['receiver_type'] & 0x3f == datatype, 'wrong CSI data type'),
        (stats['receiver_line_info'] & 0xffff == line_bytes, 'wrong CSI line byte count'),
        (stats['frames'] > 0 and stats['dma_done'] > 0, 'no received frames'),
        (stats['coherent'] == 1 and stats['errors'] == 0, 'torn frame or CSI errors'),
        (expected_seconds * 1e6 <= stats['elapsed_us'] <= expected_seconds * 1e6 + 2_000_000, 'invalid capture interval'),
    ]:
        if not valid:
            failures.append(reason)
    stats['dma_fps'] = round(stats['dma_done'] * 1e6 / stats['elapsed_us'], 3) if stats['elapsed_us'] else 0
    if not 0 < expected_fps <= 120:
        raise ValueError('Expected frame rate must be in (0,120]')
    stats['expected_fps'] = expected_fps
    # A bounded count window can exclude a frame at either edge. Keep an
    # absolute 0.2 fps allowance for slow modes and a 7% tolerance overall.
    if abs(stats['dma_fps'] - expected_fps) > max(.2, expected_fps * .07):
        failures.append('capture rate differs from configured nominal frame rate')
    if abs(stats['frames'] - stats['dma_done']) > 2:
        failures.append('CSI frame count and completed DMA count disagree')
    frame_hash = 2166136261
    for value in frame:
        frame_hash = ((frame_hash ^ value) * 16777619) & 0xffffffff
    if frame_hash != stats['hash']:
        failures.append('frame does not match firmware hash')
    if sum(v != 0xa5 for v in frame) != stats['changed']:
        failures.append('frame does not match changed-byte count')
    if sensor_format=='jpeg':
        frame=b''.join(frame[n:n+4][::-1] for n in range(0,len(frame),4))
        stats['payload_order']='axis32-byte-lanes-3,2,1,0'
        try:
            import io
            from PIL import Image
            end=jpeg_end(frame)
            stats['jpeg_bytes']=end
            stats['jpeg_padding_bytes']=len(frame)-end
            if any(frame[end:]):failures.append('JPEG fixed-frame padding is not zero')
            decoded=Image.open(io.BytesIO(frame[:end]))
            if decoded.size!=(width,height):raise ValueError('JPEG dimensions disagree with sensor mode')
            decoded.load()
            frame=decoded.convert('RGB').tobytes()
        except (ValueError,OSError,ImportError) as error:
            failures.append('JPEG decode failed: '+str(error))
    if sensor_format in ('yuyv','yvyu','uyvy','vyuy') and len(frame)==1280*480:
        stored=b''.join(frame[n:n+4][::-1] for n in range(0,len(frame),4))
        triplets=bytearray()
        for n in range(0,len(stored),4):
            a,b,c,d=stored[n:n+4]
            if sensor_format=='yuyv':y0,u,y1,v=a,b,c,d
            elif sensor_format=='yvyu':y0,v,y1,u=a,b,c,d
            elif sensor_format=='uyvy':u,y0,v,y1=a,b,c,d
            else:v,y0,u,y1=a,b,c,d
            triplets.extend((y0,u,v,y1,u,v))
        frame=triplets
        stats['payload_order']='axis32-byte-lanes-3,2,1,0'
    if sensor_format == 'yuv420' and len(frame) == 960 * 480:
        # Legacy420 uses MS-byte-first CSI32 memory words, like422.
        # Reverse complete words before interpreting sensor UYY/VYY.
        frame=bytearray(frame)
        for n in range(0,len(frame),4):
            frame[n:n+4]=frame[n:n+4][::-1]
        stats['payload_order']='axis32-byte-lanes-3,2,1,0'
        # Adjacent rows carry alternate chroma components.
        triplets=bytearray()
        for y in range(480):
            urow=(y&~1)*960
            vrow=urow+960
            for x in range(640):
                n=(x//2)*3
                triplets.extend((frame[y*960+n+1+(x&1)],frame[urow+n],frame[vrow+n]))
        frame=triplets
    if sensor_format in ('yuv444','yuv420','yuyv','yvyu','uyvy','vyuy') and len(frame) == 1920 * 480:
        # Native ISP YUV is full-range BT.601 (white255, black near0).
        # Convert independent Y/U/V payload triplets for the RGB bar oracle.
        rgb=bytearray()
        clamp=lambda value: max(0,min(255,value))
        for n in range(0,len(frame),3):
            y,u,v=frame[n],frame[n+1]-128,frame[n+2]-128
            rgb.extend((clamp((256*y+359*v+128)>>8),
                        clamp((256*y-88*u-183*v+128)>>8),
                        clamp((256*y+454*u+128)>>8)))
        frame=rgb
    if sensor_format == 'rgb888' and rgb_order != 3 and len(frame) == 1920 * 480:
        if layout != 'csi-bytes':
            failures.append('Nondefault RGB888 sequence requires CSI payload-byte layout')
        else:
            sequence={0:'GBR',1:'GRB',2:'BGR',3:'RGB',4:'BRG',5:'RBG',15:'BGR'}[rgb_order]
            indices=[sequence.index(channel) for channel in 'RGB']
            frame=bytes(frame[n+c] for n in range(0,len(frame),3) for c in indices)
    if sensor_format in ('rgb555','rgb444') and len(frame) == 1280 * 480:
        words=[(frame[n]<<8)|frame[n+1] for n in range(0,len(frame),2)]
        if sensor_format=='rgb555':
            frame=bytes(((v<<3)|(v>>2)) for word in words
                        for v in ((word>>10)&31,(word>>5)&31,word&31))
        else:
            frame=bytes(v*17 for word in words
                        for v in ((word>>8)&15,(word>>4)&15,word&15))
    if sensor_format == 'rgb565' and len(frame) == 1280 * 480:
        words = [(frame[n]<<8)|frame[n+1] for n in range(0,len(frame),2)]
        frame = bytes(component for word in words for component in (
            (((word>>11)&31)<<3)|((word>>13)&7),
            (((word>>5)&63)<<2)|((word>>9)&3),
            ((word&31)<<3)|((word>>2)&7)))
    if sensor_format == 'raw10' and layout == 'csi-bytes' and len(frame) == stride * height:
        # CSI-2 RAW10p: 4 high-byte samples followed by their packed low bits.
        # Strip per-row DMA alignment padding before unpacking CSI groups.
        frame=b''.join(frame[y*stride:y*stride+line_bytes] for y in range(height))
        frame = [((frame[offset+n]<<2) | ((frame[offset+4]>>(2*n))&3))
                 for offset in range(0,len(frame),5) for n in range(4)]
    if sensor_format == 'raw10' and layout == 'native' and len(frame) == stride * height:
        # PG278 XY10 memory: 3 samples per little-endian word, per-line
        # restart, 16-byte aligned stride. This is NOT CSI/PFNC RAW10p.
        pixels = []
        for y in range(height):
            row = []
            for offset in range(0, ((width+2)//3)*4, 4):
                word = struct.unpack_from('<I', frame, y * stride + offset)[0]
                row.extend((word & 1023, (word >> 10) & 1023, (word >> 20) & 1023))
            pixels.extend(row[:width])
        frame = pixels  # Hash/changed-byte checks above refer to original DMA bytes.
    if sensor_format == 'raw10' and len(frame) == width * height:
        stats['sample_min'] = min(frame)
        stats['sample_max'] = max(frame)
        stats['unique_samples'] = len(set(frame))
        stats['lsb_histogram'] = [sum((v & 3) == n for v in frame) for n in range(4)]
        if pattern == 'off' and (len(set(frame)) <= 256 or not all(stats['lsb_histogram'])):
            failures.append('natural scene does not establish full 10-bit sample precision')
    # Sample interiors of the eight vertical test bars; tolerate limited/full
    # range and boundary interpolation, but reject interleaved zero samples,
    # repeated rows of garbage, reversed order and missing luminance levels.
    if len(frame) == 640 * 480 and sensor_format == 'y8' and pattern == 'bars':
        levels = []
        for band in range(8):
            samples = [frame[y * 640 + x] for y in range(40, 440, 40)
                       for x in range(band * 80 + 24, band * 80 + 56)]
            levels.append(sum(samples) / len(samples))
            if max(samples) - min(samples) > 8:
                failures.append(f'bar {band} has inconsistent pixels')
        stats['bar_means'] = [round(v, 2) for v in levels]
        if not all(a - b >= 8 for a, b in zip(levels, levels[1:])):
            failures.append('expected eight descending luminance bars')
    elif sensor_format in ('raw8', 'raw10') and len(frame) == width * height and pattern == 'bars':
        # Color bars in a Bayer mosaic must retain the independent R/G/B
        # responses. A grayscale conversion must not pass a RAW8 test.
        scale = 4 if sensor_format == 'raw10' else 1
        patterns = {'R': [1, 1, 0, 0, 1, 1, 0, 0],
                    'G': [1, 1, 1, 1, 0, 0, 0, 0],
                    'B': [1, 0, 1, 0, 1, 0, 1, 0]}
        channels, phase_levels = [], []
        for dy, dx in [(0, 0), (0, 1), (1, 0), (1, 1)]:
            levels = []
            for band in range(8):
                bar_width=width//8
                left=band*bar_width+(bar_width*3//10//2)*2+dx
                right=band*bar_width+bar_width*7//10
                samples = [frame[y * width + x] for y in range(40 + dy, height-40, 40)
                           for x in range(left,right,2)]
                levels.append(sum(samples) / len(samples))
                if max(samples) - min(samples) > 8 * scale:
                    failures.append(f'RAW8 phase {dy},{dx} bar {band} is inconsistent')
            phase_levels.append([round(v, 2) for v in levels])
            threshold = (levels[0] + levels[-1]) / 2
            bits = [int(v > threshold) for v in levels]
            matches = [name for name, pattern in patterns.items() if bits == pattern]
            channels.append(matches[0] if len(matches) == 1 and levels[0] - levels[-1] >= 64 * scale else '?')
        if sensor_format == 'raw10' and max(v for levels in phase_levels for v in levels) < 900:
            failures.append('RAW10 bars do not reach the expected 10-bit range')
        stats['bayer_phase_means'] = phase_levels
        stats['bayer_order'] = ''.join(channels)
        if stats['bayer_order'] not in ('RGGB', 'BGGR', 'GRBG', 'GBRG'):
            failures.append('color bars do not form a valid Bayer mosaic')
    if sensor_format in ('rgb888', 'rgb565', 'rgb555', 'rgb444', 'yuv444', 'yuv420','jpeg','yuyv','yvyu','uyvy','vyuy') and len(frame) == 1920 * 480 and pattern == 'bars':
        patterns = {'R': [1,1,0,0,1,1,0,0], 'G': [1,1,1,1,0,0,0,0],
                    'B': [1,0,1,0,1,0,1,0]}
        means = {}
        for channel, name in enumerate('RGB'):
            levels = []
            for band in range(8):
                samples = [frame[(y*640+x)*3+channel] for y in range(40,440,40)
                           for x in range(band*80+24,band*80+56)]
                levels.append(sum(samples)/len(samples))
                if max(samples)-min(samples)>8:
                    failures.append(f'{sensor_format} {name} band {band} inconsistent')
            means[name] = levels
            threshold=(levels[0]+levels[-1])/2
            if [int(v>threshold) for v in levels] != patterns[name] or levels[0]-levels[-1]<128:
                failures.append(f'{sensor_format} {name} color bars/order incorrect')
        stats['rgb_bar_means'] = means
    if sensor_format == 'rgb888':
        stats['rgb_order'] = rgb_order
        stats['rgb_source'] = rgb_source
    stats['memory_layout'] = layout
    stats['sensor_format'] = 'yuv444' if rgb_source=='yuv444-csc' else sensor_format
    stats['output_format'] = sensor_format
    stats['pattern'] = pattern
    # Pattern readback is recorded in UART (503D), not in the fixed result ABI.
    # For a natural scene only transport integrity is established here.
    stats['validation_scope'] = 'transport-and-color-bars' if pattern == 'bars' else 'transport-only'
    stats['failures'] = failures
    stats['passed'] = not failures
    return stats


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('result')
    parser.add_argument('frame')
    parser.add_argument('--format', choices=['y8', 'raw8', 'raw10', 'rgb888', 'rgb565', 'rgb555', 'rgb444', 'yuv444', 'yuv420','jpeg','yuyv','yvyu','uyvy','vyuy'], default='y8')
    parser.add_argument('--expected-fps', type=float, default=30,
                        help='Nominal rate computed from the tested clock/HTS/VTS profile')
    parser.add_argument('--rgb-order', type=int, default=3, choices=[0,1,2,3,4,5,15])
    parser.add_argument('--rgb-source',choices=['native','yuv444-csc'],default='native')
    parser.add_argument('--layout', choices=['native', 'csi-bytes'], default='native')
    parser.add_argument('--expected-seconds', type=int, default=10)
    parser.add_argument('--width', type=int, default=640)
    parser.add_argument('--height', type=int, default=480)
    parser.add_argument('--pattern', choices=['bars', 'off'], default='bars',
                        help='off checks transport only; verify sensor 503D readback in UART')
    args = parser.parse_args()
    report = check(args.result, args.frame, args.format, args.expected_fps, args.pattern, args.expected_seconds, args.layout, args.rgb_order,args.width,args.height,args.rgb_source)
    print(json.dumps(report, indent=2))
    raise SystemExit(0 if report['passed'] else 1)
