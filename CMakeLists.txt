cmake_minimum_required(VERSION 3.13..3.27)

set(CMAKE_BUILD_TYPE "Debug")
#set(CMAKE_BUILD_TYPE "Release")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_SDK_PATH ${CMAKE_CURRENT_LIST_DIR}/pico-sdk)
set(FREERTOS_KERNEL_PATH ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel)
#set(PICOTOOL_FORCE_FETCH_FROM_GIT 1)

set(PICO_BOARD pico_w CACHE STRING "Board type")
add_compile_definitions(PCIO_BOARD pico_w)
#set(PICO_BOARD pico2_w CACHE STRING "Board type")
#add_compile_definitions(PCIO_BOARD pico2_w)

# initialize pico-sdk from submodule
# note: this must happen before project()
include(pico-sdk/pico_sdk_init.cmake)

project(usurper C CXX ASM)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

include(FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

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
        )      

target_compile_definitions(usurper PRIVATE
        )
        
#target_compile_options(usurper PRIVATE -O3 -Wall -Wunused-function -pedantic)

target_include_directories(usurper PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/pico-sdk/lib/lwip/contrib/ports/freertos/include/
        #${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/
        ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/include        
        ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel/include/
        ${CMAKE_CURRENT_LIST_DIR}/pico-sdk/src/rp2_common/pico_stdio/include
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
        hardware_rtc
        pico_flash
        hardware_pio
        hardware_i2c
        )

if(CMAKE_BUILD_TYPE STREQUAL "Release")
        pico_enable_stdio_usb(usurper 1)
        pico_enable_stdio_uart(usurper 0)
endif()

pico_add_extra_outputs(usurper)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/generated)

add_custom_command(OUTPUT ${CMAKE_CURRENT_LIST_DIR}/generated/ws2812.py
        DEPENDS ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio
        COMMAND Pioasm -o python ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio ${CMAKE_CURRENT_LIST_DIR}/generated/ws2812.py
        VERBATIM)


