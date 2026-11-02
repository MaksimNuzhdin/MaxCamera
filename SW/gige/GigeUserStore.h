#ifndef GIGE_USER_STORE_H
#define GIGE_USER_STORE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define GIGE_USERSTORE_BASE 0x01e00000U
#define GIGE_USERSTORE_BYTES 0x00200000U
#define GIGE_USERSTORE_SLOTS 10U /* factory +7 users + startup selector + device/network config */
#define GIGE_USERSTORE_MAX 2048U
#define GIGE_USERSTORE_SCHEMA 0x4d430001U
typedef struct {
    void *user;
    int (*read)(void*,uint32_t,void*,uint32_t);
    int (*program)(void*,uint32_t,const void*,uint32_t);
    int (*erase)(void*,uint32_t);
    int (*busy)(void*);
} GigeUserStoreIO;
typedef struct {
    GigeUserStoreIO io;
    uint64_t generation[GIGE_USERSTORE_SLOTS],deadline;
    int8_t bank[GIGE_USERSTORE_SLOTS];
    uint16_t lengths[GIGE_USERSTORE_SLOTS],valid;
    uint8_t available,owned,state,slot,target;
    uint32_t position,total;
    int error;
    uint8_t buffer[256+GIGE_USERSTORE_MAX];
} GigeUserStore;
uint32_t GigeUserStore_Crc(const void*,uint32_t);
int GigeUserStore_Init(GigeUserStore*,const GigeUserStoreIO*);
int GigeUserStore_Load(GigeUserStore*,unsigned,void*,uint32_t*);
int GigeUserStore_Save(GigeUserStore*,unsigned,const void*,uint32_t);
void GigeUserStore_Poll(GigeUserStore*,uint64_t now_us);
#ifdef __cplusplus
}
#endif
#endif
