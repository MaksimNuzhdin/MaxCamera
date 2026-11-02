#include "GigeRemapControl.h"
#include "GigeFeatureRegisters.h"
#include "GigeVisionRegisters.h"
#include <string.h>
int GigeRemapControl_Init(GigeRemapControl *s,uint32_t *a,uint32_t *b,size_t n)
{
    if(!s)return -1;
    memset(s,0,sizeof(*s));s->width=640;s->height=480;
    return GigeRemapMap_Init(&s->map,a,b,n);
}
int GigeRemapControl_Address(uint32_t a)
{return !(a&3U) && a>=GIGE_F_REMAP_AVAILABLE && a<=GIGE_F_REMAP_HARDWARE_STATUS;}
uint16_t GigeRemapControl_Read(const GigeRemapControl *s,uint32_t a,uint32_t *v,int available)
{
    if(!s || !v)return GIGE_STATUS_INVALID_PARAM;
    if(a==GIGE_F_REMAP_AVAILABLE){*v=available!=0;return 0;}
    if(!available)return GIGE_STATUS_NOT_IMPLEMENTED;
    switch(a) {
    case GIGE_F_REMAP_ENABLE:*v=s->enabled;break;
    case GIGE_F_REMAP_WIDTH:*v=s->width;break;
    case GIGE_F_REMAP_HEIGHT:*v=s->height;break;
    case GIGE_F_REMAP_EXPECTED_CRC:*v=s->expected_crc;break;
    case GIGE_F_REMAP_INDEX:*v=s->index;break;
    case GIGE_F_REMAP_DATA:
        if(s->index>=s->map.uploaded)return GIGE_STATUS_INVALID_PARAM;
        *v=s->map.bank[s->map.staging_bank][s->index];break;
    case GIGE_F_REMAP_STATE:*v=(uint32_t)s->map.state;break;
    case GIGE_F_REMAP_UPLOADED:*v=s->map.uploaded;break;
    case GIGE_F_REMAP_CHECKED:*v=s->map.checked;break;
    case GIGE_F_REMAP_ERROR:*v=s->map.error;break;
    case GIGE_F_REMAP_HARDWARE_STATUS:*v=s->hardware_status;break;
    default:return GIGE_STATUS_INVALID_ADDRESS;
    }
    return 0;
}
uint16_t GigeRemapControl_Write(GigeRemapControl *s,uint32_t a,uint32_t v,int available,
    int busy,uint32_t ow,uint32_t oh,uint64_t geometry,int color_route)
{
    if(!s || !GigeRemapControl_Address(a))return GIGE_STATUS_INVALID_ADDRESS;
    if(!available)return GIGE_STATUS_NOT_IMPLEMENTED;
    if(busy)return GIGE_STATUS_BUSY;
    if(a==GIGE_F_REMAP_ENABLE) {
        if(v>1)return GIGE_STATUS_INVALID_PARAM;
        if(v && (!color_route || s->map.state!=GIGE_REMAP_READY ||
                 s->map.cols!=ow || s->map.rows!=oh || s->geometry!=geometry))return GIGE_STATUS_INVALID_PARAM;
        s->enabled=v;return 0;
    }
    if(a==GIGE_F_REMAP_AVAILABLE || (a>=GIGE_F_REMAP_STATE && a<=GIGE_F_REMAP_HARDWARE_STATUS))return GIGE_STATUS_WRITE_PROTECT;
    if(s->enabled)return GIGE_STATUS_BUSY;
    if(a==GIGE_F_REMAP_ABORT) {
        if(v!=1)return GIGE_STATUS_INVALID_PARAM;
        GigeRemapMap_Abort(&s->map);return 0;
    }
    if(a==GIGE_F_REMAP_BEGIN) {
        if(v!=1 || !color_route || s->width!=ow || s->height!=oh)return GIGE_STATUS_INVALID_PARAM;
        if(GigeRemapMap_Begin(&s->map,s->height,s->width,s->expected_crc,0))return GIGE_STATUS_INVALID_PARAM;
        s->geometry=geometry;s->index=0;s->hardware_status=0;return 0;
    }
    if(a==GIGE_F_REMAP_INDEX) {
        if(s->map.state!=GIGE_REMAP_UPLOAD || v>=s->map.rows*s->map.cols)return GIGE_STATUS_INVALID_PARAM;
        s->index=v;return 0;
    }
    if(a==GIGE_F_REMAP_DATA)return GigeRemapMap_Write(&s->map,s->index,v,0)?GIGE_STATUS_INVALID_PARAM:0;
    if(a==GIGE_F_REMAP_COMMIT) {
        if(v!=1 || s->geometry!=geometry || s->map.cols!=ow || s->map.rows!=oh || !color_route)
            return GIGE_STATUS_INVALID_PARAM;
        return GigeRemapMap_Commit(&s->map,0)?GIGE_STATUS_INVALID_PARAM:0;
    }
    if(s->map.state!=GIGE_REMAP_EMPTY && s->map.state!=GIGE_REMAP_ERROR)return GIGE_STATUS_BUSY;
    switch(a) {
    case GIGE_F_REMAP_WIDTH:if(v<1 || v>640)return GIGE_STATUS_INVALID_PARAM;s->width=v;break;
    case GIGE_F_REMAP_HEIGHT:if(v<1 || v>480)return GIGE_STATUS_INVALID_PARAM;s->height=v;break;
    case GIGE_F_REMAP_EXPECTED_CRC:s->expected_crc=v;break;
    default:return GIGE_STATUS_WRITE_PROTECT;
    }
    return 0;
}
void GigeRemapControl_Poll(GigeRemapControl *s,int busy)
{if(s && !busy && s->map.state==GIGE_REMAP_VALIDATE)(void)GigeRemapMap_Step(&s->map,64);}
void GigeRemapControl_Invalidate(GigeRemapControl *s)
{if(s){s->enabled=0;GigeRemapMap_Abort(&s->map);}}
