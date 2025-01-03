/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <stdlib.h>

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/util/datetime.h"
#include "hardware/rtc.h"
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
#include "pluto.h"

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
    long int temperaturex10;
    long int humidityx10;
} CLIMATE_DATAPOINT_T;

typedef struct
{
    CLIMATE_DATAPOINT_T buffer[10];
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
    CLIMATE_DATAPOINT_T buffer[10];
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

typedef enum
{
    COOLING_MOMENTUM = 0,
    HEATING_MOMENTUM = 1,   
    NUM_MOMENTUMS   = 2
} CLIMATE_MOMENTUM_T;

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




// prototypes
int initialize_climate_metrics(void);
THERMOSTAT_STATE_T control_thermostat_relays(long int temperaturex10);
int accumlate_temperature_metrics(long int temperaturex10);
void mark_hvac_off(CLIMATE_MOMENTUM_T momentum_type, long int temperaturex10);
void track_hvac_extrema(CLIMATE_MOMENTUM_T momentum_type, long int temperaturex10);
void set_hvac_momentum(CLIMATE_MOMENTUM_T momentum_type);



// external variables
extern NON_VOL_VARIABLES_T config;
extern WEB_VARIABLES_T web;

// global variables
const uint8_t aht10_addr = 0x38;                         // i2c address of aht10 chip
const uint8_t aht10_busy_mask = 0x80;                    // aht10 busy bit in first rx byte
const uint8_t aht10_calibrated_mask = 0x08;              // aht10 calibrated bit in first rx byte
const uint ath10_i2c_timeout_us = 50000;                 // i2c timeout when reading or writing
const uint8_t aht10_initialize[]  = {0xe1, 0x08, 0x00};  // initialize, use_factory_calibration, nop
const uint8_t aht10_measurement[] = {0xac, 0x33, 0x00};  // start, measurement, nop
const uint8_t aht10_soft_reset[]  = {0xba};              // soft_reset

CLIMATE_HISTORY_T climate_history;
CLIMATE_MOMENTUM_DATA_T climate_momentum;
CLIMATE_TREND_T climate_trend;



/*!
 * \brief Monitor weather and control relay based on conditions and schedule
 *
 * \param params unused garbage
 * 
 * \return nothing
 */
void thermostat_task(void *params)
{
    bool i2c_error = false;
    int i2c_bytes_written = 0;
    int i2c_bytes_read = 0;
    bool aht10_initialized = false;
    uint32_t temperature_native; 
    long int temperaturex10;
    uint32_t humidity_native; 
    long int humidityx10;
    uint8_t aht10_temp_humidity[7];
    int retry = 0;


    //TEST TEST TEST
    web.thermostat_set_point = 260;
    web.thermostat_hysteresis = 10; 
    config.thermostat_enable = 1;
    config.heating_gpio = 7;
    config.cooling_gpio = 8;
    config.fan_gpio = 9;

    printf("thermostat_task started\n");
    
    // initialize data structures for climate metrics
    initialize_climate_metrics();
    
    // initialize i2c
    i2c_init(i2c1, 100000);
    gpio_set_function(14, GPIO_FUNC_I2C);
    gpio_set_function(15, GPIO_FUNC_I2C);
    gpio_pull_up(14);
    gpio_pull_up(15);

    while (true)
    {
        if ((config.personality == HVAC_THERMOSTAT))
        {
            i2c_error = false;

            if (!aht10_initialized)
            {
                SLEEP_MS(500);

                // initialize temperature sensor
                printf("Initializing temperature sensor...\n"); 

                // soft reset
                i2c_write_timeout_us(i2c1, aht10_addr, aht10_soft_reset, sizeof(aht10_soft_reset), false, ath10_i2c_timeout_us);
                SLEEP_MS(20);
                
                if (!i2c_error)
                {
                    // initialize
                    i2c_bytes_written = i2c_write_timeout_us(i2c1, aht10_addr, aht10_initialize, sizeof(aht10_initialize), false, ath10_i2c_timeout_us);
                    if (i2c_bytes_written < 1) // only the first byte is acknowledged by some aht10 devices
                    {    
                        i2c_error = true;                        
                    }
                } 

                if (!i2c_error)
                {                                
                    printf("Completed initialization of temperature sensor\n");
                    aht10_initialized = true;
                    SLEEP_MS(500);
                }
            }

            if (!i2c_error)
            {  
                // start measurement
                i2c_bytes_written = i2c_write_timeout_us(i2c1, aht10_addr, aht10_measurement, sizeof(aht10_measurement), true, ath10_i2c_timeout_us);
                if (i2c_bytes_written != sizeof(aht10_measurement))
                {    
                    i2c_error = true;                        
                }                
            }

            if (!i2c_error)
            {  
                // wait for measurement to settle
                retry = 0;
                do
                {                   
                    SLEEP_MS(100);
                    memset(aht10_temp_humidity, 0, sizeof(aht10_temp_humidity));
                    i2c_bytes_read = i2c_read_timeout_us(i2c1, aht10_addr, aht10_temp_humidity, sizeof(aht10_temp_humidity), false, ath10_i2c_timeout_us);
                    if (i2c_bytes_read != sizeof(aht10_temp_humidity))
                    {    
                        i2c_error = true;                        
                    }                    
                    //hex_dump(aht10_temp_humidity, sizeof(aht10_temp_humidity));                

                } while ((aht10_temp_humidity[0] & aht10_busy_mask) && !i2c_error && retry++ < 10);  // loop while busy bit set and no error         
            }                

            if (!i2c_error && i2c_bytes_read == sizeof(aht10_temp_humidity))
            {                                                                
                // if measurement completed (not busy and calibrated data received)
                if  (!(aht10_temp_humidity[0] & aht10_busy_mask) && (aht10_temp_humidity[0] & aht10_calibrated_mask))
                {
                    // extract native 20-bit temperature (2^20 steps from -50C to +150C)
                    temperature_native = ((uint32_t) (aht10_temp_humidity[3] & 0x0F) << 16) | ((uint16_t) aht10_temp_humidity[4] << 8) | aht10_temp_humidity[5];            
                    
                    // convert native temperature to Celsius x 10 (range -50C to +150C)
                    temperaturex10 = ((long int)temperature_native*2000)/1048576 - 500;

                    // extract 20-bit raw humidity data (2^20 steps from 0% to 100%)
                    humidity_native = (((uint32_t) aht10_temp_humidity[1] << 16) | ((uint16_t) aht10_temp_humidity[2] << 8) | (aht10_temp_humidity[3])) >> 4; 

                    // convert native humidity to percentage x 10
                    humidityx10 = ((long int)humidity_native*1000)/1048576;

                    //printf("Temperature = %ld.%ld Humidity = %ld.%ld\n", temperaturex10/10, temperaturex10%10, humidityx10/10, humidityx10%10);
                    send_syslog_message("temperature", "Temperature = %ld.%ld Humidity = %ld.%ld\n", temperaturex10/10, temperaturex10%10, humidityx10/10, humidityx10%10);                    

                    track_hvac_extrema(COOLING_MOMENTUM, temperaturex10);
                    track_hvac_extrema(HEATING_MOMENTUM, temperaturex10);                     
                    control_thermostat_relays(temperaturex10);
                    accumlate_temperature_metrics(temperaturex10);
                }
                else
                {
                    printf("ath10: discarded measurement because it was either incomplete or uncalibrated\n");
                    i2c_error = true;
                }           
            }

            if (i2c_error)
            {
                printf("aht10: i2c error occured will attempt soft reset\n");
                aht10_initialized = false;                
            }

            SLEEP_MS(1000);
        }  
        else
        {
            SLEEP_MS(60000); 
        }   

        // tell watchdog task that we are still alive
        watchdog_pulse((int *)params);               
    }
}

/*!
 * \brief Open or close relay based on schedule and climate conditions
 * 
 * \return 0 irrigation off, 1 irrigation on, 1 irrigation usurped
 */
THERMOSTAT_STATE_T control_thermostat_relays(long int temperaturex10)
{
    static THERMOSTAT_STATE_T thermostat_state = HEATING_AND_COOLING_OFF;
    static int active_zone = 0;       
    int weekday;
    int min_now;
    int mow_now;
    int schedule_start_mow = 0;
    int schedule_end_mow = 0;
    int mins_till_irrigation = 0;
    int zone = -1;
    int i = 0;

    // update irrigation state
    switch(thermostat_state)
    {
    default:    
    case HEATING_AND_COOLING_OFF:
        if (temperaturex10 > (web.thermostat_set_point + web.thermostat_hysteresis))
        {
            send_syslog_message("thermostat", "Cooling commenced");            
            snprintf(web.status_message, sizeof(web.status_message), "Cooling in progress");            

            thermostat_state = COOLING_IN_PROGRESS;

            set_hvac_momentum(COOLING_MOMENTUM);

        } else if (temperaturex10 < (web.thermostat_set_point - web.thermostat_hysteresis))
        {
            send_syslog_message("thermostat", "Heatinging commenced");            
            snprintf(web.status_message, sizeof(web.status_message), "Heating in progress");            

            thermostat_state = HEATING_IN_PROGRESS;
            
            set_hvac_momentum(COOLING_MOMENTUM);           
        }
        break;
    case HEATING_IN_PROGRESS:
        if (temperaturex10 > (web.thermostat_set_point + web.thermostat_hysteresis))
        {
            send_syslog_message("thermostat", "Heating completed");            
            snprintf(web.status_message, sizeof(web.status_message), "Heating completed");  
            mark_hvac_off(HEATING_MOMENTUM, temperaturex10);         

            thermostat_state = HEATING_AND_COOLING_OFF;
        } 
        break;
    case COOLING_IN_PROGRESS:
        if (temperaturex10 < (web.thermostat_set_point - web.thermostat_hysteresis))
        {
            send_syslog_message("thermostat", "Cooling completed");            
            snprintf(web.status_message, sizeof(web.status_message), "Cooling completed"); 
            mark_hvac_off(COOLING_MOMENTUM, temperaturex10);

            thermostat_state = HEATING_AND_COOLING_OFF;
        } 
        break;
    case DUCT_PURGE:
        if (0 /*time expired*/)
        {
            send_syslog_message("thermostat", "Duct purge complete");            
            snprintf(web.status_message, sizeof(web.status_message), "Duct purge completed"); 

            thermostat_state = HEATING_AND_COOLING_OFF;
        } 
        break;            
    }

    // set the gpio output connected to the relay
    if (config.thermostat_enable && gpio_valid(config.heating_gpio) && gpio_valid(config.cooling_gpio) && gpio_valid(config.fan_gpio))
    {
        switch (thermostat_state)
        {
        default:
        case HEATING_AND_COOLING_OFF:
            printf("Heating, Cooling and Fan off\n");
            // gpio_put(config.heating_gpio, 0);
            // gpio_put(config.cooling_gpio, 0);
            // gpio_put(config.fan_gpio, 0);   
            break;
        case HEATING_IN_PROGRESS:
            printf("Heating on, Cooling and Fan off\n");
            // gpio_put(config.heating_gpio, 1);
            // gpio_put(config.cooling_gpio, 0);
            // gpio_put(config.fan_gpio, 1);   
            break;
        case COOLING_IN_PROGRESS:
            printf("Cooling on, Heating and Fan off\n");
            // gpio_put(config.heating_gpio, 0);
            // gpio_put(config.cooling_gpio, 1);
            // gpio_put(config.fan_gpio, 1);   
            break;            
        case DUCT_PURGE:
            printf("Heating and Cooling off and Fan on\n");
            // gpio_put(config.heating_gpio, 0);
            // gpio_put(config.cooling_gpio, 0);
            // gpio_put(config.fan_gpio, 1);   
            break; 
        }
    }                                


    return(thermostat_state);
}


/*!
 * \brief Initialize Temperature Metrics
 * 
 * \return 0 irrigation off, 1 irrigation on, 1 irrigation usurped
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
 * \return 0 irrigation off, 1 irrigation on, 1 irrigation usurped
 */
int accumlate_temperature_metrics(long int temperaturex10)
{
    int i;
    int gradient = 0;

    climate_trend.buffer[climate_history.buffer_index].temperaturex10 = temperaturex10 - climate_history.buffer[(climate_history.buffer_index + NUM_ROWS(climate_history.buffer) - 1)%NUM_ROWS(climate_history.buffer)].temperaturex10;
    climate_history.buffer[climate_history.buffer_index].temperaturex10 = temperaturex10;

    climate_history.buffer_index  = (climate_history.buffer_index  + 1)%NUM_ROWS(climate_history.buffer);

    if (climate_history.buffer_population < NUM_ROWS(climate_history.buffer)) climate_history.buffer_population++;
    printf("population = %d\n", climate_history.buffer_population);

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

    printf("Temp Sample = %d\tMoving Average = %d [%d, %d, %d] ", temperaturex10, climate_trend.moving_average, climate_trend.deltas[NEGATIVE_DELTA], climate_trend.deltas[POSITIVE_DELTA], climate_trend.deltas[SMALL_DELATA]);

    if ((climate_trend.deltas[NEGATIVE_DELTA]>= 1) && (climate_trend.deltas[POSITIVE_DELTA] == 0))
    {
        printf("Trending down @ %d per sample\n", gradient);
        if (climate_trend.current_trend != TREND_DOWN)
        {
            climate_trend.current_tick = xTaskGetTickCount();
            climate_trend.trend_length = climate_trend.current_tick - climate_trend.trend_start_tick;
            printf("Trend changed at tick %lu.  Trend lasted %lu. Maximum = %d\n", climate_trend.current_tick, climate_trend.trend_length, climate_trend.trend_up_max);

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
        printf("Trending up @ %d per sample\n", gradient);
        if (climate_trend.current_trend != TREND_UP)
        {
            climate_trend.current_tick = xTaskGetTickCount();
            climate_trend.trend_length = climate_trend.current_tick - climate_trend.trend_start_tick;
            printf("Trend changed at tick %lu.  Trend lasted %lu. Minimum = %d\n", climate_trend.current_tick, climate_trend.trend_length, climate_trend.trend_down_min);

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
        printf("Stable\n");   // no change to trend as we allow the previous trend to continue after a plateau    
    }
    else
    {
        printf("No trend detected\n");
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

        printf("MOMENTUM LAST CYCLE:  Extrema occured %lu ms after HVAC shut off and temperature change was %ld\n", climate_momentum.momentum_delay[momentum_type], climate_momentum.momentum_temperature_delta[momentum_type]);
    }
}
