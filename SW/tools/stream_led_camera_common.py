"""Shared volatile camera setup/snapshot helpers; never invokes UserSet commands."""
import json
import time
from pathlib import Path
import gi
gi.require_version('Aravis','0.8')
from gi.repository import Aravis
from gige_feature_catalog import FEATURES
from gige_probe import Control

REG={f['key']:f['address'] for f in FEATURES}
SENSOR=['EXPOSURE_AUTO','GAIN_AUTO','AWB_AUTO','EXPOSURE_LINES','GAIN','WB_RED','WB_GREEN','WB_BLUE',
        'MIRROR','FLIP','MAX_EXPOSURE_50','MAX_EXPOSURE_60','MAX_GAIN','BANDING','TEST_PATTERN',
        'JPEG_QUALITY','SENSOR_FPS','FRAME_RATE_ENABLE','COMPRESSION','TEST_STYLE','TEST_ROLL','TEST_TRANSPARENT','TEST_BW']
SELECTORS=['FPGA_MATRIX_INDEX','FPGA_OFFSET_CHANNEL','FPGA_GAMMA_CHANNEL','FPGA_GAMMA_INDEX',
           'FPGA_LSC_PHASE','FPGA_LSC_ROW','FPGA_LSC_COLUMN']
TABLE_VALUES=['FPGA_MATRIX_VALUE','FPGA_OFFSET_VALUE','FPGA_GAMMA_VALUE','FPGA_LSC_GAIN']
SCALARS=[f['key'] for f in FEATURES if f['key'].startswith('FPGA_') and f['access']=='RW'
         and f['key'] not in SELECTORS+TABLE_VALUES]
FLAGS=['FPGA_CCM','FPGA_GAMMA','FPGA_AWB','FPGA_MEDIAN','FPGA_SCALER','FPGA_ROI_ENABLE',
       'FPGA_BILATERAL','FPGA_SHARPEN','FPGA_LSC','FPGA_SCENE','FPGA_YUV_RANGE','FPGA_INTERPOLATION','FPGA_CLAHE']


def read(device,key):return device.read_register(REG[key])
def write(device,key,value):device.write_register(REG[key],value)


def idle(device):
    end=time.monotonic()+25
    while read(device,'SENSOR_BUSY'):
        assert time.monotonic()<end,'Camera did not drain'
        time.sleep(.01)


def tables(device,values=None):
    selectors={k:read(device,k) for k in SELECTORS};result={'matrix':[],'offsets':[],'gamma':[],'lsc':[]}
    try:
        for name,selector,data,count in [('matrix','FPGA_MATRIX_INDEX','FPGA_MATRIX_VALUE',9),
                                          ('offsets','FPGA_OFFSET_CHANNEL','FPGA_OFFSET_VALUE',3)]:
            for i in range(count):
                write(device,selector,i)
                if values is not None:write(device,data,values[name][i])
                result[name].append(read(device,data))
        for c in range(3):
            write(device,'FPGA_GAMMA_CHANNEL',c)
            for x in range(256):
                write(device,'FPGA_GAMMA_INDEX',x)
                if values is not None:write(device,'FPGA_GAMMA_VALUE',values['gamma'][c*256+x])
                result['gamma'].append(read(device,'FPGA_GAMMA_VALUE'))
        for phase in range(4):
            write(device,'FPGA_LSC_PHASE',phase)
            for row in range(7):
                write(device,'FPGA_LSC_ROW',row)
                for column in range(9):
                    write(device,'FPGA_LSC_COLUMN',column)
                    if values is not None:write(device,'FPGA_LSC_GAIN',values['lsc'][phase*63+row*9+column])
                    result['lsc'].append(read(device,'FPGA_LSC_GAIN'))
        if values is not None:assert result==values,'FPGA table readback differs'
        return result
    finally:
        for key,value in selectors.items():write(device,key,value)


def fpga_scalar_keys(capabilities):
    return [k for k in SCALARS if capabilities&65536 or k not in ['FPGA_ADAPTIVE_TONE','FPGA_TONE_TARGET']]


def snapshot(camera,require_disabled=True):
    d=camera.get_device();idle(d)
    if require_disabled:assert read(d,'FPGA_ENABLE')==0,'Original FPGA processing is active; preserve without changing'
    wire=Control('10.0.0.3','10.0.0.7')
    try:identity=wire.memory(0,256)
    finally:wire.sock.close()
    capabilities=read(d,'FPGA_CAPABILITIES')
    fpga_keys=fpga_scalar_keys(capabilities)
    return {'registers':{hex(a):d.read_register(a) for a in [0x10000,0x10004,0x10008,0xd04,0xd08,0xd00,0xd18,0x1001c]},
            'features':{key:read(d,key) for key in SENSOR},'fpga':{key:read(d,key) for key in fpga_keys+SELECTORS},
            'tables':tables(d),'identity':{'0x0':identity.hex()},
            'diagnostics':{key:read(d,key) for key in ['FPGA_VERSION','FPGA_CAPABILITIES','RECOVERIES','USERSET_MASK','USERSET_DEFAULT','LED_STATE']}}


