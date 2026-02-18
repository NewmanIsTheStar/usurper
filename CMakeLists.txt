cmake_minimum_required(VERSION 3.13..3.27)

# ====================================================================================
# Set release version
set(APP_VERSION "01.00.02")

# Set build type
set(CMAKE_BUILD_TYPE "Debug")
#set(CMAKE_BUILD_TYPE "Release")

# Set board type (uncomment *one* of the following lines)
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
#set(CMAKE_EXECUTE_PROCESS_COMMAND_ECHO STDOUT)

set(PICO_SDK_PATH ${CMAKE_CURRENT_LIST_DIR}/pico-sdk)
set(FREERTOS_KERNEL_PATH ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel)
#set(PICOTOOL_FORCE_FETCH_FROM_GIT 1)

add_compile_definitions(PLUTO_VER="${APP_VERSION}")
add_compile_definitions(PICO_CYW43_ARCH_FREERTOS)
add_compile_definitions(PICO_INCLUDE_RTC_DATETIME)
add_compile_definitions(FAKE_RTC)
# if(NOT PICO_BOARD STREQUAL "pico_w") 
# add_compile_definitions(INCORPORATE_THERMOSTAT)
# add_compile_definitions(INCORPORATE_HOME_CONTROLLER)
# endif()

# initialize pico-sdk from submodule
include(pico-sdk/pico_sdk_init.cmake)

# set the project name ${PROJECT_NAME}
project(usurper C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

if(NOT PICO_BOARD STREQUAL "pico_w") 
include(FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2350_ARM_NTZ/FreeRTOS_Kernel_import.cmake)
else()
include(FreeRTOS_Kernel_import.cmake)
endif()

# create "generated" directory
file(MAKE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/generated)

# create githash used instead of APP_VERSION for debug builds
message("Creating githash.h")
execute_process(COMMAND
        ./create_githash_h
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        )
if(CMAKE_BUILD_TYPE STREQUAL "Debug")       
        add_compile_definitions(USE_GIT_HASH_AS_VERSION)
endif()

# # create html binary blob source file
# message("Running makefsdata python script")
# execute_process(COMMAND
#         ./makefsdata.py
#         WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
#         )

#add_executable(pio_ws2812)

# generate the ws2812.pio.h 
#pico_generate_pio_header(usurper ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)

# target_sources(pio_ws2812 PRIVATE ws2812.c)

# target_link_libraries(pio_ws2812 PRIVATE pico_stdlib hardware_pio)
# pico_add_extra_outputs(pio_ws2812)


# create pio header file for controlling addressable led strips 
add_custom_command(OUTPUT ${CMAKE_CURRENT_LIST_DIR}/generated/ws2812.py
        DEPENDS ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio
        COMMAND Pioasm -o python ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio ${CMAKE_CURRENT_LIST_DIR}/generated/ws2812.py
        VERBATIM)

# generate tm1637.pio.h
# pico_generate_pio_header(pio_tm1637 ${CMAKE_CURRENT_LIST_DIR}/tm1637.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)

# create pio header file for controlling seven segnment display 
add_custom_command(OUTPUT ${CMAKE_CURRENT_LIST_DIR}/generated/tm1637.py
        DEPENDS ${CMAKE_CURRENT_LIST_DIR}/tm1637.pio
        COMMAND Pioasm -o python ${CMAKE_CURRENT_LIST_DIR}/tm1637.pio ${CMAKE_CURRENT_LIST_DIR}/generated/tm1637.py
        VERBATIM)        

set(common_source_files
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
        # worker_tasks.c
        message.c
        udp.c
        wifi.c
        usurper_ping.c
        ping_core.c
        )

# if(NOT PICO_BOARD STREQUAL "pico_w")
#         set(usurper_source_files ${usurper_source_files}        
#                 thermostat_task.c
#                 powerwall.c
#                 shelly.c
#                 json_parser.c
#                 hc_task.c
#                 discovery_task.c)        
# endif()  

# ====================================================================================
# pluto common libraries
set(pluto_link_libraries
        pico_cyw43_arch_lwip_sys_freertos
        pico_lwip_freertos
        pico_lwip_api        
        pico_lwip_sntp
        pico_lwip_http
        pico_stdlib
        pico_rand
        FreeRTOS-Kernel-Heap4
        pico_flash
        hardware_pio
        hardware_i2c)

# pico_w has hardware rtc but fake rtc is currently used for all variants
# if(PICO_BOARD STREQUAL "pico_w")
#         set(pluto_link_libraries ${pluto_link_libraries}        
#                 hardware_rtc)  
# endif()

# pico_w has insufficent RAM to incorporate HTTPS web client (thus tesla powerwall monitoring is disabled on pico_w)
# if(NOT PICO_BOARD STREQUAL "pico_w")
#         set(pluto_link_libraries ${pluto_link_libraries}        
#                 pico_lwip_mbedtls
#                 pico_mbedtls)        
# endif()


# ====================================================================================
# Usurper

message("Creating HTML binary blob for USURPER")
execute_process(COMMAND
        ../makefsdata.py
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/usurper/
        )

#if(NOT PICO_BOARD STREQUAL "pico_w")
        set(usurper_source_files ${common_source_files}   
                usurper/worker_tasks.c     
                #thermostat_task.c
                #powerwall.c
                #shelly.c
                json_parser.c
                #hc_task.c
                #discovery_task.c
                #tm1637.c
                )        
#endif()  

add_executable(${PROJECT_NAME} ${usurper_source_files})      
target_compile_definitions(${PROJECT_NAME} PRIVATE APP_NAME="Usurper" HTTPD_FSDATA_FILE="usurper/htmldata.c")
# if(NOT PICO_BOARD STREQUAL "pico_w") 
# target_compile_definitions(${PROJECT_NAME} PRIVATE INCORPORATE_THERMOSTAT)
# target_compile_definitions(${PROJECT_NAME} PRIVATE INCORPORATE_HOME_CONTROLLER)
# endif()

pico_generate_pio_header(usurper ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)

# Add executable name and version
pico_set_program_name(${PROJECT_NAME} "usurper")
pico_set_program_version(${PROJECT_NAME} ${APP_VERSION})

# Enable/disable output over UART/USB
if(CMAKE_BUILD_TYPE STREQUAL "Release")
        pico_enable_stdio_usb(${PROJECT_NAME} 0)
        pico_enable_stdio_uart(${PROJECT_NAME} 0)      
else() 
        pico_enable_stdio_usb(${PROJECT_NAME} 0)
        pico_enable_stdio_uart(${PROJECT_NAME} 1)        
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/pico-sdk/lib/lwip/contrib/ports/freertos/include/ 
        if(PICO_BOARD STREQUAL "pico_w")
        ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/
        else()
        ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2350_ARM_NTZ/include/
        endif()        
        ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel/include/
        ${CMAKE_CURRENT_LIST_DIR}/pico-sdk/src/rp2_common/pico_cyw43_arch/include/
        ${CMAKE_CURRENT_LIST_DIR}/pico-sdk/lib/cyw43-driver/src/
        ${CMAKE_CURRENT_LIST_DIR}/pico-sdk/src/rp2_common/pico_cyw43_driver/include/
        ${CMAKE_CURRENT_LIST_DIR}/pico-sdk/lib/lwip/src/apps/altcp_tls/
        ${CMAKE_CURRENT_LIST_DIR}/pico-sdk/lib/mbedtls/include/
        ${CMAKE_CURRENT_LIST_DIR}/pico-sdk/lib/lwip/contrib/apps/ping/
        )




