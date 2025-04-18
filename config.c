/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/flash.h"
#include <hardware/flash.h>

#include "lwip/sockets.h"


#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

#include "config.h"
#include "pluto.h"
#include "utility.h"

#include "flash.h"

#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
//#define DISABLE_CONFIG_VALIDATION (1)


int config_validate(void);
void config_v1_to_v2(void);
void config_v2_to_v3(void);
void config_v3_to_v4(void);
void config_v4_to_v5(void);
void config_v5_to_v6(void);


NON_VOL_VARIABLES_T config;
static int config_dirty_flag = 0;
static NON_VOL_CONVERSION_T config_info[] =
{
    {1,      offsetof(NON_VOL_VARIABLES_T_VERSION_1, version),   offsetof(NON_VOL_VARIABLES_T_VERSION_1, crc),   &config_blank_to_v1},
    {2,      offsetof(NON_VOL_VARIABLES_T_VERSION_2, version),   offsetof(NON_VOL_VARIABLES_T_VERSION_2, crc),   &config_v1_to_v2}, 
    {3,      offsetof(NON_VOL_VARIABLES_T_VERSION_3, version),   offsetof(NON_VOL_VARIABLES_T_VERSION_3, crc),   &config_v2_to_v3}, 
    {4,      offsetof(NON_VOL_VARIABLES_T_VERSION_4, version),   offsetof(NON_VOL_VARIABLES_T_VERSION_4, crc),   &config_v3_to_v4},  
    {5,      offsetof(NON_VOL_VARIABLES_T_VERSION_5, version),   offsetof(NON_VOL_VARIABLES_T_VERSION_5, crc),   &config_v4_to_v5}, 
    {6,      offsetof(NON_VOL_VARIABLES_T, version),             offsetof(NON_VOL_VARIABLES_T, crc),             &config_v5_to_v6},             
};


/*!
 * \brief Set default values for configuration v1
 * 
 * \return 0 on success, -1 on error
 */
void config_blank_to_v1(void)
{
    int i;

    printf("Initializing configuration version 1\n");

    // set initial values in ram buffer
    config.version = 1;
    config.personality = SPRINKLER_USURPER;

    config.irrigation_enable = 1;

    for(i=0; i<7; i++)
    {
        config.day_schedule_enable[i] = 1;
        config.day_start[i] = 0;
        config.day_duration[i] = 0;
        config.day_start_alternate[i] = 0;
        config.day_duration_alternate[i] = 0; 
    }

    for(i=0; i<32; i++)
    {
        config.schedule_opportunity_start[i] = i;
        config.schedule_opportunity_duration[i] = i;
    }

    config.timezone_offset = -6*60;
    config.daylightsaving_enable = 1;
    STRNCPY(config.wifi_country, "World Wide", sizeof(config.wifi_country));    
    STRNCPY(config.daylightsaving_start, "Second Sunday in March", sizeof(config.daylightsaving_start));
    STRNCPY(config.daylightsaving_end, "First Sunday in November", sizeof(config.daylightsaving_end));
    STRNCPY(config.time_server[0], "pool.ntp.org", sizeof(config.time_server[0]));
    STRNCPY(config.time_server[1], "time.google.com", sizeof(config.time_server[1]));
    STRNCPY(config.time_server[2], "time.facebook.com", sizeof(config.time_server[2]));
    STRNCPY(config.time_server[3], "time.windows.com", sizeof(config.time_server[3]));        
    STRNCPY(config.weather_station_ip, "weather-station.badnet", sizeof(config.weather_station_ip)); 
    STRNCPY(config.syslog_server_ip, "spud.badnet", sizeof(config.syslog_server_ip));         
    STRNCPY(config.govee_light_ip, "govee.badnet", sizeof(config.govee_light_ip));     

    config.syslog_enable = 0;
    
    config.weather_station_enable = 1;
    config.wind_threshold = 15;
    config.rain_week_threshold = 50;
    config.rain_day_threshold = 10;
    config.relay_normally_open = false;
    config.gpio_number = 3;

    // blank network setting with DHCP enabled
    config.wifi_ssid[0] = 0;
    config.wifi_password[0] = 0;
    config.dhcp_enable = 1;
    config.ip_address[0] = 0;
    config.network_mask[0] = 0;

    // led string settings
    config.led_number = 0;
    config.led_pattern = 0;
    config.led_pattern_when_irrigation_active = 0;
    config.led_pattern_when_irrigation_terminated = 0;    
    config.led_speed = 100;
    config.led_rgbw = 0;
    config.led_pin = 7;
    config.led_sustain_duration = 0;

    // moodlight settings
    config.use_govee_to_indicate_irrigation_status = 0;
    config.govee_irrigation_active_red = 50;
    config.govee_irrigation_active_green = 200; 
    config.govee_irrigation_active_blue = 50;    
    config.govee_irrigation_usurped_red = 200;
    config.govee_irrigation_usurped_green = 50;
    config.govee_irrigation_usurped_blue = 50;
    config.govee_sustain_duration = 60;    

    // foibles
    config.use_archaic_units = 1;
    config.use_simplified_english = 1;
    config.use_monday_as_week_start = 0;
    
}

