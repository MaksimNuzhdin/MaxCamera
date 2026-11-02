#ifndef GIGE_GEM_H
#define GIGE_GEM_H
#include "lwip/netif.h"
#ifdef __cplusplus
extern "C" {
#endif
int GigeGem_SetMac(struct netif*,const uint8_t*);
err_t GigeGem_Init(struct netif *netif);
void GigeGem_Poll(struct netif *netif);
unsigned GigeGem_TxFree(void);
uint8_t *GigeGem_StreamBuffer(struct netif*,uint32_t destination);
int GigeGem_SendStream(struct netif*,uint32_t,uint16_t,const uint8_t*,size_t,int);
extern unsigned g_gem_rx_packets,g_gem_tx_packets,g_gem_rx_drops,g_gem_errors;
#ifdef __cplusplus
}
#endif
#endif
