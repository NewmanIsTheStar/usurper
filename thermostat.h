/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef THERMOSTAT_H
#define THERMOSTAT_H

#define SETPOINT_TEMP_UNDEFINED   (-10001)
#define SETPOINT_TEMP_INVALID_FAN (-10002)
#define SETPOINT_TEMP_INVALID_OFF (-10003)
#define SETPOINT_TEMP_DEFAULT_C   (210)
#define SETPOINT_TEMP_DEFAULT_F   (700)

typedef enum
{
    HVAC_AUTO = 0,
    HVAC_OFF = 1,
    HVAC_HEATING_ONLY = 2,
    HVAC_COOLING_ONLY = 3,
    HVAC_FAN_ONLY = 4,
} THERMOSTAT_MODE_T;          // set by user -- do not reorder -- html and ssi.c rely on order

typedef enum
{
    HEATING_AND_COOLING_OFF = 0,
    HEATING_IN_PROGRESS = 1,
    COOLING_IN_PROGRESS = 2,
    DUCT_PURGE = 3,
    THERMOSTAT_LOCKOUT = 4,
    EXCESSIVE_OVERSHOOT = 5
} THERMOSTAT_STATE_T;         // operational state


//prototypes
void thermostat_task(__unused void *params);
int make_schedule_grid(void);
//int update_current_setpoints(void);
int copy_schedule(int source_day, int destination_day);
void sanatize_schedule_temperatures(void);

#endif
