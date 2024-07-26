cmake_minimum_required(VERSION 3.13)

#set(CMAKE_BUILD_TYPE "Release")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_SDK_PATH ${CMAKE_CURRENT_LIST_DIR}/pico-sdk)
set(FREERTOS_KERNEL_PATH ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel)

set(PICO_BOARD pico_w CACHE STRING "Board type")
add_compile_definitions(PCIO_BOARD pico_w)

include(pico_sdk_import.cmake)
include(FreeRTOS_Kernel_import.cmake)

project(usurper C CXX ASM)

pico_sdk_init()


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

message("Prepending #include \"lwip/apps/fs.h\" to htmldata.c")
execute_process(COMMAND
        fix_htmldata_include
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
        )

#pico_set_linker_script(usurper ${CMAKE_CURRENT_LIST_DIR}/memmap_usurper.ld)        

target_compile_definitions(usurper PRIVATE
        )

target_include_directories(usurper PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        #${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts
        ${CMAKE_CURRENT_LIST_DIR}/pico-sdk/lib/lwip/contrib/ports/freertos/include/
        #${CMAKE_CURRENT_LIST_DIR}/../../../../../../usurper/
        ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel/include/
        ${CMAKE_CURRENT_LIST_DIR}/pico-sdk/src/rp2_common/pico_stdio/include
        )
target_link_libraries(usurper
        #pico_cyw43_arch_lwip_threadsafe_background
        #pico_cyw43_arch_lwip_poll
        pico_cyw43_arch_lwip_sys_freertos
        pico_lwip_freertos
        pico_lwip_api         #mentioned in documentation
        pico_lwip_sntp
        pico_lwip_http
        pico_stdlib
        pico_rand
        #pico_lwip_iperf
        FreeRTOS-Kernel-Heap4 # FreeRTOS kernel and dynamic heap
        hardware_rtc
        pico_flash
        hardware_pio
        #pico_lwip_mbedtls # for tls testing
        #pico_mbedtls      # for tls testing
        )
pico_add_extra_outputs(usurper)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/generated)

add_custom_command(OUTPUT ${CMAKE_CURRENT_LIST_DIR}/generated/ws2812.py
        DEPENDS ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio
        COMMAND Pioasm -o python ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio ${CMAKE_CURRENT_LIST_DIR}/generated/ws2812.py
        VERBATIM)


