"""Flash save/load acceptance using real images, full profiles and table contents."""
import json
from pathlib import Path
import time
import os
import struct
import numpy as np
from .cases import BY_NAME, Skip, require, CleanupError
from .acquisition import ImageSession, FORMAT_VALUES
from .imaging import validate_bars
from .transport import Control, ProtocolError

PRESET=json.loads(Path(__file__).with_name('preset.json').read_text())
EXTRA=['AcquisitionFrameRateEnable','AcquisitionFrameRate','ImageCompressionMode','SensorJpegQuality']
GROUP_APPLY=['EffectApply','GammaApply','LensCorrectionApply','OverlayApply']


def wait_store(ctx):
    end=time.monotonic()+ctx.args.flash_timeout
    while ctx.read('UserSetBusy') or ctx.read('SensorBusy'):
        require(time.monotonic()<end,'Flash/sensor operation timed out')
        ctx.c.read(0xa00);time.sleep(.05)
    require(ctx.read('UserSetLastError')==0,'User-set operation reported an error')


def operation(ctx,command,slot):
    wait_store(ctx)
    ctx.c.write(BY_NAME['UserSetSelector']['address'],slot)
    ctx.c.write(BY_NAME[command]['address'],1)
    wait_store(ctx)


def snapshot(ctx):
    values={name:ctx.read(name) for name in PRESET+EXTRA}
    values.update(Width=ctx.c.read(0x10000),Height=ctx.c.read(0x10004),PixelFormat=ctx.c.read(0x10008))
    tables={}
    for kind,selector,value,count in [('gamma','GammaPointSelector','GammaPointValue',16),('lens','LensCorrectionSelector','LensCorrectionValue',62)]:
        old=ctx.read(selector);table=[]
        try:
            for index in range(count):
                ctx.c.write(BY_NAME[selector]['address'],index);table.append(ctx.read(value))
        finally:ctx.c.write(BY_NAME[selector]['address'],old)
        tables[kind]=table
    old_selector=ctx.read('SensorRegisterSelector')
    try:
        thresholds=[]
        for address in [0x3a0f,0x3a10,0x3a11,0x3a1b,0x3a1e,0x3a1f]:
            ctx.c.write(BY_NAME['SensorRegisterSelector']['address'],address)
            thresholds.append(ctx.read('SensorRegisterValue'))
    finally:ctx.c.write(BY_NAME['SensorRegisterSelector']['address'],old_selector)
    return {'values':values,'tables':tables,'ae_thresholds':thresholds}


def apply_snapshot(ctx,profile):
    """Restore through public features, never by a private Flash or AXI address."""
    values=profile['values'];c=ctx.c
    write=lambda name,value:c.write(BY_NAME[name]['address'],value)
    for name in ['ExposureAuto','GainAuto','BalanceWhiteAuto']:write(name,0)
    write('ImageCompressionMode',0)
    c.write(0x10000,640);c.write(0x10004,480);c.write(0x10008,FORMAT_VALUES['RGB8'])
    auto={'ExposureTime':'ExposureAuto','GainFactor':'GainAuto',
          'WhiteBalanceRed':'BalanceWhiteAuto','WhiteBalanceGreen':'BalanceWhiteAuto','WhiteBalanceBlue':'BalanceWhiteAuto'}
    special={'ExposureAuto','GainAuto','BalanceWhiteAuto','ImageCompressionMode','AcquisitionFrameRate','AcquisitionFrameRateEnable'}
    # Geometry must precede timing-dependent exposure/limit restoration.
    c.write(0x10008,values['PixelFormat']);c.write(0x10000,values['Width']);c.write(0x10004,values['Height'])
    write('AcquisitionFrameRate',values['AcquisitionFrameRate'])
    write('AcquisitionFrameRateEnable',values['AcquisitionFrameRateEnable'])
    for name in PRESET+['SensorJpegQuality']:
        if name in special:continue
        if name in auto and values[auto[name]]!=0:continue
        write(name,values[name])
    for kind,selector,value_name in [('gamma','GammaPointSelector','GammaPointValue'),('lens','LensCorrectionSelector','LensCorrectionValue')]:
        previous=ctx.read(selector)
        for i,value in enumerate(profile['tables'][kind]):write(selector,i);write(value_name,value)
        write(selector,previous)
    for command in GROUP_APPLY:write(command,1)
    # Apply commands set ISP enable bits as side effects; restore the saved masks.
    for name in ['SensorIspControl0','SensorIspControl1','SensorIspControl3']:write(name,values[name])
    for name in ['ExposureAuto','GainAuto','BalanceWhiteAuto']:write(name,values[name])
    write('AcquisitionFrameRate',values['AcquisitionFrameRate'])
    write('AcquisitionFrameRateEnable',values['AcquisitionFrameRateEnable'])
    write('ImageCompressionMode',values['ImageCompressionMode'])
    ctx.idle()


