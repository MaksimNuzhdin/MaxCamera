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
// 0x020 : Data signal of blue_gain
//         bit 15~0 - blue_gain[15:0] (Read/Write)
//         others   - reserved
// 0x024 : reserved
// 0x028 : Data signal of green_gain
//         bit 15~0 - green_gain[15:0] (Read/Write)
//         others   - reserved
// 0x02c : reserved
// 0x030 : Data signal of red_gain
//         bit 15~0 - red_gain[15:0] (Read/Write)
//         others   - reserved
// 0x034 : reserved
// 0x038 : Data signal of median_enable
//         bit 0  - median_enable[0] (Read/Write)
//         others - reserved
// 0x03c : reserved
// 0x040 : Data signal of roi_x
//         bit 31~0 - roi_x[31:0] (Read/Write)
// 0x044 : reserved
// 0x048 : Data signal of roi_y
//         bit 31~0 - roi_y[31:0] (Read/Write)
// 0x04c : reserved
// 0x050 : Data signal of roi_width
//         bit 31~0 - roi_width[31:0] (Read/Write)
// 0x054 : reserved
// 0x058 : Data signal of roi_height
//         bit 31~0 - roi_height[31:0] (Read/Write)
// 0x05c : reserved
// 0x060 : Data signal of out_rows
//         bit 31~0 - out_rows[31:0] (Read/Write)
// 0x064 : reserved
// 0x068 : Data signal of out_cols
//         bit 31~0 - out_cols[31:0] (Read/Write)
// 0x06c : reserved
// 0x070 : Data signal of intensity_sum
//         bit 31~0 - intensity_sum[31:0] (Read)
// 0x074 : Data signal of intensity_sum
//         bit 31~0 - intensity_sum[63:32] (Read)
// 0x078 : Control signal of intensity_sum
//         bit 0  - intensity_sum_ap_vld (Read/COR)
//         others - reserved
// 0x088 : Data signal of clipped_pixels
//         bit 31~0 - clipped_pixels[31:0] (Read)
// 0x08c : Control signal of clipped_pixels
//         bit 0  - clipped_pixels_ap_vld (Read/COR)
//         others - reserved
// 0x098 : Data signal of dark_pixels
//         bit 31~0 - dark_pixels[31:0] (Read)
// 0x09c : Control signal of dark_pixels
//         bit 0  - dark_pixels_ap_vld (Read/COR)
//         others - reserved
// 0x0a8 : Data signal of ccm_enable
//         bit 0  - ccm_enable[0] (Read/Write)
//         others - reserved
// 0x0ac : reserved
// 0x0b0 : Data signal of gamma_enable
//         bit 0  - gamma_enable[0] (Read/Write)
//         others - reserved
// 0x0b4 : reserved
// 0x0b8 : Data signal of awb_enable
//         bit 0  - awb_enable[0] (Read/Write)
//         others - reserved
// 0x0bc : reserved
// 0x110 : Data signal of red_sum
//         bit 31~0 - red_sum[31:0] (Read)
// 0x114 : Data signal of red_sum
//         bit 31~0 - red_sum[63:32] (Read)
// 0x118 : Control signal of red_sum
//         bit 0  - red_sum_ap_vld (Read/COR)
//         others - reserved
// 0x128 : Data signal of green_sum
//         bit 31~0 - green_sum[31:0] (Read)
// 0x12c : Data signal of green_sum
//         bit 31~0 - green_sum[63:32] (Read)
// 0x130 : Control signal of green_sum
//         bit 0  - green_sum_ap_vld (Read/COR)
//         others - reserved
// 0x140 : Data signal of blue_sum
//         bit 31~0 - blue_sum[31:0] (Read)
// 0x144 : Data signal of blue_sum
//         bit 31~0 - blue_sum[63:32] (Read)
// 0x148 : Control signal of blue_sum
//         bit 0  - blue_sum_ap_vld (Read/COR)
//         others - reserved
// 0x158 : Data signal of valid_pixels
//         bit 31~0 - valid_pixels[31:0] (Read)
// 0x15c : Control signal of valid_pixels
//         bit 0  - valid_pixels_ap_vld (Read/COR)
//         others - reserved
// 0x168 : Data signal of applied_red
//         bit 31~0 - applied_red[31:0] (Read)
// 0x16c : Control signal of applied_red
//         bit 0  - applied_red_ap_vld (Read/COR)
//         others - reserved
// 0x178 : Data signal of applied_green
//         bit 31~0 - applied_green[31:0] (Read)
// 0x17c : Control signal of applied_green
//         bit 0  - applied_green_ap_vld (Read/COR)
//         others - reserved
// 0x188 : Data signal of applied_blue
//         bit 31~0 - applied_blue[31:0] (Read)
// 0x18c : Control signal of applied_blue
//         bit 0  - applied_blue_ap_vld (Read/COR)
//         others - reserved
// 0x198 : Data signal of frame_sequence
//         bit 31~0 - frame_sequence[31:0] (Read)
// 0x19c : Control signal of frame_sequence
//         bit 0  - frame_sequence_ap_vld (Read/COR)
//         others - reserved
// 0x1a8 : Data signal of bilateral_enable
//         bit 0  - bilateral_enable[0] (Read/Write)
//         others - reserved
// 0x1ac : reserved
// 0x1b0 : Data signal of sigma_color
//         bit 15~0 - sigma_color[15:0] (Read/Write)
//         others   - reserved
// 0x1b4 : reserved
// 0x1b8 : Data signal of sigma_space_q8
//         bit 15~0 - sigma_space_q8[15:0] (Read/Write)
//         others   - reserved
// 0x1bc : reserved
// 0x1c0 : Data signal of sharpen_strength
//         bit 15~0 - sharpen_strength[15:0] (Read/Write)
//         others   - reserved
// 0x1c4 : reserved
// 0x0c0 ~
// 0x0ff : Memory 'matrix' (9 * 32b)
//         Word n : bit [31:0] - matrix[n]
// 0x100 ~
// 0x10f : Memory 'offsets' (3 * 32b)
//         Word n : bit [31:0] - offsets[n]
// 0x400 ~
// 0x7ff : Memory 'gamma' (768 * 8b)
//         Word n : bit [ 7: 0] - gamma[4n]
//                  bit [15: 8] - gamma[4n+1]
//                  bit [23:16] - gamma[4n+2]
//                  bit [31:24] - gamma[4n+3]
// (SC = Self Clear, COR = Clear on Read, TOW = Toggle on Write, COH = Clear on Handshake)

