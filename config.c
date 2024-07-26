#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/flash.h"
#include <hardware/flash.h>

#include "lwip/sockets.h"


#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

#include "pluto.h"
#include "utility.h"
#include "config.h"
#include "flash.h"

#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

NON_VOL_VARIABLES_T config;
static int config_dirty_flag = 0;


/*!
 * \brief Record that configuration copy in RAM was altered and may now differ from the flash copy
 */
void config_changed(void)
{
    config_dirty_flag = 1;
}

/*!
 * \brief Check if RAM copy of configuration differs from flash copy.  Optionally clear the status flag.
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
 * \brief Set default values for configuration
 * 
 * \return 0 on success, -1 on error
 */
int config_initialize(void)
{
    int i;

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
    STRNCPY(config.wifi_country, "USA", sizeof(config.wifi_country));    
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
    config.led_pattern = 1;
    config.led_speed = 100;
    config.led_rgbw = 0;
    config.led_pin = 7;

    // moodlight settings
    config.govee_enable = 0;
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
    
    return(0);
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


/*!
 * \brief Copy the configuation from flash into RAM.  Set default values if flash is corrupt.
 * 
 * \return 0 on success, -1 on error
 */
int config_read(void)
{
    int err = 0;

    // initialize configuration
    flash_read_non_volatile_variables(); 

    if (flash_corrupt())
    {
        flash_initialize_non_volatile_variables();

        if(flash_corrupt())
        {
            printf("FUBAR!  Flash still corrupt after initialization\n");
            err = 1;
        }
        else
        {
            flash_read_non_volatile_variables();
        }
    }

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
            sleep_ms(5000);
        } while (config_dirty(true));

        flash_write_non_volatile_variables();
    }  

    return(err);
}