def profile_diff(expected,actual):
    differences={}
    dynamic={'ExposureTime':'ExposureAuto','GainFactor':'GainAuto',
             'WhiteBalanceRed':'BalanceWhiteAuto','WhiteBalanceGreen':'BalanceWhiteAuto','WhiteBalanceBlue':'BalanceWhiteAuto'}
    for name,value in expected['values'].items():
        if name in dynamic and expected['values'][dynamic[name]]!=0:continue
        if actual['values'][name]!=value:differences[name]=[value,actual['values'][name]]
    for name,value in expected['tables'].items():
        if actual['tables'][name]!=value:differences[name]=[value,actual['tables'][name]]
    if expected.get('ae_thresholds')!=actual.get('ae_thresholds'):differences['ae_thresholds']=[expected.get('ae_thresholds'),actual.get('ae_thresholds')]
    return differences


def reconnect_reset(ctx):
    old=ctx.c;trace=old.trace
    try:
        old.write(BY_NAME['DeviceReset']['address'],1)
    except TimeoutError:
        # An ACK can be lost during reset; do not replay the reset command.
        ctx.evidence['reset_ack_received']=False
    else:ctx.evidence['reset_ack_received']=True
    finally:old.close()
    end=time.monotonic()+ctx.args.reset_timeout
    while time.monotonic()<end:
        candidate=Control(ctx.args.ip,ctx.args.bind,ctx.args.timeout)
        accepted=False
        try:
            if candidate.read(0xa00)!=0 or candidate.read(BY_NAME['DeviceResetBusy']['address']) or not candidate.read(BY_NAME['SensorInitialized']['address']):
                time.sleep(.2);continue
            candidate.write(0xa00,2)
            require(candidate.read(0xa00)==2,'Reconnect did not acquire ownership')
            trace.extend(candidate.trace);candidate.trace=trace;ctx.c=candidate
            accepted=True
            ctx.idle();wait_store(ctx)
            require(ctx.read('DeviceResetLastError')==0,'DeviceReset failed')
            return
        except (TimeoutError,OSError):
            if accepted:raise
            time.sleep(.2)
        except ProtocolError as exc:
            if exc.status not in {0x8006,0x8007}:raise
            time.sleep(.2)
        finally:
            if not accepted:candidate.close()
    raise TimeoutError('Failed to reconnect after DeviceReset')


def unlock_restore_default(ctx):
    variable=ctx.fixture.get('maintenance',{}).get('factory_password_env')
    if not variable or not os.environ.get(variable):
        raise Skip('Restoring protected startup Default=0 after reset requires maintenance.factory_password_env')
    password=os.environ[variable].encode('ascii')
    require(0<len(password)<32,'Factory password has invalid length')
    response=ctx.c.command(0x86,struct.pack('!I',0x12000)+password.ljust(32,b'\0'))
    require(response==struct.pack('!I',0x12000) and ctx.read('FactoryAccessUnlocked'), 'Factory access for restoring Default=0 failed')


