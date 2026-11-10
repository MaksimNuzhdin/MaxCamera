"""Stopped-only feature transactions and independent repeated image acquisition."""
from contextlib import AbstractContextManager
import socket
import time
import json
import csv
import os
import subprocess
from pathlib import Path
import numpy as np
from .cases import BY_NAME, FORMATS, require, CleanupError
from .transport import receive_frame
from .imaging import decode
from .packet_queue import PacketQueue

FORMAT_VALUES={f['name']:f['value'] for f in FORMATS}
NAMES={f['value']:f['name'] for f in FORMATS}


class ImageSession(AbstractContextManager):
    def __init__(self,ctx):
        self.ctx=ctx;self.saved={};self.saved_tables={};self.applies=set();self.patches={}
        self.stream_saved={a:ctx.c.read(a) for a in [0xd18,0xd00,0xd04,0xd08,0x1001c]}
        self.counter=0

    def get(self,name):
        return self.ctx.c.read(BY_NAME[name]['address'] if name in BY_NAME else {'Width':0x10000,'Height':0x10004,'PixelFormat':0x10008}[name])

    def set(self,name,value):
        address=BY_NAME[name]['address'] if name in BY_NAME else {'Width':0x10000,'Height':0x10004,'PixelFormat':0x10008}[name]
        if name not in self.saved:self.saved[name]=self.ctx.c.read(address)
        self.ctx.c.write(address,value)

    def command(self,name):
        if name=='SensorFeatureApply':
            selector=self.get('SensorRegisterSelector')
            if selector not in self.patches:self.patches[selector]=self.get('SensorRegisterValue')
        touched={'EffectApply':'SensorIspControl1','LensCorrectionApply':'SensorIspControl0','OverlayApply':'SensorIspControl3'}.get(name)
        if touched and touched not in self.saved:self.saved[touched]=self.get(touched)
        self.ctx.c.write(BY_NAME[name]['address'],1);self.ctx.idle()
        require(self.ctx.read('SensorLastError')==0,f'{name} sensor error')
        if name in {'EffectApply','GammaApply','LensCorrectionApply','OverlayApply','VcmApply'}:self.applies.add(name)

    def table(self,kind,values):
        selector,value_name,count={'gamma':('GammaPointSelector','GammaPointValue',16),
                                   'lens':('LensCorrectionSelector','LensCorrectionValue',62)}[kind]
        require(len(values)==count,'Wrong table length')
        if kind not in self.saved_tables:
            previous=self.get(selector);old=[]
            try:
                for i in range(count):self.ctx.c.write(BY_NAME[selector]['address'],i);old.append(self.get(value_name))
            finally:self.ctx.c.write(BY_NAME[selector]['address'],previous)
            self.saved_tables[kind]=old
        for i,value in enumerate(values):self.set(selector,i);self.set(value_name,int(value))
        # Indexed values are restored from the complete table, never a last entry.
        self.saved.pop(value_name,None)

    def setup(self,fmt='RGB8',pattern=1,width=640,height=480):
        # Format switches can seed/reset sensor controls, so snapshot them first.
        for name in ['ExposureAuto','GainAuto','BalanceWhiteAuto','ExposureTime','GainFactor',
                     'WhiteBalanceRed','WhiteBalanceGreen','WhiteBalanceBlue','ReverseX','ReverseY']:
            if name not in self.saved:self.saved[name]=self.get(name)
        # Record dependency modes first so they are restored after manual values.
        for name in ['ExposureAuto','GainAuto','BalanceWhiteAuto']:self.set(name,0)
        self.set('ImageCompressionMode',0)
        self.set('Width',640);self.set('Height',480);self.set('PixelFormat',FORMAT_VALUES[fmt])
        self.set('Width',width);self.set('Height',height)
        if not fmt.startswith('Bayer'):
            self.set('ReverseX',0);self.set('ReverseY',0)
            for channel in ['Red','Green','Blue']:self.set('WhiteBalance'+channel,1024)
        self.set('TestPattern',pattern)
        for name in ['SensorTestPatternStyle','SensorTestRolling','SensorTestTransparent','SensorTestMonochrome']:self.set(name,0)
        self.set('EffectEnableBits',0);self.command('EffectApply')
        self.set('OverlayEnable',0);self.command('OverlayApply')
        self.set('GainFactor',16)
        self.set('ExposureTime',min(200,self.get('ExposureMaximumLines')))

    def capture(self,label, *, settle=None):
        if getattr(self.ctx.args,'c_receiver',None):
            return self.capture_native(label,settle=settle)
        ctx=self.ctx;receiver=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
        buffered=None
        try:
            receiver.setsockopt(socket.SOL_SOCKET,socket.SO_RCVBUF,16*1024*1024)
            route=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
            try:
                route.connect((ctx.args.ip,3956));host=ctx.args.bind if ctx.args.bind!='0.0.0.0' else route.getsockname()[0]
            finally:route.close()
            receiver.bind((host,0));receiver.settimeout(.1)
            ctx.c.write(0xd18,int.from_bytes(socket.inet_aton(host),'big'))
            ctx.c.write(0xd00,receiver.getsockname()[1]);ctx.c.write(0xd04,1500)
            ctx.c.write(0xd08,ctx.args.packet_delay);ctx.c.write(0x1001c,1)
            width,height=self.get('Width'),self.get('Height')
            fmt='JPEG' if self.get('ImageCompressionMode') else NAMES[self.get('PixelFormat')]
            samples=[];records=[]
            if getattr(ctx.args,'buffer_packets',False):
                buffered=PacketQueue(receiver)
            ctx.c.write(0x10014,1)
            warm=ctx.args.settle_frames if settle is None else settle
            for i in range(warm+ctx.args.frames):
                frame=receive_frame(ctx.c,buffered or receiver,ctx.args.frame_timeout,
                                    allow_reorder=getattr(ctx.args,'allow_reorder',False))
                if fmt!='JPEG':
                    require((frame['width'],frame['height'],frame['pixel'])==(width,height,self.get('PixelFormat')),'Frame leader mismatch')
                    require(len(frame['data'])==ctx.c.read(0x1000c),'Frame payload mismatch')
                else:require(frame.get('jpeg'), 'Expected JPEG payload type')
                if i<warm:continue
                image=decode(frame['data'],fmt,width,height)
                samples.append(image.astype(np.float32))
                self.counter+=1
                path=ctx.case_output/f'{label}-{self.counter:04d}.bin';path.write_bytes(frame['data'])
                np.save(path.with_suffix('.npy'),image,allow_pickle=False)
                records.append({'raw':path.name,'timestamp':frame['timestamp'],'bytes':len(frame['data']),
                                'width':width,'height':height,'format':fmt,'array':path.with_suffix('.npy').name,'received_monotonic':time.monotonic()})
            require(all(b['timestamp']>a['timestamp'] for a,b in zip(records,records[1:])),'Frame timestamps did not increase')
        finally:
            try:ctx.c.write(0x10014,0);ctx.idle();ctx.c.write(0x1001c,0)
            except Exception as exc:raise CleanupError(str(exc)) from exc
            finally:
                if buffered is not None:
                    try:buffered.close()
                    finally:(ctx.case_output/f'{label}-receiver.json').write_text(json.dumps(buffered.statistics(),indent=2)+'\n')
                    if buffered.error is not None:raise buffered.error
                else:receiver.close()
        # Reduce/save images only after Stop; reception must never pause for a median.
        result=np.median(np.stack(samples),axis=0)
        np.save(ctx.case_output/f'{label}-median.npy',result,allow_pickle=False)
        return result,records

    def capture_native(self,label, *, settle=None):
        """Capture complete frames in a separate C process before decoding them."""
        ctx=self.ctx
        route=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
        try:
            route.connect((ctx.args.ip,3956))
            host=ctx.args.bind if ctx.args.bind!='0.0.0.0' else route.getsockname()[0]
        finally:route.close()
        width,height=self.get('Width'),self.get('Height')
        jpeg=bool(self.get('ImageCompressionMode'))
        pixel=self.get('PixelFormat')
        fmt='JPEG' if jpeg else NAMES[pixel]
        warm=ctx.args.settle_frames if settle is None else settle
        total=warm+ctx.args.frames
        require(0<=warm<total<=4096,'Requested image count exceeds C backend bounds')
        capture_index=self.counter
        path=ctx.case_output/f'{label}-capture-{capture_index:04d}.bin';trace=path.with_suffix('.csv')
        env=os.environ.copy()
        env.update(MAXCAMERA_FRAME_LIMIT=str(total),MAXCAMERA_SAVE_SKIP=str(warm),MAXCAMERA_JPEG=str(int(jpeg)),
                   MAXCAMERA_FRAME_TIMEOUT=str(ctx.args.frame_timeout),MAXCAMERA_TRACE_FAILURES='1',
                   MAXCAMERA_PACKET_TRACE=str(ctx.case_output/f'{label}-packet-window-{capture_index:04d}.csv'))
        capacity=width*height*2 if jpeg else ctx.c.read(0x1000c)
        command=[str(ctx.args.c_receiver),host,str(width),str(height),str(pixel),str(capacity),
                 str(ctx.args.frame_timeout*total),str(path),str(trace)]
        process=subprocess.Popen(command,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=env)
        try:
            ready=json.loads(process.stdout.readline())
            critical=['CsiErrorEvents','DroppedFrames','EthernetErrors','EthernetReceiveDrops']
            before={name:ctx.read(name) for name in critical+['StreamSendRetries',
                    'EthernetReceivedPackets','EthernetTransmittedPackets']}
            ctx.c.write(0xd18,int.from_bytes(socket.inet_aton(host),'big'))
            ctx.c.write(0xd00,ready['port']);ctx.c.write(0xd04,1500)
            ctx.c.write(0xd08,ctx.args.packet_delay);ctx.c.write(0x1001c,1)
            ctx.c.write(0x10014,1)
            stdout,stderr=process.communicate(timeout=ctx.args.frame_timeout*total+5)
            (ctx.case_output/f'{label}-receiver-raw-{capture_index:04d}.json').write_text(
                json.dumps(dict(stdout=stdout,stderr=stderr,returncode=process.returncode),indent=2)+'\n')
            result=json.loads(stdout)
            result.update(stderr=stderr,returncode=process.returncode,backend='independent C recvmmsg',
                          counters={name:ctx.read(name)-value for name,value in before.items()})
            (ctx.case_output/f'{label}-receiver-{capture_index:04d}.json').write_text(json.dumps(result,indent=2)+'\n')
            require(process.returncode==0 and result['good']==total,'C image receiver rejected capture: '+json.dumps(result))
            require(not any(result['counters'][name] for name in critical),
                    'Camera drop, CSI or Ethernet error during image capture')
        finally:
            try:ctx.c.write(0x10014,0);ctx.idle();ctx.c.write(0x1001c,0)
            except Exception as exc:raise CleanupError(str(exc)) from exc
            finally:
                if process.poll() is None:process.kill()
                process.communicate()
        with trace.open() as stream:rows=list(csv.DictReader(stream))
        blocks=[int(row['block_id']) for row in rows]
        require(all((b-a)%65535==1 for a,b in zip(blocks,blocks[1:])),'Frame BlockID gap during C capture')
        samples=[];records=[]
        for index,row in enumerate(rows[warm:],start=warm+1):
            raw=Path(str(path)+f'.frame{index:06d}.bin')
            image=decode(raw.read_bytes(),fmt,width,height)
            samples.append(image.astype(np.float32));self.counter+=1
            array=ctx.case_output/f'{label}-{self.counter:04d}.npy'
            np.save(array,image,allow_pickle=False)
            records.append(dict(raw=raw.name,array=array.name,timestamp=int(row['timestamp_ticks']),
                                bytes=raw.stat().st_size,width=width,height=height,format=fmt,
                                received_monotonic=float(row['completed_host_seconds'])))
        require(len(samples)==ctx.args.frames,'C receiver did not save requested image count')
        require(all(b['timestamp']>a['timestamp'] for a,b in zip(records,records[1:])),'Frame timestamps did not increase')
        median=np.median(np.stack(samples),axis=0)
        np.save(ctx.case_output/f'{label}-median-{capture_index:04d}.npy',median,allow_pickle=False)
        return median,records

    def __exit__(self,typ,value,tb):
        try:
            self.ctx.c.write(0x10014,0);self.ctx.idle();self.ctx.c.write(0x1001c,0)
            # Restore an unconstrained ISP mode before geometry/orientation/manual controls.
            if 'PixelFormat' in self.saved:
                self.ctx.c.write(BY_NAME['ImageCompressionMode']['address'],0)
                self.ctx.c.write(0x10000,640);self.ctx.c.write(0x10004,480)
                self.ctx.c.write(0x10008,self.saved['PixelFormat'])
                self.ctx.c.write(0x10000,self.saved['Width']);self.ctx.c.write(0x10004,self.saved['Height'])
            original_raw=NAMES.get(self.saved.get('PixelFormat',0),'').startswith('Bayer')
            geometry={'Width','Height','PixelFormat','ImageCompressionMode'}
            for name,old in reversed(list(self.saved.items())):
                if name in geometry:continue
                if original_raw and name in {'ReverseX','ReverseY'}:continue
                if name in {'WhiteBalanceRed','WhiteBalanceGreen','WhiteBalanceBlue','GainFactor','ExposureTime'}:
                    # Auto telemetry is not a restorable manual setpoint.
                    dep={'GainFactor':'GainAuto','ExposureTime':'ExposureAuto'}.get(name,'BalanceWhiteAuto')
                    if self.saved.get(dep,0)!=0:continue
                self.ctx.c.write(BY_NAME[name]['address'],old)
            for kind,old in self.saved_tables.items():
                selector,value_name,_={'gamma':('GammaPointSelector','GammaPointValue',16),'lens':('LensCorrectionSelector','LensCorrectionValue',62)}[kind]
                for i,v in enumerate(old):
                    self.ctx.c.write(BY_NAME[selector]['address'],i);self.ctx.c.write(BY_NAME[value_name]['address'],v)
                if selector in self.saved:self.ctx.c.write(BY_NAME[selector]['address'],self.saved[selector])
            for selector,old in self.patches.items():
                for name,v in [('SensorRegisterSelector',selector),('SensorFeatureMask',255),('SensorFeatureValue',old)]:
                    self.ctx.c.write(BY_NAME[name]['address'],v)
                self.ctx.c.write(BY_NAME['SensorFeatureApply']['address'],1)
                require(self.get('SensorRegisterValue')==old,'Raw sensor patch restoration failed')
            for name in ['SensorRegisterSelector','SensorFeatureMask','SensorFeatureValue']:
                if name in self.saved:self.ctx.c.write(BY_NAME[name]['address'],self.saved[name])
            for name in sorted(self.applies):self.command(name)
            for name in ['SensorIspControl0','SensorIspControl1','SensorIspControl3']:
                if name in self.saved:self.ctx.c.write(BY_NAME[name]['address'],self.saved[name])
            if 'PixelFormat' in self.saved:
                self.ctx.c.write(BY_NAME['ImageCompressionMode']['address'],self.saved['ImageCompressionMode'])
            for a,v in self.stream_saved.items():self.ctx.c.write(a,v)
        except Exception as exc:raise CleanupError(str(exc)) from exc
        return False
