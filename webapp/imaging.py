"""Preview conversion; measurements and camera settings remain untouched."""
import io
import numpy as np
from PIL import Image

def jpeg_frame(data,width,height,pf,compression=0):
    if compression==1:
        im=Image.open(io.BytesIO(data));im.load()
    elif pf in (0x02180014,0x02180015):
        im=Image.frombytes('RGB',(width,height),data)
        if pf==0x02180015:im=Image.fromarray(np.asarray(im)[:,:,::-1])
    elif pf in (0x02100032,0x0210001f,0x82100004,0x82100005):
        a=np.frombuffer(data,np.uint8).reshape(height,width//2,4).astype(np.float32)
        if pf==0x02100032:y0,u,y1,v=np.moveaxis(a,-1,0)
        elif pf==0x0210001f:u,y0,v,y1=np.moveaxis(a,-1,0)
        elif pf==0x82100004:y0,v,y1,u=np.moveaxis(a,-1,0)
        else:v,y0,u,y1=np.moveaxis(a,-1,0)
        y=np.empty((height,width),np.float32);y[:,::2]=y0;y[:,1::2]=y1
        u=np.repeat(u,2,axis=1)-128;v=np.repeat(v,2,axis=1)-128;c=1.164*(y-16)
        im=Image.fromarray(np.stack((c+1.596*v,c-.392*u-.813*v,c+2.017*u),axis=-1).clip(0,255).astype(np.uint8))
    elif pf in (0x01080001,0x0108000b):
        # Bayer is displayed as intensity; no synthetic color interpretation.
        im=Image.frombytes('L',(width,height),data)
    elif pf==0x0110000f:
        im=Image.fromarray((np.frombuffer(data,'<u2').reshape(height,width)>>2).clip(0,255).astype(np.uint8))
    else:
        raise ValueError(f'Preview conversion unavailable for pixel format 0x{pf:08x}. Select RGB8, BGR8, Mono8 or YUV422 for preview.')
    im.thumbnail((1440,1080));out=io.BytesIO();im.save(out,'JPEG',quality=82)
    return out.getvalue()
