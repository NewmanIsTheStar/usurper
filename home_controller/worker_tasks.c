/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <stdlib.h>
#include "stdarg.h"

#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

#include "worker_tasks.h"

// include header for each worker task here
#include "weather.h"
#include "led_strip.h"
#include "message.h"
#include "thermostat.h"
#include "hc_task.h"
#include "discovery_task.h"

// worker tasks to launch and monitor
WORKER_TASK_T worker_tasks[] =
{
    //  function        name                    stack   priority        
#ifdef INCORPORATE_HOME_CONTROLLER    
    {   hc_task,        "Home Controller Task", 8096,   6},      
    {   discovery_task, "Discovery Task",       8096,   7},          
#endif

    // end of table
    {   NULL,           NULL,               0,      0,         }
};




