"""Expected images derived from a baseline and a predetermined transformation."""
import numpy as np


def transform(baseline, model):
    """Never fit a transform to the changed frame: it is not an oracle input."""
    a=np.asarray(baseline,dtype=float)
    kind=model['kind'];maximum=float(model.get('maximum',255))
    if kind=='identity':out=a.copy()
    elif kind=='flip':
        out=a
        if 'X' in model['axes']:out=out[:,::-1]
        if 'Y' in model['axes']:out=out[::-1]
    elif kind=='affine':out=a*np.asarray(model.get('scale',1))+np.asarray(model.get('offset',0))
    elif kind=='negative':out=maximum-a
    elif kind=='grayscale':
        if a.ndim!=3 or a.shape[2]!=3:raise ValueError('Grayscale relation requires RGB')
        out=np.repeat((a @ np.array([.299,.587,.114]))[:,:,None],3,axis=2)
    elif kind=='lut':
        table=np.asarray(model['values'],dtype=float)
        if table.ndim!=1 or len(table)<2:raise ValueError('LUT must be one-dimensional')
        positions=np.asarray(model.get('positions',np.linspace(0,maximum,len(table))),dtype=float)
        if positions.shape!=table.shape or not np.all(np.diff(positions)>0):raise ValueError('Invalid LUT positions')
        out=np.interp(a,positions,table)
    else:raise ValueError(f'Unsupported baseline relation: {kind}')
    if not np.isfinite(out).all():raise ValueError('Expected image contains non-finite values')
    return np.clip(out,0,maximum)


def noise_metrics(a,repeat):
    delta=np.abs(a.astype(float)-repeat.astype(float))
    return {'mae':float(delta.mean()),'p99':float(np.quantile(delta,.99))}


def gain_metrics(baseline,changed,ratio,maximum=1023):
    """Evaluate a fixed physical gain ratio on usable pixels of each Bayer phase."""
    a=np.asarray(baseline,float);b=np.asarray(changed,float)
    mask=(a>maximum*.06)&(a<maximum*.38)&(b<maximum*.9)
    expected=np.clip(a*ratio,0,maximum)
    phases=[]
    for dy,dx in [(0,0),(0,1),(1,0),(1,1)]:
        usable=mask[dy::2,dx::2]
        av=a[dy::2,dx::2][usable];bv=b[dy::2,dx::2][usable]
        phases.append({'phase':[dy,dx],'usable_fraction':float(usable.mean()),
                       'median_ratio':float(np.median(bv/av)) if av.size else None})
    return expected,mask,phases
