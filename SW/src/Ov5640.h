/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef OV5640_H
#define OV5640_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define OV5640_ADDRESS 0x3cU
#define OV5640_CHIP_ID 0x5640U
/* Transport returns 0 on success. Transfers are synchronous; no internal retries.
 * Return -5 specifically for NACK: only power-transition NACK is conditionally
 * accepted after a delay and successful readback. Other writes never retry.
 * One caller per device/bus. Delay callback must really delay, including in init.
 * Hardware power/reset and the input clock must already be available. */
typedef struct
{
    void *user;
    int (*read)(void *, uint8_t, uint16_t, uint8_t *, uint16_t);
    int (*write)(void *, uint8_t, uint16_t, const uint8_t *, uint16_t);
    void (*delay_us)(void *, uint32_t);
} OV5640_IO;
typedef enum {
    OV5640_OK = 0,
    OV5640_E_ARGUMENT = -100,
    OV5640_E_IO = -101,
    OV5640_E_ID = -102,
    OV5640_E_STATE = -103,
    OV5640_E_TIMEOUT = -104,
    OV5640_E_RANGE = -105
} OV5640_RESULT;
typedef enum {
    OV5640_BOUND,
    OV5640_READY,
    OV5640_CONFIGURED,
    OV5640_STREAMING,
    OV5640_FAULT
} OV5640_STATE;
typedef enum { OV5640_MIPI, OV5640_DVP } OV5640_INTERFACE;
typedef enum {
    OV5640_YUYV,
    OV5640_YVYU,
    OV5640_UYVY,
    OV5640_VYUY,
    OV5640_RGB565,
    OV5640_RGB888,
    OV5640_RAW8,
    OV5640_JPEG,
    /* Native ISP luminance; CSI data type/packing must be qualified on hardware. */
    OV5640_Y8,
    OV5640_RAW10,
    /* Format2 padded big-endian words. On AN5641 these two RGB payloads
     * carry CSI DT0x2a, not standard RGB555/RGB444 packet types. */
    OV5640_RGB555, /* formatter0x81: 0rrrrrgg gggbbbbb */
    OV5640_RGB444, /* formatter0xa1: 0000rrrr ggggbbbb */
    OV5640_YUV444, /* formatter0x20: Y U V; separate CSI qualification */
    OV5640_YUV420, /* MIPI-only formatter0x5f: UYY / VYY alternating rows */
    OV5640_FORMAT_COUNT
} OV5640_FORMAT;
/* Explicit clock tree, rather than unverified clock presets. SCLK =
 * xclk * multiplier / prediv / sysdiv / root_div * 2 / bit_div2x / sclk_div.
 * RAW10 uses 10-bit MIPI mode (bit_div2x=5); other formats use 8-bit
 * mode (bit_div2x=4). root_div=2,
 * sclk_div=2, sclk2x_div=1, pclk_div=1. Link clock=PLL/sysdiv/mipi_div.
 */
typedef struct
{
    uint32_t xclk_hz;
    uint8_t prediv, multiplier, sysdiv, mipi_div;
} OV5640_CLOCK;
typedef struct
{
    uint16_t x_start, y_start, x_end, y_end, width, height, hts, vts, x_offset, y_offset;
    uint8_t x_increment, y_increment; /* odd/even nibbles, each odd and nonzero */
} OV5640_WINDOW;
typedef struct
{
    OV5640_INTERFACE interface;
    OV5640_FORMAT format;
    OV5640_CLOCK clock;
    OV5640_WINDOW window;
    uint8_t lanes, virtual_channel, continuous_clock, mirror, flip;
} OV5640_CONFIG;
typedef struct
{
    OV5640_IO io;
    uint8_t address;
    OV5640_STATE state;
    uint32_t verified_power_nacks; /* 3008 NACK followed by matching state + ID */
    int transport_error;
    uint16_t error_register;
    OV5640_CONFIG config; /* valid only in CONFIGURED/STREAMING */
} OV5640;
typedef struct
{
    uint16_t reg;
    uint8_t mask, value;
} OV5640_PATCH;
/* Advanced feature configuration follows the chapter-7 register definitions.
 * Each patch explicitly selects bits; values must fit mask. Reserved/debug bits
 * are the caller's responsibility. This is not a substitute for semantic APIs.
 * OTP, SCCB address, MCU RAM and grouped-write command are deliberately excluded.
 * Raw feature patches do not update the cached config. Do not change clock,
 * format or geometry behind Configure; reset/PLL writes are rejected here.
 */
