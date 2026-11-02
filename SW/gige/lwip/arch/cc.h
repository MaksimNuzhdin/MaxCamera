#ifndef GIGE_LWIP_ARCH_CC_H
#define GIGE_LWIP_ARCH_CC_H
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#define BYTE_ORDER LITTLE_ENDIAN
#ifdef ARMR5
#include "xil_printf.h"
#define LWIP_PLATFORM_DIAG(x) do { xil_printf x; } while (0)
#define LWIP_PLATFORM_ASSERT(x) do { xil_printf("lwIP assertion: %s\r\n", (x)); for(;;) {} } while(0)
#else
#define LWIP_PLATFORM_DIAG(x) do { printf x; } while (0)
#define LWIP_PLATFORM_ASSERT(x) do { printf("lwIP assertion: %s\n", (x)); abort(); } while(0)
#endif
#define LWIP_RAND() ((uint32_t)rand())
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END
#define PACK_STRUCT_FIELD(x) x
#endif
