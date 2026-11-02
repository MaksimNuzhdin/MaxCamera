#include "GigeVision.h"
#include "GigeVisionXmlRom.h"
#include "GigeFeatureRegisters.h"
#include <string.h>

_Static_assert(sizeof(GIGE_VENDOR_NAME)<=32, "Vendor name exceeds bootstrap field");
_Static_assert(sizeof(GIGE_VENDOR_MODEL)<=32, "Model name exceeds bootstrap field");
_Static_assert(sizeof(GIGE_VENDOR_DEVICE_VERSION)<=32, "Device version exceeds bootstrap field");
_Static_assert(sizeof(GIGE_VENDOR_INFO)<=48, "Manufacturer info exceeds bootstrap field");
_Static_assert(sizeof(GIGE_UNIT_SERIAL)<=16, "Serial exceeds bootstrap field");
_Static_assert(sizeof(GIGE_UNIT_USER_NAME)<=16, "User name exceeds bootstrap field");
_Static_assert(GIGE_UNIT_TIMESTAMP_HZ==1000000U, "Clock adapter and packet delay use microseconds");
_Static_assert(GIGE_UNIT_PACKET_SIZE>=GIGE_PACKET_SIZE_MIN && GIGE_UNIT_PACKET_SIZE<=GIGE_PACKET_SIZE_MAX, "Invalid default packet size");

uint16_t Gige_Get16(const uint8_t *p) { return (uint16_t)((p[0]<<8)|p[1]); }
uint32_t Gige_Get32(const uint8_t *p) { return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3]; }
void Gige_Put16(uint8_t *p,uint16_t v) { p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v; }
void Gige_Put32(uint8_t *p,uint32_t v) { Gige_Put16(p,(uint16_t)(v>>16));Gige_Put16(p+2,(uint16_t)v); }
static int same(GigePeer a,GigePeer b) { return a.ip==b.ip && a.port==b.port; }
static void word(GigeVision *g,uint32_t a,uint32_t v) { Gige_Put32(g->bootstrap+a,v); }
static void text(GigeVision *g,uint32_t a,size_t n,const char *s)
{ size_t l=strlen(s); if(l>=n)l=n-1;memcpy(g->bootstrap+a,s,l); }
int GigeVision_FactoryAccess(const GigeVision *g) {return g && g->privilege && g->factory_until>g->now;}
void GigeVision_LockFactory(GigeVision *g)
{
    if(!g)return;
    g->factory_until=0;
    /* Do not replay a success ACK authorized in an earlier access session. */
    memset(g->replay,0,sizeof(g->replay));g->replay_next=0;
}
void GigeVision_ForgetBuffer(GigeVision *g,const uint8_t *p)
{for(unsigned i=0;i<GIGE_HISTORY_COUNT;++i)if(g->history[i].data==p)g->history[i].data=0;}
void GigeVision_ClearHistory(GigeVision *g)
{memset(g->history,0,sizeof(g->history));g->resend_head=g->resend_count=0;g->current_history=0;}
static void discard_frame(GigeVision *g) { if(g->frame){++g->frames_aborted;GigeVision_ForgetBuffer(g,g->frame);}g->frame=0; }
static void release(GigeVision *g)
{
    if(g->active && g->io.acquisition)g->io.acquisition(g->io.user,0);
    g->active=0;discard_frame(g);g->privilege=0;g->locked=0;GigeVision_LockFactory(g);
    GigeVision_ClearHistory(g);
    g->destination=0;g->destination_port=0;g->test_pending=0;
    memset(&g->owner,0,sizeof(g->owner));memset(g->replay,0,sizeof(g->replay));
}
void GigeVision_Release(GigeVision *g){release(g);}
static int ascii_string(const char *s,unsigned n)
{int ended=0;for(unsigned i=0;i<n;++i){if(!s[i])ended=1;else if(ended || (uint8_t)s[i]<32 || (uint8_t)s[i]>126)return 0;}return ended;}
static int valid_ip(uint32_t ip) {return (ip>>24)>0 && (ip>>24)<224 && (ip>>24)!=127;}
static int network_valid(uint32_t ip,uint32_t mask,uint32_t gateway)
{
    uint32_t inverse=~mask;
    return ip && (ip>>24)>0 && (ip>>24)<224 && (ip>>24)!=127 && mask &&
        !(inverse&(inverse+1U)) && (ip&inverse) && (ip&inverse)!=inverse &&
        (!gateway || (valid_ip(gateway) && (gateway&mask)==(ip&mask) && (gateway&inverse) && (gateway&inverse)!=inverse));
}
int GigeVision_ConfigValid(const GigeDeviceConfig *c)
{return c && c->version==1 && !(c->mac_high&0xffff0000U) && !(c->mac_high&0x100U) &&
    (c->mac_high || c->mac_low) && c->mode && !(c->mode&~7U) && (c->mode&4U) &&
    network_valid(c->ip,c->mask,c->gateway) && ascii_string(c->user_name,16) && ascii_string(c->serial,16) && c->serial[0] && c->provisioned<=1;}
