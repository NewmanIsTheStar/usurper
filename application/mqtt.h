/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef MQTT_H
#define MQTT_H

#include "FreeRTOS.h"

#define MQTT_TASK_LOOP_DELAY    (10000)


typedef enum
{
    MQTT_CALLBACK_CONNECTION_ID = 0,
    MQTT_CALLBACK_DISCOVERY_ID = 1,
    MQTT_CALLBACK_STATE_ID = 2,

    NUM_MQTT_CALLBACK_IDS = 3
} MQTT_CALLBACK_ID_T;

// mqtt_task.c
void mqtt_task(__unused void *params);
void mqttrs_relay_refresh(void);
void mqttrs_relay_config_change(void);


#endif
