# Build the Vitis-supplied lwIP core privately; the platform BSP is not modified.
set(GIGE_VITIS_ROOT "$ENV{VITIS_ROOT}" CACHE PATH "Vitis installation for lwIP sources")
set(LWIP_DIR "${GIGE_VITIS_ROOT}/data/embeddedsw/ThirdParty/sw_services/lwip220_v1_0/src/lwip-2.2.0")
if(NOT EXISTS "${LWIP_DIR}/src/Filelists.cmake")
    message(FATAL_ERROR "lwIP 2.2.0 missing; set GIGE_VITIS_ROOT to Vitis 2024.2")
endif()
file(GLOB lwipcore_SRCS CONFIGURE_DEPENDS "${LWIP_DIR}/src/core/*.c")
file(GLOB lwipcore4_SRCS CONFIGURE_DEPENDS "${LWIP_DIR}/src/core/ipv4/*.c")
add_library(gige_lwip STATIC ${lwipcore_SRCS} ${lwipcore4_SRCS} "${LWIP_DIR}/src/netif/ethernet.c")
target_include_directories(gige_lwip PUBLIC "${CMAKE_CURRENT_LIST_DIR}/lwip" "${LWIP_DIR}/src/include")
target_compile_options(gige_lwip PRIVATE -O2 -x c)

set(GIGE_XML_PYTHON "/usr/bin/python3" CACHE FILEPATH "Host Python 3.10+ for GenApi XML generation")
set(GIGE_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}")
add_custom_command(
    OUTPUT "${GIGE_SOURCE_DIR}/GigeVisionXmlRom.h" "${GIGE_SOURCE_DIR}/MaxCamera.xml" "${GIGE_SOURCE_DIR}/GigeFeatureRegisters.h"
    COMMAND "${GIGE_XML_PYTHON}" "${GIGE_SOURCE_DIR}/../tools/generate_gige_xml.py"
    DEPENDS "${GIGE_SOURCE_DIR}/../tools/generate_gige_xml.py"
            "${GIGE_SOURCE_DIR}/GigeVisionVendorDefines.h"
            "${GIGE_SOURCE_DIR}/GigeVisionUnitDefines.h"
            "${GIGE_SOURCE_DIR}/GigeVisionRegisters.h"
            "${GIGE_SOURCE_DIR}/../tools/gige_feature_catalog.py"
            "${GIGE_SOURCE_DIR}/../src/Ov5640Registers.h"
    COMMENT "Generating GenApi XML and GVCP ROM"
    VERBATIM)
add_custom_target(gige_xml DEPENDS "${GIGE_SOURCE_DIR}/GigeVisionXmlRom.h" "${GIGE_SOURCE_DIR}/MaxCamera.xml")

if(MAX_CAMERA_WEB_HTTP)
    target_compile_definitions(gige_lwip PUBLIC MAX_CAMERA_WEB_HTTP=1)
    add_custom_command(
        OUTPUT "${GIGE_SOURCE_DIR}/GigeWebRom.h"
        COMMAND "${GIGE_XML_PYTHON}" "${GIGE_SOURCE_DIR}/../tools/generate_web_rom.py"
        DEPENDS "${GIGE_SOURCE_DIR}/../tools/generate_web_rom.py"
                "${GIGE_SOURCE_DIR}/../ai/web/index.html"
                "${GIGE_SOURCE_DIR}/../ai/web/style.css"
                "${GIGE_SOURCE_DIR}/../ai/web/app.js"
        VERBATIM)
    add_custom_target(gige_web_rom DEPENDS "${GIGE_SOURCE_DIR}/GigeWebRom.h")
endif()
