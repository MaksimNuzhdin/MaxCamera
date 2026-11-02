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
// 0x28 : Data signal of target
//        bit 7~0 - target[7:0] (Read/Write)
//        others  - reserved
// 0x2c : reserved
// 0x30 : Data signal of epoch
//        bit 31~0 - epoch[31:0] (Read/Write)
// 0x34 : reserved
// 0x38 : Data signal of applied_gain_q8
//        bit 31~0 - applied_gain_q8[31:0] (Read)
// 0x3c : Control signal of applied_gain_q8
//        bit 0  - applied_gain_q8_ap_vld (Read/COR)
//        others - reserved
// 0x48 : Data signal of previous_mean
//        bit 31~0 - previous_mean[31:0] (Read)
// 0x4c : Control signal of previous_mean
//        bit 0  - previous_mean_ap_vld (Read/COR)
//        others - reserved
// (SC = Self Clear, COR = Clear on Read, TOW = Toggle on Write, COH = Clear on Handshake)

#define XCAMERA_ADAPTIVE_TONE_CONTROL_ADDR_AP_CTRL              0x00
#define XCAMERA_ADAPTIVE_TONE_CONTROL_ADDR_GIE                  0x04
#define XCAMERA_ADAPTIVE_TONE_CONTROL_ADDR_IER                  0x08
#define XCAMERA_ADAPTIVE_TONE_CONTROL_ADDR_ISR                  0x0c
#define XCAMERA_ADAPTIVE_TONE_CONTROL_ADDR_ROWS_DATA            0x10
#define XCAMERA_ADAPTIVE_TONE_CONTROL_BITS_ROWS_DATA            32
#define XCAMERA_ADAPTIVE_TONE_CONTROL_ADDR_COLS_DATA            0x18
#define XCAMERA_ADAPTIVE_TONE_CONTROL_BITS_COLS_DATA            32
#define XCAMERA_ADAPTIVE_TONE_CONTROL_ADDR_ENABLE_DATA          0x20
#define XCAMERA_ADAPTIVE_TONE_CONTROL_BITS_ENABLE_DATA          1
#define XCAMERA_ADAPTIVE_TONE_CONTROL_ADDR_TARGET_DATA          0x28
#define XCAMERA_ADAPTIVE_TONE_CONTROL_BITS_TARGET_DATA          8
#define XCAMERA_ADAPTIVE_TONE_CONTROL_ADDR_EPOCH_DATA           0x30
#define XCAMERA_ADAPTIVE_TONE_CONTROL_BITS_EPOCH_DATA           32
#define XCAMERA_ADAPTIVE_TONE_CONTROL_ADDR_APPLIED_GAIN_Q8_DATA 0x38
#define XCAMERA_ADAPTIVE_TONE_CONTROL_BITS_APPLIED_GAIN_Q8_DATA 32
#define XCAMERA_ADAPTIVE_TONE_CONTROL_ADDR_APPLIED_GAIN_Q8_CTRL 0x3c
#define XCAMERA_ADAPTIVE_TONE_CONTROL_ADDR_PREVIOUS_MEAN_DATA   0x48
#define XCAMERA_ADAPTIVE_TONE_CONTROL_BITS_PREVIOUS_MEAN_DATA   32
#define XCAMERA_ADAPTIVE_TONE_CONTROL_ADDR_PREVIOUS_MEAN_CTRL   0x4c

