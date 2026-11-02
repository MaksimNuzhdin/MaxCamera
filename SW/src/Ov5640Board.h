#ifndef OV5640_BOARD_H
#define OV5640_BOARD_H
#include "Ov5640MaxIic.h"
extern OV5640 g_camera;
void Ov5640Board_Init(MAX_IIC *bus);
#endif
