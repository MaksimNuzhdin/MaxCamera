#ifndef GIGE_IMAGE_H
#define GIGE_IMAGE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define GIGE_PIXEL_MONO8 0x01080001U
#define GIGE_PIXEL_RGB8 0x02180014U
#define GIGE_PIXEL_BGR8 0x02180015U
#define GIGE_PIXEL_UYVY8 0x0210001fU
#define GIGE_PIXEL_YUYV8 0x02100032U
#define GIGE_PIXEL_RGB565 0x02100035U
/* Vendor codes are explicit custom layouts, never borrowed PFNC IDs. */
#define GIGE_PIXEL_RGB555 0x82100001U
#define GIGE_PIXEL_RGB444 0x82100002U
#define GIGE_PIXEL_YUV420 0x820C0003U
#define GIGE_PIXEL_YVYU8 0x82100004U
#define GIGE_PIXEL_VYUY8 0x82100005U
#define GIGE_PIXEL_YUV444 0x02180020U
#define GIGE_PIXEL_BG8 0x0108000BU
#define GIGE_PIXEL_BG10 0x0110000FU
#define GIGE_PIXEL_BG10P 0x010A0052U
/* Return actual JPEG end (0 on malformed/truncated), ignoring fixed-canvas padding. */
uint32_t GigeImage_JpegBytes(const uint8_t*,uint32_t);
unsigned GigeImage_WidthMax(uint32_t);
unsigned GigeImage_HeightMax(uint32_t);
unsigned GigeImage_WidthIncrement(uint32_t format);
unsigned GigeImage_RowBytes(uint32_t width,uint32_t format);
unsigned GigeImage_Payload(uint32_t width,uint32_t height,uint32_t format);
int GigeImage_Raw(uint32_t format);
/* CSI no-VFB memory to tightly packed PFNC/custom output, excluding DMA padding. */
void GigeImage_FromCsi(uint8_t*,const uint8_t*,uint32_t,uint32_t,uint32_t,uint32_t);
unsigned GigeImage_BytesPerPixel(uint32_t format);
int GigeImage_Valid(uint32_t width,uint32_t height,uint32_t format);
/* pixels must be even and format must pass GigeImage_BytesPerPixel().
 * Input is tightly packed Y0 Cb Y1 Cr, full-range YCbCr (PFNC generic YUV). Output is PFNC. */
void GigeImage_Convert(uint8_t *dst,const uint8_t *src,uint32_t pixels,uint32_t format);
#ifdef __cplusplus
}
#endif
#endif
