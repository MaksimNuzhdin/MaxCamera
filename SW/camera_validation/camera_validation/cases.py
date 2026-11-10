"""Small independently reported cases; explicit evidence levels prevent inflated coverage."""
from dataclasses import dataclass
from contextlib import contextmanager
from pathlib import Path
import json
import socket
import time
from .transport import ProtocolError, receive_frame

FEATURES = json.loads(Path(__file__).with_name('features.json').read_text())
FORMATS = json.loads(Path(__file__).with_name('formats.json').read_text())
BY_NAME = {f['name']: f for f in FEATURES}


class Skip(RuntimeError):
    pass


def require(condition, message):
    if not condition:
        raise AssertionError(message)


@dataclass
class Case:
    id: str
    feature: str
    level: str
    purpose: str
    action: object
    covers: tuple = ()


class Context:
    def __init__(self, control, args):
        self.c, self.args = control, args

    def read(self, name):
        return self.c.read(BY_NAME[name]['address'])

    def idle(self):
        end = time.monotonic() + self.args.timeout
        while self.read('SensorBusy'):
            require(time.monotonic() < end, 'Sensor did not become idle')
            time.sleep(.02)

    @contextmanager
    def settings(self, addresses):
        saved = [(a, self.c.read(a)) for a in dict.fromkeys(addresses)]
        try:
            yield
        finally:
            # Cleanup failures propagate and stop the entire run.
            try:
                self.c.write(0x10014, 0)
                self.idle()
                if any(a == 0x10008 for a, _ in saved):
                    self.c.write(0x10000,640)
                    self.c.write(0x10004,480)
                for address, value in reversed(saved):
                    self.c.write(address, value)
                    require(self.c.read(address) == value, f'Restore failed at {address:#x}')
            except Exception as exc:
                raise CleanupError(str(exc)) from exc


class CleanupError(RuntimeError):
    pass


def range_case(ctx, f):
    available = f.get('available')
    if available in BY_NAME and not ctx.read(available):
        raise Skip(f'{available}=false')
    if f['access'] == 'WO' or f.get('kind') == 'Command':
        raise Skip('Command requires a dedicated behavioral procedure')
    raw = ctx.c.read(f['address'])
    value = raw - 2**32 if f.get('signed') and raw >= 2**31 else raw
    require(f['minimum'] <= value <= f['maximum'], f'Out of range: {value}')
    if 'entries' in f:
        require(value in f['entries'].values(), f'Unknown enum value {value}')
    return {'raw': raw, 'interpreted': value}


def roundtrip(ctx, f, value):
    if f.get('available') in BY_NAME and not ctx.read(f['available']):
        raise Skip(f"{f['available']}=false")
    deps = {'ExposureTime': 'ExposureAuto', 'GainFactor': 'GainAuto',
            'WhiteBalanceRed': 'BalanceWhiteAuto', 'WhiteBalanceGreen': 'BalanceWhiteAuto',
            'WhiteBalanceBlue': 'BalanceWhiteAuto'}
    dep = deps.get(f['name'])
    addresses = ([BY_NAME[dep]['address']] if dep else []) + [f['address']]
    with ctx.settings(addresses):
        if dep:
            ctx.c.write(BY_NAME[dep]['address'], 0)
        ctx.c.write(f['address'], value)
        actual = ctx.c.read(f['address'])
        require(actual == value, f'Write/read mismatch: {value} != {actual}')
    return {'written': value, 'readback': actual, 'restored': True}


def discovery(ctx):
    data = ctx.c.command(2, flags=0x11)
    require(len(data) == 248, 'Invalid discovery payload length')
    require(socket.inet_ntoa(data[36:40]) == ctx.args.ip, 'Discovery IP mismatch')
    identity = {name: data[offset:offset+size].split(b'\0')[0].decode('ascii')
                for name, offset, size in [('vendor',72,32),('model',104,32),('serial',216,16)]}
    require(all(identity.values()), 'Empty discovery identity')
    return identity


