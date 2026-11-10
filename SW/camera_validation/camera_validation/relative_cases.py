"""Camera-baseline acceptance: fixed predictions, negative controls and A/B/A cycles."""
import numpy as np
from .cases import Skip, require
from .acquisition import ImageSession, FORMAT_VALUES
from .imaging import save_comparison, yuyv_luminance
from .relations import transform, noise_metrics, gain_metrics


def raw_response(ctx,feature):
    from .functional import evidence,assert_images
    with ImageSession(ctx) as s:
        s.setup('BayerBG10',1 if ctx.args.response_source=='pattern' else 0)
        if feature=='GainFactor+ExposureTime':
            settings=[{'GainFactor':16,'ExposureTime':80},{'GainFactor':32,'ExposureTime':160}]
            ratios=[1,4]
        else:
            values=[16,32,64] if feature=='GainFactor' else [80,160,320]
            settings=[{feature:value} for value in values];ratios=[1,2,4]
        for name,value in settings[0].items():s.set(name,value)
        a,baseline_frames=s.capture('baseline')
        repeat,_=s.capture('baseline-repeat')
        noise=noise_metrics(a,repeat)
        evidence(ctx,source=ctx.args.response_source,feature=feature,baseline_frames=baseline_frames,
                 baseline_noise=noise,oracle='Expected RAW = baseline * requested ratio; no fit to changed pixels')
        # Establish valid stimulus before interpreting the feature response.
        usable=(a>1023*.06)&(a<1023*.9/4)
        if usable.mean()<.2 or any(usable[dy::2,dx::2].mean()<.1 for dy,dx in [(0,0),(0,1),(1,0),(1,1)]):
            raise Skip('Stimulus is too dark/saturated for RAW gain prediction; use a moderately illuminated scene')
        require(noise['mae']<=8 and noise['p99']<=40,'Baseline is too unstable for a valid gain measurement')
        limits={'mae_limit':max(8,noise['mae']*3),'pixel_limit':max(24,noise['p99']*3),'bad_fraction_limit':.02}
        assert_images(ctx,'noise-control',a,a,repeat,**limits)
        for configuration,ratio in zip(settings[1:],ratios[1:]):
            for name,value in configuration.items():s.set(name,value)
            actual,frames=s.capture(f'ratio-{ratio:g}')
            expected,mask,phases=gain_metrics(a,actual,ratio)
            evidence(ctx,**{f'ratio-{ratio:g}':{'requested_ratio':ratio,'phase_measurements':phases,'frames':frames}})
            # Retain expected/actual images even if the channel-ratio assertion fails.
            files=save_comparison(ctx.case_output,f'prediction-{ratio:g}',a,expected,actual)
            evidence(ctx,**{f'prediction-{ratio:g}':{'images':files}})
            for phase in phases:
                require(phase['usable_fraction']>=.1,'Too few unsaturated pixels in a Bayer phase')
                require(ratio*.8<=phase['median_ratio']<=ratio*1.2,
                        f'Wrong RAW response at phase {phase["phase"]}: {phase["median_ratio"]} != {ratio}')
            assert_images(ctx,f'raw-{ratio:g}',a,expected,actual,mask=mask,
                          mae_limit=40,pixel_limit=100,bad_fraction_limit=.1)
            measured_change=float(np.mean(np.abs(actual[mask]-a[mask])))
            require(measured_change>max(12,noise['mae']*6),'Response does not exceed measured baseline noise')
        for name,value in settings[0].items():s.set(name,value)
        restored,_=s.capture('restored')
        assert_images(ctx,'restore',a,a,restored,**limits)
    return ctx.evidence


def effect_relation(ctx,kind):
    from .functional import evidence,assert_images
    # These effect bits are defined by the project's OV5640_EFFECT interface.
    setting={'negative':0x40,'grayscale':0x20}[kind]
    with ImageSession(ctx) as s:
        if kind=='grayscale':
            # Saturated RGB bars clip channels, so weighted clipped RGB cannot
            # recover the sensor's Y. Predict neutral chroma from a raw YUYV
            # baseline captured before applying the effect, never changed data.
            s.setup('YUV422_YUYVPacked',1)
            _,luma_frames=s.capture('luminance-reference')
            width,height=s.get('Width'),s.get('Height')
            planes=[yuyv_luminance((ctx.case_output/frame['raw']).read_bytes(),width,height)
                    for frame in luma_frames]
            expected=np.repeat(np.median(np.stack(planes),axis=0)[:,:,None],3,axis=2)
            model={'kind':'grayscale-neutral-chroma','source':'Uncompressed full-range YUYV luminance before effect'}
            evidence(ctx,luminance_reference_frames=luma_frames)
            s.set('PixelFormat',FORMAT_VALUES['RGB8'])
        else:
            s.setup('RGB8',1)
            model={'kind':kind}
        a,frames=s.capture('baseline');repeat,_=s.capture('baseline-repeat')
        assert_images(ctx,'noise-control',a,a,repeat)
        if kind!='grayscale':expected=transform(a,model)
        require(np.mean(np.abs(expected-a))>20,'Baseline cannot discriminate this effect')
        # Stage first: actual image must stay unchanged until Apply.
        s.set('EffectEnableBits',setting)
        staged,_=s.capture('staged')
        assert_images(ctx,'staging-isolation',a,a,staged)
        s.command('EffectApply');actual,applied=s.capture('applied')
        evidence(ctx,oracle=model,source='sensor color bars',baseline_frames=frames,applied_frames=applied,
                 minimum_effect_mae=20,baseline_noise=noise_metrics(a,repeat))
        assert_images(ctx,'effect',a,expected,actual,mae_limit=12,pixel_limit=40,bad_fraction_limit=.04)
        require(np.mean(np.abs(actual-a))>20,'Effect command produced no observable pixel change')
        s.set('EffectEnableBits',0);s.command('EffectApply')
        restored,_=s.capture('restored');assert_images(ctx,'restore',a,a,restored)
    return ctx.evidence
