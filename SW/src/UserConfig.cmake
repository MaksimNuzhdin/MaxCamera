set(GIGE_VITIS_ROOT "$ENV{VITIS_ROOT}" CACHE PATH "Vitis 2024.2 installation")
# Copyright (C) 2023-2024 Advanced Micro Devices, Inc.  All rights reserved.
# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.16)

###    USER SETTINGS  START    ###
# Below settings can be customized
# User needs to edit it manually as per their needs.
###    DO NOT ADD OR REMOVE VARIABLES FROM THIS SECTION    ###
# -----------------------------------------
# Add any compiler definitions, they will be added as extra definitions
# Example : Adding VERBOSE=1 will pass -DVERBOSE=1 to the compiler.
set(USER_COMPILE_DEFINITIONS
"MAX_CAMERA_GIGE=1"
"MAX_CAMERA_TRANSPORT_EXPERIMENT=0"
"MAX_CAMERA_ILA_CAPTURE=0"
"MAX_CAMERA_VIDEO_CHECK=0"
"MAX_CAMERA_VIDEO_FAST_LINK=0"
"MAX_CAMERA_VIDEO_CLOCK12=0"
"MAX_CAMERA_VIDEO_BARS=0"
)

# Undefine any previously specified compiler definitions, either built in or provided with a -D option
# Example : Adding MY_SYMBOL will pass -UMY_SYMBOL to the compiler.
set(USER_UNDEFINED_SYMBOLS
"__clang__"
)

# Isolated native-luminance bringup; defaults and normal GigE build are unchanged.
option(MAX_CAMERA_NATIVE_Y8_CHECK "Build dedicated native Y8 diagnostic firmware" OFF)
option(MAX_CAMERA_FLASH_PROBE "Read-only full flash backup diagnostic" OFF)
if(MAX_CAMERA_FLASH_PROBE)
    list(APPEND USER_COMPILE_DEFINITIONS "MAX_CAMERA_FLASH_PROBE=1")
endif()
option(MAX_CAMERA_GIGE_CSI_BYTES "Use full-width CSI byte FPGA in production" OFF)
if(MAX_CAMERA_GIGE_CSI_BYTES)
    list(APPEND USER_COMPILE_DEFINITIONS "MAX_CAMERA_GIGE_CSI_BYTES=1")
else()
    list(APPEND USER_COMPILE_DEFINITIONS "MAX_CAMERA_GIGE_CSI_BYTES=0")
endif()
option(MAX_CAMERA_GIGE_GPIO_SCCB "Use GPIO SCCB with matching full-resolution GigE FPGA" OFF)
if(MAX_CAMERA_GIGE_GPIO_SCCB)
    if(MAX_CAMERA_NATIVE_Y8_CHECK OR MAX_CAMERA_TRANSPORT_ILA_CHECK)
        message(FATAL_ERROR "GigE GPIO and isolated diagnostics are separate firmware images")
    endif()
    list(APPEND USER_COMPILE_DEFINITIONS "MAX_CAMERA_GIGE_GPIO_SCCB=1")
endif()
option(MAX_CAMERA_YUV422_CHECK "Build existing YUV422 pipeline control firmware" OFF)
option(MAX_CAMERA_TRANSPORT_ILA_CHECK "Build isolated SCCB waveform diagnostic" OFF)
option(MAX_CAMERA_ILA_PREP_READ "Save 503D before arming the SCCB ILA" ON)
if(MAX_CAMERA_TRANSPORT_ILA_CHECK)
    if(MAX_CAMERA_NATIVE_Y8_CHECK OR MAX_CAMERA_YUV422_CHECK)
        message(FATAL_ERROR "Select either SCCB ILA or video capture")
    endif()
    list(REMOVE_ITEM USER_COMPILE_DEFINITIONS "MAX_CAMERA_GIGE=1"
         "MAX_CAMERA_TRANSPORT_EXPERIMENT=0" "MAX_CAMERA_ILA_CAPTURE=0")
    list(APPEND USER_COMPILE_DEFINITIONS "MAX_CAMERA_GIGE=0"
         "MAX_CAMERA_TRANSPORT_EXPERIMENT=1" "MAX_CAMERA_ILA_CAPTURE=1"
         "MAX_CAMERA_ILA_PREP_READ=$<BOOL:${MAX_CAMERA_ILA_PREP_READ}>")
