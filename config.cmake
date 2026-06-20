# See https://github.com/raspberrypi/pico-sdk.git
message(STATUS "Setting PICO Toolchain")
set(PICO_PLATFORM "rp2350-riscv") # or "rp2350", which defaults to "rp2350-arm-s" -- we want RISC-V
set(PICO_BOARD "waveshare_rp2350_lcd_1.28") # or "waveshare_rp2040_lcd_1.28"
set(PICO_COMMON_LANG_FLAGS "-march=rv32imac_zba_zbb_zbkb_zbs_zicsr_zifencei -mabi=ilp32")

if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/external")
message(STATUS "Link to external/ Directory ${CMAKE_CURRENT_LIST_DIR}/external/ does not exist")
endif()

set(PICO_DEOPTIMIZED_DEBUG 1)

# cmake_path(SET PICOTOOL_FETCH_FROM_GIT_PATH "${CMAKE_CURRENT_LIST_DIR}/external/usr/")
cmake_path(SET PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/external/pico-sdk/")
cmake_path(SET PICO_TOOLCHAIN_PATH "${CMAKE_CURRENT_LIST_DIR}/external/usr/bin/")
# This is ugly, since PICOTOOL_EXECUTABLE does NOT work: just enlarge the PATH
set(ENV{PATH} "${CMAKE_CURRENT_LIST_DIR}/external/usr/bin/:$ENV{PATH}")
message(STATUS "PATH:$ENV{PATH}")

set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_LIST_DIR}/lib/lv_conf.h" CACHE PATH "Where and under what name to find lv_conf.h")
set(LV_BUILD_SET_CONFIG_OPTS OFF CACHE BOOL "Use all values from lv_conf_internal.h")
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "Disable Demos")
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "Disable Examples")
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "Disable ThorVG")

set(FREERTOS_KERNEL_PATH "${CMAKE_CURRENT_LIST_DIR}/external/FreeRTOS-Kernel" CACHE PATH "Path to FreeRTOS-Kernel")
set(FREERTOS_PORT_PATH "${FREERTOS_KERNEL_PATH}/portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_RISC-V" CACHE PATH "Path to FreeRTOS-Kernel port for RP2350 RISC-V")
