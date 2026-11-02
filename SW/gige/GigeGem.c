/* GEM3 polling netif. Own DMA buffers, copy-on-send; no interrupt/BSP globals. */
#include "GigeGem.h"
#include "GigeVision.h"
#include "GigeVisionBoard.h"
#include "xemacps.h"
#include "xil_cache.h"
#include "xil_mpu.h"
#include "xreg_cortexr5.h"
#include "xparameters.h"
#include "xil_io.h"
#include "xil_printf.h"
#include "lwip/etharp.h"
#include "netif/ethernet.h"
#include <string.h>
#define RX_COUNT 64U
#define TX_COUNT 64U
#define BUFFER_SIZE 1600U
/* All descriptors in one MPU-sized uncached block. Adjacent cached descriptor
 * invalidation would otherwise lose hardware updates in another BD. */
static uint8_t descriptors[4096] __attribute__((aligned(4096)));
static volatile uint32_t *rx_bd,*tx_bd;
static uint8_t rx_buffers[RX_COUNT][BUFFER_SIZE] __attribute__((aligned(64)));
static uint8_t tx_buffers[TX_COUNT][BUFFER_SIZE] __attribute__((aligned(64)));
/* GEM TX addresses are byte-granular. Offset Ethernet by2 so IPv4 and
 * the GVSP image payload are word-aligned for the R5 memcpy fast path. */
static uint8_t *tx_frame(unsigned slot){return tx_buffers[slot]+2;}
static XEmacPs mac;
static unsigned rx_index,tx_index,tx_reap,tx_pending,phy=32;
static uint64_t next_link;
static int initialized;
unsigned g_gem_rx_packets,g_gem_tx_packets,g_gem_rx_drops,g_gem_errors;
static void barrier(void) { __asm__ volatile("dsb sy" ::: "memory"); }
static uint32_t rd(unsigned off) { return XEmacPs_ReadReg(mac.Config.BaseAddress,off); }
static void wr(unsigned off,uint32_t v) { XEmacPs_WriteReg(mac.Config.BaseAddress,off,v); }
static void reap(void)
{
    while(tx_pending && (tx_bd[2*tx_reap+1]&XEMACPS_TXBUF_USED_MASK)) {
        if(tx_bd[2*tx_reap+1]&(XEMACPS_TXBUF_RETRY_MASK|XEMACPS_TXBUF_URUN_MASK))++g_gem_errors;
        tx_reap=(tx_reap+1)%TX_COUNT;--tx_pending;
    }
}
unsigned GigeGem_TxFree(void) { reap();return TX_COUNT-tx_pending; }
/* Only this polling context mutates the TX ring. Staging a GVSP packet
 * directly in its DMA buffer removes lwIP allocation and packet copies. */
