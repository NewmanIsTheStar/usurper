target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_sources(${PROJECT_NAME} PRIVATE
    pluto.c
    web.c
    flash.c
    calendar.c
    utility.c
    config.c
    dhcpserver.c
    dnsserver.c
    sdk_callback.c
    watchdog.c
    udp.c
    wifi.c
    usurper_ping.c
    ping_core.c
)

target_include_directories(${PROJECT_NAME} PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
        ${PROJECT_SOURCE_DIR}/pico-sdk/lib/lwip/contrib/ports/freertos/include/ 
        if(PICO_BOARD STREQUAL "pico_w")
        ${PROJECT_SOURCE_DIR}/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/
        else()
        ${PROJECT_SOURCE_DIR}/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2350_ARM_NTZ/include/
        endif()        
        ${PROJECT_SOURCE_DIR}/FreeRTOS-Kernel/include/
        ${PROJECT_SOURCE_DIR}/pico-sdk/src/rp2_common/pico_cyw43_arch/include/
        ${PROJECT_SOURCE_DIR}/pico-sdk/lib/cyw43-driver/src/
        ${PROJECT_SOURCE_DIR}/pico-sdk/src/rp2_common/pico_cyw43_driver/include/
        ${PROJECT_SOURCE_DIR}/pico-sdk/lib/lwip/src/apps/altcp_tls/
        ${PROJECT_SOURCE_DIR}/pico-sdk/lib/mbedtls/include/
        ${PROJECT_SOURCE_DIR}/pico-sdk/lib/lwip/contrib/apps/ping/
        )

# pluto common libraries
set(pluto_link_libraries
        pico_cyw43_arch_lwip_sys_freertos
        pico_lwip_freertos
        pico_lwip_api        
        pico_lwip_sntp
        pico_lwip_http
        pico_lwip_mqtt
        pico_stdlib
        pico_rand
        FreeRTOS-Kernel-Heap4
        pico_flash
        hardware_pio
        hardware_i2c
        pico_httpd_content
        )
# pico_w has insufficent RAM to incorporate HTTPS web client (thus tesla powerwall monitoring is disabled on pico_w)
if(NOT PICO_BOARD STREQUAL "pico_w")
        set(pluto_link_libraries ${pluto_link_libraries}        
                pico_lwip_mbedtls
                pico_mbedtls)        
endif()


target_link_libraries(${PROJECT_NAME} ${pluto_link_libraries})

# create githash used instead of APP_VERSION for debug builds
message("Creating githash.h")
execute_process(COMMAND
        ./create_githash_h
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        )
        

