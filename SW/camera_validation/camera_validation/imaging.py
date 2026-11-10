"""Independent payload decoding and expected-image oracles (no firmware code import)."""
from io import BytesIO
import hashlib
import numpy as np
from PIL import Image

BARS = np.array([[1,1,1],[1,1,0],[0,1,1],[0,1,0],
                 [1,0,1],[1,0,0],[0,0,1],[0,0,0]], dtype=float)


def yuyv_luminance(data, width, height):
    """Recover the transmitted full-range Y plane before RGB clipping."""
    if width <= 0 or height <= 0 or width % 2 or len(data) != width*height*2:
        raise ValueError('Invalid packed YUYV luminance dimensions or payload')
    return np.frombuffer(data, np.uint8)[::2].reshape(height, width).copy()


def decode(data, name, width, height):
    """Return RGB on a 0..255 scale, or a native-bit-depth Bayer/Mono plane."""
    b=np.frombuffer(data,np.uint8)
    name={'YUV422_8':'YUV422_YUYVPacked','YUV422_8_UYVY':'YUV422Packed'}.get(name,name)
    if name=='JPEG':
        with Image.open(BytesIO(data)) as image:
            image.load()
            if image.size!=(width,height):raise AssertionError('JPEG dimensions mismatch')
            return np.asarray(image.convert('RGB')).copy()
    if name=='Mono8':return b.reshape(height,width).copy()
    if name=='BayerBG8':return b.reshape(height,width).copy()
    if name=='BayerBG10':
        v=np.frombuffer(data,'<u2').reshape(height,width)
        if np.any(v>1023):raise AssertionError('RAW10 contains nonzero reserved bits')
        return v.copy()
    if name=='BayerBG10p':
        if len(data)*8!=width*height*10:raise AssertionError('RAW10p length mismatch')
        bits=np.unpackbits(b,bitorder='little').reshape(-1,10)
        return (bits.astype(np.uint16)*(1<<np.arange(10,dtype=np.uint16))).sum(axis=1).reshape(height,width)
    if name in {'RGB8','BGR8'}:
        v=b.reshape(height,width,3)
        return (v[:,:,::-1] if name=='BGR8' else v).copy()
    if name in {'RGB565p','RGB555_16BE','RGB444_16BE'}:
        v=np.frombuffer(data,'<u2' if name=='RGB565p' else '>u2').reshape(height,width).astype(np.uint32)
        if name=='RGB565p':return np.stack([(v&31)*255/31,((v>>5)&63)*255/63,((v>>11)&31)*255/31],axis=-1)
        if name=='RGB555_16BE':
            if np.any(v&0x8000):raise AssertionError('RGB555 reserved bit is set')
            return np.stack([((v>>10)&31)*255/31,((v>>5)&31)*255/31,(v&31)*255/31],axis=-1)
        if np.any(v&0xf000):raise AssertionError('RGB444 reserved bits are set')
        return np.stack([((v>>8)&15)*17,((v>>4)&15)*17,(v&15)*17],axis=-1)
    if name=='YUV8_UYV':
        v=b.reshape(height,width,3);y,u,cr=v[:,:,1],v[:,:,0],v[:,:,2]
    elif name=='YUV420Legacy':
        if height%2:raise AssertionError('YUV420 requires even height')
        v=b.reshape(height,width//2,3);y=v[:,:,1:].reshape(height,width)
        u=np.repeat(np.repeat(v[0::2,:,0],2,axis=0),2,axis=1)
        cr=np.repeat(np.repeat(v[1::2,:,0],2,axis=0),2,axis=1)
    else:
        v=b.reshape(height,width//2,4)
        order={'YUV422Packed':(1,3,0,2),'YUV422_YUYVPacked':(0,2,1,3),
               'YUV422_YVYU':(0,2,3,1),'YUV422_VYUY':(1,3,2,0)}[name]
        y=v[:,:,list(order[:2])].reshape(height,width)
        u=np.repeat(v[:,:,order[2]],2,axis=1);cr=np.repeat(v[:,:,order[3]],2,axis=1)
    y=y.astype(float);u=u.astype(float)-128;cr=cr.astype(float)-128
    return np.clip(np.stack([y+1.402*cr,y-.344136*u-.714136*cr,y+1.772*u],axis=-1),0,255)


def bar_means(image):
    h,w=image.shape[:2]
    return np.array([image[h//8:7*h//8,int((i+.3)*w/8):int((i+.7)*w/8)].mean(axis=(0,1)) for i in range(8)])


def validate_bars(image, name, tolerance=50):
    """Check expected channel order and levels; edges are excluded, not realigned."""
    if name.startswith('Bayer'):
        expected_order='BGGR';channels={'R':0,'G':1,'B':2};observed=[]
        scale=1023 if name!='BayerBG8' else 255
        for (dy,dx),channel in zip([(0,0),(0,1),(1,0),(1,1)],expected_order):
            levels=bar_means(image[dy::2,dx::2]).astype(float)*255/scale
            require=np.max(np.abs(levels-BARS[:,channels[channel]]*255))<=tolerance
            if not require:raise AssertionError(f'Bayer {dy},{dx} expected {channel}: {levels.tolist()}')
            observed.append(levels.tolist())
        return {'bayer_phase':'BGGR','measured_levels':observed,'max_level_error':tolerance}
    levels=bar_means(image)
    if image.ndim==2:
        # Full-range YUV luma of the independently specified RGB bar sequence.
        expected=BARS @ np.array([.299,.587,.114])*255
    else:expected=BARS*255
    error=np.abs(levels-expected)
    if np.max(error)>tolerance:
        raise AssertionError(f'Color-bar oracle failed: max error {error.max():.2f} > {tolerance}; measured={levels.tolist()}')
    return {'expected_levels':expected.tolist(),'measured_levels':levels.tolist(),'maximum_error':float(error.max()),'tolerance':tolerance}


def compare(expected, actual, *, mae_limit=8, pixel_limit=24, bad_fraction_limit=.02):
    if expected.shape!=actual.shape:raise AssertionError(f'Image shapes differ: {expected.shape} vs {actual.shape}')
    delta=np.abs(expected.astype(float)-actual.astype(float))
    pixel_delta=delta.max(axis=2) if delta.ndim==3 else delta
    mse=float(np.mean(delta**2));mae=float(delta.mean())
    metrics={'mae':mae,'rmse':mse**.5,'maximum_error':float(delta.max()),
             'bad_pixel_fraction':float(np.mean(pixel_delta>pixel_limit)),
             'mae_limit':mae_limit,'pixel_limit':pixel_limit,'bad_fraction_limit':bad_fraction_limit}
    metrics['passed']=mae<=mae_limit and metrics['bad_pixel_fraction']<=bad_fraction_limit
    return metrics


def save_comparison(directory, label, baseline, expected, actual):
    """Retain lossless native arrays as well as human-readable previews."""
    directory.mkdir(parents=True,exist_ok=True)
    artifacts=[]
    peak=max(float(np.max(value)) for value in [baseline,expected,actual])
    display_max=1023 if 255<peak<=1023 else max(255,peak)
    delta=np.abs(actual.astype(float)-expected.astype(float))
    for name,value in [('baseline',baseline),('expected',expected),('actual',actual),('difference',delta)]:
        native=directory/f'{label}-{name}.npy';np.save(native,value,allow_pickle=False)
        view=np.clip(value*255/display_max,0,255).astype(np.uint8)
        if name=='difference':view=np.clip(value*255/display_max*4,0,255).astype(np.uint8)
        png=directory/f'{label}-{name}.png';Image.fromarray(view).save(png)
        artifacts.append({'name':name,'path':png.name,'array':native.name,
                          'sha256':hashlib.sha256(native.read_bytes()).hexdigest(),'display_max':display_max})
    return artifacts
