/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/util/datetime.h"
#include "hardware/rtc.h"
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

#include "weather.h"
#include "led_strip.h"
#include "cgi.h"
#include "ssi.h"
#include "flash.h"
#include "utility.h"
#include "config.h"
#include "watchdog.h"
#include "worker_tasks.h"

// external variables
extern WORKER_TASK_T worker_tasks[];


/*!
 * \brief Tell watchdog taskdog task that we are alive!
 *
 * \param alive poimter to alive inidcation to be altered
 * 
 * \return nothing
 */
void watchdog_pulse(int *alive) 
{
    (*alive)++;  //atomic write not necessary as all the watchdog task needs to see is a changing value
}


/*!
 * \brief Monitor for task crashes and pat the watchdog
 *
 * \param params unused garbage
 *
 * \return nothing
 */
void watchdog_task(void *params)
{
    int worker = 0;
    int reset_required = false;


    sleep_ms(100000);
    
    // start the watchdog
    watchdog_enable(5000, 1);

    // pat the watchdog
    while(true) 
    {
        // scan worker tasks to check if alive
        for(worker=0; worker_tasks[worker].functionptr != NULL; worker++)
        {
            if (worker_tasks[worker].watchdog_alive_indicator)      
            {
                worker_tasks[worker].watchdog_alive_indicator = 0;  
                worker_tasks[worker].watchdog_seconds_since_alive = 0;
            }
            else
            {
                worker_tasks[worker].watchdog_seconds_since_alive++;
                if (worker_tasks[worker].watchdog_seconds_since_alive > 60)
                {

                    printf("%s seconds_since_alive == %d\n", worker_tasks[worker].name, worker_tasks[worker].watchdog_seconds_since_alive);
                }

                if (worker_tasks[worker].watchdog_seconds_since_alive > 180) 
                {
                    reset_required = true;
                }
            }
        }

#ifdef DEBUG_DEAD_TASK
        // keep printing seconds since alive message if worker task dies
        watchdog_update();
#else
        // update watchdog if task still alive
        if (!reset_required)
        {
            watchdog_update();
        }
#endif
        sleep_ms(1000);
    }
}