static void commit(unsigned slot,size_t bytes)
{
    if(bytes<60){memset(tx_frame(slot)+bytes,0,60-bytes);bytes=60;}
    Xil_DCacheFlushRange((INTPTR)tx_buffers[slot],BUFFER_SIZE);
    /* Do not release descriptor ownership before complete packet data is visible. */
    barrier();
    tx_bd[2*slot+1]=(slot==TX_COUNT-1?XEMACPS_TXBUF_WRAP_MASK:0)|XEMACPS_TXBUF_LAST_MASK|(uint32_t)bytes;
    barrier();tx_index=(slot+1)%TX_COUNT;++tx_pending;++g_gem_tx_packets;
    wr(XEMACPS_NWCTRL_OFFSET,rd(XEMACPS_NWCTRL_OFFSET)|XEMACPS_NWCTRL_STARTTX_MASK);
}
static int stream_mac(struct netif *netif,uint32_t destination,uint8_t *out)
{
    ip4_addr_t target;target.addr=lwip_htonl(destination);
    if(ip4_addr_ismulticast(&target)) {
        out[0]=1;out[1]=0;out[2]=0x5e;out[3]=(destination>>16)&0x7f;out[4]=destination>>8;out[5]=destination;return 1;
    }
    if(ip4_addr_isbroadcast(&target,netif)){memset(out,255,6);return 1;}
    if(!ip4_addr_netcmp(&target,netif_ip4_addr(netif),netif_ip4_netmask(netif))) {
        if(ip4_addr_isany_val(*netif_ip4_gw(netif)))return 0;
        target=*netif_ip4_gw(netif);
    }
    struct eth_addr *mac_address;const ip4_addr_t *resolved;
    if(etharp_find_addr(netif,&target,&mac_address,&resolved)>=0){memcpy(out,mac_address->addr,6);return 1;}
    static uint64_t next_arp;
    uint64_t t=GigeVisionBoard_NowUs();if(t>=next_arp){next_arp=t+100000;etharp_request(netif,&target);}
    return 0;
}
uint8_t *GigeGem_StreamBuffer(struct netif *netif,uint32_t destination)
{
    uint8_t destination_mac[6];
    if(!netif_is_link_up(netif)||!netif_ip4_addr(netif)->addr||GigeGem_TxFree()<4||!stream_mac(netif,destination,destination_mac))return 0;
    return tx_frame(tx_index)+42;
}
int GigeGem_SendStream(struct netif *netif,uint32_t destination,uint16_t port,const uint8_t *data,size_t bytes,int df)
{
    if(bytes>1472 || data!=tx_frame(tx_index)+42 || !netif_is_link_up(netif))return -1;
    unsigned slot=tx_index;uint8_t *f=tx_frame(slot);
    if(!stream_mac(netif,destination,f))return -1;
    memcpy(f+6,netif->hwaddr,6);Gige_Put16(f+12,0x0800);
    memset(f+14,0,28);f[14]=0x45;Gige_Put16(f+16,(uint16_t)(bytes+28));
    static uint16_t ip_id;Gige_Put16(f+18,ip_id++);Gige_Put16(f+20,df?0x4000:0);
    f[22]=64;f[23]=17;Gige_Put32(f+26,lwip_ntohl(netif_ip4_addr(netif)->addr));Gige_Put32(f+30,destination);
    uint32_t sum=0;for(unsigned i=14;i<34;i+=2)sum+=Gige_Get16(f+i);
    while(sum>>16)sum=(sum&65535)+(sum>>16);
    Gige_Put16(f+24,(uint16_t)~sum);
    Gige_Put16(f+34,GIGE_UNIT_STREAM_SOURCE_PORT);Gige_Put16(f+36,port);Gige_Put16(f+38,(uint16_t)(bytes+8));
    // UDP checksum zero is valid for IPv4; MAC still appends Ethernet FCS.
    commit(slot,bytes+42);return 0;
}
static err_t output(struct netif *netif,struct pbuf *p)
{
    (void)netif;reap();
    if(tx_pending==TX_COUNT)return ERR_MEM;
    if(p->tot_len>1514)return ERR_IF;
    unsigned slot=tx_index;size_t bytes=p->tot_len;
    if(pbuf_copy_partial(p,tx_frame(slot),p->tot_len,0)!=p->tot_len)return ERR_IF;
    if(bytes<60) {memset(tx_frame(slot)+bytes,0,60-bytes);bytes=60;}
    commit(slot,bytes);

    return ERR_OK;
}
err_t GigeGem_Init(struct netif *netif)
{
    XEmacPs_Config *cfg=XEmacPs_LookupConfig(XPAR_XEMACPS_0_BASEADDR);
    if(!cfg || XEmacPs_CfgInitialize(&mac,cfg,cfg->BaseAddress))return ERR_IF;
    XEmacPs_SetMdioDivisor(&mac,MDC_DIV_224);
    for(phy=0;phy<32;++phy) {
        u16 id1=0,id2=0;
        if(!XEmacPs_PhyRead(&mac,phy,2,&id1) && !XEmacPs_PhyRead(&mac,phy,3,&id2) &&
            id1 && id1!=0xffff && id2 && id2!=0xffff) {
            xil_printf("GIGE PHY addr=%d id=%04x:%04x\r\n",phy,id1,id2);break;
        }
    }
    if(phy==32)return ERR_IF;
    /* Preserve board-strapped RGMII delays. Advertise full-duplex 1G only:
     * FSBL configures GEM3 IOPLL/12=125MHz; no guessed 10/100 clock divisors. */
    if(XEmacPs_PhyWrite(&mac,phy,4,0x0001) || XEmacPs_PhyWrite(&mac,phy,9,0x0200) ||
       XEmacPs_PhyWrite(&mac,phy,0,0x1200))return ERR_IF;
    Xil_DCacheFlushRange((INTPTR)descriptors,sizeof(descriptors));
    /* R5 MPU: shareable normal noncacheable, full RW (same attributes as BSP DMA examples). */
    if(Xil_SetMPURegion((INTPTR)descriptors,sizeof(descriptors),(NORM_SHARED_NCACHE|PRIV_RW_USER_RW))!=XST_SUCCESS)return ERR_IF;
    rx_bd=(volatile uint32_t *)descriptors;tx_bd=(volatile uint32_t *)(descriptors+1024);
    for(unsigned i=0;i<RX_COUNT;++i) {
        rx_bd[2*i]=(uint32_t)(UINTPTR)rx_buffers[i]|(i==RX_COUNT-1?XEMACPS_RXBUF_WRAP_MASK:0);
        rx_bd[2*i+1]=0;
    }
    for(unsigned i=0;i<TX_COUNT;++i) {
        tx_bd[2*i]=(uint32_t)(UINTPTR)tx_frame(i);
        tx_bd[2*i+1]=XEMACPS_TXBUF_USED_MASK|(i==TX_COUNT-1?XEMACPS_TXBUF_WRAP_MASK:0);
    }
    Xil_DCacheInvalidateRange((INTPTR)rx_buffers,sizeof(rx_buffers));barrier();
    XEmacPs_SetQueuePtr(&mac,(UINTPTR)rx_bd,0,XEMACPS_RECV);
    XEmacPs_SetQueuePtr(&mac,(UINTPTR)tx_bd,0,XEMACPS_SEND);
    /* GEM3 has unused priority queues. Give each a stopped descriptor. */
    for(unsigned i=1;i<mac.MaxQueues;++i) {
        volatile uint32_t *dummy=(volatile uint32_t *)(descriptors+2048+16*i);
        dummy[0]=0;dummy[1]=XEMACPS_TXBUF_USED_MASK|XEMACPS_TXBUF_WRAP_MASK;
        XEmacPs_SetQueuePtr(&mac,(UINTPTR)dummy,(u8)i,XEMACPS_SEND);
        dummy[2]=XEMACPS_RXBUF_NEW_MASK|XEMACPS_RXBUF_WRAP_MASK;dummy[3]=0;
        XEmacPs_SetQueuePtr(&mac,(UINTPTR)(dummy+2),(u8)i,XEMACPS_RECV);
    }
    netif->name[0]='g';netif->name[1]='e';netif->hwaddr_len=6;
    GigeVisionBoard_Mac(netif->hwaddr);
    netif->mtu=1500;netif->flags=NETIF_FLAG_BROADCAST|NETIF_FLAG_ETHARP|NETIF_FLAG_ETHERNET;
    netif->output=etharp_output;netif->linkoutput=output;netif->hostname="maxcamera";
    if(XEmacPs_SetMacAddress(&mac,netif->hwaddr,1))return ERR_IF;
    XEmacPs_SetOperatingSpeed(&mac,1000);
    XEmacPs_ClearOptions(&mac,XEMACPS_JUMBO_ENABLE_OPTION);
    /* Keep FCS insertion enabled; input pbuf excludes received FCS via FCS_STRIP. */
    XEmacPs_SetOptions(&mac,XEMACPS_FCS_INSERT_OPTION|XEMACPS_FCS_STRIP_OPTION|
                       XEMACPS_TRANSMITTER_ENABLE_OPTION|XEMACPS_RECEIVER_ENABLE_OPTION);
    wr(XEMACPS_DMACR_OFFSET,(rd(XEMACPS_DMACR_OFFSET)&~(XEMACPS_DMACR_RXBUF_MASK|
        XEMACPS_DMACR_TCPCKSUM_MASK))|((BUFFER_SIZE/64U)<<XEMACPS_DMACR_RXBUF_SHIFT));
    XEmacPs_Start(&mac);XEmacPs_IntDisable(&mac,XEMACPS_IXR_ALL_MASK);
    for(unsigned i=1;i<mac.MaxQueues;++i)XEmacPs_IntQiDisable(&mac,(u8)i,XEMACPS_INTQ_IXR_ALL_MASK);
    initialized=1;return ERR_OK;
}
void GigeGem_Poll(struct netif *netif)
{
    if(!initialized)return;
    reap();
    for(unsigned budget=0;budget<32;++budget) {
        unsigned slot=rx_index;
        if(!(rx_bd[2*slot]&XEMACPS_RXBUF_NEW_MASK))break;
        barrier();uint32_t status=rx_bd[2*slot+1];unsigned length=status&XEMACPS_RXBUF_LEN_MASK;
        if((status&(XEMACPS_RXBUF_SOF_MASK|XEMACPS_RXBUF_EOF_MASK))==
            (XEMACPS_RXBUF_SOF_MASK|XEMACPS_RXBUF_EOF_MASK) && length>=14 && length<=1514) {
            Xil_DCacheInvalidateRange((INTPTR)rx_buffers[slot],BUFFER_SIZE);
            struct pbuf *p=pbuf_alloc(PBUF_RAW,(u16_t)length,PBUF_POOL);
            if(p && pbuf_take(p,rx_buffers[slot],(u16_t)length)==ERR_OK) {
                ++g_gem_rx_packets;if(netif->input(p,netif)!=ERR_OK)pbuf_free(p);
            } else {if(p)pbuf_free(p);++g_gem_rx_drops;}
        } else ++g_gem_rx_drops;
        rx_bd[2*slot+1]=0;barrier();rx_bd[2*slot]&=~XEMACPS_RXBUF_NEW_MASK;barrier();
        rx_index=(slot+1)%RX_COUNT;
    }
    uint32_t errors=rd(XEMACPS_RXSR_OFFSET);
    if(errors) {if(errors&~XEMACPS_RXSR_FRAMERX_MASK)++g_gem_errors;wr(XEMACPS_RXSR_OFFSET,errors);}
    uint64_t now=GigeVisionBoard_NowUs();
    if(now>=next_link) {
        next_link=now+500000U;u16 bmsr=0,partner=0;
        XEmacPs_PhyRead(&mac,phy,1,&bmsr);XEmacPs_PhyRead(&mac,phy,1,&bmsr);
        XEmacPs_PhyRead(&mac,phy,10,&partner);
        int up=(bmsr&0x24)==0x24 && (partner&0x0800);
        if(up && !netif_is_link_up(netif)) {netif_set_link_up(netif);xil_printf("GIGE LINK 1000 full duplex\r\n");}
        else if(!up && netif_is_link_up(netif)) {netif_set_link_down(netif);xil_printf("GIGE LINK DOWN\r\n");}
    }
}

