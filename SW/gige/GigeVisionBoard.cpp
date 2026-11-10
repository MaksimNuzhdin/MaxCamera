#include "GigeVisionBoard.h"
#include "GigeVision.h"
#include "GigeGem.h"
#include "GigeCamera.h"
#include "GigeRemapControl.h"
#include "GigeAI.h"
#include "GigeHttp.h"
#include "xil_mpu.h"
#include "xreg_cortexr5.h"
#include "GigeFlash.h"
#include "GigeUserSets.h"
#include "GigeFeatureRegisters.h"
#include "MaxFanController.h"
#include "TempController.h"
#include "LedsController.h"
#include "UART_driver.h"
#include "MaxTimer.h"
#include <math.h>
extern MAX_FAN_CONTROLLER g_fanController;
extern TEMP_CONTROLLER g_tempController;
extern LEDS_CONTROLLER g_ledsController;
extern UART_DRIVER g_uart;
extern MAX_TIMER g_maxTimer;
#include "xil_io.h"
#include "xil_cache.h"
#include "xil_printf.h"
#include "xparameters.h"
extern "C" {
#include "lwip/init.h"
#include "lwip/udp.h"
#include "lwip/timeouts.h"
#include "lwip/dhcp.h"
#include "lwip/autoip.h"
#include "netif/ethernet.h"
}
#if MAX_CAMERA_FLASH_PROBE
extern "C" {uint8_t g_flash_backup[GIGE_FLASH_BYTES] __attribute__((aligned(64)));}
#endif
static GigeVision camera;
static struct netif interface;
static struct udp_pcb *control,*stream;
static uint64_t clock_ticks;
static uint32_t last_counter;
static int clock_initialized,network_ready,forced;
static uint64_t next_log,next_recovery,device_boot_us;
static int reset_pending,reset_error;
extern "C" {uint32_t g_gige_resend_datagrams,g_gige_resend_length,g_gige_resend_status;uint8_t g_gige_last_resend[32];}
uint64_t GigeVisionBoard_NowUs(void)
{
    uint32_t count=Xil_In32(XPAR_XTMRCTR_0_BASEADDR+8);
    if(!clock_initialized) {last_counter=count;clock_initialized=1;}
    clock_ticks+=(uint32_t)(count-last_counter);last_counter=count;
    return clock_ticks/(XPAR_XTMRCTR_0_CLOCK_FREQUENCY/1000000U);
}
extern "C" u32_t sys_now(void) {return (u32_t)(GigeVisionBoard_NowUs()/1000U);}
static ip_addr_t address(uint32_t ip)
{
    ip_addr_t a;IP_ADDR4(&a,(ip>>24)&255,(ip>>16)&255,(ip>>8)&255,ip&255);return a;
}
static void status(struct netif *n)
{
    uint32_t ip=lwip_ntohl(netif_ip4_addr(n)->addr);
    uint32_t mask=lwip_ntohl(netif_ip4_netmask(n)->addr),gw=lwip_ntohl(netif_ip4_gw(n)->addr);
    uint32_t mode=forced?0:(dhcp_supplied_address(n)?GIGE_IP_DHCP:
                       (autoip_supplied_address(n)?GIGE_IP_LINK_LOCAL:GIGE_UNIT_IP_CONFIG));
    GigeVision_SetAddress(&camera,ip,mask,gw,mode);
    xil_printf("GIGE ADDRESS %d.%d.%d.%d mask=%08x gateway=%08x mode=%d\r\n",ip>>24,(ip>>16)&255,(ip>>8)&255,ip&255,mask,gw,mode);
}
static uint8_t *stream_buffer(void *) {return GigeGem_StreamBuffer(&interface,camera.destination);}
static int send_stream(void *,uint32_t ip,uint16_t port,const uint8_t *data,size_t bytes,int df)
{return GigeGem_SendStream(&interface,ip,port,data,bytes,df);}

