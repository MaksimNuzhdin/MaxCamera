"""Single-owner camera lifecycle with finite operations and guaranteed release."""
import ctypes
import gc
import math
import socket
import struct
import threading
import time
from pathlib import Path
from catalog import FEATURE_MAP, NODES
from imaging import jpeg_frame

class CameraError(Exception):pass

class Camera:
    def __init__(self,control,receiver,camera_ip):
        self.control=control;self.receiver=receiver;self.ip=camera_ip
        self.control.settimeout(.2);self.receiver.setblocking(False)
        self.lock=threading.RLock();self.rid=0;self.owner=None;self.acquired=False
        self.streaming=False;self.stopping=False;self.deadline=0;self.last_touch=0
        self.values={};self.feature_errors={};self.jpeg=None;self.frame_time=0;self.frames=0;self.incomplete=0
        self.error='';self.stop_event=threading.Event();self.worker=None;self.original_transport={}
        self.lib=ctypes.CDLL(str(Path(__file__).parent/'.runtime/receiver.so'))
        self.lib.receive_frame.argtypes=[ctypes.c_int,ctypes.c_void_p,ctypes.c_uint,ctypes.c_uint,ctypes.c_uint,ctypes.c_uint,ctypes.c_uint,ctypes.POINTER(ctypes.c_uint)]
        self.lib.receive_frame.restype=ctypes.c_int
        threading.Thread(target=self._watchdog,daemon=True).start()
    def command(self,opcode,body=b'',retry=True):
        self.rid=self.rid%65535+1
        request=struct.pack('!BBHHH',66,1,opcode,len(body),self.rid)+body
        for _ in range(3 if retry else 1):
            self.control.send(request);until=time.monotonic()+(3.0 if opcode==0x82 else .3)
            while time.monotonic()<until:
                try:ack=self.control.recv(2048)
                except socket.timeout:continue
                if len(ack)<8:continue
                status,code,length,rid=struct.unpack('!HHHH',ack[:8])
                if rid!=self.rid or code!=opcode+1:continue
                if len(ack)!=8+length:raise CameraError('Malformed camera response')
                if status:raise CameraError(f'Camera rejected operation (GVCP 0x{status:04x}). Check feature dependencies and stopped acquisition.')
                return ack[8:]
        raise CameraError('Camera did not respond. It may be disconnected or owned by another application.')
    def read(self,address):
        try:return struct.unpack('!I',self.command(0x80,struct.pack('!I',address)))[0]
        except CameraError as exc:raise CameraError(f'{exc} (read 0x{address:08x})') from exc
    def write(self,address,value):
        try:ack=self.command(0x82,struct.pack('!II',address,value&0xffffffff),retry=False)
        except CameraError as exc:raise CameraError(f'{exc} (write 0x{address:08x})') from exc
        if ack!=b'\0\0\0\1':raise CameraError('Unexpected write acknowledgement')
    def memory(self,address,length):
        b=self.command(0x84,struct.pack('!IHH',address,0,length))
        if len(b)!=length+4 or struct.unpack('!I',b[:4])[0]!=address:raise CameraError('Malformed memory response')
        return b[4:]
    def _check(self,owner):
        if not self.acquired or self.owner!=owner:raise CameraError('Connect to the camera first. Another session may hold control.')
        self.last_touch=time.monotonic()
    def connect(self,owner):
        with self.lock:
            if self.acquired:
                self._check(owner);return
            if self.read(0xa00)!=0:raise CameraError('Camera is busy in another application. No takeover attempted. Try again later.')
            # Verify the unit before taking control. No discovery or user-supplied IP.
            model=self.memory(0x68,32).split(b'\0')[0].decode('ascii','replace')
            if '5640' not in model and 'MaxCamera' not in model:raise CameraError('Unexpected camera model; refusing control')
            self.write(0xa00,2);self.acquired=True;self.owner=owner;self.last_touch=time.monotonic();self.deadline=self.last_touch+900
            try:
                self.original_transport={a:self.read(a) for a in (0xd00,0xd04,0xd08,0xd18)}
                self.refresh(owner)
                self.error=''
            except Exception:
                self.release(owner);raise
    def decode(self,f,raw):
        if f.get('signed') and raw&0x80000000:raw-=1<<32
        if f['name']=='ExposureTime':return raw*self.read(FEATURE_MAP['SensorLinePeriod']['address'])/1000
        if f['kind']=='TemperatureADC':return round(raw*503.975/1024-273.15,3)
        if f['kind']=='Float':return raw/f.get('scale',1)
        if f['kind']=='Boolean':return bool(raw)
        return raw
    def refresh(self,owner,names=None):
        with self.lock:
            self._check(owner)
            if self.streaming:raise CameraError('Stop streaming before refreshing all camera parameters')
            for name in names or FEATURE_MAP:
                f=FEATURE_MAP.get(name)
                if not f or f['access']=='WO' or f['kind']=='Command':continue
                dep=f.get('available','').removesuffix('Register')
                if dep in FEATURE_MAP and self.read(FEATURE_MAP[dep]['address'])==0:
                    self.values.pop(name,None);self.feature_errors[name]='Unavailable in the current configuration';continue
                try:
                    self.values[name]=self.decode(f,self.read(f['address']));self.feature_errors.pop(name,None)
                except CameraError as exc:
                    if '0x8007' not in str(exc) and '0x8003' not in str(exc):raise
                    self.values.pop(name,None);self.feature_errors[name]=str(exc)
            return self.values.copy()
    def available(self,f):
        dep=f.get('available')
        if not dep:return True
        dep=dep.removesuffix('Register')
        if dep in self.values:return bool(self.values[dep])
        # Firmware validates complex GenApi expressions; do not invent capability.
        return True
    def set_feature(self,owner,name,value):
        with self.lock:
            self._check(owner)
            if self.streaming or self.stopping:raise CameraError('Stop the stream before changing parameters')
            f=FEATURE_MAP.get(name)
            if not f or not f['remote_writable']:raise CameraError('Feature is read-only or restricted to local maintenance')
            if not self.available(f):raise CameraError('Feature is unavailable on this camera configuration')
            for feature,auto in [('ExposureTime','ExposureAuto'),('GainFactor','GainAuto')]:
                if name==feature and self.read(FEATURE_MAP[auto]['address'])!=0:raise CameraError(f'Set {auto} to Off first')
            if name.startswith('WhiteBalance') and self.read(FEATURE_MAP['BalanceWhiteAuto']['address'])!=0:raise CameraError('Set BalanceWhiteAuto to Off first')
            if not isinstance(value,(int,float,bool)) or not math.isfinite(value):raise CameraError('A finite numeric value is required')
            if f['kind']=='Command':
                if value!=1:raise CameraError('Command value must be 1')
                raw=1
            elif name=='ExposureTime':
                period=self.read(FEATURE_MAP['SensorLinePeriod']['address'])/1000
                if period<=0:raise CameraError('Sensor timing is unavailable')
                raw=round(value/period)
            elif f['kind']=='TemperatureADC':
                if not -40<=value<=125:raise CameraError('Temperature must be between -40 and 125 C')
                raw=round((value+273.15)*1024/503.975)
            else:raw=round(value*f.get('scale',1))
            if not f['minimum']<=raw<=f['maximum']:raise CameraError('Value is outside the camera feature limits')
            if 'entries' in f and raw not in f['entries'].values():raise CameraError('Unknown enumeration value')
            if f['kind']=='Boolean' and value not in (0,1):raise CameraError('Boolean value must be 0 or 1')
            if f['kind'] not in ('Float','TemperatureADC','Command') and raw!=value:raise CameraError('An integer value is required')
            if name in ('Width','Height'):
                inc=self.read(FEATURE_MAP['WidthIncrement']['address']) if name=='Width' else 2
                maximum=self.read(FEATURE_MAP['WidthMaximum' if name=='Width' else 'HeightMaximum']['address'])
                if raw%inc or raw>maximum:raise CameraError(f'{name} must be a multiple of {inc}, maximum {maximum}')
            if name=='UserSetLoad' and self.read(FEATURE_MAP['UserSetSelector']['address'])>7:raise CameraError('Unknown UserSet')
            self.write(f['address'],raw)
            if f['access']!='WO':self.values[name]=self.decode(f,self.read(f['address']))
            # Read current dependencies after mode/profile/selector changes.
            self.refresh(owner)
    def start(self,owner):
        with self.lock:
            self._check(owner)
            if self.streaming:return
            if self.stopping:raise CameraError('Camera is stopping; wait before starting again')
            w,h,pf,size=(self.read(a) for a in (0x10000,0x10004,0x10008,0x1000c))
            compression=self.read(FEATURE_MAP['ImageCompressionMode']['address'])
            supported=(0x02180014,0x02180015,0x02100032,0x0210001f,0x82100004,0x82100005,0x01080001,0x0108000b,0x0110000f)
            if compression or pf not in supported:raise CameraError('Select uncompressed RGB8, BGR8, YUV422, Mono8, BayerBG8 or BayerBG10 for browser preview. Other formats remain configurable.')
            if not 0<size<=16000000 or w*h>2592*1944:raise CameraError('Invalid camera payload geometry')
            try:
                while True:self.receiver.recv(65536)
            except BlockingIOError:pass
            try:
                self.write(0xd18,int.from_bytes(socket.inet_aton(self.receiver.getsockname()[0]),'big'))
                self.write(0xd00,self.receiver.getsockname()[1]);self.write(0xd04,1500);self.write(0xd08,20)
                self.write(0x1001c,1);self.write(0x10014,1)
            except Exception:
                self._stop_hardware();raise
            self.stop_event.clear();self.streaming=True;self.jpeg=None;self.frames=0;self.incomplete=0;self.started=time.monotonic();self.error=''
            self.worker=threading.Thread(target=self._capture,args=(w,h,pf,size),daemon=True);self.worker.start()
    def _capture(self,w,h,pf,size):
        buf=ctypes.create_string_buffer(size);bad=ctypes.c_uint(0)
        ip=struct.unpack('=I',socket.inet_aton(self.ip))[0]
        try:
            while not self.stop_event.is_set():
                result=self.lib.receive_frame(self.receiver.fileno(),buf,size,w,h,pf,ip,ctypes.byref(bad))
                self.incomplete=bad.value
                if result==1:
                    frame=jpeg_frame(buf.raw,w,h,pf)
                    self.jpeg=frame;self.frames+=1;self.frame_time=time.monotonic()
                if time.monotonic()-(self.frame_time or self.started)>8:raise CameraError('No complete frames received for 8 seconds; camera released')
        except Exception as exc:
            self.error=str(exc);self.stop_event.set()
        finally:
            # Watchdog performs serialized stop/release on faults.
            gc.collect()
    def _stop_hardware(self):
        errors=[]
        for a,v in [(0x10014,0),(0x1001c,0),*self.original_transport.items()]:
            try:self.write(a,v)
            except Exception as exc:errors.append(str(exc))
        if errors:self.error='; '.join(errors)
    def stop(self,owner):
        with self.lock:
            self._check(owner);self.stopping=True;self.stop_event.set()
            if self.worker:self.worker.join(timeout=2)
            self._stop_hardware();self.streaming=False;self.stopping=False;self.worker=None;self.jpeg=None
    def release(self,owner=None):
        with self.lock:
            if not self.acquired:return
            if owner is not None and self.owner!=owner:raise CameraError('Camera belongs to another session')
            try:
                self.stop(self.owner)
            finally:
                try:self.write(0xa00,0)
                finally:self.acquired=False;self.owner=None;self.values={};self.jpeg=None
    def _watchdog(self):
        heartbeat=time.monotonic()
        while True:
            time.sleep(.4)
            with self.lock:
                if not self.acquired:continue
                now=time.monotonic()
                try:
                    if now-self.last_touch>30 or now>=self.deadline or (self.streaming and self.stop_event.is_set()):
                        self.release();continue
                    if now-heartbeat>.8:
                        if self.read(0xa00)!=2:raise CameraError('Camera ownership was lost')
                        heartbeat=now
                except Exception as exc:
                    self.error=str(exc)
                    try:self.release()
                    except Exception:pass
    def status(self,owner):
        with self.lock:
            mine=self.acquired and self.owner==owner
            if mine:self.last_touch=time.monotonic()
            return dict(connected=mine,busy=self.acquired and not mine,streaming=self.streaming and mine,
                        frames=self.frames if mine else 0,incomplete=self.incomplete if mine else 0,
                        fps=round(self.frames/max(.1,time.monotonic()-getattr(self,'started',time.monotonic())),1) if mine and self.streaming else 0,
                        frame_age=round(time.monotonic()-self.frame_time,1) if mine and self.jpeg else None,
                        lease_remaining=max(0,int(self.deadline-time.monotonic())) if mine else 0,
                        error=self.error,values=self.values if mine else {},feature_errors=self.feature_errors if mine else {})
