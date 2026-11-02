#ifndef GIGE_CAMERA_H
#define GIGE_CAMERA_H
#include "GigeVision.h"
#include "GigePreset.h"
#ifdef __cplusplus
extern "C" {
#endif
int GigeCamera_Init(void);
int GigeCamera_Reset(GigeVision*,int preserve_settings);
int GigeCamera_NeedsRecovery(void);
int GigeCamera_TelemetryDeferred(void);
extern unsigned g_camera_recoveries;
int GigeCamera_Export(GigePreset*);
uint16_t GigeCamera_Import(const GigePreset*);
int GigeCamera_Ready(void);
int GigeCamera_IndependentStreamLed(void);
int GigeCamera_Busy(void);
uint16_t GigeCamera_FeatureRead(uint32_t,uint32_t *);
uint16_t GigeCamera_FeatureWrite(uint32_t,uint32_t);
void GigeCamera_RefreshControls(void);
/* Request cancellation and report whether synchronous sensor work is safe.
 * False requires subsequent ServiceTelemetry calls, never a blocking wait. */
int GigeCamera_SensorAvailable(void);
void GigeCamera_ServiceTelemetry(void);
/* Cancel incomplete volatile uploads when the control lease is lost. */
void GigeCamera_RemapOwner(int present);
int GigeCamera_Acquisition(void *user,int enable);
void GigeCamera_Poll(GigeVision *g);
extern unsigned g_camera_completed,g_camera_dropped,g_camera_csi_errors;
#ifdef __cplusplus
}
#endif
#endif
