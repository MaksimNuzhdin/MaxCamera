// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2024.2 (64-bit)
// Tool Version Limit: 2024.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
// control
// 0x00 : Control signals
//        bit 0  - ap_start (Read/Write/COH)
//        bit 1  - ap_done (Read/COR)
//        bit 2  - ap_idle (Read)
//        bit 3  - ap_ready (Read/COR)
//        bit 7  - auto_restart (Read/Write)
//        bit 9  - interrupt (Read)
//        others - reserved
// 0x04 : Global Interrupt Enable Register
//        bit 0  - Global Interrupt Enable (Read/Write)
//        others - reserved
// 0x08 : IP Interrupt Enable Register (Read/Write)
//        bit 0 - enable ap_done interrupt (Read/Write)
//        bit 1 - enable ap_ready interrupt (Read/Write)
//        others - reserved
// 0x0c : IP Interrupt Status Register (Read/TOW)
//        bit 0 - ap_done (Read/TOW)
//        bit 1 - ap_ready (Read/TOW)
//        others - reserved
// 0x10 : Data signal of rows
//        bit 31~0 - rows[31:0] (Read/Write)
// 0x14 : reserved
// 0x18 : Data signal of cols
//        bit 31~0 - cols[31:0] (Read/Write)
// 0x1c : reserved
// 0x20 : Data signal of enable
//        bit 0  - enable[0] (Read/Write)
//        others - reserved
// 0x24 : reserved
// 0x28 : Data signal of focus_q16
//        bit 31~0 - focus_q16[31:0] (Read)
// 0x2c : Control signal of focus_q16
//        bit 0  - focus_q16_ap_vld (Read/COR)
//        others - reserved
// 0x38 : Data signal of motion_q16
//        bit 31~0 - motion_q16[31:0] (Read)
// 0x3c : Control signal of motion_q16
//        bit 0  - motion_q16_ap_vld (Read/COR)
//        others - reserved
// 0x48 : Data signal of motion_valid
//        bit 31~0 - motion_valid[31:0] (Read)
// 0x4c : Control signal of motion_valid
//        bit 0  - motion_valid_ap_vld (Read/COR)
//        others - reserved
// 0x58 : Data signal of frame_sequence
//        bit 31~0 - frame_sequence[31:0] (Read)
// 0x5c : Control signal of frame_sequence
//        bit 0  - frame_sequence_ap_vld (Read/COR)
//        others - reserved
// (SC = Self Clear, COR = Clear on Read, TOW = Toggle on Write, COH = Clear on Handshake)

#define XCAMERA_SCENE_METRICS_V6_CONTROL_ADDR_AP_CTRL             0x00
#define XCAMERA_SCENE_METRICS_V6_CONTROL_ADDR_GIE                 0x04
#define XCAMERA_SCENE_METRICS_V6_CONTROL_ADDR_IER                 0x08
#define XCAMERA_SCENE_METRICS_V6_CONTROL_ADDR_ISR                 0x0c
#define XCAMERA_SCENE_METRICS_V6_CONTROL_ADDR_ROWS_DATA           0x10
#define XCAMERA_SCENE_METRICS_V6_CONTROL_BITS_ROWS_DATA           32
#define XCAMERA_SCENE_METRICS_V6_CONTROL_ADDR_COLS_DATA           0x18
#define XCAMERA_SCENE_METRICS_V6_CONTROL_BITS_COLS_DATA           32
#define XCAMERA_SCENE_METRICS_V6_CONTROL_ADDR_ENABLE_DATA         0x20
#define XCAMERA_SCENE_METRICS_V6_CONTROL_BITS_ENABLE_DATA         1
#define XCAMERA_SCENE_METRICS_V6_CONTROL_ADDR_FOCUS_Q16_DATA      0x28
#define XCAMERA_SCENE_METRICS_V6_CONTROL_BITS_FOCUS_Q16_DATA      32
#define XCAMERA_SCENE_METRICS_V6_CONTROL_ADDR_FOCUS_Q16_CTRL      0x2c
#define XCAMERA_SCENE_METRICS_V6_CONTROL_ADDR_MOTION_Q16_DATA     0x38
#define XCAMERA_SCENE_METRICS_V6_CONTROL_BITS_MOTION_Q16_DATA     32
#define XCAMERA_SCENE_METRICS_V6_CONTROL_ADDR_MOTION_Q16_CTRL     0x3c
#define XCAMERA_SCENE_METRICS_V6_CONTROL_ADDR_MOTION_VALID_DATA   0x48
#define XCAMERA_SCENE_METRICS_V6_CONTROL_BITS_MOTION_VALID_DATA   32
#define XCAMERA_SCENE_METRICS_V6_CONTROL_ADDR_MOTION_VALID_CTRL   0x4c
#define XCAMERA_SCENE_METRICS_V6_CONTROL_ADDR_FRAME_SEQUENCE_DATA 0x58
#define XCAMERA_SCENE_METRICS_V6_CONTROL_BITS_FRAME_SEQUENCE_DATA 32
#define XCAMERA_SCENE_METRICS_V6_CONTROL_ADDR_FRAME_SEQUENCE_CTRL 0x5c

