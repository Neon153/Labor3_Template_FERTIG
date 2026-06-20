aux_source_directory(. CLOCK_SRCS)

add_executable(${CMAKE_PROJECT_NAME} ${CLOCK_SRCS})

target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC WANT_FREE_RTOS=1)

add_subdirectory(${FREERTOS_PORT_PATH} ${CMAKE_CURRENT_BINARY_DIR}/FreeRTOS_Kernel)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../lib/Config ${CMAKE_CURRENT_BINARY_DIR}/lib/Config)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../lib/lcd ${CMAKE_CURRENT_BINARY_DIR}/lib/lcd)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../lib/lvgl ${CMAKE_CURRENT_BINARY_DIR}/lib/lvgl)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../lib/qmi8658 ${CMAKE_CURRENT_BINARY_DIR}/lib/qmi8658)

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ../../lib)
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ../../external/FreeRTOS-Kernel/include)
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ../../external/FreeRTOS-Kernel/portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_RISC-V/include/)

target_link_libraries(${CMAKE_PROJECT_NAME} pico_stdlib hardware_adc hardware_dma hardware_gpio hardware_spi hardware_timer hardware_sync Config lcd qmi8658 FreeRTOS-Kernel FreeRTOS-Kernel-Heap4 lvgl)
pico_add_extra_outputs(${CMAKE_PROJECT_NAME})

# Additionally enable the USB port for stdio (printf) use serial console to access
pico_enable_stdio_usb(${CMAKE_PROJECT_NAME} 1)
