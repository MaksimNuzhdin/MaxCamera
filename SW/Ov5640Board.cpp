#include "Ov5640Board.h"
#include "Ov5640TransportExperiment.h"
#include "Ov5640VideoCheck.h"
#include "xil_printf.h"
OV5640 g_camera;
void Ov5640Board_Init(MAX_IIC *bus)
{
#if MAX_CAMERA_VIDEO_CHECK
    Ov5640VideoCheck_Run();
    return;
#elif MAX_CAMERA_TRANSPORT_EXPERIMENT
    Ov5640TransportExperiment_Run(bus);
    return;
#else
    OV5640_CONFIG config;
    OV5640_STATUS status;
    OV5640_RESULT e = Ov5640_BindMaxIic(&g_camera, bus);
    if (e == OV5640_OK)
        e = Ov5640_Init(&g_camera);
    Ov5640_DefaultConfig(&config);
    if (e == OV5640_OK)
        e = Ov5640_Configure(&g_camera, &config);
    if (e == OV5640_OK)
        e = Ov5640_GetStatus(&g_camera, &status);
    if (e != OV5640_OK)
    {
        xil_printf("OV5640 init/config failed: %d, bus=%d reg=0x%04x state=%d\r\n", (int)e,
                   g_camera.transport_error, g_camera.error_register, (int)g_camera.state);
        xil_printf("OV5640 verified power NACKs=%d\r\n", (int)g_camera.verified_power_nacks);
        return;
    }
    uint32_t fps = Ov5640_FrameRateMilliHz(&config);
    xil_printf(
        "OV5640 configured: CAM0 MIPI 2 lanes 640x480 YUYV; nominal %d.%03d fps, XCLK=%d\r\n",
        (int)(fps / 1000), (int)(fps % 1000), (int)config.clock.xclk_hz);
    xil_printf(
        "OV5640 readback: HTS=%d VTS=%d gain16=%d exposure=%d; OUTPUT GATED (receiver must be "
        "ready before Stream)\r\n",
        status.hts, status.vts, status.gain16, (int)status.exposure_lines);
#endif
}