def persistence_case(ctx,fmt,reboot=False):
    from .functional import evidence,assert_images
    if ctx.args.test_slot is None:
        raise Skip('Select an existing reserved Flash slot with --test-slot 1..7; its contents are backed up and restored')
    slot=ctx.args.test_slot
    require(ctx.read('UserSetStorageAvailable')==1,'Flash user-set storage unavailable')
    require(ctx.read('UserSetSavedMask') & (1<<slot),'Reserved slot must already be valid so it can be restored exactly')
    if fmt=='JPEG' and not ctx.read('SensorJpegAvailable'):raise Skip('JPEG unavailable')
    original=snapshot(ctx);old_selector=ctx.read('UserSetSelector');old_startup=ctx.read('UserSetDefault')
    slot_backup=None;slot_written=False;startup_changed=False;unlocked=False
    if reboot and old_startup==0:
        unlock_restore_default(ctx);unlocked=True
    try:
        operation(ctx,'UserSetLoad',slot);slot_backup=snapshot(ctx)
        (ctx.case_output/'slot-backup.json').write_text(json.dumps(slot_backup,indent=2)+'\n')
        # Prove that the Ethernet snapshot can be reconstructed before overwriting Flash.
        apply_snapshot(ctx,original)
        apply_snapshot(ctx,slot_backup)
        require(not profile_diff(slot_backup,snapshot(ctx)), 'Reserved-slot backup is not reconstructible; no Flash write performed')
        apply_snapshot(ctx,original)
        with ImageSession(ctx) as s:
            s.setup('YUV422_YUYVPacked' if fmt=='JPEG' else fmt,1)
            if fmt=='JPEG':s.set('SensorJpegQuality',8);s.set('ImageCompressionMode',1)
            before,frames_before=s.capture('saved-image')
            validate_bars(before,fmt,ctx.args.bar_tolerance)
            expected=snapshot(ctx)
            (ctx.case_output/'expected-profile.json').write_text(json.dumps(expected,indent=2)+'\n')
            slot_written=True
            operation(ctx,'UserSetSave',slot)
            # A different format, resolution and pattern expose no-op loads and
            # implementations that report saved values without restoring hardware.
            s.set('ImageCompressionMode',0);s.set('Width',320);s.set('Height',240)
            s.set('PixelFormat',FORMAT_VALUES['Mono8']);s.set('TestPattern',4)
            poison,frames_poison=s.capture('mutated-image')
            poisoned=snapshot(ctx)
            require(profile_diff(expected,poisoned),'Active profile failed to change before load')
            require(poison.shape!=before.shape or np.mean(np.abs(poison-before))>20,'Mutation did not change the actual image')
            if reboot:
                startup_changed=True
                ctx.c.write(BY_NAME['UserSetDefault']['address'],slot);wait_store(ctx)
                reconnect_reset(ctx)
                # Capture before any explicit load to test startup restoration itself.
            else:operation(ctx,'UserSetLoad',slot)
            actual=snapshot(ctx);difference=profile_diff(expected,actual)
            evidence(ctx,slot=slot,reboot=reboot,expected_profile=expected,restored_profile=actual,
                     profile_differences=difference,mutated_profile=poisoned,
                     before_frames=frames_before,mutation_frames=frames_poison,
                     reboot_scope='DeviceReset and Flash reload; not removal of electrical power' if reboot else 'Explicit Flash load')
            require(not difference,f'Flash profile mismatch: {difference}')
            restored,frames_restored=s.capture('loaded-image')
            validate_bars(restored,fmt,ctx.args.bar_tolerance)
            assert_images(ctx,'flash-restored',before,before,restored,
                          mae_limit=ctx.args.image_mae,pixel_limit=ctx.args.pixel_error,bad_fraction_limit=ctx.args.bad_fraction)
            evidence(ctx,restored_frames=frames_restored)
    finally:
        try:
            if startup_changed:
                if old_startup==0:unlock_restore_default(ctx)
                ctx.c.write(BY_NAME['UserSetDefault']['address'],old_startup);wait_store(ctx)
            if slot_backup is not None and slot_written:
                apply_snapshot(ctx,slot_backup);operation(ctx,'UserSetSave',slot)
                # Mutate and reload the backup too, so cleanup is verified through Flash.
                ctx.c.write(BY_NAME['TestPattern']['address'],4)
                operation(ctx,'UserSetLoad',slot)
                require(not profile_diff(slot_backup,snapshot(ctx)),'Reserved Flash slot backup restoration failed')
            apply_snapshot(ctx,original)
            require(not profile_diff(original,snapshot(ctx)),'Original live profile restoration failed')
            ctx.c.write(BY_NAME['UserSetSelector']['address'],old_selector)
            if unlocked:ctx.c.write(BY_NAME['FeatureAccessLock']['address'],1)
        except Exception as exc:raise CleanupError(str(exc)) from exc
    return ctx.evidence
