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

// worker tasks to launch and monitor
WORKER_TASK_T worker_tasks[] =
{
    //  function        name                    stack   priority        
    {   weather_task,   "Weather Task",         1024,   3},
    {   led_strip_task, "LED Strip Task",       1024,   4},  
    {   message_task,   "Message Task",         1024,   1},         

    // end of table
    {   NULL,           NULL,               0,      0,         }
};



