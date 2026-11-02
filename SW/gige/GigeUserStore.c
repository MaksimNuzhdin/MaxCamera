#include "GigeUserStore.h"
#include <string.h>
#define SECTOR 65536U
#define COMMIT 0x54494d43U
static const uint8_t magic[8]={'M','A','X','U','S','E','T','1'};
static const uint8_t partition[32]={'M','A','X','C','A','M','S','E','T','S','0','1',0xe0,1,32,0,9,2,0,0,0,8,0,0,0,0,0,0,0x4d,0x43,1,0};
static uint32_t get(const uint8_t *p) {return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void put(uint8_t *p,uint32_t v) {for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(v>>(i*8));}
uint32_t GigeUserStore_Crc(const void *data,uint32_t n)
{
    const uint8_t *p=(const uint8_t*)data;uint32_t crc=~0U;
    while(n--){crc^=*p++;for(unsigned k=0;k<8;++k)crc=(crc>>1)^(0xedb88320U&(0U-(crc&1U)));}
    return ~crc;
}
static uint32_t address(unsigned slot,unsigned bank) {return GIGE_USERSTORE_BASE+(1+slot*2+bank)*SECTOR;}
/* Invalid/torn records are not errors; transport failures are. */
static int record(GigeUserStore *s,unsigned slot,unsigned bank,uint8_t *data,uint32_t *length,uint64_t *generation,int committed)
{
    uint8_t header[64];uint32_t base=address(slot,bank);
    if(s->io.read(s->io.user,base,header,sizeof(header)))return -1;
    uint32_t n=get(header+16);
    if(memcmp(header,magic,8) || get(header+8)!=1 || get(header+12)!=slot ||
       !n || n>GIGE_USERSTORE_MAX || get(header+20)!=GIGE_USERSTORE_SCHEMA ||
       get(header+36)!=GigeUserStore_Crc(header,36) ||
       get(header+60)!=(committed?COMMIT:~0U))return 0;
    uint64_t gen=(uint64_t)get(header+24)|((uint64_t)get(header+28)<<32);
    if(!gen)return 0;
    if(s->io.read(s->io.user,base+256,data,n))return -1;
    if(get(header+32)!=GigeUserStore_Crc(data,n))return 0;
    *length=n;*generation=gen;return 1;
}
int GigeUserStore_Init(GigeUserStore *s,const GigeUserStoreIO *io)
{
    if(!s || !io || !io->read || !io->program || !io->erase || !io->busy)return -1;
    memset(s,0,sizeof(*s));s->io=*io;memset(s->bank,-1,sizeof(s->bank));
    if(io->busy(io->user)!=0)return s->error=-1;
    uint8_t page[256];
    if(io->read(io->user,GIGE_USERSTORE_BASE,page,sizeof(page)))return s->error=-1;
    s->owned=memcmp(page,partition,sizeof(partition))==0;
    if(!s->owned) {
        /* Never claim an occupied/unknown tail partition, even on factory unlock. */
        for(uint32_t off=0;off<GIGE_USERSTORE_BYTES;off+=GIGE_USERSTORE_MAX) {
            if(io->read(io->user,GIGE_USERSTORE_BASE+off,s->buffer,GIGE_USERSTORE_MAX))return s->error=-1;
            for(unsigned j=0;j<GIGE_USERSTORE_MAX;++j)if(s->buffer[j]!=255)return s->error=-2;
        }
    } else {
        for(unsigned slot=0;slot<GIGE_USERSTORE_SLOTS;++slot)for(unsigned bank=0;bank<2;++bank) {
            uint32_t length=0;uint64_t gen=0;
            int valid=record(s,slot,bank,s->buffer,&length,&gen,1);
            if(valid<0)return s->error=-1;
            if(valid && gen>s->generation[slot]) {s->generation[slot]=gen;s->bank[slot]=(int8_t)bank;s->lengths[slot]=(uint16_t)length;s->valid|=(uint16_t)(1U<<slot);}
        }
    }
    s->available=1;return 0;
}
int GigeUserStore_Load(GigeUserStore *s,unsigned slot,void *data,uint32_t *length)
{
    if(!s || !s->available || s->state || slot>=GIGE_USERSTORE_SLOTS || !data || !length || !(s->valid&(1U<<slot)) || *length<s->lengths[slot])return -1;
    uint64_t generation=0;uint32_t n=0;
    int valid=record(s,slot,(unsigned)s->bank[slot],s->buffer,&n,&generation,1);
    if(valid!=1 || generation!=s->generation[slot])return s->error=-3;
    memcpy(data,s->buffer,n);*length=n;return 0;
}
int GigeUserStore_Save(GigeUserStore *s,unsigned slot,const void *data,uint32_t length)
{
    if(!s || !s->available || s->state || slot>=GIGE_USERSTORE_SLOTS || !data || !length || length>GIGE_USERSTORE_MAX || s->generation[slot]==UINT64_MAX || s->io.busy(s->io.user)!=0)return -1;
    memset(s->buffer,255,sizeof(s->buffer));memcpy(s->buffer,magic,8);
    put(s->buffer+8,1);put(s->buffer+12,slot);put(s->buffer+16,length);put(s->buffer+20,GIGE_USERSTORE_SCHEMA);
    uint64_t gen=s->generation[slot]+1;put(s->buffer+24,(uint32_t)gen);put(s->buffer+28,(uint32_t)(gen>>32));
    put(s->buffer+32,GigeUserStore_Crc(data,length));put(s->buffer+36,GigeUserStore_Crc(s->buffer,36));
    memcpy(s->buffer+256,data,length);s->total=256+length;s->position=0;
    s->slot=(uint8_t)slot;s->target=(uint8_t)(s->bank[slot]==0?1:0);s->error=0;s->state=s->owned?3:1;return 0;
}
void GigeUserStore_Poll(GigeUserStore *s,uint64_t now)
{
    if(!s || !s->state)return;
    uint32_t base=address(s->slot,s->target);int e=0;
    if((s->state&1U)==0) {
        int busy=s->io.busy(s->io.user);
        if(busy<0 || (busy && now>=s->deadline)){s->error=-4;s->state=0;return;}
        if(busy)return;
    }
    switch(s->state) {
    case 1:e=s->io.program(s->io.user,GIGE_USERSTORE_BASE,partition,sizeof(partition));s->deadline=now+500000;s->state=2;break;
    case 2: {
        uint8_t value[32];e=s->io.read(s->io.user,GIGE_USERSTORE_BASE,value,sizeof(value));
        if(!e && memcmp(value,partition,sizeof(value)))e=-1;
        if(!e){s->owned=1;s->state=3;}break;}
    case 3:e=s->io.erase(s->io.user,base);s->deadline=now+5000000;s->state=4;break;
    case 4:s->state=5;break;
    case 5: {
        uint32_t n=s->total-s->position;if(n>256)n=256;
        e=s->io.program(s->io.user,base+s->position,s->buffer+s->position,n);
        s->position+=n;s->deadline=now+500000;s->state=6;break;}
    case 6:s->state=s->position<s->total?5:7;break;
    case 7: {
        uint8_t verify[GIGE_USERSTORE_MAX];uint32_t n=0;uint64_t gen=0;
        int valid=record(s,s->slot,s->target,verify,&n,&gen,0);
        if(valid!=1 || n!=s->total-256 || memcmp(verify,s->buffer+256,n)){e=-3;break;}
        uint8_t commit[4];put(commit,COMMIT);e=s->io.program(s->io.user,base+60,commit,4);
        s->deadline=now+500000;s->state=8;break;}
    case 8: {
        uint8_t verify[GIGE_USERSTORE_MAX];uint32_t n=0;uint64_t gen=0;
        int valid=record(s,s->slot,s->target,verify,&n,&gen,1);
        if(valid!=1){e=-3;break;}
        s->generation[s->slot]=gen;s->bank[s->slot]=(int8_t)s->target;s->lengths[s->slot]=(uint16_t)n;
        s->valid|=(uint16_t)(1U<<s->slot);s->state=0;break;}
    default:e=-1;break;
    }
    if(e){s->error=e;s->state=0;}
}