def mode(camera,width,height,fmt):
    d=camera.get_device();idle(d)
    write(d,'FPGA_ENABLE',0);write(d,'COMPRESSION',0);write(d,'FRAME_RATE_ENABLE',0)
    for key in ['EXPOSURE_AUTO','GAIN_AUTO','AWB_AUTO']:write(d,key,0)
    d.set_string_feature_value('PixelFormat','YUV422_8')
    d.set_integer_feature_value('Width',width);d.set_integer_feature_value('Height',height)
    d.set_string_feature_value('PixelFormat',fmt)
    for key,value in [('TEST_PATTERN',1),('EXPOSURE_LINES',100),('GAIN',16),
                      ('WB_RED',1024),('WB_GREEN',1024),('WB_BLUE',1024),
                      ('TEST_STYLE',0),('TEST_ROLL',0),('TEST_TRANSPARENT',0),('TEST_BW',0)]:write(d,key,value)


def configure(camera,name):
    d=camera.get_device();idle(d);write(d,'FPGA_ENABLE',0)
    for key in FLAGS:write(d,key,0)
    write(d,'FPGA_CLAHE_CLIP',4);write(d,'FPGA_BPC',0);write(d,'FPGA_BLACK',0)
    for key in ['FPGA_RED','FPGA_GREEN','FPGA_BLUE']:write(d,key,256)
    if name=='native-vga':mode(camera,640,480,'YUV422_8')
    elif name=='fpga-rgb-vga':
        mode(camera,640,480,'RGB8')
        write(d,'FPGA_INPUT_WIDTH',2592);write(d,'FPGA_INPUT_HEIGHT',1944)
        for key in ['FPGA_CCM','FPGA_GAMMA','FPGA_MEDIAN','FPGA_SCALER','FPGA_BPC',
                    'FPGA_BILATERAL','FPGA_LSC','FPGA_SCENE','FPGA_INTERPOLATION','FPGA_CLAHE']:write(d,key,1)
        write(d,'FPGA_BLACK',31);write(d,'FPGA_SHARPEN',64)
        write(d,'FPGA_SIGMA_COLOR',24);write(d,'FPGA_SIGMA_SPACE',256)
        tables(d,{'matrix':[1048576 if i%4==0 else 0 for i in range(9)],'offsets':[0]*3,
                  'gamma':[(x*x+127)//255 for c in range(3) for x in range(256)],'lsc':[4096]*252})
        write(d,'FPGA_ENABLE',1)
    elif name in ['raw-packed','raw-le16']:
        mode(camera,2592,1944,'BayerBG10p' if name=='raw-packed' else 'BayerBG10')
        write(d,'FPGA_BPC',1);write(d,'FPGA_ENABLE',1)
    else:raise ValueError(name)


def restore(camera,state):
    d=camera.get_device();camera.stop_acquisition();idle(d)
    d.write_register(0x1001c,0);write(d,'FPGA_ENABLE',0)
    for key in FLAGS:write(d,key,0)
    regs=state['registers'];f=state['features']
    mode(camera,regs['0x10000'],regs['0x10004'],'YUV422_8')
    d.write_register(0x10008,regs['0x10008'])
    for key in SENSOR:
        if key not in ['EXPOSURE_AUTO','GAIN_AUTO','AWB_AUTO','FRAME_RATE_ENABLE','COMPRESSION']:write(d,key,f[key])
    write(d,'FRAME_RATE_ENABLE',f['FRAME_RATE_ENABLE']);write(d,'COMPRESSION',f['COMPRESSION'])
    for key in ['EXPOSURE_AUTO','GAIN_AUTO','AWB_AUTO']:write(d,key,f[key])
    tables(d,state['tables'])
    for key,value in state['fpga'].items():
        if key!='FPGA_ENABLE':write(d,key,value)
    write(d,'FPGA_ENABLE',state['fpga']['FPGA_ENABLE'])
    # Older archived profiles lack destination/lock fields. Preserve their
    # original comparison scope while new profiles restore the entire route.
    for address in [0xd04,0xd08,0xd00,0xd18,0x1001c]:
        if hex(address) in regs:d.write_register(address,regs[hex(address)])
    assert {key:d.read_register(int(key,16)) for key in regs}==regs
    assert {key:read(d,key) for key in SENSOR}==f,'Original sensor settings were not restored'
    assert {key:read(d,key) for key in state['fpga']}==state['fpga'],'Original FPGA controls were not restored'
    assert tables(d)==state['tables'],'Original tables were not restored'
