#ifndef GIGE_PRESET_H
#define GIGE_PRESET_H
#include <stdint.h>
#include "GigeFeatureRegisters.h"
#include "GigeFpgaIsp.h"
#ifdef __cplusplus
extern "C" {
#endif
enum {
#define X(k) GIGE_PRESET_##k,
#include "GigePresetFeatures.inc"
#undef X
    GIGE_PRESET_COUNT
};
typedef struct {
    uint32_t width,height,pixel_format,frame_rate_enable,frame_rate;
    uint32_t values[GIGE_PRESET_COUNT];
    uint8_t gamma[16],lenc[62],ae_thresholds[6];
    uint32_t compression,jpeg_quality,line_period_ns;
    GigeFpgaConfig fpga;
} GigePreset;
extern const uint32_t gige_preset_addresses[GIGE_PRESET_COUNT];
void GigePreset_AdaptTiming(GigePreset*,uint32_t line_period_ns,uint32_t max_lines);
int GigePreset_Validate(const GigePreset *);
uint32_t GigePreset_Encode(const GigePreset*,uint8_t*,uint32_t);
int GigePreset_Decode(GigePreset*,const uint8_t*,uint32_t);
#ifdef __cplusplus
}
#endif
#endif