endif()
if(MAX_CAMERA_NATIVE_Y8_CHECK AND MAX_CAMERA_YUV422_CHECK)
    message(FATAL_ERROR "Select only one diagnostic capture path")
endif()
option(MAX_CAMERA_DIAGNOSTIC_VIDEO_RESET "Use reserved GPIO3 reset in matching source-level CSI-byte reset image" OFF)
option(MAX_CAMERA_DIAGNOSTIC_CAMERA_POWER "Pulse CAM_PWUP using matching camera-power FPGA image" OFF)
if(MAX_CAMERA_DIAGNOSTIC_CAMERA_POWER)
    if(NOT MAX_CAMERA_DIAGNOSTIC_VIDEO_RESET)
        message(FATAL_ERROR "Camera power diagnostic requires the matching video-reset image")
    endif()
    list(APPEND USER_COMPILE_DEFINITIONS "MAX_CAMERA_VIDEO_CAMERA_POWER=1")
endif()
if(MAX_CAMERA_DIAGNOSTIC_VIDEO_RESET)
    list(APPEND USER_COMPILE_DEFINITIONS "MAX_CAMERA_VIDEO_RESET=1")
endif()
option(MAX_CAMERA_RGB_SLOW_CLOCK "Use a coordinated slower RGB888 clock-tree experiment" OFF)
option(MAX_CAMERA_RGB_FAST_FORMAT_CLOCK "RGB888 experiment: SCLK/4, SCLK2X/1, original MIPI byte timing" OFF)
if(MAX_CAMERA_RGB_FAST_FORMAT_CLOCK)
    if(MAX_CAMERA_RGB_SLOW_CLOCK)
        message(FATAL_ERROR "Choose one RGB888 clock experiment")
    endif()
    list(APPEND USER_COMPILE_DEFINITIONS "MAX_CAMERA_RGB_FAST_FORMAT_CLOCK=1")
endif()
if(MAX_CAMERA_RGB_SLOW_CLOCK)
    list(APPEND USER_COMPILE_DEFINITIONS "MAX_CAMERA_RGB_SLOW_CLOCK=1")
endif()
option(MAX_CAMERA_RGB10_EXPERIMENT "Use ten-bit MIPI mode for native RGB888 isolation" OFF)
if(MAX_CAMERA_RGB10_EXPERIMENT)
    list(APPEND USER_COMPILE_DEFINITIONS "MAX_CAMERA_RGB10_EXPERIMENT=1")
endif()
option(MAX_CAMERA_RGB_STANDBY_EXPERIMENT "Apply RGB format while sensor is in software standby" OFF)
if(MAX_CAMERA_RGB_STANDBY_EXPERIMENT)
    list(APPEND USER_COMPILE_DEFINITIONS "MAX_CAMERA_RGB_STANDBY_EXPERIMENT=1")
endif()
option(MAX_CAMERA_DIAGNOSTIC_MANUAL_WB "Use unity manual white balance for format isolation" OFF)
if(MAX_CAMERA_DIAGNOSTIC_MANUAL_WB)
    list(APPEND USER_COMPILE_DEFINITIONS "MAX_CAMERA_VIDEO_MANUAL_WB=1")
endif()
option(MAX_CAMERA_DIAGNOSTIC_MINIMAL_ISP "Disable optional ISP stages for format isolation" OFF)
if(MAX_CAMERA_DIAGNOSTIC_MINIMAL_ISP)
    list(APPEND USER_COMPILE_DEFINITIONS "MAX_CAMERA_VIDEO_MINIMAL_ISP=1")
