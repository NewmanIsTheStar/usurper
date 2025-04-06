/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef THERMOSTAT_H
#define THERMOSTAT_H

typedef enum
{
    HEATING_AND_COOLING_OFF = 0,
    HEATING_IN_PROGRESS = 1,
    COOLING_IN_PROGRESS = 2,
    DUCT_PURGE = 3,
} THERMOSTAT_STATE_T;


//prototypes
void thermostat_task(__unused void *params);
int make_schedule_grid(void);
int update_current_setpoints(void);

#endif