int GigeVisionBoard_StreamIndicatorActive(void)
{return camera.active && !reset_pending && GigeCamera_Ready();}
int GigeVisionBoard_Streaming(void) {return camera.active || GigeCamera_Busy();}
int GigeVisionBoard_Ready(void)
{
    return network_ready && control && stream && netif_is_link_up(&interface) &&
           netif_ip4_addr(&interface)->addr!=0;
}
static uint16_t feature_read(void *,uint32_t a,uint32_t *v)
{
    if(GigeAI_Address(a))return GigeAI_Read(a,v);
    if(GigeUserSets_Address(a))return GigeUserSets_Read(a,v);
    switch(a) {
    case GIGE_F_FPGA_BLACK_LIVE_ACTIVE:
        if(reset_pending || GigeUserSets_Busy()){*v=0;return 0;}
        return GigeCamera_FeatureRead(a,v);
    case GIGE_F_RESEND_REQUESTS:*v=camera.resend_requests;return 0;
    case GIGE_F_RESENT_PACKETS:*v=camera.resent_packets;return 0;
    case GIGE_F_RESEND_MISSING:*v=camera.resend_unavailable;return 0;
    case GIGE_F_RESEND_QUEUE_FULL:*v=camera.resend_overflow;return 0;
    case GIGE_F_TELEMETRY_DEFERRED:*v=GigeCamera_TelemetryDeferred();return 0;
    case GIGE_F_RECOVERIES:*v=g_camera_recoveries;return 0;
    case GIGE_F_CONFIG_PENDING:*v=GigeUserSets_ConfigPending();return 0;
    case GIGE_F_CONFIG_ERROR:*v=(uint32_t)GigeUserSets_ConfigError();return 0;
    case GIGE_F_DEVICE_PROVISIONED:*v=camera.config.provisioned;return 0;
    case GIGE_F_RECOVERY_BUSY:*v=reset_pending!=0;return 0;
    case GIGE_F_RECOVERY_ERROR:*v=(uint32_t)reset_error;return 0;
    case GIGE_F_SENSOR_READY:*v=GigeCamera_Ready();return 0;
    case GIGE_F_SENSOR_BUSY:*v=GigeCamera_Busy() || GigeUserSets_Busy();return 0;
    case GIGE_F_GIGE_READY:*v=GigeVisionBoard_Ready();return 0;
    case GIGE_F_LINK_UP:*v=netif_is_link_up(&interface)!=0;return 0;
    case GIGE_F_TEMPERATURE: {
        float temperature=0;if(temp_driver_get_temp(&g_tempController,&temperature)!=MAX_RESULT_OK)return 0x8008;
        *v=(uint32_t)(int32_t)lroundf(temperature*1000.0f);return 0;}
    case GIGE_F_FAN_THRESHOLD:return maxFanController_ReadReg(&g_fanController,MAX_FAN_CONTROLLER_THRESHOLD,v)==MAX_RESULT_OK?0:0x8008;
    case GIGE_F_FAN_FLOOR:return maxFanController_ReadReg(&g_fanController,MAX_FAN_CONTROLLER_FLOOR_THRESHOLD,v)==MAX_RESULT_OK?0:0x8008;
    case GIGE_F_FAN_DUTY:return maxFanController_ReadReg(&g_fanController,MAX_FAN_CONTROLLER_DUTY_CYCLE,v)==MAX_RESULT_OK?0:0x8008;
    case GIGE_F_LED_STATE:*v=g_ledsController.m_correntState&15;return 0;
    case GIGE_F_UPTIME:*v=(uint32_t)((GigeVisionBoard_NowUs()-device_boot_us)/1000000U);return 0;
    case GIGE_F_CAPTURED:*v=g_camera_completed;return 0;
    case GIGE_F_DROPPED:*v=g_camera_dropped;return 0;
    case GIGE_F_CSI_ERRORS:*v=g_camera_csi_errors;return 0;
    case GIGE_F_SENT:*v=camera.frames_sent;return 0;
    case GIGE_F_TX_ERRORS:*v=camera.send_errors;return 0;
    case GIGE_F_RX_PACKETS:*v=g_gem_rx_packets;return 0;
    case GIGE_F_TX_PACKETS:*v=g_gem_tx_packets;return 0;
    case GIGE_F_GEM_ERRORS:*v=g_gem_errors;return 0;
    case GIGE_F_RX_DROPS:*v=g_gem_rx_drops;return 0;
    case GIGE_F_UART_BAUD:*v=g_uart.m_baundRate;return 0;
    case GIGE_F_TIMER_HZ:*v=g_maxTimer.m_frequency;return 0;
    case GIGE_F_I2C_HZ:*v=GIGE_UNIT_SENSOR_I2C_HZ;return 0;
    case GIGE_F_VCM_AVAILABLE:*v=GIGE_UNIT_VCM_AVAILABLE && GigeCamera_Ready();return 0;
    case GIGE_F_JPEG_AVAILABLE:*v=MAX_CAMERA_GIGE_CSI_BYTES;return 0;
    case GIGE_F_FREX_AVAILABLE:case GIGE_F_MCU_AVAILABLE:*v=0;return 0;
    default:return GigeCamera_FeatureRead(a,v);
    }
}
static uint16_t feature_write(void *,uint32_t a,uint32_t v)
{
    if(a==GIGE_F_RECOVER){
        if(v!=1)return GIGE_STATUS_INVALID_PARAM;
        if(GigeUserSets_Busy() || GigeUserSets_ConfigPending())return GIGE_STATUS_BUSY;
        if(!GigeVision_ConfigValid(&camera.config))return GIGE_STATUS_INVALID_PARAM;
        reset_pending=1;return 0;
    }
    if(reset_pending)return GIGE_STATUS_BUSY;
    if(GigeAI_Address(a))return GigeAI_Write(a,v);
    if(GigeUserSets_Address(a))return GigeUserSets_Write(a,v);
    if(GigeUserSets_Busy())return GIGE_STATUS_BUSY;
    uint32_t black_live=0;
    if(a==GIGE_F_FPGA_BLACK)
        (void)GigeCamera_FeatureRead(GIGE_F_FPGA_BLACK_LIVE_ACTIVE,&black_live);
    if(GigeFpgaIsp_TransportWriteLocked(a,camera.locked,black_live))return GIGE_STATUS_BUSY;
    if(camera.locked && GigeRemapControl_Address(a))return GIGE_STATUS_BUSY;
    if(a==GIGE_F_FAN_THRESHOLD || a==GIGE_F_FAN_FLOOR || a==GIGE_F_FAN_DUTY) {
        MAX_FAN_CONTROLLER_REGS reg=MAX_FAN_CONTROLLER_DUTY_CYCLE;
        if(a!=GIGE_F_FAN_DUTY) {
            /* 10-bit hardware thermal ADC representation: -40C..125C. */
            if(v<474 || v>809)return GIGE_STATUS_INVALID_PARAM;
            uint32_t other;
            MAX_FAN_CONTROLLER_REGS peer=a==GIGE_F_FAN_THRESHOLD?MAX_FAN_CONTROLLER_FLOOR_THRESHOLD:MAX_FAN_CONTROLLER_THRESHOLD;
            if(maxFanController_ReadReg(&g_fanController,peer,&other)!=MAX_RESULT_OK)return 0x8008;
            if((a==GIGE_F_FAN_THRESHOLD && v<=other) || (a==GIGE_F_FAN_FLOOR && v>=other))return GIGE_STATUS_INVALID_PARAM;
            reg=a==GIGE_F_FAN_THRESHOLD?MAX_FAN_CONTROLLER_THRESHOLD:MAX_FAN_CONTROLLER_FLOOR_THRESHOLD;
        } else if(v>100)return GIGE_STATUS_INVALID_PARAM;
        if(maxFanController_WriteReg(&g_fanController,reg,v)!=MAX_RESULT_OK)return 0x8008;
        uint32_t readback;if(maxFanController_ReadReg(&g_fanController,reg,&readback)!=MAX_RESULT_OK || readback!=v)return 0x8008;
        return 0;
    }
    if(a==GIGE_F_COMPRESSION && (camera.locked || camera.active))return GIGE_STATUS_ACCESS_DENIED;
    uint16_t e=GigeCamera_FeatureWrite(a,v);
    if(!e && a==GIGE_F_COMPRESSION)camera.compression=v;
    return e;
}
static void receive(void *,struct udp_pcb *pcb,struct pbuf *p,const ip_addr_t *from,u16_t port)
{
    uint8_t q[GIGE_CONTROL_MAX],ack[GIGE_ACK_MAX];
    if(!p)return;
    if(p->tot_len>sizeof(q) || pbuf_copy_partial(p,q,p->tot_len,0)!=p->tot_len) {pbuf_free(p);return;}
    size_t n=p->tot_len;pbuf_free(p);
    GigeVision_Tick(&camera,GigeVisionBoard_NowUs());
    GigePeer peer={lwip_ntohl(ip4_addr_get_u32(ip_2_ip4(from))),port};
    if(n>=8 && Gige_Get16(q+2)==GIGE_CMD_RESEND){++g_gige_resend_datagrams;g_gige_resend_length=n;memset(g_gige_last_resend,0,32);memcpy(g_gige_last_resend,q,n<32?n:32);}
    int broadcast=0;size_t count=GigeVision_Control(&camera,peer,q,n,ack,&broadcast);
    if(n>=8 && Gige_Get16(q+2)==GIGE_CMD_RESEND)g_gige_resend_status=Gige_Get16(q+6)?Gige_Get16(ack):0xffffU;
    /* Password writes are excluded from replay and never retained in RX scratch. */
    for(volatile uint8_t *wipe=q;wipe<q+n;++wipe)*wipe=0;
    if(count) {
        struct pbuf *reply=pbuf_alloc(PBUF_TRANSPORT,(u16_t)count,PBUF_RAM);
        if(reply) {
            pbuf_take(reply,ack,(u16_t)count);
            ip_addr_t target=broadcast?address(0xffffffffU):*from;
            (void)udp_sendto_if(pcb,reply,&target,port,&interface);pbuf_free(reply);
        }
    }
    /* ForceIP ACK was queued using the old source address, then reconfigure. */
    if(camera.force_ip_pending) {
        camera.force_ip_pending=0;forced=1;
        dhcp_stop(&interface);autoip_stop(&interface);
        ip_addr_t ip=address(camera.force_ip),mask=address(camera.force_mask),gw=address(camera.force_gateway);
        netif_set_addr(&interface,ip_2_ip4(&ip),ip_2_ip4(&mask),ip_2_ip4(&gw));
        status(&interface);
    }
}
static int acquisition(void *user,int enable)
{
    if(enable && (GigeUserSets_Busy() || reset_pending))return -1;
    return GigeCamera_Acquisition(user,enable);
}
int GigeVisionBoard_Init(void)
{
#if MAX_CAMERA_AI_IPC
    #if MAX_CAMERA_WEB_HTTP
    const uint32_t shared_bytes=0x1000000U;
#else
    const uint32_t shared_bytes=GIGE_AI_BYTES;
#endif
    if(Xil_SetMPURegion(GIGE_AI_PHYS,shared_bytes,NORM_SHARED_NCACHE|PRIV_RW_USER_RW)!=XST_SUCCESS)return -1;
    GigeAI_Init((volatile uint32_t *)GIGE_AI_PHYS);
#else
    GigeAI_Init(0);
#endif
    (void)GigeVisionBoard_NowUs();
    GigeIO io={&camera,acquisition,send_stream,feature_read,feature_write,stream_buffer};GigeVision_Init(&camera,&io);
    int flash_status=GigeFlash_Init();
    xil_printf("GIGE FLASH init=%d\r\n",flash_status);
    if(!flash_status)(void)GigeUserSets_InitStorage(&camera);
    lwip_init();ip4_addr_t zero={0};
    if(!netif_add(&interface,&zero,&zero,&zero,0,GigeGem_Init,ethernet_input)) {
        xil_printf("GIGE ERROR GEM3 init failed\r\n");return -1;
    }
    netif_set_default(&interface);netif_set_status_callback(&interface,status);
    netif_set_up(&interface);
    control=udp_new_ip_type(IPADDR_TYPE_V4);stream=udp_new_ip_type(IPADDR_TYPE_V4);
    if(!control || !stream || udp_bind(control,IP_ANY_TYPE,GIGE_GVCP_PORT)!=ERR_OK ||
       udp_bind(stream,IP_ANY_TYPE,GIGE_UNIT_STREAM_SOURCE_PORT)!=ERR_OK)return -2;
    ip_set_option(control,SOF_BROADCAST);ip_set_option(stream,SOF_BROADCAST);
    udp_recv(control,receive,0);
    if(camera.config.mode&GIGE_IP_PERSISTENT) {
        ip_addr_t ip=address(camera.config.ip),mask=address(camera.config.mask),gw=address(camera.config.gateway);
        netif_set_addr(&interface,ip_2_ip4(&ip),ip_2_ip4(&mask),ip_2_ip4(&gw));
    } else if(camera.config.mode&GIGE_IP_DHCP)dhcp_start(&interface);
    else autoip_start(&interface);
#if MAX_CAMERA_WEB_HTTP
    GigeHttp_Init((volatile uint32_t *)GIGE_HTTP_CONTROL,(volatile uint8_t *)GIGE_HTTP_REQUEST,(volatile uint8_t *)GIGE_HTTP_RESPONSE);
#endif
    network_ready=1;
    xil_printf("GIGE GVCP port=3956 MAC=%02x:%02x:%02x:%02x:%02x:%02x config=%d\r\n",
        interface.hwaddr[0],interface.hwaddr[1],interface.hwaddr[2],interface.hwaddr[3],
        interface.hwaddr[4],interface.hwaddr[5],GIGE_UNIT_IP_CONFIG);
#if MAX_CAMERA_FLASH_PROBE
    if(!flash_status) {
        for(uint32_t offset=0;offset<GIGE_FLASH_BYTES;offset+=65536) {
            if(GigeFlash_Read(offset,g_flash_backup+offset,65536)) {flash_status=-3;break;}
            if(!(offset&0xfffff))xil_printf("FLASH BACKUP read %d MiB\r\n",offset/1048576);
        }
        Xil_DCacheFlushRange((INTPTR)g_flash_backup,sizeof(g_flash_backup));
    }
    xil_printf("FLASH BACKUP COMPLETE status=%d address=%08x bytes=%d\r\n",flash_status,(unsigned)(UINTPTR)g_flash_backup,GIGE_FLASH_BYTES);
#endif
    int e=GigeCamera_Init();
    xil_printf("GIGE CAMERA prepare=%d (GVCP remains available on failure)\r\n",e);
    if(!e)GigeUserSets_Init(&camera);
    return 0;
}
void GigeVisionBoard_Mac(uint8_t *mac)
{Gige_Put16(mac,(uint16_t)camera.config.mac_high);Gige_Put32(mac+2,camera.config.mac_low);}
void GigeVisionBoard_Poll(void)
{
    GigeAI_Poll(GigeVisionBoard_NowUs());
    if(!GigeAI_WebPoll(GigeVisionBoard_NowUs(),camera.privilege!=0) && !camera.privilege)GigeAI_RevokeControl();
    GigeCamera_ServiceTelemetry(); /* Also drain without network/sensor readiness. */
    if(!network_ready)return;
    uint64_t now=GigeVisionBoard_NowUs();GigeVision_Tick(&camera,now);
    GigeCamera_RemapOwner(camera.privilege!=0);
    GigeUserSets_Poll();
    GigeGem_Poll(&interface);sys_check_timeouts();
    if(!netif_is_link_up(&interface) && camera.active) {
        GigeCamera_Acquisition(0,0);camera.active=0;camera.frame=0;
    }
    if((reset_pending || (GigeCamera_NeedsRecovery() && now>=next_recovery)) && !GigeUserSets_Busy() && GigeCamera_SensorAvailable()) {
        int explicit_reset=reset_pending;reset_pending=1;
        leds_controller_stream(&g_ledsController,GigeCamera_IndependentStreamLed(),0,now);
        reset_error=GigeCamera_Reset(&camera,!explicit_reset);next_recovery=GigeVisionBoard_NowUs()+5000000U;
        if(explicit_reset && !reset_error) {
            /* A standard DeviceReset returns to the power-up profile, not
             * the last acquired frame's configuration. Automatic fault recovery
             * above deliberately preserves that last known good profile. */
            GigeDeviceConfig saved=camera.config;GigeIO io=camera.io;
            GigeVision_Init(&camera,&io);GigeVision_ApplyConfig(&camera,&saved);
            device_boot_us=GigeVisionBoard_NowUs();camera.now=device_boot_us;camera.timestamp_origin=device_boot_us;
            reset_error=GigeUserSets_RestoreStartup();
            /* ACK has already been queued before touching the network. */
            dhcp_stop(&interface);autoip_stop(&interface);forced=0;
            GigeVision_ApplyConfig(&camera,&camera.config);
            uint8_t mac[6];GigeVisionBoard_Mac(mac);reset_error=GigeGem_SetMac(&interface,mac);
            ip_addr_t zero=address(0);netif_set_addr(&interface,ip_2_ip4(&zero),ip_2_ip4(&zero),ip_2_ip4(&zero));
            if(camera.config.mode&GIGE_IP_PERSISTENT){ip_addr_t ip=address(camera.config.ip),mask=address(camera.config.mask),gw=address(camera.config.gateway);netif_set_addr(&interface,ip_2_ip4(&ip),ip_2_ip4(&mask),ip_2_ip4(&gw));}
            else if(camera.config.mode&GIGE_IP_DHCP)dhcp_start(&interface);else autoip_start(&interface);
        }
        reset_pending=0;
    }
    GigeCamera_Poll(&camera);
    GigeCamera_RefreshControls();
    /* Bound latency for DMA re-arm/GVCP while amortizing status-register
     * polling. RAW10 full-array blanking is about850us at80MHz; return to
     * the writer within200us even with GevSCPD=0. */
    uint64_t burst_end=GigeVisionBoard_NowUs()+200U;
    for(unsigned budget=0;budget<256;++budget) {
        uint64_t tick=GigeVisionBoard_NowUs();if(tick>=burst_end)break;
        GigeVision_Tick(&camera,tick);GigeVision_StreamPoll(&camera);
    }
    GigeHttp_Poll(GigeVisionBoard_NowUs(),lwip_ntohl(netif_ip4_addr(&interface)->addr),
                  GigeVisionBoard_Streaming(),GigeGem_TxFree());
    if(now>=next_log && !GigeVisionBoard_Streaming()) {
        next_log=now+5000000U;
        xil_printf("GIGE STAT rx=%d tx=%d macerr=%d cp=%d active=%d capture=%d sent=%d drop=%d csierr=%d txerr=%d frameus=%d\r\n",
          g_gem_rx_packets,g_gem_tx_packets,g_gem_errors,camera.privilege,camera.active,
          g_camera_completed,camera.frames_sent,g_camera_dropped,g_camera_csi_errors,camera.send_errors,camera.frame_send_us);
    }
}
