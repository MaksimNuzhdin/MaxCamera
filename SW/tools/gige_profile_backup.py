"""Non-default UserSet backup/restore helpers through supported volatile controls."""
import re
from pathlib import Path
from stream_led_camera_common import snapshot,restore,read,write,idle
KEYS=re.findall(r'^X\((\w+)\)',(Path(__file__).resolve().parents[1]/'gige/GigePresetFeatures.inc').read_text(),re.M)
AE_REGS=[0x3a0f,0x3a10,0x3a11,0x3a1b,0x3a1e,0x3a1f]

def full_snapshot(camera):
    d=camera.get_device();state=snapshot(camera,require_disabled=False)
    state['preset_sensor']={k:read(d,k) for k in KEYS}
    selected={k:read(d,k) for k in ['GAMMA_INDEX','LENC_INDEX','REG_SELECTOR']}
    try:
        for name,index,value,count in [('sensor_gamma','GAMMA_INDEX','GAMMA_VALUE',16),('sensor_lenc','LENC_INDEX','LENC_VALUE',62)]:
            state[name]=[]
            for i in range(count):write(d,index,i);state[name].append(read(d,value))
        state['ae_thresholds']=[]
        for reg in AE_REGS:write(d,'REG_SELECTOR',reg);state['ae_thresholds'].append(read(d,'REG_VALUE'))
    finally:
        for k,v in selected.items():write(d,k,v)
    state['sensor_selectors']=selected
    state['line_period_ns']=read(d,'LINE_PERIOD')
    return state

def full_restore(camera,state):
    # AE_TARGET is derived and cannot recreate arbitrary six-byte factory AE
    # thresholds. Seed matching thresholds by loading the preserved native
    # anchor first; do not bypass the protected sensor-register whitelist.
    # The final exact comparison rejects a missing/wrong anchor.
    d=camera.get_device();restore(camera,state)
    for key in ['EXPOSURE_AUTO','GAIN_AUTO','AWB_AUTO']:write(d,key,0)
    for key,value in state['preset_sensor'].items():
        if key not in ['EXPOSURE_AUTO','GAIN_AUTO','AWB_AUTO','ISP0','ISP1','ISP3','AE_TARGET']:write(d,key,value)
    write(d,'FX_APPLY',1);write(d,'DRAW_APPLY',1)
    for name,index,value,apply in [('sensor_gamma','GAMMA_INDEX','GAMMA_VALUE','GAMMA_APPLY'),('sensor_lenc','LENC_INDEX','LENC_VALUE','LENC_APPLY')]:
        for i,v in enumerate(state[name]):write(d,index,i);write(d,value,v)
        write(d,apply,1)
    for key in ['ISP0','ISP1','ISP3']:write(d,key,state['preset_sensor'][key])
    for key in ['EXPOSURE_AUTO','GAIN_AUTO','AWB_AUTO']:write(d,key,state['preset_sensor'][key])
    for key,value in state['sensor_selectors'].items():write(d,key,value)
    actual=full_snapshot(camera)
    for key in ['registers','features','fpga','tables','preset_sensor','sensor_gamma','sensor_lenc','ae_thresholds','line_period_ns']:
        assert actual[key]==state[key],key+' cannot be restored exactly through supported controls'
    return actual
