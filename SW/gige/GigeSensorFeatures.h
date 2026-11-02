#ifndef GIGE_SENSOR_FEATURES_H
#define GIGE_SENSOR_FEATURES_H
#include "Ov5640.h"
#include "GigePreset.h"
#include "GigeFeatureRegisters.h"
#ifdef __cplusplus
extern "C" {
#endif
void GigeSensorFeatures_Bind(OV5640 *sensor);
void GigeSensorFeatures_Observe(uint16_t reg,const uint8_t *data,uint16_t count);
int GigeSensorFeatures_Prepare(void);
int GigeSensorFeatures_RefreshOne(void);
void GigeSensorFeatures_TelemetryError(uint16_t reg,int error);
int GigeSensorFeatures_Export(GigePreset*);
uint16_t GigeSensorFeatures_Import(const GigePreset*);
uint16_t GigeSensorFeatures_Read(uint32_t address,uint32_t *value);
uint16_t GigeSensorFeatures_Write(uint32_t address,uint32_t value);
#ifdef __cplusplus
}
#endif
#endif