target_link_libraries(${PROJECT_NAME} ${pluto_link_libraries})


# create uf2 
pico_add_extra_outputs(${PROJECT_NAME})



# # ====================================================================================
# # Thermostat

# message("Creating HTML binary blob for THERMOSTAT")
# execute_process(COMMAND
#         ../makefsdata.py
#         WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/thermostat/
#         )

# if(NOT PICO_BOARD STREQUAL "pico_w")
#         set(thermostat_source_files ${common_source_files}
#                 thermostat/worker_tasks.c        
#                 thermostat_task.c
#                 thermostat_aht10.c
#                 thermostat_metrics.c
#                 thermostat_physical_ui.c
#                 thermostat_web_ui.c
#                 thermostat_hvac.c
#                 powerwall.c
#                 json_parser.c
#                 tm1637.c)        
# endif()  

# add_executable(thermostat ${thermostat_source_files})  
# target_compile_definitions(thermostat PRIVATE APP_NAME="Thermostat" HTTPD_FSDATA_FILE="thermostat/htmldata.c")
# if(NOT PICO_BOARD STREQUAL "pico_w") 
# target_compile_definitions(thermostat PRIVATE INCORPORATE_THERMOSTAT)
# endif()

# pico_generate_pio_header(thermostat ${CMAKE_CURRENT_LIST_DIR}/tm1637.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)

# pico_set_program_name(thermostat "thermostat")
# pico_set_program_version(thermostat ${APP_VERSION})

# # Enable/disable output over UART/USB
# if(CMAKE_BUILD_TYPE STREQUAL "Release")
#         pico_enable_stdio_usb(thermostat 1)
#         pico_enable_stdio_uart(thermostat 0)        
# else() 
#         pico_enable_stdio_usb(thermostat 0)
#         pico_enable_stdio_uart(thermostat 1)        
# endif()