#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_AP_CTRL               0x000
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_GIE                   0x004
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_IER                   0x008
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_ISR                   0x00c
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_ROWS_DATA             0x010
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_BITS_ROWS_DATA             32
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_COLS_DATA             0x018
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_BITS_COLS_DATA             32
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_BLUE_GAIN_DATA        0x020
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_BITS_BLUE_GAIN_DATA        16
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_GREEN_GAIN_DATA       0x028
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_BITS_GREEN_GAIN_DATA       16
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_RED_GAIN_DATA         0x030
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_BITS_RED_GAIN_DATA         16
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_MEDIAN_ENABLE_DATA    0x038
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_BITS_MEDIAN_ENABLE_DATA    1
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_ROI_X_DATA            0x040
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_BITS_ROI_X_DATA            32
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_ROI_Y_DATA            0x048
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_BITS_ROI_Y_DATA            32
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_ROI_WIDTH_DATA        0x050
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_BITS_ROI_WIDTH_DATA        32
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_ROI_HEIGHT_DATA       0x058
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_BITS_ROI_HEIGHT_DATA       32
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_OUT_ROWS_DATA         0x060
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_BITS_OUT_ROWS_DATA         32
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_OUT_COLS_DATA         0x068
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_BITS_OUT_COLS_DATA         32
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_INTENSITY_SUM_DATA    0x070
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_BITS_INTENSITY_SUM_DATA    64
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_INTENSITY_SUM_CTRL    0x078
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_CLIPPED_PIXELS_DATA   0x088
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_BITS_CLIPPED_PIXELS_DATA   32
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_CLIPPED_PIXELS_CTRL   0x08c
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_DARK_PIXELS_DATA      0x098
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_BITS_DARK_PIXELS_DATA      32
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_DARK_PIXELS_CTRL      0x09c
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_CCM_ENABLE_DATA       0x0a8
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_BITS_CCM_ENABLE_DATA       1
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_GAMMA_ENABLE_DATA     0x0b0
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_BITS_GAMMA_ENABLE_DATA     1
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_AWB_ENABLE_DATA       0x0b8
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_BITS_AWB_ENABLE_DATA       1
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_RED_SUM_DATA          0x110
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_BITS_RED_SUM_DATA          64
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_RED_SUM_CTRL          0x118
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_GREEN_SUM_DATA        0x128
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_BITS_GREEN_SUM_DATA        64
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_GREEN_SUM_CTRL        0x130
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_BLUE_SUM_DATA         0x140
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_BITS_BLUE_SUM_DATA         64
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_BLUE_SUM_CTRL         0x148
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_VALID_PIXELS_DATA     0x158
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_BITS_VALID_PIXELS_DATA     32
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_VALID_PIXELS_CTRL     0x15c
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_APPLIED_RED_DATA      0x168
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_BITS_APPLIED_RED_DATA      32
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_APPLIED_RED_CTRL      0x16c
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_APPLIED_GREEN_DATA    0x178
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_BITS_APPLIED_GREEN_DATA    32
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_APPLIED_GREEN_CTRL    0x17c
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_APPLIED_BLUE_DATA     0x188
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_BITS_APPLIED_BLUE_DATA     32
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_APPLIED_BLUE_CTRL     0x18c
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_FRAME_SEQUENCE_DATA   0x198
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_BITS_FRAME_SEQUENCE_DATA   32
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_FRAME_SEQUENCE_CTRL   0x19c
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_BILATERAL_ENABLE_DATA 0x1a8
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_BITS_BILATERAL_ENABLE_DATA 1
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_SIGMA_COLOR_DATA      0x1b0
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_BITS_SIGMA_COLOR_DATA      16
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_SIGMA_SPACE_Q8_DATA   0x1b8
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_BITS_SIGMA_SPACE_Q8_DATA   16
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_SHARPEN_STRENGTH_DATA 0x1c0
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_BITS_SHARPEN_STRENGTH_DATA 16
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_MATRIX_BASE           0x0c0
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_MATRIX_HIGH           0x0ff
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_WIDTH_MATRIX               32
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_DEPTH_MATRIX               9
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_OFFSETS_BASE          0x100
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_OFFSETS_HIGH          0x10f
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_WIDTH_OFFSETS              32
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_DEPTH_OFFSETS              3
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_GAMMA_BASE            0x400
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_ADDR_GAMMA_HIGH            0x7ff
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_WIDTH_GAMMA                8
#define XCAMERA_COLOR_PIPELINE_V9_CONTROL_DEPTH_GAMMA                768

