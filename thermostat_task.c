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
extern uint32_t unix_time;
extern NON_VOL_VARIABLES_T config;
extern WEB_VARIABLES_T web;


/*!
 * \brief Monitor weather and control relay based on conditions and schedule
 *
 * \param params unused garbage
 * 
 * \return nothing
 */
void thermostat_task(void *params)
{
    int ath10_error = 0;
    int i2c_bytes_written = 0;
    int i2c_bytes_read = 0;
    bool aht10_initialized = false;
    bool tm1637_initialized = false;
    long int temperaturex10 = 0;
    long int humidityx10 = 0;
    CLIMATE_DATAPOINT_T sample;
    int retry = 0;
    int oneshot = false;
    int i;
    bool button_pressed = false;

    if (strcasecmp(APP_NAME, "Thermostat") == 0)
    {
        // single purpose application -- force personality and enable
        config.personality = HVAC_THERMOSTAT;
        config.thermostat_enable = 1;
    }

    if (config.use_archaic_units)
    {
        temperaturex10 = SETPOINT_DEFAULT_FAHRENHEIT_X_10;
    }
    else
    {
        temperaturex10 = SETPOINT_DEFAULT_CELSIUS_X_10;
    }
    
    web.powerwall_grid_status = GRID_UNKNOWN;

    
    //config.thermostat_hysteresis = 5; 

    // make sure safeguards are valid to prevent short cycling
    CLIP(config.heating_to_cooling_lockout_mins, 1, 60);
    CLIP(config.minimum_heating_on_mins, 1, 60);
    CLIP(config.minimum_cooling_on_mins, 1, 60);
    CLIP(config.minimum_heating_off_mins, 1, 60);
    CLIP(config.minimum_cooling_off_mins, 1, 60);
    CLIP(config.thermostat_hysteresis, 5, 50);

 
    printf("thermostat_task started!\n");

    // initialize data structures for climate metrics
    initialize_climate_metrics();
  
    // initialize hvac gpio and timers
    initialize_hvac_control();
  
    // initialize powerwall communication
    powerwall_init();

    // create the schedule grid used in web inteface
    make_schedule_grid();
  
    // enable front panel buttons
    initialize_physical_buttons(config.thermostat_mode_button_gpio, config.thermostat_increase_button_gpio, config.thermostat_decrease_button_gpio);
    
    while (true)
    {        
        if ((config.personality == HVAC_THERMOSTAT))  // TODO should this be config.thermostat_enable ?
        {
            if (!tm1637_initialized)
            {
                tm1637_init(config.thermostat_seven_segment_display_clock_gpio, config.thermostat_seven_segment_display_data_gpio);
                tm1637_set_brightness(config.thermostat_display_brightness); 
                tm1637_display_word("BOOT", false);
                
                tm1637_initialized = true;               
            }
            
            ath10_error = 0;

            if (!aht10_initialized)
            {
                SLEEP_MS(500);

                // initialize temperature sensor
                ath10_error = aht10_initialize(config.thermostat_temperature_sensor_clock_gpio, config.thermostat_temperature_sensor_data_gpio);

                if (!ath10_error)
                {
                    aht10_initialized = true;                
                }                

                SLEEP_MS(500);
            }

            ath10_error = aht10_measurement(&temperaturex10, &humidityx10);

            if (ath10_error)
            {
                printf("aht10: i2c error occured will attempt soft reset\n");
                aht10_initialized = false;                
            }
            else
            {
                //send_syslog_message("temperature", "Temperature = %ld.%ld Humidity = %ld.%ld\n", temperaturex10/10, temperaturex10%10, humidityx10/10, humidityx10%10);                    
                log_climate_change(temperaturex10, humidityx10);

                track_hvac_extrema(COOLING_MOMENTUM, temperaturex10);
                track_hvac_extrema(HEATING_MOMENTUM, temperaturex10); 
                
                // create sample
                sample.unix_time = unix_time;
                sample.temperaturex10 = temperaturex10;
                sample.humidityx10 = humidityx10;

                accumlate_metrics(&sample);

                // update web ui
                web.thermostat_temperature = temperaturex10;
            }

            // check powerwall status
            powerwall_check();

            // set hvac relays
            control_thermostat_relays(temperaturex10);

            // process button presses until 1 second of inactivity occurs
            button_pressed = handle_button_press_with_timeout(1000);
        }  
        else
        {
            SLEEP_MS(60000); 
        }   

        // tell watchdog task that we are still alive
        watchdog_pulse((int *)params);               
    }
}