void GigeVision_ApplyConfig(GigeVision *g,const GigeDeviceConfig *c)
{
    if(!GigeVision_ConfigValid(c))return;
    g->config=*c;
    word(g,GIGE_REG_MAC_HIGH,c->mac_high);word(g,GIGE_REG_MAC_LOW,c->mac_low);
    word(g,GIGE_REG_IP_CURRENT,c->mode);word(g,GIGE_REG_PERSISTENT_IP,c->ip);
    word(g,GIGE_REG_PERSISTENT_MASK,c->mask);word(g,GIGE_REG_PERSISTENT_GATEWAY,c->gateway);
    memcpy(g->bootstrap+GIGE_REG_USER_NAME,c->user_name,16);memcpy(g->bootstrap+GIGE_REG_SERIAL,c->serial,16);
}
static void config_changed(GigeVision *g){g->config_dirty=1;g->config_changed=g->now;}
void GigeVision_Init(GigeVision *g,const GigeIO *io)
{
    memset(g,0,sizeof(*g));
    g->width=GIGE_UNIT_WIDTH;g->height=GIGE_UNIT_HEIGHT;g->pixel_format=GIGE_UNIT_PIXEL_FORMAT;g->payload_size=GIGE_UNIT_PAYLOAD_SIZE;g->io=*io;
    word(g,GIGE_REG_VERSION,GIGE_BOOTSTRAP_VERSION);word(g,GIGE_REG_DEVICE_MODE,GIGE_DEVICE_MODE);
    word(g,GIGE_REG_MAC_HIGH,GIGE_UNIT_MAC_HIGH);word(g,GIGE_REG_MAC_LOW,GIGE_UNIT_MAC_LOW);
    word(g,GIGE_REG_IP_SUPPORTED,GIGE_UNIT_IP_SUPPORTED);
    text(g,GIGE_REG_MANUFACTURER,32,GIGE_VENDOR_NAME);text(g,GIGE_REG_MODEL,32,GIGE_VENDOR_MODEL);
    text(g,GIGE_REG_DEVICE_VERSION,32,GIGE_VENDOR_DEVICE_VERSION);
    text(g,GIGE_REG_MANUFACTURER_INFO,48,GIGE_VENDOR_INFO);
    text(g,GIGE_REG_SERIAL,16,GIGE_UNIT_SERIAL);text(g,GIGE_REG_USER_NAME,16,GIGE_UNIT_USER_NAME);
    text(g,GIGE_REG_XML_URL0,512,GIGE_XML_URL);
    word(g,GIGE_REG_INTERFACE_COUNT,1);word(g,GIGE_REG_STREAM_COUNT,1);
    word(g,GIGE_REG_PERSISTENT_IP,GIGE_UNIT_PERSISTENT_IP);
    word(g,GIGE_REG_PERSISTENT_MASK,GIGE_UNIT_PERSISTENT_MASK);
    word(g,GIGE_REG_PERSISTENT_GATEWAY,GIGE_UNIT_PERSISTENT_GATEWAY);
    word(g,GIGE_REG_GVCP_CAPABILITY,GIGE_DEFAULT_CAPABILITY);
    word(g,GIGE_REG_TIMESTAMP_LO,GIGE_UNIT_TIMESTAMP_HZ);
    word(g,GIGE_REG_SOURCE_PORT,GIGE_UNIT_STREAM_SOURCE_PORT);
    g->heartbeat=GIGE_UNIT_HEARTBEAT_MS;g->packet_size=GIGE_UNIT_PACKET_SIZE;
    g->packet_delay=GIGE_UNIT_PACKET_DELAY;
    GigeDeviceConfig config={1,GIGE_UNIT_MAC_HIGH,GIGE_UNIT_MAC_LOW,GIGE_UNIT_IP_CONFIG,
        GIGE_UNIT_PERSISTENT_IP,GIGE_UNIT_PERSISTENT_MASK,GIGE_UNIT_PERSISTENT_GATEWAY,GIGE_UNIT_USER_NAME,GIGE_UNIT_SERIAL,0};
    GigeVision_ApplyConfig(g,&config);
    GigeVision_SetAddress(g,GIGE_UNIT_PERSISTENT_IP,GIGE_UNIT_PERSISTENT_MASK,
                          GIGE_UNIT_PERSISTENT_GATEWAY,GIGE_UNIT_IP_CONFIG);
}
void GigeVision_SetAddress(GigeVision *g,uint32_t ip,uint32_t mask,uint32_t gateway,uint32_t mode)
{
    if(Gige_Get32(g->bootstrap+GIGE_REG_CURRENT_IP)!=ip && g->privilege)release(g);
    word(g,GIGE_REG_CURRENT_IP,ip);word(g,GIGE_REG_CURRENT_MASK,mask);
    word(g,GIGE_REG_CURRENT_GATEWAY,gateway);(void)mode;word(g,GIGE_REG_IP_CURRENT,g->config.mode);
}
void GigeVision_Tick(GigeVision *g,uint64_t now)
{
    g->now=now;
    if(g->factory_until && now>=g->factory_until)GigeVision_LockFactory(g);
    if(g->privilege && now-g->owner_time >= (uint64_t)g->heartbeat*1000U)release(g);
}
uint64_t GigeVision_TimestampAt(const GigeVision *g,uint64_t now)
{return now-g->timestamp_origin;}
static void refresh(GigeVision *g)
{
    word(g,GIGE_REG_HEARTBEAT,g->heartbeat);word(g,GIGE_REG_CCP,g->privilege);
    word(g,GIGE_REG_STREAM_PORT,g->destination_port);word(g,GIGE_REG_PACKET_SIZE,g->packet_size);
    word(g,GIGE_REG_PACKET_DELAY,g->packet_delay);word(g,GIGE_REG_DESTINATION_IP,g->destination);
    word(g,GIGE_REG_TIMESTAMP_LATCH_HI,(uint32_t)(g->timestamp_latch>>32));
    word(g,GIGE_REG_TIMESTAMP_LATCH_LO,(uint32_t)g->timestamp_latch);
}
static uint32_t feature(GigeVision *g,uint32_t a)
{
    switch(a) {
    case GIGE_REG_WIDTH:return g->width;
    case GIGE_REG_HEIGHT:return g->height;
    case GIGE_REG_PIXEL_FORMAT:return g->pixel_format;
    case GIGE_REG_PAYLOAD_SIZE:return g->payload_size;
    case GIGE_REG_ACQUISITION_MODE:return GIGE_UNIT_ACQUISITION_MODE;
    case GIGE_REG_ACQUISITION_ACTIVE:return g->active;
    case GIGE_REG_TL_PARAMS_LOCKED:return g->locked;
    case GIGE_REG_TIMESTAMP_CURRENT_HI:return (uint32_t)((g->now-g->timestamp_origin)>>32);
    case GIGE_REG_TIMESTAMP_CURRENT_LO:return (uint32_t)(g->now-g->timestamp_origin);
    default:return 0;
    }
}
static void config_encode(const GigeDeviceConfig *c,uint8_t *p)
{const uint32_t v[]={c->version,c->mac_high,c->mac_low,c->mode,c->ip,c->mask,c->gateway};
 for(unsigned i=0;i<7;++i)Gige_Put32(p+4*i,v[i]);
 memcpy(p+28,c->user_name,16);memcpy(p+44,c->serial,16);Gige_Put32(p+60,c->provisioned);}