# target_include_directories(thermostat PRIVATE
#         ${CMAKE_CURRENT_LIST_DIR}
#         ${CMAKE_CURRENT_LIST_DIR}/pico-sdk/lib/lwip/contrib/ports/freertos/include/ 
#         if(PICO_BOARD STREQUAL "pico_w")
#         ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/
#         else()
#         ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2350_ARM_NTZ/include/
#         endif()        
#         ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel/include/
#         ${CMAKE_CURRENT_LIST_DIR}/pico-sdk/src/rp2_common/pico_cyw43_arch/include/
#         ${CMAKE_CURRENT_LIST_DIR}/pico-sdk/lib/cyw43-driver/src/
#         ${CMAKE_CURRENT_LIST_DIR}/pico-sdk/src/rp2_common/pico_cyw43_driver/include/
#         ${CMAKE_CURRENT_LIST_DIR}/pico-sdk/lib/lwip/src/apps/altcp_tls/
#         ${CMAKE_CURRENT_LIST_DIR}/pico-sdk/lib/mbedtls/include/
#         ${CMAKE_CURRENT_LIST_DIR}/pico-sdk/lib/lwip/contrib/apps/ping/
#         )

# target_link_libraries(thermostat ${pluto_link_libraries})

# # create uf2 
# pico_add_extra_outputs(thermostat)



# # ====================================================================================
# # Home Controller

# message("Creating HTML binary blob for HOME CONTROLLER")
# execute_process(COMMAND
#         ../makefsdata.py
#         WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/home_controller/
#         )

# if(NOT PICO_BOARD STREQUAL "pico_w")
#         set(home_controller_source_files ${common_source_files}   
#                 home_controller/worker_tasks.c   
#                 #thermostat_task.c
#                 powerwall.c
#                 shelly.c
#                 json_parser.c
#                 hc_task.c
#                 discovery_task.c)        
# endif()  

# add_executable(home_controller ${home_controller_source_files})  
# target_compile_definitions(home_controller PRIVATE APP_NAME="Home_Controller" HTTPD_FSDATA_FILE="home_controller/htmldata.c")
# if(NOT PICO_BOARD STREQUAL "pico_w") 
# # target_compile_definitions(home_controller PRIVATE INCORPORATE_THERMOSTAT)
# target_compile_definitions(home_controller PRIVATE INCORPORATE_HOME_CONTROLLER)
# endif()

# pico_set_program_name(home_controller "home_controller")
# pico_set_program_version(home_controller ${APP_VERSION})

# # Enable/disable output over UART/USB
# if(CMAKE_BUILD_TYPE STREQUAL "Release")
#         pico_enable_stdio_usb(home_controller 1)
#         pico_enable_stdio_uart(home_controller 0)        
# else() 
#         pico_enable_stdio_usb(home_controller 0)
#         pico_enable_stdio_uart(home_controller 1)        
# endif()

# target_include_directories(home_controller PRIVATE
#         ${CMAKE_CURRENT_LIST_DIR}
#         ${CMAKE_CURRENT_LIST_DIR}/pico-sdk/lib/lwip/contrib/ports/freertos/include/ 
#         if(PICO_BOARD STREQUAL "pico_w")
#         ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/
#         else()
#         ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2350_ARM_NTZ/include/
#         endif()        
#         ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel/include/
#         ${CMAKE_CURRENT_LIST_DIR}/pico-sdk/src/rp2_common/pico_cyw43_arch/include/
#         ${CMAKE_CURRENT_LIST_DIR}/pico-sdk/lib/cyw43-driver/src/
#         ${CMAKE_CURRENT_LIST_DIR}/pico-sdk/src/rp2_common/pico_cyw43_driver/include/
#         ${CMAKE_CURRENT_LIST_DIR}/pico-sdk/lib/lwip/src/apps/altcp_tls/
#         ${CMAKE_CURRENT_LIST_DIR}/pico-sdk/lib/mbedtls/include/
#         ${CMAKE_CURRENT_LIST_DIR}/pico-sdk/lib/lwip/contrib/apps/ping/
#         )

# target_link_libraries(home_controller ${pluto_link_libraries})

# # create uf2 
# pico_add_extra_outputs(home_controller)

# ====================================================================================

# TODO -- rebuild modified html without running cmake again
# pico_set_lwip_httpd_content(pico_httpd_content INTERFACE
# ${CMAKE_CURRENT_LIST_DIR}/html_files/index.shtml
# ${CMAKE_CURRENT_LIST_DIR}/html_files/config.shtml
# ${CMAKE_CURRENT_LIST_DIR}/html_files/page2.shtml
# ${CMAKE_CURRENT_LIST_DIR}/html_files/ip_config.js
# ${CMAKE_CURRENT_LIST_DIR}/html_files/sprajax.js
# ${CMAKE_CURRENT_LIST_DIR}/html_files/favicon.ico
# )

# set(WEB_FILES)
# file(GLOB WEB_FILES "${CMAKE_CURRENT_SOURCE_DIR}/thermostat/html_files/*.shtml")
# pico_set_lwip_httpd_content(pico_httpd_content INTERFACE ${WEB_FILES})

#pico_set_lwip_httpd_content(pico_httpd_content INTERFACE ${CMAKE_CURRENT_LIST_DIR}/thermostat/html_files/t_gpio.shtml)
