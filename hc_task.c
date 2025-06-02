
/**
 * Copyright (c) 2025 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <stdlib.h>


#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "generated/ws2812.pio.h"

// TODO - prune this list of includes
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/rand.h"
#include "pico/util/datetime.h"
//#include "hardware/rtc.h"
#include "hardware/watchdog.h"

#include "lwip/opt.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/sys.h"
#include <lwip/dns.h>


#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/apps/lwiperf.h"
#include "lwip/apps/sntp.h"
#include "lwip/apps/httpd.h"
#include "dhcpserver.h"
#include "dnsserver.h"

#include "time.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

#include "stdarg.h"

#include "weather.h"
#include "cgi.h"
#include "ssi.h"
#include "flash.h"
#include "utility.h"
#include "config.h"
#include "watchdog.h"
#include "pluto.h"
#include "led_strip.h"
#include "udp.h"
#include "message.h"
#include "message_defs.h"
#include "powerwall.h"
#include "shelly.h"
#include "hc_task.h"


//#define DEBUG_UDP_MESSAGES

#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)





//prototypes


// external variables
extern NON_VOL_VARIABLES_T config;
extern WEB_VARIABLES_T web;

//static variables


/*!
 * \brief home controller task
 *
 * \param[in]  params  alive counter that must be incremented periodically to prevent watchdog reset
 * 
 * \return nothing
 */
void hc_task(__unused void *params) 
{
    SOCKADDR_IN sClientAddress;  
    int received_bytes = 0;         
    
    printf("home controller task started\n");
    while (true)
    {        
        if ((config.personality == HOME_CONTROLLER))
        {
            //TEST TEST TEST
            printf("Begin shelly test\n");
            discover_shelly_devices();
            printf("End shelly test\n");
        }
        else
        {
            SLEEP_MS(1000);
        }

        // tell watchdog task that we are still alive
        watchdog_pulse((int *)params);  
    } 
}
