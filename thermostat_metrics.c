/**
 * Copyright (c) 2025 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <stdlib.h>

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/util/datetime.h"
//#include "hardware/rtc.h"
#include "hardware/watchdog.h"
#include <hardware/flash.h>
#include "hardware/i2c.h"

#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/apps/lwiperf.h"
#include "lwip/opt.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "timers.h"
#include "queue.h"

#include "stdarg.h"

#include "watchdog.h"
#include "weather.h"
#include "thermostat.h"
#include "flash.h"
#include "calendar.h"
#include "utility.h"
#include "config.h"
#include "led_strip.h"
#include "message.h"
#include "altcp_tls_mbedtls_structs.h"
#include "powerwall.h"
#include "pluto.h"
#include "tm1637.h"

// chews up a lot of RAM
#define SIZE_CLIMATE_HISTORY (100)

typedef enum
{
    HEATING_DATASET = 0,
    COOLING_DATASET = 1,
    STABLE_DATASET  = 2,    
    NUM_DATASETS    = 3
} CLIMATE_DATASET_T;

typedef enum
{
    NEGATIVE_DELTA = 0,
    POSITIVE_DELTA = 1,
    SMALL_DELATA   = 2,
    NUM_DELTAS     = 3    
} CLIMATE_DELTA_T;

typedef struct
{
    time_t unix_time;
    long int temperaturex10;
    long int humidityx10;
} CLIMATE_DATAPOINT_T;

typedef struct
{
    CLIMATE_DATAPOINT_T buffer[SIZE_CLIMATE_HISTORY];
    int buffer_index;
    int buffer_population;
} CLIMATE_HISTORY_T;

typedef enum
{
    TREND_DOWN    = 0,
    TREND_UP      = 1,
    TREND_STABLE  = 2,    
    NUM_TRENDS    = 3
} CLIMATE_TREND_DIRECTION_T;

typedef struct
{
    CLIMATE_DATAPOINT_T buffer[SIZE_CLIMATE_HISTORY];
    int buffer_index;
    int buffer_population;
    CLIMATE_DATAPOINT_T moving_average;
    int deltas[NUM_DELTAS];
    CLIMATE_TREND_DIRECTION_T current_trend;
    TickType_t trend_start_tick;
    TickType_t current_tick;
    TickType_t trend_length;
    int trend_up_max;
    int trend_down_min;
} CLIMATE_TREND_T;

typedef struct
{
    TickType_t hvac_off_tick[NUM_MOMENTUMS];
    long int hvac_off_temperature[NUM_MOMENTUMS];      
    bool measurement_in_progress[NUM_MOMENTUMS];
    TickType_t extrema_delay[NUM_MOMENTUMS]; 
    long int extrema_temperature[NUM_MOMENTUMS];
    TickType_t momentum_delay[NUM_MOMENTUMS];    
    long int  momentum_temperature_delta[NUM_MOMENTUMS];    
} CLIMATE_MOMENTUM_DATA_T;

// external variables
extern uint32_t unix_time;

// gobal variables
CLIMATE_HISTORY_T climate_history;
CLIMATE_MOMENTUM_DATA_T climate_momentum;
CLIMATE_TREND_T climate_trend;


/*!
 * \brief Initialize Temperature Metrics
 * 
 * \return 0 
 */
int initialize_climate_metrics(void)
{
    memset(&climate_history, 0, sizeof(climate_history));
    memset(&climate_momentum, 0, sizeof(climate_momentum));
    memset(&climate_trend, 0, sizeof(climate_trend));
    climate_trend.trend_up_max   = -500;   // -50.0  Celcius
    climate_trend.trend_down_min = -1500;  // +150.0 Celcius

    return(0);
}

/*!
 * \brief Accumulate Temperature Metrics
 * 
 * \return moving_average temperature
 */