endif()
option(MAX_CAMERA_DIAGNOSTIC_RGB_ROI "Use unbinned RAW-qualified crop for RGB888 experiment" OFF)
if(MAX_CAMERA_DIAGNOSTIC_RGB_ROI)
    list(APPEND USER_COMPILE_DEFINITIONS "MAX_CAMERA_VIDEO_RGB_ROI=1")
endif()
option(MAX_CAMERA_CSI_BYTES_CHECK "Capture CSI memory-layout bytes without VFB" OFF)
option(MAX_CAMERA_DIAGNOSTIC_RGB888_CSC "YUV444 sensor transport to hardware RGB888" OFF)
if(MAX_CAMERA_DIAGNOSTIC_RGB888_CSC)
    if(MAX_CAMERA_CSI_BYTES_CHECK OR NOT MAX_CAMERA_DIAGNOSTIC_GPIO_SCCB OR
       NOT MAX_CAMERA_NATIVE_BYTE_FORMAT STREQUAL "RGB888")
        message(FATAL_ERROR "RGB888 CSC requires its matching GPIO/VFB image and RGB888 memory layout")
    endif()
    list(APPEND USER_COMPILE_DEFINITIONS "MAX_CAMERA_VIDEO_RGB888_CSC=1")
endif()
option(MAX_CAMERA_DIAGNOSTIC_FULL_RAW "Capture full sensor RAW using full-width GPIO FPGA" OFF)
if(MAX_CAMERA_DIAGNOSTIC_FULL_RAW)
    if(NOT MAX_CAMERA_CSI_BYTES_CHECK OR NOT MAX_CAMERA_DIAGNOSTIC_GPIO_SCCB OR
       NOT MAX_CAMERA_NATIVE_BYTE_FORMAT MATCHES "^(RAW8|RAW10)$")
        message(FATAL_ERROR "Full RAW requires RAW8/RAW10 and the full-width GPIO byte image")
    endif()
    list(APPEND USER_COMPILE_DEFINITIONS "MAX_CAMERA_VIDEO_FULL_RAW=1")
endif()
option(MAX_CAMERA_DIAGNOSTIC_GPIO_SCCB "Use open-drain GPIO SCCB in matching FPGA image" OFF)
if(MAX_CAMERA_DIAGNOSTIC_GPIO_SCCB)
    if((NOT MAX_CAMERA_CSI_BYTES_CHECK AND NOT MAX_CAMERA_DIAGNOSTIC_RGB888_CSC) OR NOT MAX_CAMERA_DIAGNOSTIC_VIDEO_RESET)
        message(FATAL_ERROR "GPIO SCCB requires the matching CSI-byte/reset FPGA image")
    endif()
    list(APPEND USER_COMPILE_DEFINITIONS "MAX_CAMERA_VIDEO_GPIO_SCCB=1")
endif()
if(MAX_CAMERA_DIAGNOSTIC_VIDEO_RESET AND NOT MAX_CAMERA_CSI_BYTES_CHECK AND NOT MAX_CAMERA_DIAGNOSTIC_RGB888_CSC)
    message(FATAL_ERROR "Diagnostic video reset requires CSI-byte capture")
endif()
if(MAX_CAMERA_CSI_BYTES_CHECK AND NOT MAX_CAMERA_NATIVE_Y8_CHECK)
    message(FATAL_ERROR "CSI byte capture needs the native diagnostic path")
endif()
if(MAX_CAMERA_CSI_BYTES_CHECK)
    list(APPEND USER_COMPILE_DEFINITIONS "MAX_CAMERA_VIDEO_CSI_BYTES=1")
