#include "GigeVision.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static GigeVision g;
static uint8_t q[128],ack[524];
static GigePeer owner={0x0a000007,40000};
static uint16_t command(uint16_t code,unsigned n)
{static uint16_t id=1;int broadcast;q[0]=0x42;q[1]=1;Gige_Put16(q+2,code);Gige_Put16(q+4,n);Gige_Put16(q+6,id++);assert(GigeVision_Control(&g,owner,q,n+8,ack,&broadcast)>=8);return Gige_Get16(ack);}
static uint16_t write(uint32_t a,uint32_t v)
{Gige_Put32(q+8,a);Gige_Put32(q+12,v);return command(GIGE_CMD_WRITEREG,8);}
int main(void)
{
 GigeIO io={0};GigeVision_Init(&g,&io);assert(GigeVision_ConfigValid(&g.config));
 assert(!write(GIGE_REG_CCP,2));assert(!write(GIGE_REG_IP_CURRENT,1) && g.config.mode==5);
 assert(write(GIGE_REG_IP_CURRENT,8)==GIGE_STATUS_INVALID_PARAM);
 assert(!write(GIGE_REG_PERSISTENT_IP,0x0a000003));assert(!write(GIGE_REG_PERSISTENT_GATEWAY,0x0a00008a));assert(GigeVision_ConfigValid(&g.config));
 assert(write(GIGE_REG_PERSISTENT_MASK,0xff00ff00)==GIGE_STATUS_INVALID_PARAM);
 assert(write(GIGE_REG_PERSISTENT_IP,0x7f000001)==GIGE_STATUS_INVALID_PARAM);
 assert(write(GIGE_REG_PERSISTENT_GATEWAY,0xe0000001)==GIGE_STATUS_INVALID_PARAM);
 GigeDeviceConfig c=g.config;c.mac_high=0x300;assert(!GigeVision_ConfigValid(&c));c=g.config;c.mask=0xff00ff00;assert(!GigeVision_ConfigValid(&c));
 c=g.config;c.gateway=0xc0a80101;assert(!GigeVision_ConfigValid(&c));c=g.config;memset(c.serial,'X',16);assert(!GigeVision_ConfigValid(&c));
 Gige_Put32(q+8,0xe8);memset(q+12,0,16);memcpy(q+12,"Inspection-1",12);assert(!command(GIGE_CMD_WRITEMEM,20) && !strcmp(g.config.user_name,"Inspection-1"));
 Gige_Put32(q+8,0x13000);Gige_Put16(q+12,0);Gige_Put16(q+14,64);assert(!command(GIGE_CMD_READMEM,8));uint8_t config[64];memcpy(config,ack+12,64);
 Gige_Put32(q+8,0x13000);memcpy(q+12,config,64);assert(command(GIGE_CMD_WRITEMEM,68)==GIGE_STATUS_ACCESS_DENIED);
 Gige_Put32(q+8,0x12000);memset(q+12,0,32);memcpy(q+12,"DinxTechCam",11);assert(!command(GIGE_CMD_WRITEMEM,36));
 Gige_Put32(q+8,0x13000);memcpy(q+12,config,64);q[72]=0;q[73]=0;q[74]=0;q[75]=1;assert(!command(GIGE_CMD_WRITEMEM,68) && g.config.provisioned);
 /* WRITEMEM mapped words share register validation, including access guards. */
 Gige_Put32(q+8,GIGE_REG_HEARTBEAT);Gige_Put32(q+12,5000);assert(!command(GIGE_CMD_WRITEMEM,8) && g.heartbeat==5000);
 Gige_Put32(q+8,GIGE_XML_BASE);Gige_Put32(q+12,0);assert(command(GIGE_CMD_WRITEMEM,8)==GIGE_STATUS_WRITE_PROTECT);
 g.active=1;assert(write(GIGE_REG_PERSISTENT_IP,0x0a000004)==GIGE_STATUS_BUSY);
 puts("PASS: persistent network staging, mode masks, identity validation, DeviceUserID, password-gated atomic provisioning and streaming locks");
}
