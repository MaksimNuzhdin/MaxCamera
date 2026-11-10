"""Functional cases: expected pixels, reversible image changes and persisted profiles."""
import hashlib
import json
from pathlib import Path
import numpy as np
from .cases import Case, FEATURES, FORMATS, BY_NAME, Skip, require
from .acquisition import ImageSession
from .imaging import BARS, bar_means, validate_bars, compare, save_comparison


def evidence(ctx,**values):
    ctx.evidence.update(values)
    (ctx.case_output/'assertions.json').write_text(json.dumps(ctx.evidence,indent=2)+'\n')


def assert_images(ctx,label,baseline,expected,actual,mask=None,**thresholds):
    if mask is not None:
        require(mask.shape==actual.shape[:2] and np.any(mask),'Invalid or empty comparison mask')
        np.save(ctx.case_output/f'{label}-mask.npy',mask,allow_pickle=False)
        metrics=compare(expected[mask],actual[mask],**thresholds)
        metrics['compared_fraction']=float(np.mean(mask))
    else:metrics=compare(expected,actual,**thresholds)
    files=save_comparison(ctx.case_output,label,baseline,expected,actual)
    evidence(ctx,**{label:{'metrics':metrics,'images':files}})
    require(metrics['passed'],f'{label}: expected-image mismatch: {metrics}')
    return metrics