endif()
if(MAX_CAMERA_NATIVE_Y8_CHECK)
    set(MAX_CAMERA_NATIVE_BYTE_FORMAT "Y8" CACHE STRING "Native sensor diagnostic format")
    if(MAX_CAMERA_NATIVE_BYTE_FORMAT STREQUAL "RAW8")
        list(APPEND USER_COMPILE_DEFINITIONS "MAX_CAMERA_VIDEO_RAW8=1")
    elseif(MAX_CAMERA_NATIVE_BYTE_FORMAT STREQUAL "RAW10")
        list(APPEND USER_COMPILE_DEFINITIONS "MAX_CAMERA_VIDEO_RAW10=1")
    elseif(MAX_CAMERA_NATIVE_BYTE_FORMAT MATCHES "^(YUYV|YVYU|UYVY|VYUY)$")
        if(NOT MAX_CAMERA_CSI_BYTES_CHECK)
            message(FATAL_ERROR "Native YUV422 orders require CSI-byte capture")
        endif()
        set(_camera_yuv_orders YUYV YVYU UYVY VYUY)
        list(FIND _camera_yuv_orders "${MAX_CAMERA_NATIVE_BYTE_FORMAT}" _camera_yuv_order)
        list(APPEND USER_COMPILE_DEFINITIONS "MAX_CAMERA_VIDEO_YUV422_BYTES=1" "MAX_CAMERA_VIDEO_YUV422_ORDER=${_camera_yuv_order}")
    elseif(MAX_CAMERA_NATIVE_BYTE_FORMAT STREQUAL "RGB555" OR MAX_CAMERA_NATIVE_BYTE_FORMAT STREQUAL "RGB444" OR MAX_CAMERA_NATIVE_BYTE_FORMAT STREQUAL "YUV444" OR MAX_CAMERA_NATIVE_BYTE_FORMAT STREQUAL "YUV420" OR MAX_CAMERA_NATIVE_BYTE_FORMAT STREQUAL "JPEG")
        if(NOT MAX_CAMERA_CSI_BYTES_CHECK)
            message(FATAL_ERROR "Packed RGB requires CSI payload byte capture")
        endif()
        list(APPEND USER_COMPILE_DEFINITIONS "MAX_CAMERA_VIDEO_${MAX_CAMERA_NATIVE_BYTE_FORMAT}=1")
    elseif(MAX_CAMERA_NATIVE_BYTE_FORMAT STREQUAL "RGB565")
        if(NOT MAX_CAMERA_CSI_BYTES_CHECK)
            message(FATAL_ERROR "RGB565 requires CSI payload byte capture")
        endif()
        list(APPEND USER_COMPILE_DEFINITIONS "MAX_CAMERA_VIDEO_RGB565=1")
    elseif(MAX_CAMERA_NATIVE_BYTE_FORMAT STREQUAL "RGB888")
        list(APPEND USER_COMPILE_DEFINITIONS "MAX_CAMERA_VIDEO_RGB888=1")
    elseif(NOT MAX_CAMERA_NATIVE_BYTE_FORMAT STREQUAL "Y8")
        message(FATAL_ERROR "Unknown native diagnostic format: ${MAX_CAMERA_NATIVE_BYTE_FORMAT}")
    endif()
    list(APPEND USER_COMPILE_DEFINITIONS "MAX_CAMERA_VIDEO_Y8=1")
endif()
if(MAX_CAMERA_NATIVE_Y8_CHECK OR MAX_CAMERA_YUV422_CHECK)
    set(MAX_CAMERA_DIAGNOSTIC_PREP_TESTS "1" CACHE STRING "Strict full preparation count; 1 uses normal whole-prepare attempts")
    if(NOT MAX_CAMERA_DIAGNOSTIC_PREP_TESTS MATCHES "^([1-9]|1[0-9]|20)$")
        message(FATAL_ERROR "Preparation test count must be 1..20")
    endif()
    list(APPEND USER_COMPILE_DEFINITIONS "MAX_CAMERA_VIDEO_PREP_TESTS=${MAX_CAMERA_DIAGNOSTIC_PREP_TESTS}")
    set(MAX_CAMERA_DIAGNOSTIC_IIC_HZ "50000" CACHE STRING "Diagnostic SCCB clock rate")
    if(NOT MAX_CAMERA_DIAGNOSTIC_IIC_HZ MATCHES "^(10000|50000|100000)$")
        message(FATAL_ERROR "Diagnostic SCCB rate must be 10000, 50000 or 100000")
    endif()
    list(APPEND USER_COMPILE_DEFINITIONS "MAX_CAMERA_VIDEO_IIC_HZ=${MAX_CAMERA_DIAGNOSTIC_IIC_HZ}")
    option(MAX_CAMERA_DIAGNOSTIC_BARS "Enable sensor color bars for diagnostic capture" ON)
    list(REMOVE_ITEM USER_COMPILE_DEFINITIONS "MAX_CAMERA_GIGE=1"
        "MAX_CAMERA_VIDEO_CHECK=0" "MAX_CAMERA_VIDEO_FAST_LINK=0"
        "MAX_CAMERA_VIDEO_CLOCK12=0" "MAX_CAMERA_VIDEO_BARS=0")
    list(APPEND USER_COMPILE_DEFINITIONS "MAX_CAMERA_GIGE=0" "MAX_CAMERA_VIDEO_CHECK=1"
        "MAX_CAMERA_VIDEO_FAST_LINK=1" "MAX_CAMERA_VIDEO_CLOCK12=1"
        "MAX_CAMERA_VIDEO_BARS=$<BOOL:${MAX_CAMERA_DIAGNOSTIC_BARS}>")
