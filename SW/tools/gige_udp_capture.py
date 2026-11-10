"""GenApi-controlled capture with the independent classic GVSP receiver."""
import json,os,socket,subprocess,time
from pathlib import Path

class CaptureFailure(RuntimeError):
 """Keep receiver evidence when acquisition cleanup also fails."""
 def __init__(self,primary_error,stop_error,stdout,stderr,returncode):
  self.primary_error=primary_error;self.stop_error=stop_error
  self.receiver_stdout=stdout;self.receiver_stderr=stderr
  self.receiver_returncode=returncode
  super().__init__('Capture failed: '+json.dumps({
   'primary_error':repr(primary_error) if primary_error is not None else None,
   'stop_error':repr(stop_error) if stop_error is not None else None,
   'receiver_returncode':returncode,'receiver_stdout':stdout,'receiver_stderr':stderr}))

def capture(camera,ip,output,seconds,on_started=None,trace=None,receiver=None,packet_trace=None):
 root=Path(__file__).resolve().parent.parent
 source=root/'tools/gige_udp_receiver.c';exe=Path(receiver) if receiver is not None else root/'build/tests/gige_udp_receiver'
 if receiver is None and (not exe.exists() or exe.stat().st_mtime<source.stat().st_mtime):
  exe.parent.mkdir(parents=True,exist_ok=True)
  subprocess.run(['cc','-O2','-std=c11','-Wall','-Wextra','-Werror',str(source),'-o',str(exe)],check=True)
 d=camera.get_device();w,h=camera.get_region()[2:];pf=int(camera.get_pixel_format())&0xffffffff;payload=camera.get_payload()
 probe=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);probe.connect((ip,3956));host=probe.getsockname()[0];probe.close()
 command=[str(exe),host,str(w),str(h),str(pf),str(payload),str(seconds),str(output)]
 if trace is not None:command.append(str(trace))
 environment=os.environ.copy()
 if packet_trace is not None:
  environment.update(MAXCAMERA_PACKET_TRACE=str(packet_trace),MAXCAMERA_TRACE_FAILURES='1')
 process=subprocess.Popen(command,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=environment)
 try:
  ready=json.loads(process.stdout.readline())
  d.write_register(0xd00,ready['port']);d.write_register(0xd18,int.from_bytes(socket.inet_aton(host),'big'));d.write_register(0xd04,1500)
  counters=['CapturedFrames','DroppedFrames','TransmittedFrames','CsiErrorEvents',
            'StreamSendRetries','EthernetErrors','EthernetReceiveDrops',
            'EthernetReceivedPackets','EthernetTransmittedPackets']
  baseline={n:d.get_integer_feature_value(n) for n in counters}
  acquisition_started=time.monotonic()
  camera.start_acquisition()
  acquisition_ack=time.monotonic()
  text=errors='';primary_error=stop_error=None
  try:
   if on_started is not None:on_started()
   text,errors=process.communicate(timeout=seconds+25)
  except Exception as error:
   primary_error=error
   if isinstance(error,subprocess.TimeoutExpired):
    text=error.output or '';errors=error.stderr or ''
    if isinstance(text,bytes):text=text.decode(errors='replace')
    if isinstance(errors,bytes):errors=errors.decode(errors='replace')
  finally:
   try:
    camera.stop_acquisition();end=time.monotonic()+25
    while d.get_boolean_feature_value('SensorBusy'):
     assert time.monotonic()<end;time.sleep(.01)
   except Exception as error:
    stop_error=error
  if primary_error is not None or stop_error is not None:
   # Terminate failed receivers as before, and drain their pipes so a
   # secondary Stop error cannot discard the original receiver evidence.
   if primary_error is not None:
    if process.poll() is None:process.kill()
    try:
     text,errors=process.communicate()
    except Exception as drain_error:
     errors+='\nReceiver output drain failed: '+repr(drain_error)
   raise CaptureFailure(primary_error,stop_error,text,errors,process.returncode) from (primary_error or stop_error)
  if not text.strip():raise RuntimeError('UDP receiver failed: '+errors)
  record=json.loads(text.strip());record.update(receiver='recvmmsg',receiver_stderr=errors,encoding=d.get_string_feature_value('PixelFormat'),width=w,height=h,pixel_format=hex(pf),payload=payload,resulting_fps=d.get_float_feature_value('ResultingFrameRate'),maximum_fps=d.get_float_feature_value('AcquisitionFrameRateMaximum'),packet_delay=d.read_register(0xd08),counters={n:d.get_integer_feature_value(n)-baseline[n] for n in counters},csi=d.get_integer_feature_value('CsiErrorEvents')-baseline['CsiErrorEvents'])
  if trace is not None:
   rows=Path(trace).read_text().splitlines()
   if len(rows)>1:
    record['first_complete_after_start_ms']=1000*(float(rows[1].split(',')[0])-acquisition_started)
    record['acquisition_command_ms']=1000*(acquisition_ack-acquisition_started)
  record['passed']=process.returncode==0 and record['csi']==0
  return record
 finally:
  if process.poll() is None:process.kill();process.wait()
