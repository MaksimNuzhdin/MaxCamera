#include "GigeFlash.h"
#include "xqspipsu.h"
#include "xil_printf.h"
#include <string.h>
static XQspiPsu device;
static uint32_t jedec;
static bool ready;
static int transfer(uint8_t *command,unsigned count,void *receive,unsigned bytes,const void *send=0)
{
    XQspiPsu_Msg messages[2]={};
    messages[0].TxBfrPtr=command;messages[0].ByteCount=count;
    messages[0].BusWidth=XQSPIPSU_SELECT_MODE_SPI;messages[0].Flags=XQSPIPSU_MSG_FLAG_TX;
    if(bytes) {
        messages[1].RxBfrPtr=(uint8_t*)receive;messages[1].TxBfrPtr=(uint8_t*)send;
        messages[1].ByteCount=bytes;messages[1].BusWidth=XQSPIPSU_SELECT_MODE_SPI;
        messages[1].Flags=receive?XQSPIPSU_MSG_FLAG_RX:XQSPIPSU_MSG_FLAG_TX;
    }
    return XQspiPsu_PolledTransfer(&device,messages,bytes?2:1)==0?0:-1;
}
static int status(uint8_t *value) {uint8_t command=5;return transfer(&command,1,value,1);}
static int enable_write()
{
    uint8_t command=6,value=0;
    if(transfer(&command,1,0,0) || status(&value) || !(value&2))return -1;
    return 0;
}
static void address_command(uint8_t *command,uint8_t opcode,uint32_t address)
{command[0]=opcode;for(unsigned i=0;i<4;++i)command[i+1]=(uint8_t)(address>>(24-8*i));}
int GigeFlash_Init(void)
{
    ready=false;jedec=0;
    XQspiPsu_Config *config=XQspiPsu_LookupConfig(0xff0f0000U);
    if(!config || XQspiPsu_CfgInitialize(&device,config,config->BaseAddress))return -1;
    if(XQspiPsu_SetOptions(&device,XQSPIPSU_MANUAL_START_OPTION) ||
       XQspiPsu_SetClkPrescaler(&device,XQSPIPSU_CLK_PRESCALE_16) ||
       XQspiPsu_SetReadMode(&device,XQSPIPSU_READMODE_IO))return -1;
    XQspiPsu_SelectFlash(&device,XQSPIPSU_SELECT_FLASH_CS_LOWER,XQSPIPSU_SELECT_FLASH_BUS_LOWER);
    uint8_t command=0x9f,id[3]={};
    if(transfer(&command,1,id,3))return -1;
    jedec=((uint32_t)id[0]<<16)|((uint32_t)id[1]<<8)|id[2];
    xil_printf("GIGE FLASH JEDEC=%06x\r\n",jedec);
    /* Qualified board part: Micron MT25QU256, 32MiB, 1.8V. No chip-erase,
     * global address-mode, quad-enable or nonvolatile configuration writes. */
    if(jedec!=0x20bb19U)return -2;
    ready=true;return 0;
}
uint32_t GigeFlash_Id(void) {return jedec;}
int GigeFlash_Read(uint32_t address,void *data,uint32_t bytes)
{
    if(!ready || !data || !bytes || address>=GIGE_FLASH_BYTES || bytes>GIGE_FLASH_BYTES-address)return -1;
    uint8_t command[5];address_command(command,0x13,address);
    return transfer(command,5,data,bytes);
}
int GigeFlash_Busy(void)
{
    uint8_t value=0;if(!ready || status(&value))return -1;
    return (value&1)?1:0;
}
int GigeFlash_Erase(uint32_t address)
{
    if(!ready || address<GIGE_FLASH_USER_BASE || address>=GIGE_FLASH_BYTES || (address&65535U) || GigeFlash_Busy()!=0)return -1;
    if(enable_write())return -1;
    uint8_t command[5];address_command(command,0xdc,address);
    return transfer(command,5,0,0);
}
int GigeFlash_Program(uint32_t address,const void *data,uint32_t bytes)
{
    if(!ready || !data || !bytes || bytes>256 || address<GIGE_FLASH_USER_BASE ||
       address>=GIGE_FLASH_BYTES || bytes>GIGE_FLASH_BYTES-address ||
       (address&255)+bytes>256 || GigeFlash_Busy()!=0)return -1;
    if(enable_write())return -1;
    uint8_t command[5];address_command(command,0x12,address);
    return transfer(command,5,0,bytes,data);
}
