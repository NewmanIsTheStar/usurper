
#ifndef WORKER_TASKS_H
#define WORKER_TASKS_H

typedef struct WORKER_TASK_STRUCT
{
    const void(*functionptr)(void *param);
    const char *name;
    const configSTACK_DEPTH_TYPE stack_size;  //stack size in WORDS not bytes!
    const int priority;
    TaskHandle_t task_handle;
    int watchdog_alive_indicator;       // altered by task to indicate that it is alive by calling watchdog_pulse()
    int watchdog_seconds_since_alive;   // counts seconds since the watchdog task last recieved an indication that the task is alive
    UBaseType_t stack_high_water_mark;
} WORKER_TASK_T;

#endif
