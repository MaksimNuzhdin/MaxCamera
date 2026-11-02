#ifndef GIGE_REMAP_READER_H
#define GIGE_REMAP_READER_H
#include "GigeFpgaIsp.h"
#define GIGE_REMAP_READER_BASE 0xA0190000U
/* Exported integer_remap AXI-Lite ABI; verified against synthesized control RTL.
 * Use single-invocation mode. AP_IDLE gates status consumption and safe reset.
 * No master-only reset is allowed during a map-reading invocation. */
typedef struct {
    GigeFpgaIO io;
    int present,configured,map_mode,last_start_used_map;
} GigeRemapReader;
#ifdef __cplusplus
extern "C" {
#endif
void GigeRemapReader_Init(GigeRemapReader*,const GigeFpgaIO*,int present);
int GigeRemapReader_CanReset(const GigeRemapReader*);
int GigeRemapReader_ResetObserved(GigeRemapReader*);
int GigeRemapReader_Configure(GigeRemapReader*,uint32_t rows,uint32_t cols,
                             uint64_t map_address,int enabled);
int GigeRemapReader_Start(GigeRemapReader*);
/* 0 pending,1 completed,-1 unavailable. Status belongs to the completed manual
 * invocation and must be consumed before starting another. No automatic retry. */
int GigeRemapReader_Completed(const GigeRemapReader*,uint32_t *status);
#ifdef __cplusplus
}
#endif
#endif
