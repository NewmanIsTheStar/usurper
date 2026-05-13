cmake_minimum_required(VERSION 3.13..3.27)

# ====================================================================================
# Set release version
set(APP_VERSION "01.00.03")

# Set build type
set(CMAKE_BUILD_TYPE "Debug")
#set(CMAKE_BUILD_TYPE "Release")

# Set board type
set(PICO_BOARD pico_w CACHE STRING "Board type")
#set(PICO_BOARD pico2_w CACHE STRING "Board type")
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

add_compile_definitions(PLUTO_VER="${APP_VERSION}")
add_compile_definitions(PICO_CYW43_ARCH_FREERTOS)
add_compile_definitions(PICO_INCLUDE_RTC_DATETIME)
add_compile_definitions(FAKE_RTC)

# ====================================================================================
include(pico-sdk/pico_sdk_init.cmake)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

if(PICO_BOARD STREQUAL "pico_w") 
    include(FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)
elseif(PICO_BOARD STREQUAL "pico2_w") 
    include(FreeRTOS-Kernel/portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_ARM_NTZ/FreeRTOS_Kernel_import.cmake)
else()
    message(FATAL_ERROR "Unsupported board type: ${BOARD_TYPE}")
endif()

# create "generated" directory
file(MAKE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/generated)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")       
        add_compile_definitions(USE_GIT_HASH_AS_VERSION)
endif()

      
# ====================================================================================
# usurper

project(usurper C CXX ASM)   # set the project name ${PROJECT_NAME}

add_executable(${PROJECT_NAME})
add_subdirectory(pluto)  
add_subdirectory(application)

target_compile_definitions(${PROJECT_NAME} PRIVATE APP_NAME="${PROJECT_NAME}")
# target_compile_definitions(${PROJECT_NAME} PRIVATE INCORPORATE_THERMOSTAT)

pico_set_program_name(${PROJECT_NAME} "${PROJECT_NAME}")
pico_set_program_version(${PROJECT_NAME} ${APP_VERSION})

# enable/disable output over UART/USB
if(CMAKE_BUILD_TYPE STREQUAL "Release")
        pico_enable_stdio_usb(${PROJECT_NAME} 0)    # this should be 1 but it started causing random crashes at startup with some sdk versions!
        pico_enable_stdio_uart(${PROJECT_NAME} 0)        
else() 
        pico_enable_stdio_usb(${PROJECT_NAME} 0)
        pico_enable_stdio_uart(${PROJECT_NAME} 1)        
endif()

# create uf2 
pico_add_extra_outputs(${PROJECT_NAME})