endif()


# Add any directories below, they will be added as extra include directories.
# Example 1: Adding /proj/data/include will pass -I/proj/data/include.
# Example 2: Adding ../../common/include will consider the path as relative to this component directory.
# Example 3: Adding ${CMAKE_SOURCE_DIR}/data/include to add data/include from this project.

set(USER_INCLUDE_DIRECTORIES
"${CMAKE_INCLUDE_PATH}"
"../drivers/LedsController"
".."
"../drivers/MaxFanController"
"../drivers/MaxTimer"
"../drivers/TempController"
"../drivers/UART"
)
set(USER_COMPILE_SOURCES
"../main.cpp"
"../gige/GigeVision.c"
"../gige/GigeImage.c"
"../gige/GigeSensorFeatures.c"
"../gige/GigeFpgaIsp.c"
"../gige/GigeRemapMap.c"
"../gige/GigeRemapControl.c"
"../gige/GigeRemapReader.c"
"../gige/GigeFpgaOutput.c"
"../gige/GigeGem.c"
"../gige/GigeCamera.cpp"
"../gige/GigeFlash.cpp"
"../gige/GigePreset.c"
"../gige/GigeTiming.c"
"../gige/GigeUserStore.c"
"../gige/GigeUserSets.cpp"
"../gige/GigeVisionBoard.cpp"
"../drivers/MaxTimer/MaxTimer.c"
"../drivers/UART/UART_driver.c"
"../drivers/TempController/TempController.c"
"../drivers/MaxFanController/MaxFanController.c"
"../drivers/LedsController/LedsController.c"
"../MaxIicDriver.c"
"../CameraDiagnostics.cpp"
"../Ov5640.c"
"../Ov5640MaxIic.c"
"../Ov5640Board.cpp"
"../AxiIicPolled.c"
"../AxiGpioSccb.c"
"../AxiGpioSccbAsync.c"
"../gige/GigeSensorTelemetry.c"
"../Ov5640TransportExperiment.cpp"
"../Ov5640VideoCheck.cpp"
)

# -----------------------------------------

# Turn on all optional warnings (-Wall)
set(USER_COMPILE_WARNINGS_ALL "-Wall")

# Enable extra warning flags (-Wextra)
set(USER_COMPILE_WARNINGS_EXTRA "-Wextra")

# Make all warnings into hard errors (-Werror)
set(USER_COMPILE_WARNINGS_AS_ERRORS "")

# Check the code for syntax errors, but don't do anything beyond that (-fsyntax-only)
set(USER_COMPILE_WARNINGS_CHECK_SYNTAX_ONLY "")

# Issue all the mandatory diagnostics listed in the C standard (-pedantic)
set(USER_COMPILE_WARNINGS_PEDANTIC "")

# Issue all the mandatory diagnostics, and make all mandatory diagnostics into errors. (-pedantic-errors)
set(USER_COMPILE_WARNINGS_PEDANTIC_AS_ERRORS "")

# Suppress all warnings (-w)
set(USER_COMPILE_WARNINGS_INHIBIT_ALL "")

