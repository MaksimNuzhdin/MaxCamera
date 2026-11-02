#ifndef GIGE_VISION_BOARD_H
#define GIGE_VISION_BOARD_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void GigeVisionBoard_Mac(uint8_t*);
int GigeVisionBoard_Init(void);
int GigeVisionBoard_Ready(void);
int GigeVisionBoard_Streaming(void);
int GigeVisionBoard_StreamIndicatorActive(void);
void GigeVisionBoard_Poll(void);
uint64_t GigeVisionBoard_NowUs(void);
#ifdef __cplusplus
}
#endif
#endif
