
/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef WATCHDOG_H
#define WATCHDOG_H

void watchdog_pulse(int *alive);
void watchdog_task(__unused void *params); 


#endif // WATCHDOG_H

