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

// external variables
extern NON_VOL_VARIABLES_T config;
extern WEB_VARIABLES_T web;

// global variables
int hvac_state_change_log_index = 0;
HVAC_STATE_CHANGE_LOG_T hvac_state_change_log[32];


/*!
 * \brief Create thermostat schedule grid
 * 
 * \return nothing
 */
int make_schedule_grid(void)
{
    int i, j, x, y;
    int key_mow = 0;
    int key_temp = 0;
    //int mow;
    int mod;
    int setpointtemperaturex10 = 0;
    bool found = false;
    int populated_rows = 0;
    int mow[NUM_ROWS(config.setpoint_start_mow)];
    int temp[NUM_ROWS(config.setpoint_start_mow)];

    // erase start time colum
    for(y=0;y<8; y++)
    {
        web.thermostat_grid[0][y]= -1;
    }

    // set all temperatures undefined
    for(x=1; x<8; x++)
    {
        for (y=0; y<8; y++)
        {
            web.thermostat_grid[x][y] = SETPOINT_TEMP_UNDEFINED; 
        }
    }

    // copy schedule to local arrays for sorting
    for(i=0; i<NUM_ROWS(config.setpoint_start_mow); i++)
    {
        mow[i] = config.setpoint_start_mow[i];
        temp[i] = config.setpoint_temperaturex10[i];
    }

    // sort the schedule into ascending order by time of day (tod)
    for(i=1; i<NUM_ROWS(mow); i++)
    {
        key_mow = mow[i];
        key_temp = temp[i];        
        j = i - 1;

        while ((j >= 0) && ((mow[j]%(60*24)) > (key_mow%(60*24))))
        {
            mow[j+1] = mow[j];
            temp[j+1] = temp[j];            
            j = j - 1;
        }

        mow[j+1] = key_mow;
        temp[j+1] = key_temp;            
    }

    // scan list of configured setpoints
    for (i=0; i<NUM_ROWS(mow); i++)
    {
        if ((mow[i] >= 0) && (mow[i] < 60*24*7))
        {
            x = mow[i]/(60*24) + 1;  // day + 1
            mod = mow[i]%(60*24);        
            
            found = false;

            for(y=0; y < populated_rows; y++)
            {
                if (web.thermostat_grid[0][y] == mod)
                {
                    //insert into existing grid row
                    CLIP(x, 1, 7);
                    web.thermostat_grid[x][y] = temp[i];
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                if (populated_rows < 7)
                {
                    web.thermostat_grid[0][populated_rows] = mod;
                    web.thermostat_grid[x][populated_rows] = temp[i];

                    populated_rows++;
                }
            }
        }
    }

    // // dump grid
    // for(y=0; y<8; y++)
    // {
    //     printf("\nGRID: ");
    //     for (x=0; x<8; x++)
    //     {
    //          printf("[%d]", web.thermostat_grid[x][y]);
    //     }
    // } 
    // printf("\n");

    return(0);
}

/*!
 * \brief Copy daily schedule to other day(s)
 * 
 * \return nothing
 */
int copy_schedule(int source_day, int destination_day)
{
    int i, j, k, x, y;
    int key_mow = 0;
    int key_temp = 0;
    //int mow;
    int mod;
    int setpointtemperaturex10 = 0;
    bool found = false;
    int populated_rows = 0;
    int mow[NUM_ROWS(config.setpoint_start_mow)];
    int temp[NUM_ROWS(config.setpoint_start_mow)];
    int day = 0;

    CLIP(source_day, 0, 6);

    // erase existing entries on destination days
    for(i=0; i<NUM_ROWS(config.setpoint_start_mow); i++)
    {
        if ((config.setpoint_start_mow[i] >= 0) && (config.setpoint_start_mow[i] < 60*24*7))
        {
            day = config.setpoint_start_mow[i]/(60*24);
            CLIP(day, 0, 6);
        }

        // 0-6 = sunday to saturday, 7 = everyday, 8 = weekdays, 9 = weekend days
        if ((day != source_day) && (!day_compare(day, destination_day)))                                                                      
        {
            // mark unused    
            config.setpoint_start_mow[i] = -1;
            config.setpoint_temperaturex10[i] = SETPOINT_TEMP_UNDEFINED;
        }
    }


    for (day = 0; day < 7; day++)   // day to copy into
    {
        for (i=0; i < NUM_ROWS(config.setpoint_start_mow); i++)  // scan existing schedule
        {
            if (schedule_row_valid(i))
            {
                j = config.setpoint_start_mow[i]/(60*24);
                mod = config.setpoint_start_mow[i]%(60*24);
                CLIP(j, 0, 6);
                CLIP(mod, 0, 60*24);

                // 0-6 = sunday to saturday, 7 = everyday, 8 = weekdays, 9 = weekend days
                if ((day != source_day) && (j == source_day) && (!day_compare(day, destination_day)))
                {
                    k = get_free_schedule_row();

                    if ((k >= 0) && (k < NUM_ROWS(config.setpoint_start_mow)))
                    {
                        // copy schedule
                        config.setpoint_start_mow[k] = day*(60*24) + mod;
                        config.setpoint_temperaturex10[k] = config.setpoint_temperaturex10[i];
                        config.setpoint_mode[k] = config.setpoint_mode[i];

                        //printf("Copied row. New row %d [dest day = %d source day j = %d source row i = %d]\n", k, day, j, i);
                    }
                }
             }
        }
    }

    return(0);
}

/*!
 * \brief check if day matchs
 * 
 * \return return false if match, true if diffeent
 */
bool day_compare(int day1, int day2)
{
    bool diff = true;
    int multi_day = 0;
    int compare_day = 0;

    if (day1 == day2)
    {
        diff = false;
    }

    if (day1 > 6)
    {
        multi_day = day1;
        compare_day = day2;
    }
    else
    {
        multi_day = day2;
        compare_day = day1;    
    }

    switch(multi_day)
    {
        case 7: // everyday
            diff = false;
            break;
        case 8: // weekday
            if ((compare_day >=1) && (compare_day <= 5))
            {
                diff = false;
            } 
            break;
        case 9: // weekend
            if ((compare_day == 0) || (compare_day == 6))
            {
                diff = false;
            }
            break;
    }
    
    return(diff);
}

/*!
 * \brief check if temperature schedule row is valid
 * 
 * \return true if temperature schedule entry is valid
 */
bool schedule_row_valid(int row)  // TODO: handle INVALID temperature constants e.g. copying OFF mode
{
    bool valid = true;


    if ((row < 0) || (row > NUM_ROWS(config.setpoint_start_mow)))
    {
        valid = false;
    }
    else if ((config.setpoint_start_mow[row] < 0) ||  (config.setpoint_start_mow[row] > (60*24*7))) 
    {
        valid = false;
    }
    else if ((config.setpoint_temperaturex10[row] < (-1000)) ||  (config.setpoint_temperaturex10[row] > (1000))) 
    {
        valid = false;
    }    

    //printf("ROW %d mow = %d temp = %d %s\n", row, config.setpoint_start_mow[row], config.setpoint_temperaturex10[row], valid?"TRUE":"FALSE");

    return(valid);
}

/*!
 * \brief Copy daily schedule to other day(s)
 * 
 * \return nothing
 */
int get_free_schedule_row(void)
{
    int i = 0;
    bool found = false;

    for(i=0; i<NUM_ROWS(config.setpoint_start_mow); i++)
    {
        if (!schedule_row_valid(i))
        {
            found = true;
            break;
        }
    }

    if (!found) i = -1;

    return(i);
}

 /*!
 * \brief Timer callback
 *
 * \param[in]   new_state      current thermostat state
 * 
 * \return nothing
 */
void hvac_log_state_change(THERMOSTAT_STATE_T new_state)
{
    static THERMOSTAT_STATE_T previous_state = DUCT_PURGE;

    if (new_state != previous_state)
    {
        hvac_state_change_log[hvac_state_change_log_index].change_tick = xTaskGetTickCount();
        hvac_state_change_log[hvac_state_change_log_index].new_state = new_state;
        hvac_state_change_log[hvac_state_change_log_index].change_temperature = web.thermostat_temperature;

        hvac_state_change_log_index = (hvac_state_change_log_index + 1) % NUM_ROWS(hvac_state_change_log);
    }
}
 


/*!
 * \brief ensure scheduled temperatures are consistent with units
 * 
 * \return nothing
 */
void sanatize_schedule_temperatures(void)
{
    int i;

    for(i=0; i<NUM_ROWS(config.setpoint_start_mow); i++)
    {
        if ((config.setpoint_start_mow[i] >= 0) && (config.setpoint_start_mow[i] < 60*24*7))
        {
            if (config.use_archaic_units)
            {
                // convert suspected celsius to fahrenheit 
                if ((config.setpoint_temperaturex10[i] < 500) && (config.setpoint_temperaturex10[i] > -5000))
                {
                    config.setpoint_temperaturex10[i] = (config.setpoint_temperaturex10[i]*9)/5 + 320;
                }
            }
            else
            {
                // convert suspected fahrenheit to celsius
                if (config.setpoint_temperaturex10[i] >= 50)
                {
                    config.setpoint_temperaturex10[i] = ((config.setpoint_temperaturex10[i] - 320)*5)/9;
                }
            }

        }
    }
}

