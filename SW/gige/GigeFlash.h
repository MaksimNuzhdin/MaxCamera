#ifndef GIGE_FLASH_H
#define GIGE_FLASH_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define GIGE_FLASH_BYTES 0x02000000U
#define GIGE_FLASH_USER_BASE 0x01e00000U
#define GIGE_FLASH_USER_BYTES 0x00200000U
int GigeFlash_Init(void);
int GigeFlash_Read(uint32_t address,void *data,uint32_t bytes);
/* Mutations are restricted to the reserved user-set partition. */
int GigeFlash_Erase(uint32_t address);
int GigeFlash_Program(uint32_t address,const void *data,uint32_t bytes);
int GigeFlash_Busy(void);
uint32_t GigeFlash_Id(void);
#ifdef __cplusplus
}
#endif
#endif