int accumlate_temperature_metrics(long int temperaturex10)
{
    int i;
    int gradient = 0;

    climate_trend.buffer[climate_history.buffer_index].temperaturex10 = temperaturex10 - climate_history.buffer[(climate_history.buffer_index + NUM_ROWS(climate_history.buffer) - 1)%NUM_ROWS(climate_history.buffer)].temperaturex10;
    climate_history.buffer[climate_history.buffer_index].temperaturex10 = temperaturex10;
    climate_history.buffer[climate_history.buffer_index].unix_time = unix_time;

    climate_history.buffer_index  = (climate_history.buffer_index  + 1)%NUM_ROWS(climate_history.buffer);

    if (climate_history.buffer_population < NUM_ROWS(climate_history.buffer)) climate_history.buffer_population++;
    //printf("population = %d\n", climate_history.buffer_population);

    climate_trend.moving_average.temperaturex10 = 0;
    gradient = 0;
    for(i=0; i < climate_history.buffer_population; i++)
    {
        climate_trend.moving_average.temperaturex10 += climate_history.buffer[i].temperaturex10; 
        gradient += climate_trend.buffer[i].temperaturex10;
    }
    climate_trend.moving_average.temperaturex10 = climate_trend.moving_average.temperaturex10/climate_history.buffer_population;
    gradient = gradient*100/climate_history.buffer_population;

    // zero delta counters
    climate_trend.deltas[NEGATIVE_DELTA] = 0;
    climate_trend.deltas[POSITIVE_DELTA] = 0;
    climate_trend.deltas[SMALL_DELATA] = 0;

    // count deltas
    for(i=0; i < climate_history.buffer_population; i++)
    {
        if (climate_trend.buffer[i].temperaturex10 < 0) climate_trend.deltas[NEGATIVE_DELTA]++;
        if (climate_trend.buffer[i].temperaturex10 > 0) climate_trend.deltas[POSITIVE_DELTA]++;
        if (abs(climate_trend.buffer[i].temperaturex10) < 5) climate_trend.deltas[SMALL_DELATA]++;
    }

    //printf("Temp Sample = %d\tMoving Average = %d [%d, %d, %d] ", temperaturex10, climate_trend.moving_average, climate_trend.deltas[NEGATIVE_DELTA], climate_trend.deltas[POSITIVE_DELTA], climate_trend.deltas[SMALL_DELATA]);

    if ((climate_trend.deltas[NEGATIVE_DELTA]>= 1) && (climate_trend.deltas[POSITIVE_DELTA] == 0))
    {
        //printf("Trending down @ %d per sample\n", gradient);
        if (climate_trend.current_trend != TREND_DOWN)
        {
            climate_trend.current_tick = xTaskGetTickCount();
            climate_trend.trend_length = climate_trend.current_tick - climate_trend.trend_start_tick;
            //printf("Trend changed at tick %lu.  Trend lasted %lu. Maximum = %d\n", climate_trend.current_tick, climate_trend.trend_length, climate_trend.trend_up_max);

            climate_trend.current_trend = TREND_DOWN;
            climate_trend.trend_down_min = 1500;
            climate_trend.trend_start_tick = climate_trend.current_tick;
        }
        else
        {
            if (temperaturex10 < climate_trend.trend_down_min)
            {
                climate_trend.trend_down_min = temperaturex10;
            }
        }
    } 
    else if ((climate_trend.deltas[POSITIVE_DELTA] >= 1) && (climate_trend.deltas[NEGATIVE_DELTA] == 0))
    {
        //printf("Trending up @ %d per sample\n", gradient);
        if (climate_trend.current_trend != TREND_UP)
        {
            climate_trend.current_tick = xTaskGetTickCount();
            climate_trend.trend_length = climate_trend.current_tick - climate_trend.trend_start_tick;
            //printf("Trend changed at tick %lu.  Trend lasted %lu. Minimum = %d\n", climate_trend.current_tick, climate_trend.trend_length, climate_trend.trend_down_min);

            climate_trend.current_trend  = TREND_UP;
            climate_trend.trend_up_max = -500;
            climate_trend.trend_start_tick = climate_trend.current_tick;
        }   
        else
        {
            if (temperaturex10 > climate_trend.trend_up_max)
            {
                climate_trend.trend_up_max = temperaturex10;
            }
        }          
    }
    else if ((climate_trend.deltas[SMALL_DELATA] == 10) && ((climate_trend.deltas[NEGATIVE_DELTA]-climate_trend.deltas[POSITIVE_DELTA]) < 3))
    {
        //printf("Stable\n");   // no change to trend as we allow the previous trend to continue after a plateau    
    }
    else
    {
        //printf("No trend detected\n");
    } 

    return(climate_trend.moving_average.temperaturex10);
}


/*!
 * \brief Record when hvac heating or cooling ended
 * 
 * \return nothing
 */
void mark_hvac_off(CLIMATE_MOMENTUM_T momentum_type, long int temperaturex10)
{
    climate_momentum.hvac_off_tick[momentum_type] = xTaskGetTickCount();
    climate_momentum.hvac_off_temperature[momentum_type] = temperaturex10;    
    climate_momentum.extrema_delay[momentum_type]= 0;
    climate_momentum.extrema_temperature[momentum_type]= 0;    
    climate_momentum.measurement_in_progress[momentum_type] = true;
}

/*!
 * \brief Track how long heating or cooling continued after hvac stopped
 * 
 * \return nothing
 */
void track_hvac_extrema(CLIMATE_MOMENTUM_T momentum_type, long int temperaturex10)
{
    if (climate_momentum.measurement_in_progress[momentum_type])
    {
        switch(momentum_type)
        {
        case HEATING_MOMENTUM:
            if (temperaturex10 > climate_momentum.extrema_temperature[momentum_type])
            {
                climate_momentum.extrema_delay[momentum_type] = xTaskGetTickCount() - climate_momentum.hvac_off_tick[momentum_type];
                climate_momentum.extrema_temperature[momentum_type] =  temperaturex10;
            }
            break;
        case COOLING_MOMENTUM:
            if (temperaturex10 < climate_momentum.extrema_temperature[momentum_type])
            {
                climate_momentum.extrema_delay[momentum_type] = xTaskGetTickCount() - climate_momentum.hvac_off_tick[momentum_type];
                climate_momentum.extrema_temperature[momentum_type] =  temperaturex10;
            }    
            break;
        default:
            break;
        }
    }
}