# -----------------------------------------

# Optimization level   "-O0" [None], "-O1" [Optimize] , "-O2" [Optimize More], "-O3" [Optimize Most] or "-Os" [Optimize Size]
set(USER_COMPILE_OPTIMIZATION_LEVEL "-O2")

# Other flags related to optimization
set(USER_COMPILE_OPTIMIZATION_OTHER_FLAGS "")

# -----------------------------------------

# Debug level "" [None], "-g1" [Minimum], "g2" [Default], "g3" [Maximum]
set(USER_COMPILE_DEBUG_LEVEL "-g3")

# Other flags related to debugging
set(USER_COMPILE_DEBUG_OTHER_FLAGS "")

# -----------------------------------------

# Enable profiling (-pg) (This feature is not supported currently)
# set(USER_COMPILE_PROFILING_ENABLE )

# -----------------------------------------

# Verbose (-v)
set(USER_COMPILE_VERBOSE "")

# Support ANSI_PROGRAM (-ansi)
set(USER_COMPILE_ANSI "")

# Add any compiler options that are not covered by the above variables, they will be added as extra compiler options
# To enable profiling -pg [ for gprof ]  or -p [ for prof information ]
set(USER_COMPILE_OTHER_FLAGS "")

# -----------------------------------------

# Linker options
# Do not use the standard system startup files when linking.
# The standard system libraries are used normally, unless -nostdlib or -nodefaultlibs is used. (-nostartfiles)
set(USER_LINK_NO_START_FILES "")

# Do not use the standard system libraries when linking. (-nodefaultlibs)
set(USER_LINK_NO_DEFAULT_LIBS "")

# Do not use the standard system startup files or libraries when linking. (-nostdlib)
set(USER_LINK_NO_STDLIB "")

# Omit all symbol information. (-s)
set(USER_LINK_OMIT_ALL_SYMBOL_INFO "")


# -----------------------------------------

# Add any libraries to be linked below, they will be added as extra libraries.
# User needs to update USER_LINK_DIRECTORIES below with these library search paths.
set(USER_LINK_LIBRARIES
)

# Add any directories to look for the libraries to be linked.
# Example 1: Adding /proj/compression/lib will pass -L/proj/compression/lib to the linker.
# Example 2: Adding ../../common/lib will consider the path as relative to this directory and will pass the path to -L option.
set(USER_LINK_DIRECTORIES
)

# -----------------------------------------

