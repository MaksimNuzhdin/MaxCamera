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
// 0x048 : Data signal of median_enable
//         bit 0  - median_enable[0] (Read/Write)
//         others - reserved
// 0x04c : reserved
// 0x050 : Data signal of roi_x
//         bit 31~0 - roi_x[31:0] (Read/Write)
// 0x054 : reserved
// 0x058 : Data signal of roi_y
//         bit 31~0 - roi_y[31:0] (Read/Write)
// 0x05c : reserved
// 0x060 : Data signal of roi_width
//         bit 31~0 - roi_width[31:0] (Read/Write)
// 0x064 : reserved
// 0x068 : Data signal of roi_height
//         bit 31~0 - roi_height[31:0] (Read/Write)
// 0x06c : reserved
// 0x070 : Data signal of out_rows
//         bit 31~0 - out_rows[31:0] (Read/Write)
// 0x074 : reserved
// 0x078 : Data signal of out_cols
//         bit 31~0 - out_cols[31:0] (Read/Write)
// 0x07c : reserved
// 0x080 : Data signal of intensity_sum
//         bit 31~0 - intensity_sum[31:0] (Read)
// 0x084 : Data signal of intensity_sum
//         bit 31~0 - intensity_sum[63:32] (Read)
// 0x088 : Control signal of intensity_sum
//         bit 0  - intensity_sum_ap_vld (Read/COR)
//         others - reserved
// 0x098 : Data signal of clipped_pixels
//         bit 31~0 - clipped_pixels[31:0] (Read)
// 0x09c : Control signal of clipped_pixels
//         bit 0  - clipped_pixels_ap_vld (Read/COR)
//         others - reserved
// 0x0a8 : Data signal of dark_pixels
//         bit 31~0 - dark_pixels[31:0] (Read)
// 0x0ac : Control signal of dark_pixels
//         bit 0  - dark_pixels_ap_vld (Read/COR)
//         others - reserved
// 0x0b8 : Data signal of ccm_enable
//         bit 0  - ccm_enable[0] (Read/Write)
//         others - reserved
// 0x0bc : reserved
// 0x0c0 : Data signal of gamma_enable
//         bit 0  - gamma_enable[0] (Read/Write)
//         others - reserved
// 0x0c4 : reserved
// 0x0c8 : Data signal of awb_enable
//         bit 0  - awb_enable[0] (Read/Write)
//         others - reserved
// 0x0cc : reserved
// 0x0e0 : Data signal of red_sum
//         bit 31~0 - red_sum[31:0] (Read)
// 0x0e4 : Data signal of red_sum
//         bit 31~0 - red_sum[63:32] (Read)
// 0x0e8 : Control signal of red_sum
//         bit 0  - red_sum_ap_vld (Read/COR)
//         others - reserved
// 0x0f8 : Data signal of bilateral_enable
//         bit 0  - bilateral_enable[0] (Read/Write)
//         others - reserved
// 0x0fc : reserved
// 0x140 : Data signal of green_sum
//         bit 31~0 - green_sum[31:0] (Read)
// 0x144 : Data signal of green_sum
//         bit 31~0 - green_sum[63:32] (Read)
// 0x148 : Control signal of green_sum
//         bit 0  - green_sum_ap_vld (Read/COR)
//         others - reserved
// 0x158 : Data signal of blue_sum
//         bit 31~0 - blue_sum[31:0] (Read)
// 0x15c : Data signal of blue_sum
//         bit 31~0 - blue_sum[63:32] (Read)
// 0x160 : Control signal of blue_sum
//         bit 0  - blue_sum_ap_vld (Read/COR)
//         others - reserved
// 0x170 : Data signal of valid_pixels
//         bit 31~0 - valid_pixels[31:0] (Read)
// 0x174 : Control signal of valid_pixels
//         bit 0  - valid_pixels_ap_vld (Read/COR)
//         others - reserved
// 0x180 : Data signal of applied_red
//         bit 31~0 - applied_red[31:0] (Read)
// 0x184 : Control signal of applied_red
//         bit 0  - applied_red_ap_vld (Read/COR)
//         others - reserved
// 0x190 : Data signal of applied_green
//         bit 31~0 - applied_green[31:0] (Read)
// 0x194 : Control signal of applied_green
//         bit 0  - applied_green_ap_vld (Read/COR)
//         others - reserved
// 0x1a0 : Data signal of applied_blue
//         bit 31~0 - applied_blue[31:0] (Read)
// 0x1a4 : Control signal of applied_blue
//         bit 0  - applied_blue_ap_vld (Read/COR)
//         others - reserved
// 0x1b0 : Data signal of frame_sequence
//         bit 31~0 - frame_sequence[31:0] (Read)
// 0x1b4 : Control signal of frame_sequence
//         bit 0  - frame_sequence_ap_vld (Read/COR)
//         others - reserved
// 0x1c0 : Data signal of sigma_color
//         bit 15~0 - sigma_color[15:0] (Read/Write)
//         others   - reserved
// 0x1c4 : reserved
// 0x1c8 : Data signal of sigma_space_q8
//         bit 15~0 - sigma_space_q8[15:0] (Read/Write)
//         others   - reserved
// 0x1cc : reserved
// 0x1d0 : Data signal of sharpen_strength
//         bit 15~0 - sharpen_strength[15:0] (Read/Write)
//         others   - reserved
// 0x1d4 : reserved
// 0x1d8 : Data signal of lsc_enable
//         bit 0  - lsc_enable[0] (Read/Write)
//         others - reserved
// 0x1dc : reserved
// 0x0d0 ~
// 0x0df : Memory 'offsets' (3 * 32b)
//         Word n : bit [31:0] - offsets[n]
// 0x100 ~
// 0x13f : Memory 'matrix' (9 * 32b)
//         Word n : bit [31:0] - matrix[n]
// 0x200 ~
// 0x3ff : Memory 'lsc_gains' (252 * 16b)
//         Word n : bit [15: 0] - lsc_gains[2n]
//                  bit [31:16] - lsc_gains[2n+1]
// 0x400 ~
// 0x7ff : Memory 'gamma' (768 * 8b)
//         Word n : bit [ 7: 0] - gamma[4n]
//                  bit [15: 8] - gamma[4n+1]
//                  bit [23:16] - gamma[4n+2]
//                  bit [31:24] - gamma[4n+3]
// (SC = Self Clear, COR = Clear on Read, TOW = Toggle on Write, COH = Clear on Handshake)