/*!
 * \brief Set momentum based on tracked extrema
 * 
 * \return nothing
 */
void set_hvac_momentum(CLIMATE_MOMENTUM_T momentum_type)
{
    if (climate_momentum.measurement_in_progress[momentum_type])
    {
        climate_momentum.momentum_delay[momentum_type] = xTaskGetTickCount() - climate_momentum.hvac_off_tick[momentum_type];
        climate_momentum.momentum_temperature_delta[momentum_type] =  climate_momentum.extrema_temperature[momentum_type] - climate_momentum.hvac_off_temperature[momentum_type]; 
        climate_momentum.measurement_in_progress[momentum_type] = false;

        // add sanity check / constraints

        //printf("MOMENTUM LAST CYCLE:  Extrema occured %lu ms after HVAC shut off and temperature change was %ld\n", climate_momentum.momentum_delay[momentum_type], climate_momentum.momentum_temperature_delta[momentum_type]);
    }
}

/*!
 * \brief Send a climate syslog message
 *
 * \param[in]   log_name      name of log file on server
 * \param[in]   format, ...   variable parameters printf style  
 * 
 * \return num bytes sent or -1 on error
 */
void log_climate_change(int temperaturex10, int humidityx10)
{
    static int sent_temperaturex10 = 0;
    static int sent_humidityx10 = 0;

    // check if values changed
    if ((temperaturex10 != sent_temperaturex10) || (humidityx10 != sent_humidityx10))
    {
        send_syslog_message("temperature", "Temperature = %ld.%ld Humidity = %ld.%ld\n", temperaturex10/10, temperaturex10%10, humidityx10/10, humidityx10%10);

        // remember what we sent
        sent_temperaturex10 = temperaturex10;
        sent_humidityx10 = humidityx10;
    }
    
    return;
}

/*!
 * \brief print temperature history for insertion into web page  
 *
 * \param[in]   log_name      name of log file on server
 * \param[in]   format, ...   variable parameters printf style  
 * 
 * \return num bytes sent or -1 on error
 */
int print_temperature_history(char *buffer, int length, int start_position, int num_data_points)
{
    int i;
    int history_index = 0;
    int printed_characters = 0;
    int total_printed_characters = 0;
    char iso_timestamp[32];
    char *buff = 0;

    buff = buffer;
    *buffer = 0;

    if (start_position < climate_history.buffer_population)
    {
        // output xy data in format expected by javascript
        // { x: '2025-10-01T08:00:00', y: 65 },
        //for(i=(start_position+1; (i<climate_history.buffer_population) && (i<(start_position+num_data_points)); i++)

        // limit data points to number in the circular buffer
        if (start_position > climate_history.buffer_population)
        {
            num_data_points = 0;
        }
        else if ((start_position + num_data_points) > climate_history.buffer_population)
        {
            num_data_points = climate_history.buffer_population - start_position;
        }

        if (num_data_points > 0)
        {
            for(i=0; i < num_data_points; i++)
            {
                if (climate_history.buffer_population == NUM_ROWS(climate_history.buffer))
                {
                    // circular buffer is full, so oldest entry is the current buffer index (that we will overwrite next with new data)
                    history_index = (climate_history.buffer_index + start_position + i)%climate_history.buffer_population;
                }
                else
                {
                    // circular buffer not full, so oldest entry is at buffer index 0
                    history_index = start_position + i;   
                    
                    //printf("Circular buffer not full because %d != %d\n", climate_history.buffer_population, NUM_ROWS(climate_history.buffer));
                }

                // generatre timestamp string
                unix_to_iso8601(climate_history.buffer[history_index].unix_time, iso_timestamp, sizeof(iso_timestamp)); 

                // print xy data
                printed_characters = snprintf(buffer, length, "{x: '%s', y: %d.%d },\n", iso_timestamp, climate_history.buffer[history_index].temperaturex10/10, climate_history.buffer[history_index].temperaturex10%10);
                //printf("i=%d history_index=%d time=%s\n", i, history_index, iso_timestamp);
                if (printed_characters < length)
                {
                    total_printed_characters += printed_characters;
                    length -= printed_characters;
                    buffer += printed_characters;
                }
                else
                {
                    // hit the end of buffer truncate at START of last print attempt (erasing the last line printed)
                    *buffer = 0;
                    printf("BUFFER TERMINATED due to excess length\n%s\n", buff);
                    break;
                }
            }
        }
    }
    // printf("At exit strlen = %d and total_printed = %d\n", strlen(buff), total_printed_characters);
    // printf("FINAL BUFFER =\n%s\n", buff);

    //total_printed_characters = snprintf(buffer, length, "MONKEY%d\n", start_position);

    //printf("Minimum heap = %d\n", xPortGetMinimumEverFreeHeapSize());

    return(total_printed_characters);
}
        
