#ifndef GIGE_USER_SETS_H
#define GIGE_USER_SETS_H
#include "GigeVision.h"
#ifdef __cplusplus
extern "C" {
#endif
int GigeUserSets_InitStorage(GigeVision*);
void GigeUserSets_Init(GigeVision*);
void GigeUserSets_Poll(void);
int GigeUserSets_RestoreStartup(void);
int GigeUserSets_Busy(void);
int GigeUserSets_ConfigPending(void);
int GigeUserSets_ConfigError(void);
int GigeUserSets_Address(uint32_t);
uint16_t GigeUserSets_Read(uint32_t,uint32_t*);
uint16_t GigeUserSets_Write(uint32_t,uint32_t);
#ifdef __cplusplus
}
#endif
#endif
