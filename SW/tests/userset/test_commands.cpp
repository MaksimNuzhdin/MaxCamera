#include "GigeUserSets.h"
#include "GigeCamera.h"
#include "GigeFlash.h"
#include <cassert>
#include <cstring>
#include <cstdio>
static unsigned char flash[GIGE_FLASH_USER_BYTES];
static unsigned mutations;
static uint64_t clock_us=1000;
static GigePreset current;
static int camera_busy;
static int sensor_available=1;
extern "C" int GigeCamera_SensorAvailable(void){return sensor_available;}
extern "C" uint64_t GigeVisionBoard_NowUs(void){return clock_us;}
extern "C" int GigeFlash_Read(uint32_t a,void *p,uint32_t n){assert(a>=GIGE_FLASH_USER_BASE && n<=sizeof(flash)-(a-GIGE_FLASH_USER_BASE));memcpy(p,flash+a-GIGE_FLASH_USER_BASE,n);return 0;}
extern "C" int GigeFlash_Program(uint32_t a,const void *p,uint32_t n){assert(a>=GIGE_FLASH_USER_BASE && n && n<=256 && (a&255)+n<=256);auto d=(const unsigned char*)p;for(unsigned i=0;i<n;++i){auto &v=flash[a-GIGE_FLASH_USER_BASE+i];assert((v&d[i])==d[i]);v&=d[i];}++mutations;return 0;}
extern "C" int GigeFlash_Erase(uint32_t a){assert(a>=GIGE_FLASH_USER_BASE && !(a&65535));memset(flash+a-GIGE_FLASH_USER_BASE,255,65536);++mutations;return 0;}
extern "C" int GigeFlash_Busy(void){return 0;}
extern "C" int GigeCamera_Export(GigePreset *p){*p=current;return 0;}
extern "C" uint16_t GigeCamera_Import(const GigePreset *p){current=*p;return 0;}
extern "C" int GigeCamera_Ready(void){return 1;}
extern "C" int GigeCamera_Busy(void){return camera_busy;}
static void finish(){for(unsigned i=0;GigeUserSets_Busy() && i<1000;++i){clock_us+=10000;GigeUserSets_Poll();}assert(!GigeUserSets_Busy());uint32_t e;assert(!GigeUserSets_Read(GIGE_F_USERSET_ERROR,&e)&&!e);}
int main(){
 memset(flash,255,sizeof(flash));current={};current.width=640;current.height=480;current.pixel_format=GIGE_PIXEL_YUYV8;current.frame_rate=30000;current.line_period_ns=32000;
 for(unsigned i=0;i<GIGE_PRESET_COUNT;++i)current.values[i]=(uint32_t)gige_feature_descriptors[(gige_preset_addresses[i]-GIGE_FEATURE_FIRST)/4].minimum;
 GigeFpgaConfig_Default(&current.fpga);
 current.fpga.value[GIGE_ISP_MEDIAN]=1;current.fpga.value[GIGE_ISP_SCALER]=1;
 current.ae_thresholds[0]=current.ae_thresholds[3]=1;
 assert(!GigePreset_Validate(&current));
 GigeVision g;GigeIO io={};GigeVision_Init(&g,&io);GigeVision_Tick(&g,clock_us);g.privilege=2;
 assert(!GigeUserSets_InitStorage(&g));GigeUserSets_Init(&g);
 assert(GigeUserSets_Address(GIGE_F_FACTORY_SAVE));
 assert(GigeUserSets_Write(GIGE_F_USERSET_SAVE,1)==GIGE_STATUS_WRITE_PROTECT);
 assert(GigeUserSets_Write(GIGE_F_FACTORY_SAVE,1)==GIGE_STATUS_ACCESS_DENIED && !mutations);
 g.factory_until=clock_us+300000000;
 assert(GigeUserSets_Write(GIGE_F_USERSET_SAVE,1)==GIGE_STATUS_WRITE_PROTECT && !mutations);
 assert(!GigeUserSets_Write(GIGE_F_FACTORY_SAVE,1));
 sensor_available=0;GigeUserSets_Poll();assert(GigeUserSets_Busy() && !mutations);
 sensor_available=1;finish();assert(mutations);
 GigePreset original=current;current.width=320;assert(!GigeUserSets_Write(GIGE_F_USERSET_LOAD,1));
 sensor_available=0;unsigned saved_mutations=mutations;
 for(unsigned i=0;i<10;++i)GigeUserSets_Poll();
 assert(GigeUserSets_Busy() && current.width==320 && mutations==saved_mutations);
 sensor_available=1;finish();assert(!memcmp(&current,&original,sizeof(current)));
 for(unsigned slot=1;slot<=7;++slot){
  assert(!GigeUserSets_Write(GIGE_F_USERSET_SELECTOR,slot));current.width=160+slot*16;
  assert(GigeUserSets_Write(GIGE_F_FACTORY_SAVE,1)==GIGE_STATUS_ACCESS_DENIED);
  assert(!GigeUserSets_Write(GIGE_F_USERSET_SAVE,1));finish();current.width=320;
  assert(!GigeUserSets_Write(GIGE_F_USERSET_LOAD,1));finish();assert(current.width==160+slot*16);
 }
 unsigned before=mutations;assert(!GigeUserSets_Write(GIGE_F_USERSET_SELECTOR,0));
 assert(!GigeUserSets_Write(GIGE_F_FACTORY_SAVE,1));GigeVision_LockFactory(&g);GigeUserSets_Poll();
 assert(mutations==before && !GigeUserSets_Busy());
 uint32_t e;assert(!GigeUserSets_Read(GIGE_F_USERSET_ERROR,&e) && (int32_t)e==-13);
 assert(GigeUserSets_Write(GIGE_F_USERSET_LOAD,1)==GIGE_STATUS_ACCESS_DENIED);
 camera_busy=1;assert(GigeUserSets_Write(GIGE_F_USERSET_SELECTOR,1)==GIGE_STATUS_BUSY);
 puts("PASS: immutable standard Default, guarded factory maintenance save/load, all 7 user slots, revocation before commit, busy guards; actual journal/preset codec over simulated NOR");
}
