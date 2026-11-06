#include "GigeAI.h"
#include "GigeFeatureRegisters.h"
#include <string.h>
#include <limits.h>
#if defined(__arm__) || defined(__aarch64__)
#define BARRIER() __asm__ volatile("dmb sy" ::: "memory")
#else
#define BARRIER() __sync_synchronize()
#endif
#define INVALID 0x8002U
#define DENIED 0x8006U
#define BUSY 0x8007U
static volatile uint32_t *m;
static uint32_t enabled,selector,objects,sequence,generation,heartbeat,latest,valid;
static uint32_t snapshot[GIGE_AI_MAX_OBJECTS*16],pending[GIGE_AI_MAX_OBJECTS*16],metadata[10],pending_meta[10],latched_meta[10],latency,error,latched_count,latched_sequence;
static const void *pin;
static uint64_t now_us,heartbeat_us,next_offer,offer_us,pending_us,latched_us;
void GigeAI_Init(volatile uint32_t *mailbox)
{
    m=mailbox; enabled=selector=objects=sequence=heartbeat=latest=valid=0;
    generation=2;pin=0;now_us=heartbeat_us=next_offer=0;latency=error=0;offer_us=pending_us=latched_us=0;
    latched_count=latched_sequence=0;memset(snapshot,0,sizeof snapshot);memset(metadata,0,sizeof metadata);
    if(m){for(unsigned i=0;i<GIGE_AI_BYTES/4;++i)m[i]=0;
        m[AI_VERSION]=1;m[AI_CONFIG]=generation;m[AI_SCORE]=500000;
        m[AI_FACE_THRESHOLD]=450000;m[AI_PERIOD]=500000;BARRIER();m[AI_MAGIC]=GIGE_AI_MAGIC;}
}
static int alive(void){return m && heartbeat && now_us-heartbeat_us<3000000;}
int GigeAI_Address(uint32_t a){return a>=GIGE_F_AI_AVAILABLE && a<=GIGE_F_AI_RESULT_AGE && !(a&3);}
void GigeAI_Poll(uint64_t now)
{
    now_us=now;if(!m)return;
    uint32_t h=m[AI_HEARTBEAT];BARRIER();if(h!=heartbeat){heartbeat=h;heartbeat_us=now;}
    if(pin && m[AI_COPY_ACK]==sequence){BARRIER();pin=0;}
    uint32_t r=m[AI_RESULT];BARRIER();
    if(r && r!=m[AI_RESULT_ACK]){
        uint32_t request=m[AI_OUTPUT],cfg=m[AI_OUTPUT+1],n=m[AI_OUTPUT+2];
        uint32_t ok=request==sequence && cfg==generation && n<=GIGE_AI_MAX_OBJECTS;
        uint32_t tmp[GIGE_AI_MAX_OBJECTS*16];
        if(ok){for(unsigned i=0;i<n*16;++i)tmp[i]=m[AI_OBJECTS+i];
            for(unsigned i=0;i<n;++i){uint32_t *o=tmp+i*16;
                if(o[0]>=8 || o[2]>1000000 || !o[5] || !o[6] ||
                   o[3]>=metadata[3] || o[4]>=metadata[4] ||
                   o[5]>metadata[3]-o[3] || o[6]>metadata[4]-o[4])ok=0;
                o[15]&=0x00ffffffU;
            }
        }
        error=m[AI_OUTPUT+4];latency=m[AI_OUTPUT+3];
        if(ok && !error){memcpy(pending,tmp,n*16*4);memcpy(pending_meta,metadata,sizeof metadata);pending_us=offer_us;objects=n;latest=request;}
        else {objects=0;latest=0;if(!error)error=2;}
        BARRIER();m[AI_RESULT_ACK]=r;
    }
}
int GigeAI_Pinned(const void *b){return pin==b;}
void GigeAI_Offer(const void *b,uint32_t w,uint32_t h,uint32_t stride,uint32_t fmt,uint64_t ts,uint32_t capture)
{
    if(!alive() || !enabled || pin || now_us<next_offer ||
       m[AI_REQUEST]!=m[AI_RESULT_ACK] || sequence==UINT32_MAX)return;
    if(fmt!=0x02180014U && fmt!=0x02180015U && fmt!=0x01080001U && fmt!=0x02100032U)return;
    uint32_t bpp=fmt==0x01080001U?1:(fmt==0x02100032U?2:3);
    uintptr_t a=(uintptr_t)b;
    if(!w || !h || w>2592 || h>1944 || stride<w*bpp || stride>7808 ||
       a<0x100000U || a>=0x10000000U || (uint64_t)a+(uint64_t)stride*h>0x10000000U)return;
    pin=b;offer_us=now_us;++sequence;
    uint32_t d[10]={sequence,generation,(uint32_t)a,w,h,stride,fmt,(uint32_t)ts,(uint32_t)(ts>>32),capture};
    memcpy(metadata,d,sizeof d);for(unsigned i=0;i<10;++i)m[AI_DESCRIPTOR+i]=d[i];
    BARRIER();m[AI_REQUEST]=sequence;next_offer=now_us+m[AI_PERIOD];
}
void GigeAI_RevokeControl(void)
{
    if(!enabled)return;
    enabled=0;valid=latest=0;objects=0;
    if(m && generation<UINT32_MAX-2){m[AI_CONFIG]=generation+1;BARRIER();m[AI_MASK]=0;BARRIER();generation+=2;m[AI_CONFIG]=generation;}
    /* Keep the capture lease until the exact copy acknowledgement, even on timeout. */
}
uint16_t GigeAI_Read(uint32_t a,uint32_t *v)
{
    *v=0;
    if(a==GIGE_F_AI_AVAILABLE){*v=alive();return 0;}
    if(a==GIGE_F_AI_SUPPORTED){*v=alive()?m[AI_SUPPORTED]:0;return 0;}
    if(a==GIGE_F_AI_ENABLE){*v=enabled;return 0;}
    if(a==GIGE_F_AI_STATE){*v=!alive()?0:pin?3:enabled?2:1;return 0;}
    if(a==GIGE_F_AI_ERROR){*v=error;return 0;}
    if(a==GIGE_F_AI_CONFIG){*v=generation;return 0;}
    if(a==GIGE_F_AI_SCORE){*v=m?m[AI_SCORE]:500000;return 0;}
    if(a==GIGE_F_AI_PERIOD){*v=m?m[AI_PERIOD]:500000;return 0;}
    if(a==GIGE_F_AI_FACE_THRESHOLD){*v=m?m[AI_FACE_THRESHOLD]:450000;return 0;}
    if(a==GIGE_F_AI_VALID){*v=valid && alive();return 0;}
    if(a==GIGE_F_AI_COUNT){*v=valid?latched_count:0;return 0;}
    if(a==GIGE_F_AI_SEQUENCE){*v=valid?latched_sequence:0;return 0;}
    if(a==GIGE_F_AI_LATENCY){*v=latency;return 0;}
    if(a==GIGE_F_AI_SELECTOR){*v=selector;return 0;}
    if(a>=GIGE_F_AI_OBJECT_FEATURE && a<=GIGE_F_AI_TEXT_7){
        if(valid && selector<latched_count)*v=snapshot[selector*16+(a-GIGE_F_AI_OBJECT_FEATURE)/4];
        return 0;}
    if(a==GIGE_F_AI_CAPTURE){*v=valid?latched_meta[9]:0;return 0;}
    if(a==GIGE_F_AI_TIMESTAMP_LOW){*v=valid?latched_meta[7]:0;return 0;}
    if(a==GIGE_F_AI_TIMESTAMP_HIGH){*v=valid?latched_meta[8]:0;return 0;}
    if(a==GIGE_F_AI_RESULT_WIDTH){*v=valid?latched_meta[3]:0;return 0;}
    if(a==GIGE_F_AI_RESULT_HEIGHT){*v=valid?latched_meta[4]:0;return 0;}
    if(a==GIGE_F_AI_RESULT_PFNC){*v=valid?latched_meta[6]:0;return 0;}
    if(a==GIGE_F_AI_RESULT_CONFIG){*v=valid?latched_meta[1]:0;return 0;}
    if(a==GIGE_F_AI_RESULT_AGE){uint64_t age=now_us-latched_us;*v=valid?(age>UINT32_MAX?UINT32_MAX:(uint32_t)age):0;return 0;}
    return DENIED;
}
uint16_t GigeAI_Write(uint32_t a,uint32_t v)
{
    if(a==GIGE_F_AI_SELECTOR){if(v>=GIGE_AI_MAX_OBJECTS)return INVALID;selector=v;return 0;}
    if(a==GIGE_F_AI_LATCH){if(v!=1)return INVALID;valid=latest && alive();if(valid){memcpy(snapshot,pending,sizeof snapshot);memcpy(latched_meta,pending_meta,sizeof metadata);latched_count=objects;latched_sequence=latest;latched_us=pending_us;}return 0;}
    if(a==GIGE_F_AI_RELEASE){if(v!=1)return INVALID;valid=0;return 0;}
    if(!m)return BUSY;
    if(a==GIGE_F_AI_ENABLE || a==GIGE_F_AI_SCORE || a==GIGE_F_AI_PERIOD || a==GIGE_F_AI_FACE_THRESHOLD){
        if(generation>=UINT32_MAX-2)return BUSY;
        if(a==GIGE_F_AI_ENABLE && (v>255 || (v && (!alive() || (v&~m[AI_SUPPORTED])))))return INVALID;
        if((a==GIGE_F_AI_SCORE || a==GIGE_F_AI_FACE_THRESHOLD) && (v<1 || v>1000000))return INVALID;
        if(a==GIGE_F_AI_PERIOD && (v<100000 || v>60000000))return INVALID;
        m[AI_CONFIG]=generation+1;BARRIER();
        if(a==GIGE_F_AI_ENABLE){enabled=v;m[AI_MASK]=v;}
        else m[a==GIGE_F_AI_SCORE?AI_SCORE:a==GIGE_F_AI_PERIOD?AI_PERIOD:AI_FACE_THRESHOLD]=v;
        valid=latest=0;objects=0;BARRIER();generation+=2;m[AI_CONFIG]=generation;return 0;
    }
    return DENIED;
}