def xml_case(ctx):
    import xml.etree.ElementTree as ET
    url = ctx.c.memory(0x200,512).split(b'\0')[0].decode('ascii')
    require(url.startswith('Local:'), 'Expected camera-local XML')
    _, address, length = url[6:].split(';')
    address, length = int(address,16), int(length,16)
    require(0 < length <= 4*1024*1024, 'Invalid XML size')
    data = b''.join(ctx.c.memory(address+off,min(512,(length-off+3)//4*4))
                    for off in range(0,length,512))[:length]
    root = ET.fromstring(data)
    names = {e.get('Name') for e in root.iter()}
    missing = [f['name'] for f in FEATURES if f['name'] not in names]
    require(not missing, f'Missing declared features: {missing}')
    (ctx.args.output/'camera.xml').write_bytes(data)
    return {'bytes': length, 'features': len(FEATURES)}


def apply_case(ctx, name):
    if name == 'VcmApply' and not ctx.read('SensorVcmAvailable'):
        raise Skip('No verified VCM actuator on this unit')
    if name not in {'EffectApply','GammaApply','LensCorrectionApply','OverlayApply','SensorOtpRefresh'}:
        raise Skip('Maintenance, persistence or physical fixture procedure required')
    ctx.c.write(BY_NAME[name]['address'],1)
    ctx.idle()
    require(ctx.read('SensorLastError') == 0, 'Sensor command failed')
    if name == 'SensorOtpRefresh':
        require(ctx.read('SensorOtpValid') == 1, 'OTP shadow invalid')
        with ctx.settings([BY_NAME['SensorOtpSelector']['address']]):
            values=[]
            for i in range(32):
                ctx.c.write(BY_NAME['SensorOtpSelector']['address'],i)
                values.append(ctx.read('SensorOtpValue'))
        return {'otp_shadow': values, 'programming': False}
    return {'command_completed': name, 'scope': 'Commit existing staged values; no optical assertion'}


def stream_case(ctx, fmt, width=640, height=480, pattern=None, check_lock=False):
    if 'YUV8_UYV' == fmt['name']:
        width,height=640,480
    addresses=[BY_NAME['ImageCompressionMode']['address'],0x10000,0x10004,0x10008,0xd18,0xd00,0xd04,0xd08,0x1001c]
    if pattern is not None:
        addresses += [BY_NAME['TestPattern']['address']]
    with ctx.settings(addresses):
        receiver=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
        try:
            receiver.setsockopt(socket.SOL_SOCKET,socket.SO_RCVBUF,8*1024*1024)
            route=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
            try:
                route.connect((ctx.args.ip,3956));host=ctx.args.bind if ctx.args.bind!='0.0.0.0' else route.getsockname()[0]
            finally:
                route.close()
            receiver.bind((host,0));receiver.settimeout(.1)
            ctx.c.write(BY_NAME['ImageCompressionMode']['address'],0)
            ctx.c.write(0x10000,width);ctx.c.write(0x10004,height)
            ctx.c.write(0x10008,fmt['value'])
            if pattern is not None:
                ctx.c.write(BY_NAME['TestPattern']['address'],pattern)
            ctx.c.write(0xd18,int.from_bytes(socket.inet_aton(host),'big'))
            ctx.c.write(0xd00,receiver.getsockname()[1]);ctx.c.write(0xd04,1500)
            ctx.c.write(0xd08,ctx.args.packet_delay);ctx.c.write(0x1001c,1)
            before={n:ctx.read(n) for n in ['CsiErrorEvents','EthernetErrors','TransmittedFrames']}
            ctx.c.write(0x10014,1)
            frames=[]
            for _ in range(ctx.args.frames):
                frame=receive_frame(ctx.c,receiver,ctx.args.frame_timeout,
                                    allow_reorder=getattr(ctx.args,'allow_reorder',False))
                if check_lock:
                    address=BY_NAME['ReverseX']['address']
                    original=ctx.c.read(address)
                    try:
                        ctx.c.write(address,original ^ 1)
                    except ProtocolError as exc:
                        require(exc.status in {0x8006,0x8007}, 'Unexpected streaming lock status')
                    else:
                        # Stop before restoring a backend that incorrectly accepted the write.
                        ctx.c.write(0x10014,0);ctx.idle();ctx.c.write(address,original)
                        raise AssertionError('Backend accepted sensor write while streaming')
                require((frame['pixel'],frame['width'],frame['height']) == (fmt['value'],width,height), 'Leader metadata mismatch')
                require(len(frame['data']) == ctx.c.read(0x1000c), 'Payload size mismatch')
                require(len(frame['data'])>0,'Empty image')
                frames.append(frame)
            require(all(b['timestamp']>a['timestamp'] for a,b in zip(frames,frames[1:])), 'Timestamp did not increase')
            for name in ['CsiErrorEvents','EthernetErrors']:
                require(ctx.read(name)==before[name], f'{name} increased')
            if pattern == 4 and fmt['name']=='Mono8':
                average=sum(frames[-1]['data'])/len(frames[-1]['data'])
                require(average <= ctx.args.black_max, f'Black pattern too bright: {average}')
            artifact=ctx.args.output/f"{fmt['name']}-{width}x{height}-{pattern}.raw"
            artifact.write_bytes(frames[-1]['data'])
            return {'frames':len(frames),'bytes':len(frames[-1]['data']), 'timestamps':[f['timestamp'] for f in frames], 'artifact':artifact.name}
        finally:
            receiver.close()


def cases():
    result=[Case('protocol.discovery','Discovery','behavior','Validate identity and network address',discovery),
            Case('protocol.xml','GenICam XML','behavior','Download XML and reconcile every catalog feature',xml_case)]
    # Every declared catalog node receives its own case, including explicit gaps.
    for f in FEATURES:
        result.append(Case('read.'+f['name'],f['name'],'register','Validate raw range and enumeration',lambda c,f=f:range_case(c,f)))
        if f.get('kind')=='Command':
            result.append(Case('command.'+f['name'],f['name'],'command','Execute command and verify completion',lambda c,n=f['name']:apply_case(c,n)))
        elif f['access']=='RW':
            # Selectors, persistent settings, arbitrary register masks and coupled
            # geometry/threshold settings need dedicated procedures, not blind writes.
            excluded={'UserSetControl','SensorRegisters','SensorTiming','SensorActuator','SensorOverlay','ImageCalibration','ImageEffects','SensorCapabilities'}
            if f['group'] not in excluded and f['name'] not in {'FanStartTemperature','FanStopTemperature','SensorIspControl0','SensorIspControl1','SensorIspControl3'}:
                values=list(f.get('entries',{}).values()) or [f['minimum'], f['maximum']]
                for value in dict.fromkeys(values):
                    result.append(Case(f"write.{f['name']}.{value}",f['name'],'register-write','Change value, verify readback and restore',lambda c,f=f,v=value:roundtrip(c,f,v)))
    for fmt in FORMATS:
        result.append(Case('stream.'+fmt['name'],fmt['name'],'image','Receive complete frames and validate metadata, timestamps and counters',lambda c,f=fmt:stream_case(c,f)))
    for fmt in FORMATS:
        if fmt['name']!='YUV8_UYV':
            result.append(Case('stream.maximum.'+fmt['name'],fmt['name'],'image','Validate complete frames at 2592x1944',lambda c,f=fmt:stream_case(c,f,2592,1944)))
    mono=next(f for f in FORMATS if f['name']=='Mono8')
    result.append(Case('stream.backend_lock','ReverseX','interaction','Reject raw sensor writes during a verified active stream',lambda c:stream_case(c,mono,check_lock=True)))
    result.append(Case('image.black','TestPattern','image','Combine Mono8 capture with black-pattern luminance assertion',lambda c:stream_case(c,mono,pattern=4)))
    from .behavior import protected, staged, fan_hysteresis, timestamp, table
    for f in FEATURES:
        if f['access']=='RO':
            result.append(Case('protect.'+f['name'],f['name'],'access','Reject direct write to read-only register',lambda c,f=f:protected(c,f)))
        if f['group'] in {'ImageEffects','ImageCalibration','SensorOverlay'} and f['access']=='RW' and not f['name'].endswith('Selector'):
            result.append(Case('staged.'+f['name'],f['name'],'staged-apply','Edit staged value, commit and restore original value',lambda c,f=f:staged(c,f)))
    result += [Case('board.fan_hysteresis','FanStartTemperature','behavior','Reject equal fan thresholds',fan_hysteresis),
               Case('protocol.timestamp','Timestamp','behavior','Latch monotonically increasing device timestamps',timestamp),
               Case('table.gamma','GammaPointSelector','table','Read every gamma table entry',lambda c:table(c,'GammaPointSelector','GammaPointValue',16)),
               Case('table.lens','LensCorrectionSelector','table','Read every lens correction table entry',lambda c:table(c,'LensCorrectionSelector','LensCorrectionValue',62))]
    # Explicit behavioral gaps are always visible, even when a register case passed.
    for name,reason in {
        'UserSetSave':'Use the functional flash suite with a reserved test slot; this diagnostic case is not functional evidence',
        'FactoryUserSetSave':'Factory baseline overwrite requires a dedicated maintenance procedure',
        'DeviceReset':'DeviceReset/startup reload is in the functional flash suite; saved-network migration still requires a network fixture',
        'NetworkControl':'DHCP, AutoIP, persistent IP and subnet migration require an isolated network fixture',
        'PacketResendRequests':'Loss injection and resend reconstruction procedure is not implemented',
        'Multicast':'Multiple-receiver membership and delivery procedure is not implemented',
        'ImageCompressionMode':'Independent JPEG decode and expected-image comparison are in pixels.jpeg',
        'SensorVcmAvailable':'Lens movement needs a verified actuator and optical fixture',
        'StatusLEDs':'Physical polarity and cadence require observation or a photodiode',
        'FanDutyCycle':'PWM waveform and thermal response require external measurement',
        'OpticalControls':'Use the functional suite for expected-image/response assertions and independent ISP fixture references',
    }.items():
        def gap(c,reason=reason):
            raise Skip(reason)
        result.append(Case('qualification.'+name,name,'qualification','Dedicated functional evidence required',gap))
    return result