static uint16_t readmem(GigeVision *g,uint32_t a,uint32_t n,uint8_t *p)
{
    if(a>=0x13000U && a<0x13040U && n<=0x13040U-a){uint8_t wire[64];config_encode(&g->config,wire);memcpy(p,wire+a-0x13000U,n);return 0;}
    if(a<sizeof(g->bootstrap) && n<=sizeof(g->bootstrap)-a) {
        if(a<=GIGE_REG_TIMESTAMP_CONTROL && a+n>GIGE_REG_TIMESTAMP_CONTROL)return GIGE_STATUS_INVALID_ADDRESS;
        memcpy(p,g->bootstrap+a,n);return 0;
    }
    if(a>=GIGE_REG_WIDTH && a<=GIGE_REG_TIMESTAMP_CURRENT_LO && n<=GIGE_REG_TIMESTAMP_CURRENT_LO+4-a) {
        if(a<=GIGE_REG_ACQUISITION_COMMAND && a+n>GIGE_REG_ACQUISITION_COMMAND)return GIGE_STATUS_INVALID_ADDRESS;
        for(uint32_t k=0;k<n;k+=4)Gige_Put32(p+k,feature(g,a+k));
        return 0;
    }
    const uint32_t padded=(GIGE_XML_LENGTH+3U)&~3U;
    if(a>=GIGE_XML_BASE && a-GIGE_XML_BASE<padded && n<=padded-(a-GIGE_XML_BASE)) {
        for(uint32_t k=0;k<n;++k) { uint32_t off=a-GIGE_XML_BASE+k;p[k]=off<GIGE_XML_LENGTH?gige_xml_rom[off]:0; }
        return 0;
    }
    if(g->io.feature_read && a>=GIGE_FEATURE_FIRST && a<GIGE_FEATURE_END && n<=GIGE_FEATURE_END-a) {
        for(uint32_t k=0;k<n;k+=4) {
            const GigeFeatureDescriptor *d=&gige_feature_descriptors[(a+k-GIGE_FEATURE_FIRST)/4];
            if(!d->readable)return GIGE_STATUS_INVALID_ADDRESS;
            uint32_t value;uint16_t e=g->io.feature_read(g->io.user,a+k,&value);
            if(e)return e;
            Gige_Put32(p+k,value);
        }
        return 0;
    }
    return GIGE_STATUS_INVALID_ADDRESS;
}
static int valid_destination(uint32_t ip) {return valid_ip(ip) || (ip>=0xe0000100U && ip<0xf0000000U);}
static uint16_t validate_write(GigeVision *g,GigePeer peer,uint32_t a,uint32_t v)
{
    if(a&3U)return GIGE_STATUS_BAD_ALIGNMENT;
    if(a==GIGE_REG_CCP) {
        if(v>2)return GIGE_STATUS_INVALID_PARAM;
        return g->privilege && !same(peer,g->owner)?GIGE_STATUS_ACCESS_DENIED:0;
    }
    if(!g->privilege || !same(peer,g->owner))return GIGE_STATUS_ACCESS_DENIED;
    switch(a) {
    case GIGE_REG_WIDTH:case GIGE_REG_HEIGHT:case GIGE_REG_PIXEL_FORMAT:
        if(g->locked || g->active)return GIGE_STATUS_ACCESS_DENIED;
        return GigeImage_Valid(a==GIGE_REG_WIDTH?v:g->width,a==GIGE_REG_HEIGHT?v:g->height,
                               a==GIGE_REG_PIXEL_FORMAT?v:g->pixel_format)?0:GIGE_STATUS_INVALID_PARAM;
    case GIGE_REG_IP_CURRENT:
        return g->active || g->locked?GIGE_STATUS_BUSY:((v&~7U)?GIGE_STATUS_INVALID_PARAM:0);
    case GIGE_REG_PERSISTENT_IP:case GIGE_REG_PERSISTENT_MASK:case GIGE_REG_PERSISTENT_GATEWAY:
        if(g->active || g->locked)return GIGE_STATUS_BUSY;
        if(a==GIGE_REG_PERSISTENT_MASK){uint32_t inverse=~v;return !v || (inverse&(inverse+1U))?GIGE_STATUS_INVALID_PARAM:0;}
        return (a==GIGE_REG_PERSISTENT_GATEWAY && !v) || valid_ip(v)?0:GIGE_STATUS_INVALID_PARAM;
    case GIGE_REG_HEARTBEAT:
        return v<GIGE_HEARTBEAT_MIN_MS || v>GIGE_HEARTBEAT_MAX_MS?GIGE_STATUS_INVALID_PARAM:0;
    case GIGE_REG_TIMESTAMP_CONTROL:return v>3?GIGE_STATUS_INVALID_PARAM:0;
    case GIGE_REG_TL_PARAMS_LOCKED:return v>1?GIGE_STATUS_INVALID_PARAM:0;
    case GIGE_REG_ACQUISITION_COMMAND:
        if(v>1)return GIGE_STATUS_INVALID_PARAM;
        if(v && (!valid_destination(g->destination) || !g->destination_port))return GIGE_STATUS_INVALID_PARAM;
        return 0;
    case GIGE_REG_STREAM_PORT:
    case GIGE_REG_PACKET_SIZE:
    case GIGE_REG_PACKET_DELAY:
    case GIGE_REG_DESTINATION_IP:
        if(g->locked || g->active)return GIGE_STATUS_ACCESS_DENIED;
        if(a==GIGE_REG_STREAM_PORT)return v>65535?GIGE_STATUS_INVALID_PARAM:0;
        if(a==GIGE_REG_DESTINATION_IP)return v && !valid_destination(v)?GIGE_STATUS_INVALID_PARAM:0;
        if(a==GIGE_REG_PACKET_DELAY)return v>1000000U?GIGE_STATUS_INVALID_PARAM:0;
        if((v & ~(GIGE_PACKET_SIZE_MASK|GIGE_PACKET_DO_NOT_FRAGMENT|GIGE_PACKET_FIRE_TEST)) ||
            (v&GIGE_PACKET_SIZE_MASK)<GIGE_PACKET_SIZE_MIN ||
            (v&GIGE_PACKET_SIZE_MASK)>GIGE_PACKET_SIZE_MAX)return GIGE_STATUS_INVALID_PARAM;
        if((v&GIGE_PACKET_FIRE_TEST) && (!valid_destination(g->destination)||!g->destination_port))return GIGE_STATUS_INVALID_PARAM;
        return 0;
    default:
        if(a>=GIGE_FEATURE_FIRST && a<GIGE_FEATURE_END && g->io.feature_write) {
            const GigeFeatureDescriptor *d=&gige_feature_descriptors[(a-GIGE_FEATURE_FIRST)/4];
            if(!d->writable)return GIGE_STATUS_WRITE_PROTECT;
            int64_t value=d->minimum<0?(int32_t)v:(int64_t)v;
            return value<d->minimum || value>d->maximum?GIGE_STATUS_INVALID_PARAM:0;
        }
        if(a<sizeof(g->bootstrap) || (a>=GIGE_REG_WIDTH && a<=GIGE_REG_TIMESTAMP_CURRENT_LO) ||
            (a>=GIGE_XML_BASE && a-GIGE_XML_BASE<GIGE_XML_CAPACITY))return GIGE_STATUS_WRITE_PROTECT;
        return GIGE_STATUS_INVALID_ADDRESS;
    }
}
static uint16_t write_reg(GigeVision *g,GigePeer peer,uint32_t a,uint32_t v)
{
    switch(a) {
    case GIGE_REG_CCP:
        if(!v)release(g);else {g->privilege=v;g->owner=peer;g->owner_time=g->now;}
        break;
    case GIGE_REG_WIDTH:case GIGE_REG_HEIGHT:case GIGE_REG_PIXEL_FORMAT: {
        if(g->io.feature_write) {
            uint16_t e=g->io.feature_write(g->io.user,a,v);if(e)return e;
        }
        if(a==GIGE_REG_WIDTH)g->width=v;
        else if(a==GIGE_REG_HEIGHT)g->height=v;
        else g->pixel_format=v;
        g->payload_size=GigeImage_Payload(g->width,g->height,g->pixel_format);
        break;
    }
    case GIGE_REG_IP_CURRENT:g->config.mode=v|4U;word(g,a,v|4U);config_changed(g);break;
    case GIGE_REG_PERSISTENT_IP:g->config.ip=v;word(g,a,v);config_changed(g);break;
    case GIGE_REG_PERSISTENT_MASK:g->config.mask=v;word(g,a,v);config_changed(g);break;
    case GIGE_REG_PERSISTENT_GATEWAY:g->config.gateway=v;word(g,a,v);config_changed(g);break;
    case GIGE_REG_HEARTBEAT:g->heartbeat=v;break;
    case GIGE_REG_TIMESTAMP_CONTROL:
        if(v&GIGE_TIMESTAMP_RESET)g->timestamp_origin=g->now;
        if(v&GIGE_TIMESTAMP_LATCH)g->timestamp_latch=g->now-g->timestamp_origin;
        break;
    case GIGE_REG_TL_PARAMS_LOCKED:g->locked=v;break;
    case GIGE_REG_ACQUISITION_COMMAND:
        if(v!=g->active) {
            if(g->io.acquisition && g->io.acquisition(g->io.user,(int)v))return GIGE_STATUS_BUSY;
            g->active=(uint8_t)v;if(!v)discard_frame(g);
        }
        break;
    case GIGE_REG_STREAM_PORT:g->destination_port=(uint16_t)v;break;
    case GIGE_REG_DESTINATION_IP:g->destination=v;break;
    case GIGE_REG_PACKET_DELAY:g->packet_delay=v;break;
    case GIGE_REG_PACKET_SIZE:g->packet_size=v&~GIGE_PACKET_FIRE_TEST;g->test_pending=(v&GIGE_PACKET_FIRE_TEST)!=0;break;
    default:
        if(a>=GIGE_FEATURE_FIRST && a<GIGE_FEATURE_END && g->io.feature_write)
            return g->io.feature_write(g->io.user,a,v);
        break;
    }
    return 0;
}
size_t GigeVision_Control(GigeVision *g,GigePeer peer,const uint8_t *q,size_t n,uint8_t ack[GIGE_ACK_MAX],int *broadcast)
{
    uint16_t status=0;size_t count=0;
    *broadcast=0;
    if(n<8 || n>GIGE_CONTROL_MAX || q[0]!=0x42 || (size_t)Gige_Get16(q+4)!=n-8)return 0;
    uint16_t cmd=Gige_Get16(q+2),id=Gige_Get16(q+6);
    if(!id)return 0;
    int owner=g->privilege && same(peer,g->owner);
    int password_write=cmd==GIGE_CMD_WRITEMEM && n>=12 && Gige_Get32(q+8)>=0x12000U && Gige_Get32(q+8)<0x12020U;
    if(owner)g->owner_time=g->now;
    /* Discovery is deliberately fresh, including when IP assignment changes. */
    if(cmd!=GIGE_CMD_DISCOVERY && cmd!=GIGE_CMD_RESEND && !password_write && !(g->privilege==GIGE_CCP_EXCLUSIVE && !owner))for(unsigned i=0;i<8;++i) {
        GigeReplay *r=&g->replay[i];
        if(r->request_size==n && same(r->peer,peer) && g->now-r->time<1000000U && !memcmp(q,r->request,n)) {
            memcpy(ack,r->ack,r->ack_size);return (q[1]&1U)?r->ack_size:0;
        }
    }
    refresh(g);
    if(cmd==GIGE_CMD_DISCOVERY) {
        if(n!=8)status=GIGE_STATUS_INVALID_PARAM;
        else {count=GIGE_DISCOVERY_BYTES;memcpy(ack+8,g->bootstrap,count);*broadcast=(q[1]&0x10)!=0 || !peer.ip;}
    } else if(cmd==GIGE_CMD_FORCEIP) {
        if(n!=64)status=GIGE_STATUS_INVALID_PARAM;
        else if(Gige_Get16(q+10)!=(Gige_Get32(g->bootstrap+GIGE_REG_MAC_HIGH)&0xffffU)||Gige_Get32(q+12)!=Gige_Get32(g->bootstrap+GIGE_REG_MAC_LOW))return 0;
        else if(g->privilege && !owner)status=GIGE_STATUS_ACCESS_DENIED;
        else {
            uint32_t ip=Gige_Get32(q+28), mask=Gige_Get32(q+44),gateway=Gige_Get32(q+60);
            uint32_t inverse=~mask;
            if(!valid_ip(ip)||!mask||(inverse&(inverse+1U)) ||
               (ip&inverse)==0 || (ip&inverse)==inverse ||
               (gateway && (!valid_ip(gateway)||(gateway&mask)!=(ip&mask))))status=GIGE_STATUS_INVALID_PARAM;
            else {g->force_ip=ip;g->force_mask=mask;g->force_gateway=gateway;g->force_ip_pending=1;*broadcast=1;}
        }
    } else if(cmd==GIGE_CMD_RESEND) {
        /* Receivers send requests from their stream socket, not necessarily
         * the controlling GVCP port. Classic IDs, stream channel zero only. */
        if(!g->privilege || (peer.ip!=g->destination && peer.ip!=g->owner.ip) || !g->destination_port)status=GIGE_STATUS_ACCESS_DENIED;
        else if(n!=20 || (q[1]&0x10U) || Gige_Get16(q+8) || !Gige_Get16(q+10) ||
                (Gige_Get32(q+12)|Gige_Get32(q+16))>0xffffffU || Gige_Get32(q+12)>Gige_Get32(q+16))status=GIGE_STATUS_INVALID_PARAM;
        else if(g->resend_count==GIGE_RESEND_QUEUE){++g->resend_overflow;status=GIGE_STATUS_BUSY;}
        else {
            GigeResend *r=&g->resend[(g->resend_head+g->resend_count++)%GIGE_RESEND_QUEUE];
            r->block=Gige_Get16(q+10);r->first=Gige_Get32(q+12);r->last=Gige_Get32(q+16);++g->resend_requests;
        }
    } else if(g->privilege==GIGE_CCP_EXCLUSIVE && !owner)status=GIGE_STATUS_ACCESS_DENIED;
    else if(cmd==GIGE_CMD_READREG) {
        if(n<=8 || (n-8)%4 || n-8>512)status=GIGE_STATUS_INVALID_PARAM;
        else for(size_t k=8;k<n;k+=4) {
            uint32_t a=Gige_Get32(q+k);
            status=(a&3)?GIGE_STATUS_BAD_ALIGNMENT:readmem(g,a,4,ack+8+count);
            if(status)break;
            count+=4;
        }
    } else if(cmd==GIGE_CMD_READMEM) {
        if(n!=16 || Gige_Get16(q+12))status=GIGE_STATUS_INVALID_PARAM;
        else {
            uint32_t a=Gige_Get32(q+8),bytes=Gige_Get16(q+14);
            if((a|bytes)&3)status=GIGE_STATUS_BAD_ALIGNMENT;
            else if(!bytes||bytes>GIGE_READ_MEMORY_MAX)status=GIGE_STATUS_INVALID_PARAM;
            else {status=readmem(g,a,bytes,ack+12);Gige_Put32(ack+8,a);count=4+bytes;}
        }
    } else if(cmd==0x86) { /* WRITEMEM: mapped registers, strings and guarded provisioning. */
        if(!g->privilege || !owner)status=GIGE_STATUS_ACCESS_DENIED;
        else if(n==76 && Gige_Get32(q+8)==0x13000U) {
            if(!GigeVision_FactoryAccess(g))status=GIGE_STATUS_ACCESS_DENIED;
            else if(g->active || g->locked)status=GIGE_STATUS_BUSY;
            else {
                GigeDeviceConfig c={0};c.version=Gige_Get32(q+12);c.mac_high=Gige_Get32(q+16);c.mac_low=Gige_Get32(q+20);
                c.mode=Gige_Get32(q+24);c.ip=Gige_Get32(q+28);c.mask=Gige_Get32(q+32);c.gateway=Gige_Get32(q+36);
                memcpy(c.user_name,q+40,16);memcpy(c.serial,q+56,16);c.provisioned=Gige_Get32(q+72);
                if(!GigeVision_ConfigValid(&c))status=GIGE_STATUS_INVALID_PARAM;
                else {g->config=c;config_changed(g);Gige_Put32(ack+8,0x13000);count=4;}
            }
        }
        else if(n>=16 && n<=28 && !(n&3U) && Gige_Get32(q+8)==GIGE_REG_USER_NAME) {
            char name[16]={0};memcpy(name,q+12,n-12);
            if(g->active || g->locked)status=GIGE_STATUS_BUSY;
            else if(!ascii_string(name,16))status=GIGE_STATUS_INVALID_PARAM;
            else {memcpy(g->config.user_name,name,16);memcpy(g->bootstrap+GIGE_REG_USER_NAME,name,16);config_changed(g);Gige_Put32(ack+8,GIGE_REG_USER_NAME);count=4;}
        }
        else if(n>=16 && !(n&3U) && Gige_Get32(q+8)!=0x12000U) {
            uint32_t a=Gige_Get32(q+8);
            if(a&3U)status=GIGE_STATUS_BAD_ALIGNMENT;
            else if((uint64_t)a+n-12>0x100000000ULL)status=GIGE_STATUS_INVALID_ADDRESS;
            else for(size_t k=12;k<n;k+=4){uint32_t at=a+(uint32_t)k-12,v=Gige_Get32(q+k);
                status=validate_write(g,peer,at,v);if(status)break;
                status=write_reg(g,peer,at,v);if(status)break;}
            if(!status){Gige_Put32(ack+8,a);count=4;}
        }
        else if(n<24 || n>44 || (n&3U) || Gige_Get32(q+8)!=0x12000U){GigeVision_LockFactory(g);status=GIGE_STATUS_INVALID_ADDRESS;}
        else {
            static const uint8_t expected[32]="DinxTechCam";
            unsigned difference=0;
            for(size_t i=0;i<n-12;++i)difference|=q[12+i]^expected[i];
            GigeVision_LockFactory(g);
            if(difference)status=GIGE_STATUS_ACCESS_DENIED;
            else {g->factory_until=g->now+300000000ULL;Gige_Put32(ack+8,0x12000U);count=4;}
        }
    } else if(cmd==GIGE_CMD_WRITEREG) {
        if(n<=8 || (n-8)%8)status=GIGE_STATUS_INVALID_PARAM;
        else {
            size_t k;
            for(k=8;k<n;k+=8) {
                uint32_t a=Gige_Get32(q+k),v=Gige_Get32(q+k+4);
                status=validate_write(g,peer,a,v);
                if(status)break;
                /* Execute in wire order; index ACK reports partial progress on failure. */
                status=write_reg(g,peer,a,v);if(status)break;
            }
            Gige_Put16(ack+8,0);Gige_Put16(ack+10,(uint16_t)((k-8)/8));count=4;
        }
    } else status=GIGE_STATUS_NOT_IMPLEMENTED;
    /* Error ACK payload only retained for WRITEREG progress reporting. */
    if(status && cmd!=GIGE_CMD_WRITEREG)count=0;
    Gige_Put16(ack,status);Gige_Put16(ack+2,(uint16_t)(cmd+1));
    Gige_Put16(ack+4,(uint16_t)count);Gige_Put16(ack+6,id);
    size_t bytes=count+8;
    if(cmd!=GIGE_CMD_DISCOVERY && cmd!=GIGE_CMD_FORCEIP && cmd!=GIGE_CMD_RESEND && !password_write) {
        GigeReplay *r=&g->replay[g->replay_next++%8];r->peer=peer;r->time=g->now;
        r->request_size=(uint16_t)n;r->ack_size=(uint16_t)bytes;
        memcpy(r->request,q,n);memcpy(r->ack,ack,bytes);
    }
    return (q[1]&1U) || cmd==GIGE_CMD_DISCOVERY || cmd==GIGE_CMD_FORCEIP?bytes:0;
}
int GigeVision_FrameBusy(const GigeVision *g) { return g->frame!=0; }
int GigeVision_SubmitFrame(GigeVision *g,const uint8_t *data,uint64_t timestamp)
{return GigeVision_SubmitFrameSized(g,data,g->payload_size,timestamp);}
int GigeVision_SubmitFrameSized(GigeVision *g,const uint8_t *data,uint32_t bytes,uint64_t timestamp)
{
    if(!g->active || g->frame || !data || !bytes || bytes>g->payload_size || (!g->compression && bytes!=g->payload_size))return -1;
    if(++g->block_id==0)g->block_id=1;
    g->frame_row_bytes=bytes;g->frame_stride=bytes;
    g->frame=data;g->frame_size=bytes;g->frame_offset=0;g->packet_id=0;
    g->frame_submitted=g->now;g->frame_timestamp=timestamp;g->next_packet=g->now;
    GigeVision_ForgetBuffer(g,data);
    GigeFrameHistory *h=&g->history[g->history_next++%GIGE_HISTORY_COUNT];
    g->current_history=h;
    *h=(GigeFrameHistory){data,bytes,bytes,bytes,g->width,g->height,g->pixel_format,g->compression,g->packet_size,0,g->block_id,g->frame_timestamp};
    return 0;
}
int GigeVision_SubmitFrameStrided(GigeVision *g,const uint8_t *data,uint32_t row_bytes,uint32_t stride,uint64_t timestamp)
{
    if(!row_bytes || stride<row_bytes || g->compression ||
       (uint64_t)row_bytes*g->height!=g->payload_size)return -1;
    if(GigeVision_SubmitFrameSized(g,data,g->payload_size,timestamp))return -1;
    g->frame_row_bytes=row_bytes;g->frame_stride=stride;
    GigeFrameHistory *h=&g->history[(g->history_next-1)%GIGE_HISTORY_COUNT];h->row_bytes=row_bytes;h->stride=stride;return 0;
}
static GigeFrameHistory *history(GigeVision *g,uint16_t block)
{for(unsigned i=0;i<GIGE_HISTORY_COUNT;++i)if(g->history[i].data && g->history[i].block==block)return &g->history[i];return 0;}
static size_t historical_packet(const GigeFrameHistory *h,uint32_t id,uint8_t *p)
{
    uint32_t chunk=(h->packet_size&GIGE_PACKET_SIZE_MASK)-36U;
    uint32_t trailer=(h->size+chunk-1)/chunk+1;
    memset(p,0,44);Gige_Put16(p,0x0100);Gige_Put16(p+2,h->block);
    if(!id) {
        Gige_Put32(p+4,1U<<24);Gige_Put16(p+10,h->compression?6:1);
        Gige_Put32(p+12,(uint32_t)(h->timestamp>>32));Gige_Put32(p+16,(uint32_t)h->timestamp);
        if(h->compression){Gige_Put32(p+24,h->size);Gige_Put32(p+32,GIGE_UNIT_TIMESTAMP_HZ);return 40;}
        Gige_Put32(p+20,h->format);Gige_Put32(p+24,h->width);Gige_Put32(p+28,h->height);return 44;
    }
    if(id==trailer){Gige_Put32(p+4,(2U<<24)|id);Gige_Put32(p+8,h->compression?6:1);Gige_Put32(p+12,h->height);return h->compression?12:16;}
    uint32_t offset=(id-1)*chunk,n=h->size-offset;if(n>chunk)n=chunk;
    Gige_Put32(p+4,(3U<<24)|id);
    uint32_t copied=0;
    while(copied<n){uint32_t at=offset+copied,row=at/h->row_bytes,col=at%h->row_bytes;
        uint32_t take=h->row_bytes-col;if(take>n-copied)take=n-copied;
        memcpy(p+8+copied,h->data+(size_t)row*h->stride+col,take);copied+=take;}
    return n+8;
}
static void __attribute__((noinline)) resend_poll(GigeVision *g,uint8_t *p)
{
    GigeResend *r=&g->resend[g->resend_head];GigeFrameHistory *h=history(g,r->block);
    uint32_t trailer=h?(h->size+(h->packet_size&GIGE_PACKET_SIZE_MASK)-37U)/((h->packet_size&GIGE_PACKET_SIZE_MASK)-36U)+1:0;
    uint32_t available=h && h==g->current_history && g->frame?g->packet_id:(h?h->sent:0);
    int unavailable=!h || r->first>trailer || r->first>=available;
    size_t bytes;
    if(unavailable){memset(p,0,8);Gige_Put16(p,0x800c);Gige_Put16(p+2,r->block);Gige_Put32(p+4,(3U<<24)|r->first);bytes=8;}
    else bytes=historical_packet(h,r->first,p);
    if(g->io.send_stream(g->io.user,g->destination,g->destination_port,p,bytes,(g->packet_size&GIGE_PACKET_DO_NOT_FRAGMENT)!=0)){++g->send_errors;return;}
    ++g->packets_sent;if(unavailable)++g->resend_unavailable;else ++g->resent_packets;
    if(unavailable || r->first++==r->last){g->resend_head=(g->resend_head+1)%GIGE_RESEND_QUEUE;--g->resend_count;}
    g->next_packet=g->now+g->packet_delay;
}
void GigeVision_StreamPoll(GigeVision *g)
{
    uint8_t fallback[1472],*packet=fallback;size_t length;
    if(!g->io.send_stream || g->now<g->next_packet)return;
    if(!g->test_pending && !g->resend_count && (!g->active || !g->frame))return;
    if(g->io.stream_buffer) {packet=g->io.stream_buffer(g->io.user);if(!packet)return;}
    /* Alternate retransmissions with original traffic; never monopolize GVCP
     * or DMA service even for a request covering a complete frame. */
    if(g->resend_count && (!g->frame || (g->resend_turn++&1U))){resend_poll(g,packet);return;}
    if(g->test_pending) {
        length=(g->packet_size&GIGE_PACKET_SIZE_MASK)-28U;
        memset(packet,0,length);Gige_Put32(packet+4,3U<<24); /* Test: block ID=0, generic payload */
        if(!g->io.send_stream(g->io.user,g->destination,g->destination_port,packet,length,
                             (g->packet_size&GIGE_PACKET_DO_NOT_FRAGMENT)!=0))g->test_pending=0;
        return;
    }
    if(!g->active || !g->frame)return;
    memset(packet,0,8);Gige_Put16(packet+2,g->block_id);
    if(g->packet_id==0) {
        Gige_Put32(packet+4,1U<<24);memset(packet+8,0,36);
        Gige_Put16(packet+10,1);Gige_Put32(packet+12,(uint32_t)(g->frame_timestamp>>32));
        Gige_Put32(packet+16,(uint32_t)g->frame_timestamp);
        Gige_Put32(packet+20,g->pixel_format);Gige_Put32(packet+24,g->width);
        Gige_Put32(packet+28,g->height);length=44;
        if(g->compression) {
            memset(packet+8,0,32);Gige_Put16(packet+10,6);
            Gige_Put32(packet+12,(uint32_t)(g->frame_timestamp>>32));Gige_Put32(packet+16,(uint32_t)g->frame_timestamp);
            Gige_Put32(packet+24,g->frame_size); /* payload length uint64 BE */
            Gige_Put32(packet+32,GIGE_UNIT_TIMESTAMP_HZ); /* tick frequency uint64 BE */
            length=40;
        }
    } else if(g->frame_offset<g->frame_size) {
        size_t bytes=(g->packet_size&GIGE_PACKET_SIZE_MASK)-36U;
        if(bytes>g->frame_size-g->frame_offset)bytes=g->frame_size-g->frame_offset;
        Gige_Put32(packet+4,(3U<<24)|g->packet_id);
        size_t copied=0;
        while(copied<bytes) {
            uint32_t offset=g->frame_offset+(uint32_t)copied;
            uint32_t row=offset/g->frame_row_bytes,col=offset%g->frame_row_bytes;
            size_t chunk=g->frame_row_bytes-col;
            if(chunk>bytes-copied)chunk=bytes-copied;
            memcpy(packet+8+copied,g->frame+(size_t)row*g->frame_stride+col,chunk);
            copied+=chunk;
        }
        length=8+bytes;
    } else {
        Gige_Put32(packet+4,(2U<<24)|g->packet_id);
        Gige_Put32(packet+8,g->compression?6:1);Gige_Put32(packet+12,g->height);length=g->compression?12:16;
    }
    if(g->io.send_stream(g->io.user,g->destination,g->destination_port,packet,length,
                         (g->packet_size&GIGE_PACKET_DO_NOT_FRAGMENT)!=0)) {
        ++g->send_errors;return; /* bounded retry by main loop, heartbeat remains serviced */
    }
    ++g->packets_sent;g->next_packet=g->now+g->packet_delay;
    if(g->packet_id && g->frame_offset>=g->frame_size) {if(g->current_history)g->current_history->sent=g->packet_id+1;g->frame_send_us=(uint32_t)(g->now-g->frame_submitted);g->frame=0;++g->frames_sent;}
    else {if(g->packet_id)g->frame_offset+=(uint32_t)(length-8);++g->packet_id;}
}
