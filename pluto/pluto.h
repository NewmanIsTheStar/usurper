
/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef PLUTO_H
#define PLUTO_H



#include "pico/cyw43_arch.h"
#include "pico/types.h"
#include "pico/stdlib.h"
//#include "hardware/rtc.h"
#include "pico/util/datetime.h"
#include "hardware/watchdog.h"

#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/apps/lwiperf.h"
#include "lwip/apps/sntp.h"
#include "lwip/apps/httpd.h"
#include "dhcpserver.h"
#include "dnsserver.h"

#include "lwip/sockets.h"

#include "time.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

// #include "weather.h"
// #include "led_strip.h"
#include "cgi.h"
#include "flash.h"
#include "utility.h"
#include "config.h"
#include "watchdog.h"
#include "worker_tasks.h"
#include "wifi.h"
#include "calendar.h"
// #include "powerwall.h"
// #include "shelly.h"
// #include  "ping_core.h"
#include "web.h"

//#define PLUTO_VER "01.00.00"          // release version set in CMakeLists.txt
//#define USE_GIT_HASH_AS_VERSION       // automatically set in CMakeLists.txt for debug builds

#define STRNCPY(dst, src, size)  {strncpy((dst), (src), (size)); *((dst)+(size)-1)=0;}
#define STRNCAT(dst, src, size)  {if ((size) > 0) {*((dst)+(size)-1)=0; strncat((dst), (src), (size)-strlen((dst))-1);};}
#define STRAPPEND(dst, src)  {*((dst)+(sizeof(dst))-1)=0; strncat((dst), (src), (sizeof(dst))-strlen((dst))-1);}
#define NUM_ROWS(x) (sizeof(x)/sizeof(x[0]))
#define MASKED_WRITE(dest,src,mask) {(dest) = (((dest) & (~(mask))) | ((src) & (mask)));}
#define CLIP(x, low, high)  (x=(((x)>(high))?(high):(((x)<(low))?(low):(x))))
#define SLEEP_MS(x) (vTaskDelay(x));
#define FORCE_STRING_TERMINATION(x) {x[sizeof(x)-1]=0;}
#define SAFEAROUND(x) ((x) >= 0 ? (int)((x) + 0.5f) : (int)((x) - 0.5f))

typedef enum
{
    REBOOT_USER_REQUEST     = 0,
    REBOOT_SNTP_FAILURE     = 1,
    REBOOT_WEATHER_FAILURE  = 2,
    REBOOT_WATCHDOG         = 3,
    REBOOT_MQTT_F1          = 4, 
    REBOOT_MQTT_F2          = 5,   
    REBOOT_MQTT_F3          = 6,   
    REBOOT_MQTT_F4          = 7,   
    REBOOT_MQTT_F5          = 8,                      
    REBOOT_UNKNOWN          = 4294967295,   //INT_MAX inadequate 
} REBOOT_REASON_T;

//int applet_entry_point(void);
void hex_dump(const uint8_t *bptr, uint32_t len);
int application_restart(REBOOT_REASON_T reason);
int print_gpio_pins_matching_default(char *buffer, int len, GPIO_DEFAULT_T gpio_default);
int get_int_with_tenths_from_string(char *value_string);
void unix_to_iso8601(time_t unix_timestamp, char *iso_string, size_t buffer_size);
uint32_t get_reboot_reason(void);

#endif 
