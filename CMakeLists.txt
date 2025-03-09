cmake_minimum_required(VERSION 3.13..3.27)

# ====================================================================================
# Set release type
set(APP_VERSION "01.00.02")
# Set release type
set(CMAKE_BUILD_TYPE "Debug")
#set(CMAKE_BUILD_TYPE "Release")

# Set board type
#set(PICO_BOARD pico_w CACHE STRING "Board type")
set(PICO_BOARD pico2_w CACHE STRING "Board type")
# ====================================================================================

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ====================================================================================
# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

set(PICO_SDK_PATH ${CMAKE_CURRENT_LIST_DIR}/pico-sdk)
set(FREERTOS_KERNEL_PATH ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel)
#set(PICOTOOL_FORCE_FETCH_FROM_GIT 1)

add_compile_definitions(PLUTO_VER="${APP_VERSION}")
add_compile_definitions(PICO_CYW43_ARCH_FREERTOS)
add_compile_definitions(PICO_INCLUDE_RTC_DATETIME)
add_compile_definitions(FAKE_RTC)

# initialize pico-sdk from submodule
include(pico-sdk/pico_sdk_init.cmake)

project(usurper C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

include(FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2350_ARM_NTZ/FreeRTOS_Kernel_import.cmake)
#include(FreeRTOS_Kernel_import.cmake)

message("Creating githash.h")
execute_process(COMMAND
        ./create_githash_h
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        )

message("Running makefsdata python script")
execute_process(COMMAND
        ./makefsdata.py
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        )
        
add_executable(usurper
        pluto.c
        weather.c
        flash.c
        ssi.c
        cgi.c
        calendar.c
        utility.c
        config.c
        dhcpserver.c
        dnsserver.c
        led_strip.c
        sdk_callback.c
        watchdog.c
        worker_tasks.c
        message.c
        udp.c
        wifi.c
        thermostat.c
        powerwall.c
        shelly.c
        )      

# Add executable. Default name is the project name, version 0.1
pico_set_program_name(usurper "usurper")
pico_set_program_version(usurper ${APP_VERSION})

# Enable/disable output over UART/USB
if(CMAKE_BUILD_TYPE STREQUAL "Release")
        pico_enable_stdio_usb(usurper 1)
        pico_enable_stdio_uart(usurper 0)
else() 
        pico_enable_stdio_usb(usurper 0)
        pico_enable_stdio_uart(usurper 1)
endif()

target_include_directories(usurper PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/pico-sdk/lib/lwip/contrib/ports/freertos/include/ 
        #${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/
        ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2350_ARM_NTZ/include/        
        ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel/include/
        ${CMAKE_CURRENT_LIST_DIR}/pico-sdk/src/rp2_common/pico_cyw43_arch/include/
        ${CMAKE_CURRENT_LIST_DIR}/pico-sdk/lib/cyw43-driver/src/
        ${CMAKE_CURRENT_LIST_DIR}/pico-sdk/src/rp2_common/pico_cyw43_driver/include/
        ${CMAKE_CURRENT_LIST_DIR}/pico-sdk/lib/lwip/src/apps/altcp_tls/
        ${CMAKE_CURRENT_LIST_DIR}/pico-sdk/lib/mbedtls/include/
        )
target_link_libraries(usurper
        pico_cyw43_arch_lwip_sys_freertos
        pico_lwip_freertos
        pico_lwip_api        
        pico_lwip_sntp
        pico_lwip_http
        pico_stdlib
        pico_rand
        FreeRTOS-Kernel-Heap4
       # hardware_rtc
        pico_flash
        hardware_pio
        hardware_i2c
        pico_lwip_mbedtls
        pico_mbedtls
        )

# create uf2 
pico_add_extra_outputs(usurper)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/generated)

add_custom_command(OUTPUT ${CMAKE_CURRENT_LIST_DIR}/generated/ws2812.py
        DEPENDS ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio
        COMMAND Pioasm -o python ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio ${CMAKE_CURRENT_LIST_DIR}/generated/ws2812.py
        VERBATIM)