typedef enum {
    OV5640_FEATURE_PAD_SYSTEM,
    OV5640_FEATURE_AEC_AGC,
    OV5640_FEATURE_LIGHT_DETECTOR,
    OV5640_FEATURE_STROBE_FREX,
    OV5640_FEATURE_MC,
    OV5640_FEATURE_BLC,
    OV5640_FEATURE_FRAME,
    OV5640_FEATURE_FORMAT,
    OV5640_FEATURE_JPEG,
    OV5640_FEATURE_VFIFO,
    OV5640_FEATURE_DVP,
    OV5640_FEATURE_MIPI,
    OV5640_FEATURE_ISP_FRAME,
    OV5640_FEATURE_ISP,
    OV5640_FEATURE_AWB,
    OV5640_FEATURE_CIP,
    OV5640_FEATURE_CMX,
    OV5640_FEATURE_GAMMA,
    OV5640_FEATURE_SDE,
    OV5640_FEATURE_SCALE,
    OV5640_FEATURE_AVERAGE,
    OV5640_FEATURE_LENC,
    OV5640_FEATURE_AFC,
    OV5640_FEATURE_VCM
} OV5640_FEATURE;
typedef struct
{
    uint8_t enable_bits; /* 5580: fixedY,negative,gray,fixedV,fixedU,contrast,sat,hue */
    uint8_t hue_cos, hue_sin, u, v, y_offset, contrast, brightness;
    uint8_t signs; /* 5588[5:0], 5588[6] is saturation manual mode */
    uint8_t manual_saturation, uv_threshold1;
    uint16_t uv_threshold2;
} OV5640_EFFECT;
typedef struct
{
    uint32_t exposure_lines;
    uint16_t gain16, r_gain, g_gain, b_gain, hts, vts;
    uint8_t manual_exposure, manual_gain, manual_awb, test_pattern;
} OV5640_STATUS;
OV5640_RESULT Ov5640_Bind(OV5640 *, const OV5640_IO *, uint8_t address);
OV5640_RESULT Ov5640_Probe(OV5640 *, uint16_t *id);
/* Init resets and loads analog/ISP baseline; returns READY with frame output gated. */
OV5640_RESULT Ov5640_Init(OV5640 *); /* board convenience: XCLK=25MHz */
/* Configure.clock.xclk_hz must match this physical input clock. */
OV5640_RESULT Ov5640_InitWithClock(OV5640 *, uint32_t xclk_hz);
void Ov5640_DefaultConfig(OV5640_CONFIG *); /* board 25MHz, 2 lanes, VGA YUYV ~30fps */
/* AN5641, assumed physical XCLK=12MHz, 2 lanes, unbinned 640x480 RAW8 ROI.
 * Nominal 8.933fps, HTS2844/VTS1968. Qualified with dedicated RAW8 FPGA path;
 * a YUV-only receiver cannot capture this preset. InitWithClock(12000000) first.
 */
void Ov5640_Raw8VgaConfig(OV5640_CONFIG *);
/* Same AN5641 crop, native RAW10, nominal 7.146fps. Requires dedicated RAW10
 * FPGA path preserving all ten sample bits. InitWithClock(12000000) first. */
void Ov5640_Raw10VgaConfig(OV5640_CONFIG *);
/* Full unbinned 2592x1944 variants, same HTS/VTS and nominal rates as above.
 * Require the qualified full-width FPGA path; CSI RAW10 active row3240 bytes,
 * DMA stride3248. These presets do not add a GigE/GenICam pixel format. */
void Ov5640_Raw8FullConfig(OV5640_CONFIG *);
void Ov5640_Raw10FullConfig(OV5640_CONFIG *);
/* Replacing an existing configuration holds the core in standby until Stream(1).
 * Configure also clears an earlier fixed-frame JPEG canvas; set it again after. */
OV5640_RESULT Ov5640_Configure(OV5640 *, const OV5640_CONFIG *);
OV5640_RESULT Ov5640_Stream(OV5640 *, int enable);
/* Legacy 8-bit clock helper. Use ConfigSystemClockHz for format-aware timing. */
uint32_t Ov5640_SystemClockHz(const OV5640_CLOCK *);
uint32_t Ov5640_ConfigSystemClockHz(const OV5640_CONFIG *);
uint32_t Ov5640_FrameRateMilliHz(const OV5640_CONFIG *);
OV5640_RESULT Ov5640_Read(OV5640 *, uint16_t, uint8_t *, uint16_t);
/* Advanced raw writes invalidate configuration; run Init + Configure before Stream(1).
 * Raw functions can access reserved/irreversible registers: use with datasheet.
 */
OV5640_RESULT Ov5640_Write(OV5640 *, uint16_t, const uint8_t *, uint16_t);
OV5640_RESULT Ov5640_ApplyFeature(OV5640 *, OV5640_FEATURE, const OV5640_PATCH *, size_t);
OV5640_RESULT Ov5640_SetAutoExposure(OV5640 *, int exposure_auto, int gain_auto);
OV5640_RESULT Ov5640_SetAutoExposureTarget(OV5640 *, uint8_t target);
OV5640_RESULT Ov5640_SetCip(OV5640 *, int denoise, int automatic, uint8_t strength);
OV5640_RESULT Ov5640_SetExposure(OV5640 *, uint32_t lines, uint16_t gain16);
OV5640_RESULT Ov5640_SetExposureLimits(OV5640 *, uint16_t max50, uint16_t max60,
                                       uint16_t max_gain16, int night_mode);