set(USER_LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/lscript.ld")

# Add linker options to be passed, they will be added as extra linker options
# Example : Adding -s will pass -s to the linker.
set(USER_LINK_OTHER_FLAGS
)

# -----------------------------------------

###   END OF USER SETTINGS SECTION ###
###   DO NOT EDIT BEYOND THIS LINE ###

set(USER_COMPILE_OPTIONS
    " ${USER_COMPILE_WARNINGS_ALL}"
    " ${USER_COMPILE_WARNINGS_EXTRA}"
    " ${USER_COMPILE_WARNINGS_AS_ERRORS}"
    " ${USER_COMPILE_WARNINGS_CHECK_SYNTAX_ONLY}"
    " ${USER_COMPILE_WARNINGS_PEDANTIC}"
    " ${USER_COMPILE_WARNINGS_PEDANTIC_AS_ERRORS}"
    " ${USER_COMPILE_WARNINGS_INHIBIT_ALL}"
    " ${USER_COMPILE_OPTIMIZATION_LEVEL}"
    " ${USER_COMPILE_OPTIMIZATION_OTHER_FLAGS}"
    " ${USER_COMPILE_DEBUG_LEVEL}"
    " ${USER_COMPILE_DEBUG_OTHER_FLAGS}"
    " ${USER_COMPILE_VERBOSE}"
    " ${USER_COMPILE_ANSI}"
    " ${USER_COMPILE_OTHER_FLAGS}"
)
foreach(entry ${USER_UNDEFINED_SYMBOLS})
    list(APPEND USER_COMPILE_OPTIONS " -U${entry}")
endforeach()

set(USER_LINK_OPTIONS
    " ${USER_LINKER_NO_START_FILES}"
    " ${USER_LINKER_NO_DEFAULT_LIBS}"
    " ${USER_LINKER_NO_STDLIB}"
    " ${USER_LINKER_OMIT_ALL_SYMBOL_INFO}"
    " ${USER_LINK_OTHER_FLAGS}"
)

set(MAX_CAMERA_VIDEO_SECONDS 10 CACHE STRING "Diagnostic capture duration in seconds")
if(NOT MAX_CAMERA_VIDEO_SECONDS MATCHES "^[0-9]+$" OR MAX_CAMERA_VIDEO_SECONDS LESS 1 OR MAX_CAMERA_VIDEO_SECONDS GREATER 1800)
    message(FATAL_ERROR "MAX_CAMERA_VIDEO_SECONDS must be in [1,1800]")
endif()
list(APPEND USER_COMPILE_DEFINITIONS "MAX_CAMERA_VIDEO_SECONDS=${MAX_CAMERA_VIDEO_SECONDS}")


set(MAX_CAMERA_DIAGNOSTIC_RGB_ORDER 3 CACHE STRING "RGB888 output order0..5 or15")
if(NOT MAX_CAMERA_DIAGNOSTIC_RGB_ORDER MATCHES "^([0-5]|15)$")
    message(FATAL_ERROR "RGB888 order must be0..5 or15")
endif()
list(APPEND USER_COMPILE_DEFINITIONS "MAX_CAMERA_VIDEO_RGB_ORDER=${MAX_CAMERA_DIAGNOSTIC_RGB_ORDER}")

option(MAX_CAMERA_GIGE_RAW10_HW "Use FPGA RAW10 repacker; requires matching csi-raw10-hw bitstream" OFF)
if(MAX_CAMERA_GIGE_RAW10_HW)
    if(NOT MAX_CAMERA_GIGE_CSI_BYTES OR NOT MAX_CAMERA_GIGE_GPIO_SCCB)
        message(FATAL_ERROR "RAW10 hardware requires CSI bytes, GPIO SCCB and the matching csi-raw10-hw FPGA")
    endif()
    list(APPEND USER_COMPILE_DEFINITIONS "MAX_CAMERA_GIGE_RAW10_HW=1")
endif()
option(MAX_CAMERA_GIGE_FPGA_ISP "Use integrated RAW10 ISP; requires matching fpga-isp bitstream" OFF)
if(MAX_CAMERA_GIGE_FPGA_ISP)
    if(NOT MAX_CAMERA_GIGE_RAW10_HW)
        message(FATAL_ERROR "Integrated FPGA ISP requires RAW10 hardware capture")
    endif()
    list(APPEND USER_COMPILE_DEFINITIONS "MAX_CAMERA_GIGE_FPGA_ISP=1")
endif()

# The matching v8 image uses the installed official Xilinx VPSS scaler drivers.
if(MAX_CAMERA_GIGE_FPGA_ISP)
    set(_camera_video_drivers "${GIGE_VITIS_ROOT}/data/embeddedsw/XilinxProcessorIPLib/drivers")
    list(APPEND USER_COMPILE_DEFINITIONS "GIGE_POLYPHASE_SUPPORT=1")
    list(APPEND USER_COMPILE_SOURCES "../gige/GigeFpgaPolyphase.c")
    foreach(_camera_scaler h v)
        set(_camera_scaler_dir "${_camera_video_drivers}/v_${_camera_scaler}scaler_v3_3/src")
        list(APPEND USER_INCLUDE_DIRECTORIES "${_camera_scaler_dir}")
        foreach(_camera_suffix "" "_l2" "_coeff")
            list(APPEND USER_COMPILE_SOURCES "${_camera_scaler_dir}/xv_${_camera_scaler}scaler${_camera_suffix}.c")
        endforeach()
    endforeach()
    list(APPEND USER_INCLUDE_DIRECTORIES "${_camera_video_drivers}/video_common_v4_16/src")
endif()
