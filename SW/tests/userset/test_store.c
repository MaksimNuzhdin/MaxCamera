#include "GigeUserStore.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char flash[GIGE_USERSTORE_BYTES],baseline[GIGE_USERSTORE_BYTES];
static unsigned mutations,cut_at,partial;static int power_cut,busy_ticks,stuck;
static int read_flash(void *u,uint32_t a,void *d,uint32_t n)
{(void)u;assert(a>=GIGE_USERSTORE_BASE && n<=GIGE_USERSTORE_BYTES-(a-GIGE_USERSTORE_BASE));memcpy(d,flash+a-GIGE_USERSTORE_BASE,n);return 0;}
static unsigned cut(unsigned n) {++mutations;if(cut_at==mutations){power_cut=1;return partial<n?partial:n;}return n;}
static int erase_flash(void *u,uint32_t a)
{(void)u;assert(a>=GIGE_USERSTORE_BASE && !(a&65535U));memset(flash+a-GIGE_USERSTORE_BASE,255,cut(65536));busy_ticks=3;return 0;}
static int program_flash(void *u,uint32_t a,const void *data,uint32_t n)
{
 (void)u;assert(a>=GIGE_USERSTORE_BASE && n && n<=256 && (a&255)+n<=256);
 const unsigned char *d=(const unsigned char*)data;n=cut(n);
 for(unsigned i=0;i<n;++i){unsigned char *p=&flash[a-GIGE_USERSTORE_BASE+i];assert((*p&d[i])==d[i]);*p&=d[i];}busy_ticks=2;return 0;
}
static int busy_flash(void *u) {(void)u;if(stuck)return 1;if(busy_ticks){--busy_ticks;return 1;}return 0;}
static const GigeUserStoreIO io={0,read_flash,program_flash,erase_flash,busy_flash};
static void finish(GigeUserStore *s)
{for(unsigned i=0;s->state && i<1000 && !power_cut;++i)GigeUserStore_Poll(s,(uint64_t)i*10000);assert(!s->state || power_cut);}
int main(void)
{
 GigeUserStore s;unsigned char old[1700],next[1700],value[2048];memset(old,0x42,sizeof(old));memset(next,0x85,sizeof(next));
 memset(flash,255,sizeof(flash));assert(!GigeUserStore_Init(&s,&io) && s.available && !s.owned && !s.valid);
 assert(!GigeUserStore_Save(&s,3,old,sizeof(old)));finish(&s);assert(!s.error && s.valid==(1U<<3));
 uint32_t n=sizeof(value);assert(!GigeUserStore_Load(&s,3,value,&n) && n==sizeof(old) && !memcmp(value,old,n));
 memcpy(baseline,flash,sizeof(flash));
 /* Interrupt every mutation, including partial header/payload/commit writes.
  * Previous committed data must survive; only a complete new record may win. */
 for(unsigned event=1;event<=10;++event)for(unsigned bytes=0;bytes<=260;bytes+=13) {
  memcpy(flash,baseline,sizeof(flash));busy_ticks=0;stuck=0;power_cut=0;cut_at=0;
  assert(!GigeUserStore_Init(&s,&io));mutations=0;cut_at=event;partial=bytes;
  assert(!GigeUserStore_Save(&s,3,next,sizeof(next)));finish(&s);
  busy_ticks=0;cut_at=0;power_cut=0;assert(!GigeUserStore_Init(&s,&io));n=sizeof(value);
  assert(!GigeUserStore_Load(&s,3,value,&n));assert(n==sizeof(old));
  assert(!memcmp(value,old,n) || !memcmp(value,next,n));
 }
 memcpy(flash,baseline,sizeof(flash));busy_ticks=0;assert(!GigeUserStore_Init(&s,&io));
 for(unsigned slot=0;slot<GIGE_USERSTORE_SLOTS;++slot){assert(!GigeUserStore_Save(&s,slot,next,sizeof(next)));finish(&s);assert(!s.error);}
 assert(s.valid==1023);busy_ticks=0;assert(!GigeUserStore_Init(&s,&io) && s.valid==1023);
 /* CRC failure of newest copy falls back to the older committed copy. */
 uint32_t address=GIGE_USERSTORE_BASE+(1+3*2+(unsigned)s.bank[3])*65536U;
 flash[address-GIGE_USERSTORE_BASE+256]^=1;busy_ticks=0;
 assert(!GigeUserStore_Init(&s,&io));n=sizeof(value);assert(!GigeUserStore_Load(&s,3,value,&n) && !memcmp(value,old,n));
 assert(GigeUserStore_Save(&s,10,next,1) && GigeUserStore_Save(&s,0,next,0) && GigeUserStore_Save(&s,0,next,2049));
 assert(!GigeUserStore_Save(&s,3,next,sizeof(next)));GigeUserStore_Poll(&s,0);stuck=1;GigeUserStore_Poll(&s,6000000);assert(!s.state && s.error);
 stuck=0;busy_ticks=0;memset(flash,255,sizeof(flash));flash[128]=0;mutations=0;
 assert(GigeUserStore_Init(&s,&io)!=0 && !s.available && mutations==0);
 puts("PASS: 10 journal entries, alternating copies, CRC fallback, 210 torn saves, bounds, timeout, occupied-partition refusal");
}
