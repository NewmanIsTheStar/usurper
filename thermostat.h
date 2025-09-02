/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef THERMOSTAT_H
#define THERMOSTAT_H

typedef enum
{
    HVAC_OFF = 0,
    HVAC_HEATING_ONLY = 1,
    HVAC_COOLING_ONLY = 2,
    HVAC_FAN_ONLY = 3,
    HVAC_AUTO = 4,
} THERMOSTAT_MODE_T;

typedef enum
{
    HEATING_AND_COOLING_OFF = 0,
    HEATING_IN_PROGRESS = 1,
    COOLING_IN_PROGRESS = 2,
    DUCT_PURGE = 3,
    THERMOSTAT_LOCKOUT = 4,
    EXCESSIVE_OVERSHOOT = 5
} THERMOSTAT_STATE_T;


//prototypes
void thermostat_task(__unused void *params);
int make_schedule_grid(void);
//int update_current_setpoints(void);
int copy_schedule(int source_day, int destination_day);

#endif
