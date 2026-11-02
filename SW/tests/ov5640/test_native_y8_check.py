"""Exercise acceptance checks with synthetic evidence, never board measurements."""
import importlib.util
from pathlib import Path
import struct
import tempfile

spec = importlib.util.spec_from_file_location(
    'native_y8', Path(__file__).resolve().parents[2] / 'tools/check_native_y8.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

with tempfile.TemporaryDirectory() as directory:
    result_path, frame_path = Path(directory) / 'result.bin', Path(directory) / 'frame.bin'
    frame = bytes(v for v in [235, 210, 170, 145, 106, 81, 41, 16] for _ in range(80)) * 480
    checksum = 2166136261
    for value in frame:
        checksum = ((checksum ^ value) * 16777619) & 0xffffffff
    values = [4, len(frame), 299, 299, 0, 10000000, 1, len(frame), checksum,
              0x2a, 0x10, 0, 0, 0, 3, (480 << 16) | 640]
    frame_path.write_bytes(frame)
    result_path.write_bytes(struct.pack('<16I', *values))
    assert module.check(result_path, frame_path)['passed']
    # Reject wrong CSI type, incomplete DMA, CRC errors, torn frames and a
    # Bayer formatter incorrectly relabelled Mono8.
    for index, bad in [(9, 0x1e), (3, 0), (3, 1), (2, 100), (4, 1), (6, 0), (10, 0), (15, 1)]:
        broken = values.copy()
        broken[index] = bad
        result_path.write_bytes(struct.pack('<16I', *broken))
        assert not module.check(result_path, frame_path)['passed'], index
    result_path.write_bytes(struct.pack('<16I', *values))
    frame_path.write_bytes(frame[:-1] + b'\x00')
    assert not module.check(result_path, frame_path)['passed']
    frame_path.write_bytes(frame[:-1])
    assert not module.check(result_path, frame_path)['passed']
    patterns = {'R': [1, 1, 0, 0, 1, 1, 0, 0], 'G': [1, 1, 1, 1, 0, 0, 0, 0],
                'B': [1, 0, 1, 0, 1, 0, 1, 0]}
    for order in ('RGGB', 'BGGR', 'GRBG', 'GBRG'):
        mosaic = bytes(240 if patterns[order[(y % 2) * 2 + x % 2]][x // 80] else 8
                       for y in range(480) for x in range(640))
        checksum = 2166136261
        for value in mosaic:
            checksum = ((checksum ^ value) * 16777619) & 0xffffffff
        raw_values = values.copy()
        raw_values[7:9] = [len(mosaic), checksum]
        raw_values[10:12] = [0, 3]
        result_path.write_bytes(struct.pack('<16I', *raw_values))
        frame_path.write_bytes(mosaic)
        report = module.check(result_path, frame_path, 'raw8')
        assert report['passed'] and report['bayer_order'] == order, report
        assert not module.check(result_path, frame_path)['passed']
    raw_values[2:4] = [89, 89]
    result_path.write_bytes(struct.pack('<16I', *raw_values))
    assert module.check(result_path, frame_path, 'raw8', 8.933)['passed']
    assert not module.check(result_path, frame_path, 'raw8')['passed']
    raw_values[3] = 23
    result_path.write_bytes(struct.pack('<16I', *raw_values))
    assert not module.check(result_path, frame_path, 'raw8', 8.933)['passed']
    # Natural-scene mode keeps the same transport checks, but must explicitly
    # report that Bayer/color-bar contents were not qualified by this run.
    raw_values[2:4] = [89, 89]
    result_path.write_bytes(struct.pack('<16I', *raw_values))
    report = module.check(result_path, frame_path, 'raw8', 8.933, 'off')
    assert report['passed'] and report['validation_scope']=='transport-only'
    assert 'bayer_order' not in report
    frame_path.write_bytes(mosaic[:-1]+bytes([mosaic[-1]^1]))
    assert not module.check(result_path, frame_path, 'raw8', 8.933, 'off')['passed']
    # Three 10-bit samples per word, with independent row/stride padding.
    def raw10_evidence(pixels):
        packed = bytearray(864 * 480)
        for y in range(480):
            row = list(pixels[y*640:(y+1)*640]) + [0, 0]
            for x in range(0,640,3):
                word = row[x] | (row[x+1]<<10) | (row[x+2]<<20)
                struct.pack_into('<I', packed, y*864+(x//3)*4, word)
        checksum=2166136261
        for value in packed:
            checksum=((checksum^value)*16777619)&0xffffffff
        evidence=[4,len(packed),71,71,0,10000000,1,sum(v!=0xa5 for v in packed),
                  checksum,0x2b,0,3,0,0,2,800]
        result_path.write_bytes(struct.pack('<16I',*evidence))
        frame_path.write_bytes(packed)
        return evidence
    pixels=[1000 if patterns['BGGR'[(y%2)*2+x%2]][x//80] else 16
            for y in range(480) for x in range(640)]
    evidence=raw10_evidence(pixels)
    report=module.check(result_path,frame_path,'raw10',7.146)
    assert report['passed'] and report['bayer_order']=='BGGR',report
    csi=bytearray()
    for offset in range(0,len(pixels),4):
        group=pixels[offset:offset+4]
        csi.extend(v>>2 for v in group)
        csi.append(sum((v&3)<<(2*n) for n,v in enumerate(group)))
    checksum=2166136261
    for value in csi:
        checksum=((checksum^value)*16777619)&0xffffffff
    csi_evidence=evidence.copy();csi_evidence[1]=len(csi)
    csi_evidence[7:9]=[sum(v!=0xa5 for v in csi),checksum]
    result_path.write_bytes(struct.pack('<16I',*csi_evidence));frame_path.write_bytes(csi)
    assert module.check(result_path,frame_path,'raw10',7.146,layout='csi-bytes')['passed']
    assert not module.check(result_path,frame_path,'raw10',7.146)['passed']
    raw10_evidence(pixels)
    # Duration validation must cover timer rollover in long captures.
    evidence[2:4]=[428,428];evidence[5]=60000500
    result_path.write_bytes(struct.pack('<16I',*evidence))
    assert module.check(result_path,frame_path,'raw10',7.146,expected_seconds=60)['passed']
    assert not module.check(result_path,frame_path,'raw10',7.146)['passed']
    # A truncated/shifted 8-bit image cannot qualify as full RAW10.
    raw10_evidence([v//4 for v in pixels])
    assert not module.check(result_path,frame_path,'raw10',7.146)['passed']
    raw10_evidence([((x+y*3)%1024) for y in range(480) for x in range(640)])
    assert module.check(result_path,frame_path,'raw10',7.146,'off')['passed']
    raw10_evidence([((x+y*3)%256)*4 for y in range(480) for x in range(640)])
    assert not module.check(result_path,frame_path,'raw10',7.146,'off')['passed']
    rgb=bytes(250 if patterns[channel][x//80] else 4
              for y in range(480) for x in range(640) for channel in 'RGB')
    def rgb_evidence(data):
        checksum=2166136261
        for value in data:
            checksum=((checksum^value)*16777619)&0xffffffff
        evidence=[4,len(data),299,299,0,10000000,1,sum(v!=0xa5 for v in data),
                  checksum,0x24,0x23,1,0,0,2,1920]
        result_path.write_bytes(struct.pack('<16I',*evidence))
        frame_path.write_bytes(data)
    rgb_evidence(rgb)
    assert module.check(result_path,frame_path,'rgb888')['passed']
    csc_evidence=list(struct.unpack('<16I',result_path.read_bytes()))
    csc_evidence[10:12]=[0x20,0]
    result_path.write_bytes(struct.pack('<16I',*csc_evidence))
    assert module.check(result_path,frame_path,'rgb888',rgb_source='yuv444-csc')['passed']
    assert not module.check(result_path,frame_path,'rgb888')['passed']
    rgb_evidence(rgb)
    # Correct transport/hash cannot hide swapped R/B or a missing color plane.
    rgb_evidence(bytes(v for i in range(0,len(rgb),3) for v in (rgb[i+2],rgb[i+1],rgb[i])))
    assert not module.check(result_path,frame_path,'rgb888')['passed']
    packed565=bytearray()
    for y in range(480):
        for x in range(640):
            band=x//80
            word=(31*patterns['R'][band]<<11)|(63*patterns['G'][band]<<5)|(31*patterns['B'][band])
            packed565.extend((word>>8,word&255))
    def evidence565(data):
        checksum=2166136261
        for value in data:checksum=((checksum^value)*16777619)&0xffffffff
        evidence=[4,len(data),299,299,0,10000000,1,sum(v!=0xa5 for v in data),
                  checksum,0x22,0x61,1,0,0,2,1280]
        result_path.write_bytes(struct.pack('<16I',*evidence));frame_path.write_bytes(data)
    evidence565(packed565)
    assert module.check(result_path,frame_path,'rgb565',layout='csi-bytes')['passed']
    assert not module.check(result_path,frame_path,'rgb565')['passed']
    evidence565(bytes(v for n in range(0,len(packed565),2) for v in (packed565[n+1],packed565[n])))
    assert not module.check(result_path,frame_path,'rgb565',layout='csi-bytes')['passed']
    for name, formatter, datatype, maximum, shifts in (
            ('rgb555', 0x81, 0x2a, 31, (10,5,0)),
            ('rgb444', 0xa1, 0x2a, 15, (8,4,0))):
        packed=bytearray()
        for y in range(480):
            for x in range(640):
                word=sum(maximum*patterns[channel][x//80]<<shift
                         for channel,shift in zip('RGB',shifts))
                packed.extend((word>>8,word&255))
        evidence565(packed)
        evidence=list(struct.unpack('<16I',result_path.read_bytes()))
        evidence[9:11]=[datatype,formatter]
        result_path.write_bytes(struct.pack('<16I',*evidence))
        assert module.check(result_path,frame_path,name,layout='csi-bytes')['passed']
        evidence[15]=1280-1
        result_path.write_bytes(struct.pack('<16I',*evidence))
        assert not module.check(result_path,frame_path,name,layout='csi-bytes')['passed']
    yuv=bytearray()
    for y in range(480):
        for x in range(640):
            r,g,b=(255*patterns[channel][x//80] for channel in 'RGB')
            yuv.extend(((77*r+150*g+29*b+128)>>8,
                        min(255,128+((-43*r-85*g+128*b+128)>>8)),
                        min(255,128+((128*r-107*g-21*b+128)>>8))))
    rgb_evidence(yuv)
    evidence=list(struct.unpack('<16I',result_path.read_bytes()))
    evidence[10:12]=[0x20,0]
    result_path.write_bytes(struct.pack('<16I',*evidence))
    assert module.check(result_path,frame_path,'yuv444',layout='csi-bytes')['passed']
    yuv420=bytearray()
    for y in range(480):
        for x in range(0,640,2):
            n=(y*640+x)*3
            yuv420.extend((yuv[n+1+(y&1)],yuv[n],yuv[n+3]))
    for n in range(0,len(yuv420),4):
        yuv420[n:n+4]=yuv420[n:n+4][::-1]
    rgb_evidence(yuv420)
    evidence=list(struct.unpack('<16I',result_path.read_bytes()))
    evidence[9:12]=[0x1a,0x5f,0];evidence[15]=960
    result_path.write_bytes(struct.pack('<16I',*evidence))
    assert module.check(result_path,frame_path,'yuv420',layout='csi-bytes')['passed']
    # RGB bytes must not accidentally pass as YUV444 because they have the
    # same payload size and packet type.
    rgb_evidence(rgb)
    evidence=list(struct.unpack('<16I',result_path.read_bytes()))
    evidence[10:12]=[0x20,0]
    result_path.write_bytes(struct.pack('<16I',*evidence))
    assert not module.check(result_path,frame_path,'yuv444',layout='csi-bytes')['passed']
    for order,name in enumerate(('yuyv','yvyu','uyvy','vyuy')):
        stored=bytearray()
        for n in range(0,len(yuv),6):
            y0,u,v,y1=yuv[n],yuv[n+1],yuv[n+2],yuv[n+3]
            wire=((y0,u,y1,v),(y0,v,y1,u),(u,y0,v,y1),(v,y0,u,y1))[order]
            stored.extend(wire[::-1])
        rgb_evidence(stored)
        evidence=list(struct.unpack('<16I',result_path.read_bytes()))
        evidence[9:12]=[0x1e,0x30+order,0];evidence[15]=1280
        result_path.write_bytes(struct.pack('<16I',*evidence))
        assert module.check(result_path,frame_path,name,layout='csi-bytes')['passed']
    # Full sensor RAW10:3240 active bytes,3248 DMA stride. The padding must
    # never become pixels or shift the Bayer phase on subsequent rows.
    full_rows=[]
    for y in range(2):
        row=bytearray()
        pixels=[1023 if patterns[['B','G','G','R'][(y&1)*2+(x&1)]][x//324] else 16
                for x in range(2592)]
        for x in range(0,2592,4):
            group=pixels[x:x+4]
            row.extend(v>>2 for v in group)
            row.append(sum((v&3)<<(2*n) for n,v in enumerate(group)))
        row.extend(b'\xa5'*8)
        full_rows.append(row)
    full_frame=(full_rows[0]+full_rows[1])*972
    checksum=2166136261
    for value in full_frame:checksum=((checksum^value)*16777619)&0xffffffff
    full_evidence=[4,len(full_frame),71,71,0,10000000,1,
                   sum(v!=0xa5 for v in full_frame),checksum,0x2b,0,3,0,0,2,3240]
    result_path.write_bytes(struct.pack('<16I',*full_evidence));frame_path.write_bytes(full_frame)
    full=module.check(result_path,frame_path,'raw10',7.146,layout='csi-bytes',width=2592,height=1944)
    assert full['passed'],full['failures']
    assert full['bayer_order']=='BGGR' and full['memory_stride']==3248
    assert not module.check(result_path,frame_path,'raw10',7.146,layout='csi-bytes')['passed']
    # JPEG syntax/decoding is independent of CSI transport validation. A
    # marker-like byte pair inside COM is not the end of the entropy stream.
    import io
    from PIL import Image
    output=io.BytesIO()
    Image.frombytes('RGB',(640,480),rgb).save(output,format='JPEG',quality=80)
    jpeg=output.getvalue()
    jpeg=jpeg[:2]+b'\xff\xfe\x00\x07\xff\xd9foo'+jpeg[2:]
    assert module.jpeg_end(jpeg)==len(jpeg)
    def jpeg_evidence(payload,padding=0):
        canvas=payload+bytes([padding])*(614400-len(payload))
        canvas=b''.join(canvas[n:n+4][::-1] for n in range(0,len(canvas),4))
        rgb_evidence(canvas)
        evidence=list(struct.unpack('<16I',result_path.read_bytes()))
        evidence[9:12]=[0x1e,0x30,0];evidence[15]=1280
        result_path.write_bytes(struct.pack('<16I',*evidence))
    jpeg_evidence(jpeg)
    assert module.check(result_path,frame_path,'jpeg',layout='csi-bytes')['passed']
    jpeg_evidence(jpeg[:-2])
    assert not module.check(result_path,frame_path,'jpeg',layout='csi-bytes')['passed']
    jpeg_evidence(jpeg,1)
    assert not module.check(result_path,frame_path,'jpeg',layout='csi-bytes')['passed']
print('PASS: Y8 evidence checker rejects CSI, DMA, formatter and frame corruption')
print('PASS: RAW8 evidence checker recognizes all four Bayer phases and rejects Y8 labelling')

print('PASS: RAW10 XY10 unpacking, stride, 10-bit precision and long captures')

print('PASS: RGB888 direct color bars and channel ordering')

print('PASS: RGB565 payload order and exact component unpacking')
print('PASS: RGB555/RGB444 component unpacking and CSI line length')
print('PASS: YUV444 and alternating-chroma YUV420 bars')
print('PASS: full-sensor RAW10 stride padding and Bayer phase')
print('PASS: JPEG markers, real image decode, color bars, padding and truncation')
print('PASS: four native YUV422 byte orders and CSI memory normalization')
