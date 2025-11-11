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
const uint8_t aht10_addr = 0x38;                                // i2c address of aht10 chip
const uint8_t aht10_busy_mask = 0x80;                           // aht10 busy bit in first rx byte
const uint8_t aht10_calibrated_mask = 0x08;                     // aht10 calibrated bit in first rx byte
const uint ath10_i2c_timeout_us = 50000;                        // i2c timeout when reading or writing
const uint8_t aht10_i2c_initialize[]  = {0xe1, 0x08, 0x00};     // initialize, use_factory_calibration, nop
const uint8_t aht10_i2c_measurement[] = {0xac, 0x33, 0x00};     // start, measurement, nop
const uint8_t aht10_soft_reset[]  = {0xba};                     // soft_reset
bool ath10_gpio_ok = false;                                     // ok to use configured gpio
i2c_inst_t *ath10_i2c_block = NULL;                                   // i2c block to use

int aht10_initialize(int clock_gpio, int data_gpio)
{
    bool i2c_error = false;
    int i2c_bytes_written = 0;

    if (ath10_gpio_ok && ath10_i2c_block)
    {
        i2c_init(ath10_i2c_block, 100000);
        gpio_set_function(config.thermostat_temperature_sensor_data_gpio, GPIO_FUNC_I2C);
        gpio_set_function(config.thermostat_temperature_sensor_clock_gpio, GPIO_FUNC_I2C);
        gpio_pull_up(config.thermostat_temperature_sensor_data_gpio);
        gpio_pull_up(config.thermostat_temperature_sensor_clock_gpio);

        printf("Temperature Sensor using pins: %d, %d\n", config.thermostat_temperature_sensor_data_gpio, config.thermostat_temperature_sensor_clock_gpio);

        // initialize temperature sensor
        printf("Initializing temperature sensor...\n"); 

        // soft reset
        i2c_bytes_written = i2c_write_timeout_us(ath10_i2c_block, aht10_addr, aht10_soft_reset, sizeof(aht10_soft_reset), false, ath10_i2c_timeout_us);
        if (i2c_bytes_written < 1) // only the first byte is acknowledged by some aht10 devices
        {
            printf("aht10: reset command i2c error\n");    
            i2c_error = true;                        
        }    
        SLEEP_MS(20);
        
        if (!i2c_error)
        {
            // initialize
            i2c_bytes_written = i2c_write_timeout_us(ath10_i2c_block, aht10_addr, aht10_i2c_initialize, sizeof(aht10_i2c_initialize), false, ath10_i2c_timeout_us);
            if (i2c_bytes_written < 1) // only the first byte is acknowledged by some aht10 devices
            { 
                printf("ath10: initialize command i2c error\n");   
                i2c_error = true;                        
            }
        } 

        if (!i2c_error)
        {                                
            printf("Completed initialization of temperature sensor\n");
        }
    }
    else
    {
        // initialization not attempted
        i2c_error = true; 
    }

    return((int)i2c_error);
}

int aht10_measurement(long int *temperaturex10, long int *humidityx10)
{
    bool i2c_error = false;
    int i2c_bytes_written = 0;
    int i2c_bytes_read = 0;
    int retry = 0;
    uint32_t temperature_native; 
    uint32_t humidity_native; 
    uint8_t aht10_temp_humidity[7];

    if (ath10_gpio_ok && ath10_i2c_block)
    {
        if (!i2c_error)
        {  
            // start measurement
            i2c_bytes_written = i2c_write_timeout_us(ath10_i2c_block, aht10_addr, aht10_i2c_measurement, sizeof(aht10_i2c_measurement), true, ath10_i2c_timeout_us);
            if (i2c_bytes_written != sizeof(aht10_i2c_measurement))
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
                *temperaturex10 = ((long int)temperature_native*2000)/1048576 - 500;

                if (config.use_archaic_units)
                {
                    *temperaturex10 = (*temperaturex10*9)/5 + 320;
                }

                // extract 20-bit raw humidity data (2^20 steps from 0% to 100%)
                humidity_native = (((uint32_t) aht10_temp_humidity[1] << 16) | ((uint16_t) aht10_temp_humidity[2] << 8) | (aht10_temp_humidity[3])) >> 4; 

                // convert native humidity to percentage x 10
                *humidityx10 = ((long int)humidity_native*1000)/1048576;

                //printf("Temperature = %ld.%ld Humidity = %ld.%ld\n", temperaturex10/10, temperaturex10%10, humidityx10/10, humidityx10%10);

            }
            else
            {
                printf("ath10: discarded measurement because it was either incomplete or uncalibrated\n");
                i2c_error = true;
            }           
        }
    }
    else
    {
        // measurement not attempted
        i2c_error = true;  
    }

    return((int)i2c_error);
}

int ath10_gpio_enable(bool enable)
{
    ath10_gpio_ok = enable;

    ath10_i2c_block = gpio_get_i2c(config.thermostat_temperature_sensor_clock_gpio, config.thermostat_temperature_sensor_data_gpio);
}