#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_AP_CTRL               0x000
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_GIE                   0x004
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_IER                   0x008
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_ISR                   0x00c
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_ROWS_DATA             0x010
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_BITS_ROWS_DATA             32
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_COLS_DATA             0x018
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_BITS_COLS_DATA             32
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_BPC_ENABLE_DATA       0x020
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_BITS_BPC_ENABLE_DATA       1
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_BLACK_LEVEL_DATA      0x028
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_BITS_BLACK_LEVEL_DATA      16
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_BLUE_GAIN_DATA        0x030
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_BITS_BLUE_GAIN_DATA        16
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_GREEN_GAIN_DATA       0x038
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_BITS_GREEN_GAIN_DATA       16
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_RED_GAIN_DATA         0x040
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_BITS_RED_GAIN_DATA         16
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_MEDIAN_ENABLE_DATA    0x048
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_BITS_MEDIAN_ENABLE_DATA    1
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_ROI_X_DATA            0x050
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_BITS_ROI_X_DATA            32
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_ROI_Y_DATA            0x058
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_BITS_ROI_Y_DATA            32
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_ROI_WIDTH_DATA        0x060
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_BITS_ROI_WIDTH_DATA        32
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_ROI_HEIGHT_DATA       0x068
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_BITS_ROI_HEIGHT_DATA       32
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_OUT_ROWS_DATA         0x070
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_BITS_OUT_ROWS_DATA         32
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_OUT_COLS_DATA         0x078
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_BITS_OUT_COLS_DATA         32
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_INTENSITY_SUM_DATA    0x080
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_BITS_INTENSITY_SUM_DATA    64
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_INTENSITY_SUM_CTRL    0x088
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_CLIPPED_PIXELS_DATA   0x098
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_BITS_CLIPPED_PIXELS_DATA   32
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_CLIPPED_PIXELS_CTRL   0x09c
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_DARK_PIXELS_DATA      0x0a8
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_BITS_DARK_PIXELS_DATA      32
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_DARK_PIXELS_CTRL      0x0ac
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_CCM_ENABLE_DATA       0x0b8
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_BITS_CCM_ENABLE_DATA       1
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_GAMMA_ENABLE_DATA     0x0c0
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_BITS_GAMMA_ENABLE_DATA     1
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_AWB_ENABLE_DATA       0x0c8
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_BITS_AWB_ENABLE_DATA       1
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_RED_SUM_DATA          0x0e0
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_BITS_RED_SUM_DATA          64
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_RED_SUM_CTRL          0x0e8
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_BILATERAL_ENABLE_DATA 0x0f8
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_BITS_BILATERAL_ENABLE_DATA 1
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_GREEN_SUM_DATA        0x140
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_BITS_GREEN_SUM_DATA        64
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_GREEN_SUM_CTRL        0x148
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_BLUE_SUM_DATA         0x158
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_BITS_BLUE_SUM_DATA         64
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_BLUE_SUM_CTRL         0x160
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_VALID_PIXELS_DATA     0x170
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_BITS_VALID_PIXELS_DATA     32
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_VALID_PIXELS_CTRL     0x174
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_APPLIED_RED_DATA      0x180
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_BITS_APPLIED_RED_DATA      32
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_APPLIED_RED_CTRL      0x184
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_APPLIED_GREEN_DATA    0x190
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_BITS_APPLIED_GREEN_DATA    32
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_APPLIED_GREEN_CTRL    0x194
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_APPLIED_BLUE_DATA     0x1a0
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_BITS_APPLIED_BLUE_DATA     32
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_APPLIED_BLUE_CTRL     0x1a4
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_FRAME_SEQUENCE_DATA   0x1b0
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_BITS_FRAME_SEQUENCE_DATA   32
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_FRAME_SEQUENCE_CTRL   0x1b4
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_SIGMA_COLOR_DATA      0x1c0
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_BITS_SIGMA_COLOR_DATA      16
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_SIGMA_SPACE_Q8_DATA   0x1c8
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_BITS_SIGMA_SPACE_Q8_DATA   16
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_SHARPEN_STRENGTH_DATA 0x1d0
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_BITS_SHARPEN_STRENGTH_DATA 16
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_LSC_ENABLE_DATA       0x1d8
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_BITS_LSC_ENABLE_DATA       1
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_OFFSETS_BASE          0x0d0
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_OFFSETS_HIGH          0x0df
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_WIDTH_OFFSETS              32
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_DEPTH_OFFSETS              3
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_MATRIX_BASE           0x100
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_MATRIX_HIGH           0x13f
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_WIDTH_MATRIX               32
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_DEPTH_MATRIX               9
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_LSC_GAINS_BASE        0x200
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_LSC_GAINS_HIGH        0x3ff
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_WIDTH_LSC_GAINS            16
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_DEPTH_LSC_GAINS            252
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_GAMMA_BASE            0x400
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_ADDR_GAMMA_HIGH            0x7ff
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_WIDTH_GAMMA                8
#define XCAMERA_COLOR_PIPELINE_V5_CONTROL_DEPTH_GAMMA                768

