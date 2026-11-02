// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2024.2 (64-bit)
// Tool Version Limit: 2024.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
// control
// 0x000 : Control signals
//         bit 0  - ap_start (Read/Write/COH)
//         bit 1  - ap_done (Read/COR)
//         bit 2  - ap_idle (Read)
//         bit 3  - ap_ready (Read/COR)
//         bit 7  - auto_restart (Read/Write)
//         bit 9  - interrupt (Read)
//         others - reserved
// 0x004 : Global Interrupt Enable Register
//         bit 0  - Global Interrupt Enable (Read/Write)
//         others - reserved
// 0x008 : IP Interrupt Enable Register (Read/Write)
//         bit 0 - enable ap_done interrupt (Read/Write)
//         bit 1 - enable ap_ready interrupt (Read/Write)
//         others - reserved
// 0x00c : IP Interrupt Status Register (Read/TOW)
//         bit 0 - ap_done (Read/TOW)
//         bit 1 - ap_ready (Read/TOW)
//         others - reserved
// 0x010 : Data signal of rows
//         bit 31~0 - rows[31:0] (Read/Write)
// 0x014 : reserved
// 0x018 : Data signal of cols
//         bit 31~0 - cols[31:0] (Read/Write)
// 0x01c : reserved
// 0x020 : Data signal of bpc_enable
//         bit 0  - bpc_enable[0] (Read/Write)
//         others - reserved
// 0x024 : reserved
// 0x028 : Data signal of black_level
//         bit 15~0 - black_level[15:0] (Read/Write)
//         others   - reserved
// 0x02c : reserved
// 0x030 : Data signal of blue_gain
//         bit 15~0 - blue_gain[15:0] (Read/Write)
//         others   - reserved
// 0x034 : reserved
// 0x038 : Data signal of green_gain
//         bit 15~0 - green_gain[15:0] (Read/Write)
//         others   - reserved
// 0x03c : reserved
// 0x040 : Data signal of red_gain
//         bit 15~0 - red_gain[15:0] (Read/Write)
//         others   - reserved
// 0x044 : reserved
// 0x048 : Data signal of lsc_enable
//         bit 0  - lsc_enable[0] (Read/Write)
//         others - reserved
// 0x04c : reserved
// 0x200 ~
// 0x3ff : Memory 'lsc_gains' (252 * 16b)
//         Word n : bit [15: 0] - lsc_gains[2n]
//                  bit [31:16] - lsc_gains[2n+1]
// (SC = Self Clear, COR = Clear on Read, TOW = Toggle on Write, COH = Clear on Handshake)

#define XCAMERA_RAW_CORRECT_V5_CONTROL_ADDR_AP_CTRL          0x000
#define XCAMERA_RAW_CORRECT_V5_CONTROL_ADDR_GIE              0x004
#define XCAMERA_RAW_CORRECT_V5_CONTROL_ADDR_IER              0x008
#define XCAMERA_RAW_CORRECT_V5_CONTROL_ADDR_ISR              0x00c
#define XCAMERA_RAW_CORRECT_V5_CONTROL_ADDR_ROWS_DATA        0x010
#define XCAMERA_RAW_CORRECT_V5_CONTROL_BITS_ROWS_DATA        32
#define XCAMERA_RAW_CORRECT_V5_CONTROL_ADDR_COLS_DATA        0x018
#define XCAMERA_RAW_CORRECT_V5_CONTROL_BITS_COLS_DATA        32
#define XCAMERA_RAW_CORRECT_V5_CONTROL_ADDR_BPC_ENABLE_DATA  0x020
#define XCAMERA_RAW_CORRECT_V5_CONTROL_BITS_BPC_ENABLE_DATA  1
#define XCAMERA_RAW_CORRECT_V5_CONTROL_ADDR_BLACK_LEVEL_DATA 0x028
#define XCAMERA_RAW_CORRECT_V5_CONTROL_BITS_BLACK_LEVEL_DATA 16
#define XCAMERA_RAW_CORRECT_V5_CONTROL_ADDR_BLUE_GAIN_DATA   0x030
#define XCAMERA_RAW_CORRECT_V5_CONTROL_BITS_BLUE_GAIN_DATA   16
#define XCAMERA_RAW_CORRECT_V5_CONTROL_ADDR_GREEN_GAIN_DATA  0x038
#define XCAMERA_RAW_CORRECT_V5_CONTROL_BITS_GREEN_GAIN_DATA  16
#define XCAMERA_RAW_CORRECT_V5_CONTROL_ADDR_RED_GAIN_DATA    0x040
#define XCAMERA_RAW_CORRECT_V5_CONTROL_BITS_RED_GAIN_DATA    16
#define XCAMERA_RAW_CORRECT_V5_CONTROL_ADDR_LSC_ENABLE_DATA  0x048
#define XCAMERA_RAW_CORRECT_V5_CONTROL_BITS_LSC_ENABLE_DATA  1
#define XCAMERA_RAW_CORRECT_V5_CONTROL_ADDR_LSC_GAINS_BASE   0x200
#define XCAMERA_RAW_CORRECT_V5_CONTROL_ADDR_LSC_GAINS_HIGH   0x3ff
#define XCAMERA_RAW_CORRECT_V5_CONTROL_WIDTH_LSC_GAINS       16
#define XCAMERA_RAW_CORRECT_V5_CONTROL_DEPTH_LSC_GAINS       252

