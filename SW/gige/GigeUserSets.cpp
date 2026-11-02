#include "GigeUserSets.h"
#include "GigeUserStore.h"
#include "GigeFlash.h"
#include "GigeCamera.h"
#include "GigeVisionBoard.h"
#include "xil_printf.h"
#include "sleep.h"
#include <string.h>
static GigeUserStore store;
static GigeVision *device;
static GigePreset factory;
static uint32_t selector,startup,pending_slot;
static int last_error,config_error;
static uint8_t pending,writing,factory_valid;
static int read_flash(void*,uint32_t a,void *p,uint32_t n) {return GigeFlash_Read(a,p,n);}
static int write_flash(void*,uint32_t a,const void *p,uint32_t n) {return GigeFlash_Program(a,p,n);}
static int erase_flash(void*,uint32_t a) {return GigeFlash_Erase(a);}
static int busy_flash(void*) {return GigeFlash_Busy();}
int GigeUserSets_Address(uint32_t a) {return (a>=GIGE_F_USERSET_AVAILABLE && a<=GIGE_F_FACTORY_LOCK && !(a&3U)) || a==GIGE_F_FACTORY_SAVE;}
int GigeUserSets_ConfigPending(void){return device && (device->config_dirty || writing==4);}
int GigeUserSets_ConfigError(void){return store.available?config_error:-1;}
int GigeUserSets_Busy(void) {return pending || writing || store.state;}
static int load(unsigned slot)
{
    GigePreset preset;uint8_t bytes[GIGE_USERSTORE_MAX];uint32_t count=sizeof(bytes);
    if(slot==0 && !(store.valid&1U)) {
        if(!factory_valid)return -10;
        preset=factory;
    } else if(GigeUserStore_Load(&store,slot,bytes,&count) || GigePreset_Decode(&preset,bytes,count))return -10;
    if(GigeCamera_Import(&preset))return -11;
    device->compression=preset.compression;
    device->width=preset.width;device->height=preset.height;device->pixel_format=preset.pixel_format;
    device->payload_size=GigeImage_Payload(preset.width,preset.height,preset.pixel_format);
    return 0;
}
int GigeUserSets_InitStorage(GigeVision *g)
{
    device=g;
    GigeUserStoreIO io={0,read_flash,write_flash,erase_flash,busy_flash};
    /* A processor reset can interrupt polling while the NOR device itself
     * is still completing an erase/program. Do not scan it while WIP is set. */
    uint64_t until=GigeVisionBoard_NowUs()+5000000U;
    while(GigeFlash_Busy()==1 && GigeVisionBoard_NowUs()<until)usleep(1000);
    int result=GigeUserStore_Init(&store,&io);
    if(result){last_error=result;xil_printf("GIGE USERSETS unavailable error=%d\r\n",result);return result;}
    if(store.valid&(1U<<9)) {
        GigeDeviceConfig config;uint32_t n=sizeof(config);
        if(GigeUserStore_Load(&store,9,&config,&n) || n!=sizeof(config) || !GigeVision_ConfigValid(&config))last_error=-20;
        else GigeVision_ApplyConfig(g,&config);
    }
    return 0;
}
int GigeUserSets_RestoreStartup(void)
{
    selector=0;
    if(!store.available)return 0; /* firmware defaults are the power-up fallback */
    int e=load(startup);
    if(e && startup){last_error=e;e=load(0);}
    return e;
}
void GigeUserSets_Init(GigeVision *g)
{
    device=g;selector=startup=pending_slot=0;pending=writing=0;
    factory_valid=GigeCamera_Export(&factory)==0;
    if(!store.available)return;
    int result=0;
    uint8_t chosen[4];uint32_t bytes=sizeof(chosen);
    if((store.valid&(1U<<8)) && !GigeUserStore_Load(&store,8,chosen,&bytes) && bytes==4 && chosen[0]<=7 && !chosen[1] && !chosen[2] && !chosen[3])startup=chosen[0];
    if(startup || (store.valid&1U)) {
        result=load(startup);
        if(result) {
            last_error=result;
            /* A damaged/incompatible preset never becomes partially decoded.
             * The already-configured firmware defaults stay usable. */
            xil_printf("GIGE USERSETS startup rejected slot=%d error=%d\r\n",startup,result);
            if(GigeCamera_Ready() && factory_valid) {
                if((store.valid&1U) && startup) (void)load(0);
                startup=0;
            }
        }
    }
    xil_printf("GIGE USERSETS ready mask=%02x startup=%d factory=%d error=%d\r\n",store.valid&255,startup,factory_valid,last_error);
}
uint16_t GigeUserSets_Read(uint32_t a,uint32_t *v)
{
    switch(a) {
    case GIGE_F_USERSET_AVAILABLE:*v=store.available && factory_valid;break;
    case GIGE_F_USERSET_BUSY:*v=GigeUserSets_Busy();break;
    case GIGE_F_USERSET_ERROR:*v=(uint32_t)last_error;break;
    case GIGE_F_USERSET_SELECTOR:*v=selector;break;
    case GIGE_F_USERSET_DEFAULT:*v=startup;break;
    case GIGE_F_USERSET_VALID:*v=selector==0?factory_valid:((store.valid>>selector)&1U);break;
    case GIGE_F_USERSET_MASK:*v=store.valid&255U;break;
    case GIGE_F_FACTORY_UNLOCKED:*v=GigeVision_FactoryAccess(device);break;
    case GIGE_F_USERSET_LOAD:case GIGE_F_USERSET_SAVE:case GIGE_F_FACTORY_SAVE:*v=GigeUserSets_Busy()?1:0;break;
    default:return GIGE_STATUS_INVALID_ADDRESS;
    }
    return 0;
}
uint16_t GigeUserSets_Write(uint32_t a,uint32_t v)
{
    if(a==GIGE_F_FACTORY_LOCK) {if(v!=1)return GIGE_STATUS_INVALID_PARAM;GigeVision_LockFactory(device);return 0;}
    if(!store.available || !factory_valid)return GIGE_STATUS_NOT_IMPLEMENTED;
    if(GigeUserSets_Busy() || GigeCamera_Busy() || !GigeCamera_Ready() || device->locked || device->active)return GIGE_STATUS_BUSY;
    if(a==GIGE_F_USERSET_SELECTOR){if(v>7)return GIGE_STATUS_INVALID_PARAM;selector=v;return 0;}
    if(a!=GIGE_F_USERSET_DEFAULT && a!=GIGE_F_USERSET_LOAD && a!=GIGE_F_USERSET_SAVE && a!=GIGE_F_FACTORY_SAVE)return GIGE_STATUS_WRITE_PROTECT;
    if(a==GIGE_F_USERSET_SAVE && !selector)return GIGE_STATUS_WRITE_PROTECT;
    if(a==GIGE_F_FACTORY_SAVE && (selector || !GigeVision_FactoryAccess(device)))return GIGE_STATUS_ACCESS_DENIED;
    if(a==GIGE_F_USERSET_DEFAULT) {
        if(v>7)return GIGE_STATUS_INVALID_PARAM;
        if(v && !(store.valid&(1U<<v)))return GIGE_STATUS_INVALID_PARAM;
        pending_slot=v;pending=3;
    } else {
        if(v!=1)return GIGE_STATUS_INVALID_PARAM;
        if(a==GIGE_F_USERSET_LOAD && selector && !(store.valid&(1U<<selector)))return GIGE_STATUS_INVALID_PARAM;
        pending_slot=selector;pending=a==GIGE_F_USERSET_LOAD?2:1;
    }
    if(!pending_slot && !GigeVision_FactoryAccess(device)){pending=0;return GIGE_STATUS_ACCESS_DENIED;}
    last_error=0;return 0;
}
void GigeUserSets_Poll(void)
{
    if(!store.available)return;
    if(!writing && !pending && device->config_dirty && !device->active && !device->locked &&
       !GigeCamera_Busy() && GigeVisionBoard_NowUs()-device->config_changed>=250000U) {
        if(!GigeVision_ConfigValid(&device->config)){config_error=-20;device->config_dirty=0;return;}
        /* Repeated SDK writes of unchanged settings must not wear NOR. */
        if(store.valid&(1U<<9)) {
            GigeDeviceConfig old;uint32_t size=sizeof(old);
            if(!GigeUserStore_Load(&store,9,&old,&size) && size==sizeof(old) && !memcmp(&old,&device->config,size)) {
                device->config_dirty=0;config_error=0;return;
            }
        }
        if(GigeUserStore_Save(&store,9,&device->config,sizeof(device->config))){config_error=-21;return;}
        writing=4;config_error=0;device->config_dirty=0;
    }
    if(pending) {
        /* Keep the queued operation until cancellation releases the sensor bus. */
        if((pending==1 || pending==2) && !GigeCamera_SensorAvailable())return;
        uint8_t operation=pending;pending=0;
        if(!pending_slot && !GigeVision_FactoryAccess(device)){last_error=-13;return;}
        if(operation==2) {last_error=load(pending_slot);return;}
        uint8_t bytes[GIGE_USERSTORE_MAX];uint32_t count=0;
        if(operation==1) {
            GigePreset preset;
            if(GigeCamera_Export(&preset)){last_error=-10;return;}
            count=GigePreset_Encode(&preset,bytes,sizeof(bytes));
        } else {bytes[0]=(uint8_t)pending_slot;bytes[1]=bytes[2]=bytes[3]=0;count=4;}
        if(!count || GigeUserStore_Save(&store,operation==3?8:pending_slot,bytes,count)){last_error=-14;return;}
        writing=operation;
    }
    if(writing) {
        GigeUserStore_Poll(&store,GigeVisionBoard_NowUs());
        if(!store.state){if(writing==4)config_error=store.error;else last_error=store.error;if(!store.error && writing==3)startup=pending_slot;writing=0;}
    }
}