/*!
 * \brief Convert configuration from v1 to v2 and set default values for new parameters
 * 
 * \return 0 on success, -1 on error
 */
void config_v1_to_v2(void)
{
    int i = 0;

    printf("Converting configuration from version 1 to version 2\n");
    config.version = 2;
    
    for(i = 0; i < NUM_ROWS(config.soil_moisture_threshold); i++)
    {
        config.soil_moisture_threshold[i] = 70; 
    }
}

/*!
 * \brief Convert configuration from v2 to v3 and set default values for new parameters
 * 
 * \return 0 on success, -1 on error
 */
void config_v2_to_v3(void)
{
    int i = 0;
    int j = 0;

    printf("Converting configuration from version 2 to version 3\n"); 
    config.version = 3;     

    config.zone_max = 1;

    for (j=0; j < 16; j++)
    {
        config.zone_gpio[j] = -1;
    }
    config.zone_gpio[0] = config.gpio_number;

    for(i = 0; i < 16; i++)
    {
        sprintf(config.zone_name[i], "Zone %d", j);
        config.zone_enable[i] = 1;

        for (j=0; j < 7; j++)
        {                      
            config.zone_duration[i][j] = 0;
        }
    }

    for (j=0; j < 7; j++)
    {        
        config.zone_duration[0][j] = config.day_duration[j];
    }    
}

/*!
 * \brief Convert configuration from v3 to v4 and set default values for new parameters
 * 
 * \return 0 on success, -1 on error
 */
void config_v3_to_v4(void)
{
    int i = 0;
    int j = 0;

    printf("Converting configuration from version 3 to version 4\n"); 
    config.version = 4;     

    config.led_strip_remote_enable = 0;

    for (i=0; i < 6; i++)
    {
        config.led_strip_remote_ip[i][0] = 0;
    }   
}

/*!
 * \brief Convert configuration from v4 to v5 and set default values for new parameters
 * 
 * \return 0 on success, -1 on error
 */
