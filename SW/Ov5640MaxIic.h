/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef OV5640_MAX_IIC_H
#define OV5640_MAX_IIC_H
#include "MaxIicDriver.h"
#include "Ov5640.h"
#ifdef __cplusplus
extern "C" {
#endif
OV5640_RESULT Ov5640_BindMaxIic(OV5640 *, MAX_IIC *);
#ifdef __cplusplus
}
#endif
#endif
