#ifndef CAMERA_DIAGNOSTICS_H
#define CAMERA_DIAGNOSTICS_H
#include "MaxIicDriver.h"
// Startup diagnostics for the current 200 MHz AXI IIC hardware.
// Only chip-ID register pointers are written; no sensor configuration is changed.
void CameraDiagnostics_Run(MAX_IIC *cam0, MAX_IIC *cam1);
#endif