def expected_bars(shape):
    h,w=shape[:2];expected=np.empty((h,w,3),np.float32)
    for i,color in enumerate(BARS):expected[:,i*w//8:(i+1)*w//8]=color*255
    return expected


def color_bars(ctx,fmt,width=640,height=480):
    with ImageSession(ctx) as s:
        s.setup(fmt,1,width,height)
        actual,frames=s.capture('bars')
        evidence(ctx,frames=frames,oracle='Independent white/yellow/cyan/green/magenta/red/blue/black levels')
        # Save previews before assertions, including failing buffers.
        expected=expected_bars(actual.shape)
        preview=actual
        if fmt.startswith('Bayer'):
            scale=1023 if fmt!='BayerBG8' else 255
            plane=actual*255/scale
            preview=np.repeat(plane[:,:,None],3,axis=2)
        elif actual.ndim==2:
            expected=expected @ np.array([.299,.587,.114]);preview=actual
        files=save_comparison(ctx.case_output,'bars',preview,expected,preview)
        evidence(ctx,bar_images=files)
        metrics=validate_bars(actual,fmt,ctx.args.bar_tolerance)
        evidence(ctx,bar_oracle=metrics)
    return ctx.evidence


def black(ctx):
    with ImageSession(ctx) as s:
        s.setup('Mono8',1)
        before,_=s.capture('before')
        validate_bars(before,'Mono8',ctx.args.bar_tolerance)
        s.set('TestPattern',4)
        actual,frames=s.capture('black')
        assert_images(ctx,'black',before,np.zeros_like(actual),actual,
                      mae_limit=ctx.args.black_max,pixel_limit=ctx.args.black_max,bad_fraction_limit=.01)
        require(np.mean(np.abs(before-actual))>40,'Black command did not change image')
        s.set('TestPattern',1)
        restored,_=s.capture('restored')
        assert_images(ctx,'restored',before,before,restored)
        evidence(ctx,frames=frames)
    return ctx.evidence


def orientation(ctx,axes):
    with ImageSession(ctx) as s:
        s.setup('RGB8',0)
        a,_=s.capture('baseline');a2,_=s.capture('baseline-repeat')
        assert_images(ctx,'noise-control',a,a,a2)
        expected=a
        if 'X' in axes:expected=expected[:,::-1]
        if 'Y' in axes:expected=expected[::-1,:]
        if np.mean(np.abs(expected-a))<=20:raise Skip('Current scene is too symmetric or flat to discriminate orientation')
        if 'X' in axes:s.set('ReverseX',1)
        if 'Y' in axes:s.set('ReverseY',1)
        actual,frames=s.capture('transformed')
        assert_images(ctx,'orientation',a,expected,actual,mae_limit=ctx.args.image_mae,
                      pixel_limit=ctx.args.pixel_error,bad_fraction_limit=ctx.args.bad_fraction)
        require(np.mean(np.abs(actual-a))>20,'Setting produced no observable orientation change')
        s.set('ReverseX',0);s.set('ReverseY',0)
        restored,_=s.capture('restored');assert_images(ctx,'restore',a,a,restored)
        evidence(ctx,frames=frames,transform=axes)
    return ctx.evidence


def manual_response(ctx,name):
    with ImageSession(ctx) as s:
        s.setup('RGB8',1 if ctx.args.response_source=='pattern' else 0)
        # ExposureTime GVCP words are sensor lines; GainFactor uses 1/16.
        values=(80,160) if name=='ExposureTime' else (16,32)
        s.set(name,values[0]);a,_=s.capture('low');a2,_=s.capture('low-repeat')
        assert_images(ctx,'noise-control',a,a,a2)
        s.set(name,values[1]);b,_=s.capture('high')
        luma=lambda x:x @ np.array([.299,.587,.114])
        la,lb=luma(a),luma(b)
        mask=(la>8)&(la<100)&(lb<230)
        if np.mean(mask)<=.2:raise Skip('Current scene has too few unsaturated pixels to establish response')
        ratio=float(np.median(lb[mask]/la[mask]))
        # ISP gamma prevents an unjustified exact 2x output-intensity assertion.
        evidence(ctx,response_ratio=ratio,expected_ratio_interval=[1.15,3.0],usable_fraction=float(mask.mean()))
        files=save_comparison(ctx.case_output,'response',a,a*1.15,b);evidence(ctx,response_images=files,
             expected_model='Expected preview is the minimum 1.15x response bound, not an exact ISP prediction')
        require(1.15<=ratio<=3.0,'Doubling manual control did not produce expected luminance response')
        s.set(name,values[0]);restored,_=s.capture('restored');assert_images(ctx,'restore',a,a,restored)
    return ctx.evidence


def white_balance(ctx,channel):
    index={'Red':0,'Green':1,'Blue':2}[channel]
    with ImageSession(ctx) as s:
        s.setup('RGB8',1 if ctx.args.response_source=='pattern' else 0);a,_=s.capture('unity');a2,_=s.capture('repeat')
        assert_images(ctx,'noise-control',a,a,a2)
        s.set('WhiteBalance'+channel,1536);b,_=s.capture('gain')
        mask=np.all((a>15)&(a<150)&(b<230),axis=2)
        if mask.mean()<=.2:raise Skip('Current scene lacks enough usable color-channel pixels')
        ratios=np.median(b[mask]/a[mask],axis=0)
        files=save_comparison(ctx.case_output,'white-balance',a,a,b)
        evidence(ctx,channel=channel,channel_ratios=ratios.tolist(),images=files,
                 expected_model='Selected color channel must increase more than the other channels; image comparison previews show baseline, not an exact ISP prediction')
        require(ratios[index]>1.12,'Selected channel did not increase')
        require(ratios[index]>max(ratios[i] for i in range(3) if i!=index)+.06,'Wrong color channel changed')
        s.set('WhiteBalance'+channel,1024);restored,_=s.capture('restored')
        assert_images(ctx,'restore',a,a,restored)
    return ctx.evidence


def jpeg(ctx):
    if not ctx.read('SensorJpegAvailable'):raise Skip('JPEG unavailable on this production image')
    with ImageSession(ctx) as s:
        s.setup('YUV422_YUYVPacked',1)
        reference,_=s.capture('uncompressed')
        s.set('ImageCompressionMode',1);s.set('SensorJpegQuality',4)
        actual,frames=s.capture('jpeg-high-quality')
        evidence(ctx,frames=frames,jpeg_bar_oracle=validate_bars(actual,'JPEG',ctx.args.bar_tolerance))
        # Decode actual encoded bytes independently; compare against the previously
        # received uncompressed reference rather than just checking JPEG markers.
        assert_images(ctx,'jpeg-decoded',reference,reference,actual,mae_limit=12,pixel_limit=55,bad_fraction_limit=.08)
        sizes_high=[f['bytes'] for f in frames]
        s.set('SensorJpegQuality',40);lower,frames_low=s.capture('jpeg-low-quality')
        validate_bars(lower,'JPEG',ctx.args.bar_tolerance)
        sizes_low=[f['bytes'] for f in frames_low]
        evidence(ctx,quality_sizes={'scale4':sizes_high,'scale40':sizes_low})
        require(np.median(sizes_low)<np.median(sizes_high),'JPEG quality setting did not reduce actual encoded size')
    return ctx.evidence


def profile_case(ctx,name):
    """Use a predetermined baseline relation, with golden files as an optional alternative."""
    profile=ctx.fixture.get('profiles',{}).get(name)
    if name.startswith('Vcm') and not ctx.read('SensorVcmAvailable'):raise Skip('No qualified VCM actuator')
    if not profile:raise Skip(f'Feature profile {name!r} with a baseline relation or optional golden image is required')
    oracle=profile.get('oracle')
    if oracle is None:require(profile.get('provenance'),'Golden image provenance is required')
    require(set(profile.get('tables',{})).issubset(profile.get('baseline_tables',{})), 'Changed tables require explicit baseline_tables')
    require(set(profile['settings']).issubset(profile.get('baseline',{})), 'Each changed setting needs an explicit baseline for A/B/A validation')
    expected=None;digest=None
    if oracle is None:
        path=(ctx.fixture_root/profile['expected']).resolve()
        digest=hashlib.sha256(path.read_bytes()).hexdigest()
        require(digest==profile['expected_sha256'],'Expected image digest mismatch')
        expected=np.load(path,allow_pickle=False)
    with ImageSession(ctx) as s:
        s.setup(profile.get('format','RGB8'),profile.get('pattern',0))
        def configure(settings):
            for feature,value in settings.items():s.set(feature,value)
        configure(profile.get('baseline',{}))
        for kind,values in profile.get('baseline_tables',{}).items():s.table(kind,values)
        commands=profile.get('apply',[])
        for command in commands:s.command(command)
        a,_=s.capture('baseline');a2,_=s.capture('baseline-repeat')
        assert_images(ctx,'noise-control',a,a,a2,**profile.get('noise_limits',{}))
        if oracle is not None:
            from .relations import transform
            # Only the baseline and predeclared model define expected pixels.
            expected=transform(a,oracle)
        configure(profile['settings'])
        for kind,values in profile.get('tables',{}).items():s.table(kind,values)
        if commands:
            staged,_=s.capture('staged-before-apply')
            assert_images(ctx,'staging-isolation',a,a,staged,**profile.get('noise_limits',{}))
        for command in commands:s.command(command)
        actual,frames=s.capture('applied',settle=profile.get('settle_frames',ctx.args.settle_frames))
        evidence(ctx,oracle=oracle or {'kind':'golden','provenance':profile['provenance']},expected_sha256=digest,frames=frames)
        if expected.ndim==actual.ndim+1:
            require(len(expected)==len(frames),'Golden sequence frame count mismatch')
            for i,(target,frame) in enumerate(zip(expected,frames)):
                observed=np.load(ctx.case_output/frame['array'],allow_pickle=False)
                assert_images(ctx,f'expected-{i}',a,target,observed,**profile.get('limits',{}))
        else:
            assert_images(ctx,'predicted' if oracle is not None else 'golden',a,expected,actual,**profile.get('limits',{}))
        minimum_change=profile.get('minimum_change_mae',2)
        require(np.mean(np.abs(actual-a))>=minimum_change,'Feature produced no observable image effect')
        configure(profile['baseline'])
        if profile.get('tables'):
            require(set(profile['tables']).issubset(profile.get('baseline_tables',{})), 'Modified tables require baseline_tables')
            for kind,values in profile['baseline_tables'].items():s.table(kind,values)
        for command in commands:s.command(command)
        restored,_=s.capture('restored')
        assert_images(ctx,'restore',a,a,restored,**profile.get('noise_limits',{}))
    return ctx.evidence


def functional_cases():
    result=[]
    for f in FORMATS:
        result.append(Case('pixels.bars.'+f['name'],f['name'],'functional-image','Decode payload and compare channel levels, packing and Bayer phase with independent expected color bars',lambda c,n=f['name']:color_bars(c,n)))
    result += [Case('pixels.black','TestPattern','functional-image','Compare black pixels to zero image and verify reversible change',black),
               Case('pixels.jpeg','ImageCompressionMode','functional-image','Decode JPEG, compare pixels to uncompressed reference and check quality changes encoded size',jpeg)]
    for axes in ['X','Y','XY']:
        result.append(Case('pixels.orientation.'+axes,'Reverse'+axes,'functional-image','Compare real pixels with an independently mirrored/flipped baseline and restore',lambda c,a=axes:orientation(c,a),tuple('Reverse'+a for a in axes)))
    for name in ['ExposureTime','GainFactor']:
        result.append(Case('pixels.response.'+name,name,'functional-image','Double manual control and assert scene luminance response with A/B/A restoration',lambda c,n=name:manual_response(c,n)))
    for channel in ['Red','Green','Blue']:
        result.append(Case('pixels.balance.'+channel,'WhiteBalance'+channel,'functional-image','Verify measured channel-selective image response and restoration',lambda c,ch=channel:white_balance(c,ch)))
    # Feature profiles can define camera-baseline relations without reference files.
    names=['ExposureAuto','GainAuto','BalanceWhiteAuto','AcquisitionFrameRate','SensorBanding',
           'ExposureLimit50Hz','ExposureLimit60Hz','AutoGainLimit','SensorNightMode',
           'AutoExposureTarget','SensorSharpness','SensorDenoise','SensorSharpnessAuto','SensorDenoiseAuto',
           'SensorIspControl0','SensorIspControl1','SensorIspControl3','SensorTestPatternStyle',
           'SensorTestRolling','SensorTestTransparent','SensorTestMonochrome',
           'GammaApply','LensCorrectionApply','OverlayApply','EffectApply','VcmApply','SensorFeatureApply']
    names += [f['name'] for f in FEATURES if f['group'] in {'ImageEffects','ImageCalibration','SensorOverlay','SensorActuator'} and f['access']=='RW']
    for name in dict.fromkeys(names):
        result.append(Case('pixels.fixture.'+name,name,'functional-image','Compare pixels with a predetermined baseline transform or optional independent golden image',lambda c,n=name:profile_case(c,n)))
    from .relative_cases import raw_response, effect_relation
    for feature in ['GainFactor','ExposureTime','GainFactor+ExposureTime']:
        result.append(Case('pixels.raw_response.'+feature,feature,'functional-image','Predict RAW pixels from baseline at 2x/4x control, reject no-op and wrong slopes, then restore',lambda c,n=feature:raw_response(c,n),tuple(feature.split('+'))))
    for effect in ['negative','grayscale']:
        result.append(Case('pixels.effect.'+effect,'EffectEnableBits','functional-image','Predict effect pixels from the camera baseline; check staging, change and restoration',lambda c,n=effect:effect_relation(c,n),('EffectEnableBits','EffectApply')))
    result.append(Case('pixels.frame_rate','AcquisitionFrameRate','functional-image','Measure actual delivered frame timestamps at 1/2/1 FPS',frame_rate,('AcquisitionFrameRate','AcquisitionFrameRateEnable')))
    from .persistence import persistence_case
    for name in ['Mono8','RGB8','BayerBG10','BayerBG10p','JPEG']:
        for reboot in [False,True]:
            result.append(Case('flash.'+('reboot.' if reboot else 'load.')+name,'UserSetSave/UserSetLoad','functional-persistence',
                               'Save actual profile to Flash, mutate active state, '+('reset/reconnect, ' if reboot else '')+'load and compare restored settings, tables and real images',
                               lambda c,n=name,r=reboot:persistence_case(c,n,r),('UserSetSave','UserSetLoad','UserSetDefault','DeviceReset') if reboot else ('UserSetSave','UserSetLoad')))
    return result


def frame_rate(ctx):
    with ImageSession(ctx) as s:
        s.setup('Mono8',1);s.set('AcquisitionFrameRateEnable',1)
        measurements=[]
        frequency=(ctx.c.read(0x93c)<<32)|ctx.c.read(0x940)
        require(frequency>0,'Invalid timestamp tick frequency')
        for rate in [1,2,1]:
            s.set('AcquisitionFrameRate',rate*1000)
            image,frames=s.capture(f'rate-{rate}')
            validate_bars(image,'Mono8',ctx.args.bar_tolerance)
            ticks=np.diff([f['timestamp'] for f in frames]);measured=float(frequency/np.median(ticks))
            evidence(ctx,frame_rate_measurements=measurements)
            arrival_fps=float(1/np.median(np.diff([f['received_monotonic'] for f in frames])))
            measurements.append({'requested_fps':rate,'timestamp_fps':measured,'arrival_fps':arrival_fps,'frames':frames})
            require(abs(arrival_fps-rate)<=rate*.2,f'Actual arrival cadence {arrival_fps} does not match {rate}')
            require(abs(measured-rate)<=rate*.15,f'Actual frame cadence {measured} does not match {rate}')
        evidence(ctx,frame_rate_measurements=measurements)
    return ctx.evidence