int GigeGem_SetMac(struct netif *netif,const uint8_t *address)
{
    if(!initialized)return -1;
    if(!memcmp(netif->hwaddr,address,6))return 0;
    /* The Xilinx driver rejects address changes while started. DeviceReset
     * already released acquisition/ownership; discard old ring contents and
     * restart both DMA queues consistently with their software cursors. */
    XEmacPs_Stop(&mac);
    int result=XEmacPs_SetMacAddress(&mac,(void*)address,1);
    for(unsigned i=0;i<RX_COUNT;++i){rx_bd[2*i]=(uint32_t)(UINTPTR)rx_buffers[i]|(i==RX_COUNT-1?XEMACPS_RXBUF_WRAP_MASK:0);rx_bd[2*i+1]=0;}
    for(unsigned i=0;i<TX_COUNT;++i){tx_bd[2*i]=(uint32_t)(UINTPTR)tx_frame(i);tx_bd[2*i+1]=XEMACPS_TXBUF_USED_MASK|(i==TX_COUNT-1?XEMACPS_TXBUF_WRAP_MASK:0);}
    rx_index=tx_index=tx_reap=tx_pending=0;
    Xil_DCacheInvalidateRange((INTPTR)rx_buffers,sizeof(rx_buffers));barrier();
    XEmacPs_SetQueuePtr(&mac,(UINTPTR)rx_bd,0,XEMACPS_RECV);
    XEmacPs_SetQueuePtr(&mac,(UINTPTR)tx_bd,0,XEMACPS_SEND);
    if(!result)memcpy(netif->hwaddr,address,6);
    XEmacPs_Start(&mac);XEmacPs_IntDisable(&mac,XEMACPS_IXR_ALL_MASK);
    for(unsigned i=1;i<mac.MaxQueues;++i)XEmacPs_IntQiDisable(&mac,(u8)i,XEMACPS_INTQ_IXR_ALL_MASK);
    return result?-1:0;
}