void config_v4_to_v5(void)
{
    int i = 0;
    int j = 0;

    printf("Converting configuration from version 4 to version 5\n"); 
    config.version = 5;     

    // v5 is now defunct -- all new parameters will be initialized on upgrade to v6


    // config.thermostat_enable = 0;
    // config.heating_gpio = -1;
    // config.cooling_gpio = -1;
    // config.fan_gpio = -1;
    // config.heating_to_cooling_lockout_mins = 1;
    // config.minimum_heating_on_mins = 1;
    // config.minimum_cooling_on_mins = 1;
    // config.minimum_heating_off_mins = 1;
    // config.minimum_cooling_off_mins = 1;

    // for(i=0; i<NUM_ROWS(config.thermostat_period_end_mow); i++)
    // {
    //     config.setpoint_start_mow[i] = -1;
    //     config.thermostat_period_end_mow[i] = 0;
    //     config.thermostat_period_setpoint_index[i] = 0;
    //     config.thermostat_period_number = i;
    // }

    // for(i=0; i<NUM_ROWS(config.setpoint_name); i++)
    // {
    //     config.setpoint_name[i][0] = 0;
    //     config.setpoint_temperaturex10[i] = 21;
    //     config.setpoint_number = i;

    //     sprintf(config.setpoint_name[i], "Setpoint%d", i);
    // }

    // config.powerwall_ip[0] = 0;
    // STRNCPY(config.powerwall_hostname, "powerwall", sizeof(config.powerwall_hostname));
    // config.powerwall_password[0] = 0;

    // config.grid_down_heating_setpoint_decrease = 10;
    // config.grid_down_cooling_setpoint_increase = 10;
    // config.grid_down_heating_disable_battery_level = 40;
    // config.grid_down_heating_enable_battery_level = 60;
    // config.grid_down_cooling_disable_battery_level = 70;
    // config.grid_down_cooling_enable_battery_level = 90;

    // for(i=0; i<NUM_ROWS(config.gpio_default); i++)
    // {
    //     config.gpio_default[i] = GP_UNINITIALIZED;
    // }
}

/*!
 * \brief Convert configuration from v4 to v5 and set default values for new parameters
 * 
 * \return 0 on success, -1 on error
 */
void config_v5_to_v6(void)
{
    int i = 0;
    int j = 0;

    printf("Converting configuration from version 5 to version 6\n"); 
    config.version = 6;     

    for(i=0; i<NUM_ROWS(config.gpio_default); i++)
    {
        config.gpio_default[i] = GP_UNINITIALIZED;
    }

    config.thermostat_enable = 0;
    config.heating_gpio = -1;
    config.cooling_gpio = -1;
    config.fan_gpio = -1;
    config.heating_to_cooling_lockout_mins = 1;
    config.minimum_heating_on_mins = 1;
    config.minimum_cooling_on_mins = 1;
    config.minimum_heating_off_mins = 1;
    config.minimum_cooling_off_mins = 1;
    config.thermostat_mode = 0;
    config.max_cycles_per_hour = 6;

    for(i=0; i<NUM_ROWS(config.setpoint_temperaturex10); i++)
    {
        config.setpoint_start_mow[i] = -1;
        config.setpoint_temperaturex10[i] = 21;
    }

    config.powerwall_ip[0] = 0;
    STRNCPY(config.powerwall_hostname, "powerwall", sizeof(config.powerwall_hostname));
    config.powerwall_password[0] = 0;

    config.grid_down_heating_setpoint_decrease = 10;
    config.grid_down_cooling_setpoint_increase = 10;
    config.grid_down_heating_disable_battery_level = 40;
    config.grid_down_heating_enable_battery_level = 60;
    config.grid_down_cooling_disable_battery_level = 70;
    config.grid_down_cooling_enable_battery_level = 90;
    
    for (i=0; i < 6; i++)
    {
        config.temperature_sensor_remote_ip[i][0] = 0;
    }     

}


/*!
 * \brief Record that configuration copy in RAM was altered and may now differ from the flash copy
 */
void config_changed(void)
{
    config_dirty_flag = 1;
}

/*!
 * \brief Check if RAM copy of configuration differs from flash copy.  Optionally clear the dirty flag.
 * 
 * \param[in]    clear_flag Set the dirty flag to false after returning its value
 * 
 * \return true if config in RAM differs from config in flash, otherwise flase
 */
bool config_dirty(bool clear_flag)
{
    int dirty = false;

    if (config_dirty_flag)
    {
        dirty = true;

        if (clear_flag)
        {
            config_dirty_flag = 0;
        }
    }

    return (dirty);
}

/*!
 * \brief Copy the configuation from flash into RAM.  Set default values if flash is corrupt.
 * 
 * \return 0 on success, -1 on error
 */
int config_read(void)
{
    int err = 0;

    // read configuration from flash
    flash_read_non_volatile_variables(); 

#ifdef DISABLE_CONFIG_VALIDATION
    printf("Configuration validation disabled!  Using whatever random garbage happens to be in flash...\n");
#else
    // check and correct configuration
    config_validate();
#endif

    return(err);
}

