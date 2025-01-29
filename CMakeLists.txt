# ====================================================================================
# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)



# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

set(PICO_BOARD pico2_w CACHE STRING "Board type")
#add_compile_definitions(PICO_BOARD pico2_w)
add_compile_definitions(PICO_CYW43_ARCH_FREERTOS)
add_compile_definitions(PICO_INCLUDE_RTC_DATETIME)
add_compile_definitions(FAKE_RTC)

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(gumbo C CXX ASM)

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
        
add_executable(gumbo
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

# Add executable. Default name is the project name, version 0.1

#add_executable(gumbo gumbo.c )

pico_set_program_name(gumbo "gumbo")
pico_set_program_version(gumbo "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(gumbo 1)
pico_enable_stdio_usb(gumbo 0)

target_include_directories(gumbo PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${USERHOME}/.pico-sdk/sdk/2.1.0/lib/lwip/contrib/ports/freertos/include 
        #${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/
        ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2350_ARM_NTZ/include        
        ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel/include/
        ${USERHOME}/.pico-sdk/sdk/2.1.0/src/rp2_common/pico_cyw43_arch/include
        ${USERHOME}/.pico-sdk/sdk/2.1.0/lib/cyw43-driver/src/
        ${USERHOME}/.pico-sdk/sdk/2.1.0/src/rp2_common/pico_cyw43_driver/include/
        )
target_link_libraries(gumbo
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
        )


pico_add_extra_outputs(gumbo)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/generated)

add_custom_command(OUTPUT ${CMAKE_CURRENT_LIST_DIR}/generated/ws2812.py
        DEPENDS ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio
        COMMAND Pioasm -o python ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio ${CMAKE_CURRENT_LIST_DIR}/generated/ws2812.py
        VERBATIM)
