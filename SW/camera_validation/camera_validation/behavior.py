"""Dedicated checks for coupled controls and staged register banks."""
from .cases import BY_NAME, require, Skip
from .transport import ProtocolError
import time


def protected(ctx, f):
    value=ctx.c.read(f['address'])
    try:
        ctx.c.write(f['address'],value)
    except ProtocolError as exc:
        require(exc.status in {0x8004,0x8006},f'Unexpected protection error: {exc}')
        return {'rejected_status':exc.status}
    raise AssertionError('Read-only register accepted a write')


def staged(ctx, f):
    groups={'ImageEffects':'EffectApply','ImageCalibration':('LensCorrectionApply' if f['name'].startswith('Lens') else 'GammaApply'),'SensorOverlay':'OverlayApply'}
    command=BY_NAME[groups[f['group']]]['address']
    address=f['address'];old=ctx.c.read(address)
    # Leave selectors fixed so value restoration addresses the same table entry.
    new=old ^ 1
    if new>f['maximum'] or new<f['minimum']:new=old
    try:
        ctx.c.write(address,new)
        require(ctx.c.read(address)==new,'Staged readback mismatch')
        ctx.c.write(command,1);ctx.idle()
        require(ctx.read('SensorLastError')==0,'Apply failed')
    finally:
        try:
            ctx.c.write(address,old);ctx.c.write(command,1);ctx.idle()
            require(ctx.c.read(address)==old,'Staged restore mismatch')
        except Exception as exc:
            from .cases import CleanupError
            raise CleanupError(str(exc)) from exc
    return {'before':old,'staged':new,'apply':groups[f['group']], 'scope':'Staging and command completion; optical effect needs a controlled fixture'}


def fan_hysteresis(ctx):
    upper=BY_NAME['FanStartTemperature']['address'];lower=BY_NAME['FanStopTemperature']['address']
    # Invalid write must be rejected without changing existing thermal protection.
    old=ctx.c.read(upper)
    try:ctx.c.write(upper,ctx.c.read(lower))
    except ProtocolError as exc:
        require(exc.status==0x8002,'Expected invalid-parameter status')
        require(ctx.c.read(upper)==old,'Rejected write changed threshold')
        return {'invalid_equal_threshold_rejected':True,'scope':'Electrical PWM and thermal hysteresis need instrumentation'}
    raise AssertionError('Equal fan thresholds were accepted')


def timestamp(ctx):
    values=[]
    for _ in range(2):
        ctx.c.write(0x944,2)
        values.append((ctx.c.read(0x948)<<32)|ctx.c.read(0x94c))
        time.sleep(.03)
    require(values[1]>values[0],'Latched timestamp failed to advance')
    return {'latched_ticks':values}


def table(ctx, selector_name, value_name, count):
    selector=BY_NAME[selector_name]['address']
    with ctx.settings([selector]):
        values=[]
        for index in range(count):
            ctx.c.write(selector,index)
            value=ctx.read(value_name)
            require(0<=value<=255,'Table byte out of range')
            values.append(value)
    return {'entries':count,'values':values}