/* 0=disable, 50/60=fixed, 1=automatic detector. Recomputes banding steps. */
OV5640_RESULT Ov5640_SetBanding(OV5640 *, unsigned int hz);
OV5640_RESULT Ov5640_SetWhiteBalance(OV5640 *, int automatic, uint16_t red, uint16_t green,
                                     uint16_t blue);
OV5640_RESULT Ov5640_SetOrientation(OV5640 *, int mirror, int flip);
/* 5000 mask allowed=a7, 5001=a7, 5003=07; other bits preserved. */
OV5640_RESULT Ov5640_SetIsp(OV5640 *, uint8_t isp0, uint8_t isp1, uint8_t isp3);
OV5640_RESULT Ov5640_SetEffect(OV5640 *, const OV5640_EFFECT *);
/* pattern 0 bars,1 random,2 square,3 black; style0..3, flags roll/transparency/BW. */
OV5640_RESULT Ov5640_SetTestPattern(OV5640 *, int enable, uint8_t pattern, uint8_t style,
                                    int rolling, int transparent, int bw);
OV5640_RESULT Ov5640_SetGamma(OV5640 *, const uint8_t curve[16], uint8_t control);
OV5640_RESULT Ov5640_SetLensCorrection(OV5640 *, const uint8_t table[62], int enable);
OV5640_RESULT Ov5640_SetDrawWindow(OV5640 *, uint16_t left, uint16_t top, uint16_t right,
                                   uint16_t bottom, uint8_t u, uint8_t v, int enable);
OV5640_RESULT Ov5640_SetJpegQuality(OV5640 *, uint8_t quantization_scale);
/* Mode2 fixed byte canvas with dummy padding; encoded image geometry stays
 * in config.window. Call after Configure(JPEG), while output is stopped. */
OV5640_RESULT Ov5640_SetJpegFixedFrame(OV5640 *,uint16_t byte_width,uint16_t lines,uint8_t padding);
OV5640_RESULT Ov5640_SetJpegQuantTable(OV5640 *, uint8_t start, const uint8_t *, uint16_t count);
OV5640_RESULT Ov5640_SetJpegComment(OV5640 *, const uint8_t *, uint8_t count, uint8_t marker);
OV5640_RESULT Ov5640_GetJpegStatus(OV5640 *, uint32_t *length, uint8_t *overflow);
OV5640_RESULT Ov5640_SetVcm(OV5640 *, uint16_t target, uint8_t slew, uint16_t divider,
                            uint8_t current, int rih, int power_down);
OV5640_RESULT Ov5640_SetFrexExposure(OV5640 *, uint32_t lines, uint32_t strobe_width,
                                     uint8_t frame_delay);
OV5640_RESULT Ov5640_RequestFrex(OV5640 *);
OV5640_RESULT Ov5640_GetStatus(OV5640 *, OV5640_STATUS *);
/* Atomic launch on frame boundary; group storage has limited sensor SRAM.
 * Only masked, pre-read values are written while group hold is active.
 * No nested groups. In standby writes apply directly; streaming uses SRB hold. Max 13 writes.
 * Failure enters FAULT; Init is required.
 */
OV5640_RESULT Ov5640_GroupUpdate(OV5640 *, uint8_t group, const OV5640_PATCH *, size_t count);
/* OTP READ only: loads the sensor's 32-byte OTP shadow and polls busy boundedly.
 * Programming is a separately gated manufacturing API below, never used at boot.
 */
OV5640_RESULT Ov5640_ReadOtp(OV5640 *, uint8_t data[32]);
OV5640_RESULT Ov5640_SetAddress(OV5640 *, uint8_t address);
#define OV5640_OTP_CONFIRM 0x4f545021UL
/* Disabled unless compiled with OV5640_ENABLE_OTP_PROGRAM=1. Irreversible.
 * Caller must physically supply/measure AVDD 2.1V +-5% (avdd_mv is an assertion,
 * not a sensor voltage measurement). Bytes0..4 are protected manufacturer data.
 * desired is the final value of user bytes5..31; only 0->1 allowed.
 * Manufacturing sequence is unqualified on this module; never called in tests
 * against the board. Validate independently before production use.
 */
OV5640_RESULT Ov5640_ProgramOtp(OV5640 *, const uint8_t desired[27], uint32_t avdd_mv,
                                uint32_t confirmation);
/* MCU firmware is supplied by caller, not an autofocus implementation.
 * Halt/load/start touches 3000[5], RAM8000..8fff. Standby required.
 */
OV5640_RESULT Ov5640_LoadMcu(OV5640 *, const uint8_t *, uint16_t count);
OV5640_RESULT Ov5640_McuCommand(OV5640 *, uint8_t command, const uint8_t parameters[5],
                                uint32_t timeout_us);
#ifdef __cplusplus
}
#endif
#endif
