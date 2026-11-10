#!/usr/bin/env python3
"""Self-contained stopped configuration for full-field FPGA RGB preview; no save/load."""
from stream_led_camera_common import read,write,tables


def apply_working(camera):
    d=camera.get_device()
    if read(d,'SENSOR_BUSY'):raise RuntimeError('Stop acquisition and wait for SensorBusy=false')
    if read(d,'FPGA_VERSION')<9 or read(d,'FPGA_CAPABILITIES')&65535!=65535:
        raise RuntimeError('This recipe requires the qualified v9/v10 processing capability set')
    if read(d,'FPGA_CAPABILITIES')&65536:
        write(d,'FPGA_ADAPTIVE_TONE',0);write(d,'FPGA_TONE_TARGET',128)
    write(d,'FPGA_ENABLE',0);write(d,'COMPRESSION',0);write(d,'FRAME_RATE_ENABLE',0)
    for key in ['FPGA_CCM','FPGA_GAMMA','FPGA_AWB','FPGA_MEDIAN','FPGA_BILATERAL','FPGA_SHARPEN',
                'FPGA_LSC','FPGA_CLAHE','FPGA_ROI_ENABLE','FPGA_SCALER','FPGA_INTERPOLATION']:write(d,key,0)
    d.set_string_feature_value('PixelFormat','YUV422_8')
    d.set_integer_feature_value('Width',640);d.set_integer_feature_value('Height',480)
    d.set_string_feature_value('PixelFormat','RGB8')
    for key,value in [('TEST_PATTERN',0),('TEST_STYLE',0),('TEST_ROLL',0),('TEST_TRANSPARENT',0),('TEST_BW',0),
                      ('MIRROR',0),('FLIP',0),('AWB_AUTO',0),('NIGHT_MODE',0),
                      ('FPGA_BLACK',0),('FPGA_BPC',1),('FPGA_RED',256),('FPGA_GREEN',256),('FPGA_BLUE',256),
                      ('FPGA_INPUT_WIDTH',2592),('FPGA_INPUT_HEIGHT',1944),('FPGA_ROI_X',0),('FPGA_ROI_Y',0),
                      ('FPGA_ROI_WIDTH',2592),('FPGA_ROI_HEIGHT',1944),('FPGA_SIGMA_COLOR',24),
                      ('FPGA_SIGMA_SPACE',256),('FPGA_CLAHE_CLIP',4),('FPGA_YUV_RANGE',0)]:write(d,key,value)
    lut=[round(255*(x/255)**(1/1.8)) for x in range(256)]
    tables(d,{'matrix':[1048576 if i%4==0 else 0 for i in range(9)],'offsets':[0]*3,
              'gamma':lut*3,'lsc':[4096]*252})
    for key,value in [('FPGA_SCALER',1),('FPGA_INTERPOLATION',1),('FPGA_GAMMA',1),
                      ('FPGA_AWB',2),('FPGA_SCENE',1),('FPGA_ENABLE',1)]:write(d,key,value)
    # This stopped timing write also configures the selected RAW full-array
    # sensor geometry before setting its exposure limits and automation.
    write(d,'SENSOR_FPS',read(d,'FRAME_RATE_MAX'))
    maximum=read(d,'EXPOSURE_MAX_LINES')
    write(d,'MAX_EXPOSURE_50',maximum);write(d,'MAX_EXPOSURE_60',maximum);write(d,'MAX_GAIN',512)
    write(d,'EXPOSURE_AUTO',2);write(d,'GAIN_AUTO',2)
    return {'width':640,'height':480,'input_width':2592,'input_height':1944,
            'interpolation':'Polyphase','display_gamma':1.8,'fpga_awb':'Continuous',
            'black':0,'base_rgb_gains':[1,1,1],'flash_written':False}