/*!
 * \brief Copy the configuation from RAM into flash if they differ.
 * 
 * \return 0 on success, -1 on error
 */
int config_write(void)
{
    int err = 0;

    // write configuration to flash if altered recently
    if (config_dirty(true))
    {
        // wait for 5 second period with no config changes
        do 
        {
            SLEEP_MS(5000);
        } while (config_dirty(true));

        // update crc
        config.crc = crc_buffer((uint8_t *)&config, offsetof(NON_VOL_VARIABLES_T, crc));  

        // compare ram and flash copies
        if (memcmp((char *)(XIP_BASE +  FLASH_TARGET_OFFSET), ((char *)&config), sizeof(config)))
        {
            printf("Writing configuration to flash\n");
            flash_write_non_volatile_variables();
        }
        else
        {
            printf("Refusing to write configuration to flash as RAM and flash copies are identical\n");
        }

        // check for collision
        if (config.crc != crc_buffer((uint8_t *)&config, offsetof(NON_VOL_VARIABLES_T, crc)))
        {
            // config was updated by another task after we computed the crc and possibly before we wrote to flash
            printf("Config update occured while writing to flash, will retry\n");

            // printf("config.crc = %d\n", config.crc);
            // printf("calculated crc = %d\n", crc_buffer((uint8_t *)&config, offsetof(NON_VOL_VARIABLES_T, crc)));
            
            config_changed();

            err = -1;
        }          
    }  

    return(err);
}

/*!
 * \brief Compare flash and RAM copies of configuration
 * 
 * \return 0 = no difference, 1 = difference
 */
bool config_compare_flash_ram(void)
{
    NON_VOL_VARIABLES_T *non_vol;
    int i;
    int len;
    uint16_t ram_crc;
    uint16_t flash_crc;    
    bool difference_found = false;

    for (i=0; i<sizeof(config); i++)
    {
        if (((char *)(XIP_BASE +  FLASH_TARGET_OFFSET))[i] != ((char *)&config)[i])
        {
            printf("Found byte difference at offset %d so will write flash\n", i);
            difference_found = true;
            break;
        }
    }
    
    return(difference_found);
}

/*!
 * \brief Check configuration is valid and upgrade if necessary 
 * 
 * \return 0 on success, -1 on error
 */
int config_validate(void)
{
    int err = 0;
    int i = 0;
    int version_from_flash = 0;
    uint16_t crc_from_flash = 0;
    uint16_t calculated_crc = 0;
    int latest_valid_config_version = 0;

    // read configuration into RAM
    flash_read_non_volatile_variables(); 


    // check for valid configuration
    for(i=0; i < NUM_ROWS(config_info); i++)
    {
        version_from_flash = *((int *)((uint8_t *)&config + config_info[i].version_offset));
        crc_from_flash = *((uint16_t *)((uint8_t *)&config + config_info[i].crc_offset));
        calculated_crc = crc_buffer((uint8_t *)&config, config_info[i].crc_offset);        

        if ((version_from_flash == config_info[i].version) && (crc_from_flash == calculated_crc))
        {
            printf("Found valid configuration version %d\n", version_from_flash);
            latest_valid_config_version = version_from_flash;
        }
    }

    // upgrade configuration sequentially to latest version 
    for(i=0; i < NUM_ROWS(config_info); i++)
    {
        if (latest_valid_config_version < config_info[i].version)
        {
            config_info[i].upgrade_function();
        }
    }

    return(err);
}

/*!
 * \brief Set a default time server in config if all four time server entries are blank
 * 
 * \return 0 on success, -1 on error
 */
int config_timeserver_failsafe(void)
{
    // failsafe - if no timeserver configured try pool.ntp.org
    if ((config.time_server[0][0] == 0) &&
        (config.time_server[1][0] == 0) &&
        (config.time_server[2][0] == 0) &&
        (config.time_server[3][0] == 0))
    {
        STRNCPY(config.time_server[0], "pool.ntp.org", sizeof(config.time_server[0]));
    }

    return(0);
}