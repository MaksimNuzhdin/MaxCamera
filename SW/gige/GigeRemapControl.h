#ifndef GIGE_REMAP_CONTROL_H
#define GIGE_REMAP_CONTROL_H
#include "GigeRemapMap.h"
typedef struct {
    GigeRemapMap map;
    uint32_t width,height,expected_crc,index,enabled,hardware_status;
    uint64_t geometry;
} GigeRemapControl;
#ifdef __cplusplus
extern "C" {
#endif
int GigeRemapControl_Init(GigeRemapControl*,uint32_t*,uint32_t*,size_t);
int GigeRemapControl_Address(uint32_t);
uint16_t GigeRemapControl_Read(const GigeRemapControl*,uint32_t,uint32_t*,int available);
uint16_t GigeRemapControl_Write(GigeRemapControl*,uint32_t,uint32_t,int available,
    int busy,uint32_t output_width,uint32_t output_height,uint64_t geometry,int color_route);
void GigeRemapControl_Poll(GigeRemapControl*,int busy);
/* Volatile transfer/config only: never part of persistent UserSet serialization. */
void GigeRemapControl_Invalidate(GigeRemapControl*);
#ifdef __cplusplus
}
#endif
#endif
