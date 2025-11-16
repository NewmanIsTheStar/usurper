/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <limits.h>

void config_changed(void);
bool config_dirty(bool clear_flag);
void config_blank_to_v1(void);
int config_timeserver_failsafe(void);
int config_read(void);
int config_write(void);

// device personality
typedef enum
{
    SPRINKLER_USURPER          =   0,             // add wifi control to exising "dumb" sprinkler controller
    SPRINKLER_CONTROLLER       =   1,             // multizone sprinkler control 
    LED_STRIP_CONTROLLER       =   2,             // allows remote control of an led strip
    HVAC_THERMOSTAT            =   3,             // wifi confrolled thermostat
    HOME_CONTROLLER            =   4,             // home controller
    
    NO_PERSONALITY             =   4294967295     // force enum to be 4 bytes long 
} PERSONALITY_E;

// non-vol structure conversion info
typedef struct
{
    int version;
    size_t version_offset;
    size_t crc_offset;
    void (*upgrade_function)(void);
} NON_VOL_CONVERSION_T;

// gpio defaults
typedef enum
{
    GP_UNINITIALIZED          =   0,         
    GP_INPUT_FLOATING         =   1,              
    GP_INPUT_PULLED_HIGH      =   2,             
    GP_INPUT_PULLED_LOW       =   3,
    GP_OUTPUT_HIGH            =   4,
    GP_OUTPUT_LOW             =   5,
    
    GP_LAST                   =   4294967295     // force enum to be 4 bytes long 
} GPIO_DEFAULT_T;

/*
* current non-volatile memory structure
* Modification Rule 1 -- copy this structure, append a version number and place at the bottom of this file before making changes
* Modification Rule 2 -- only add new fields, do not reorder or resize existing fields (except crc)
* Modification Rule 3 -- crc field must always be last (used to find end of config in flash)
* Modification Rule 4 -- add an upgrade function to convert from previous version and add this function to the config_info table
*/

// current version
typedef struct
{
    int version;
    PERSONALITY_E personality;
    char wifi_ssid[32];
    char wifi_password[32];
    char wifi_country[32];
    char dhcp_enable;
    char ip_address[32];
    char network_mask[32];    
    char gateway[32];      
    char irrigation_enable;
    char day_schedule_enable[7];
    int day_start[7];
    int day_duration[7];
    int day_start_alternate[7];
    int day_duration_alternate[7];    
    char schedule_opportunity_start[32];
    char schedule_opportunity_duration[32];
    int timezone_offset;
    char daylightsaving_enable;
    char daylightsaving_start[32];
    char daylightsaving_end[32];
    char time_server[4][32];
    int weather_station_enable;
    char weather_station_ip[32];
    int wind_threshold;
    int rain_week_threshold;
    int rain_day_threshold;
    int relay_normally_open;
    int gpio_number;
    int led_pattern;
    int led_speed;
    int led_number;
    int led_pin;
    int led_rgbw;
    int use_led_strip_to_indicate_irrigation_status;
    int led_pattern_when_irrigation_active;
    int led_pattern_when_irrigation_terminated;
    int led_sustain_duration; 
    int led_strip_remote_enable;  
    char led_strip_remote_ip[6][32];  
    char govee_light_ip[32]; 
    int use_govee_to_indicate_irrigation_status;
    int govee_irrigation_active_red;
    int govee_irrigation_active_green; 
    int govee_irrigation_active_blue;    
    int govee_irrigation_usurped_red;
    int govee_irrigation_usurped_green;
    int govee_irrigation_usurped_blue;
    int govee_sustain_duration;
    int syslog_enable;
    char syslog_server_ip[32];    
    int use_archaic_units; 
    int use_simplified_english;
    int use_monday_as_week_start; 
    int soil_moisture_threshold[16];
    int zone_max;
    int zone_gpio[16];
    char zone_name[16][32];
    char zone_enable[16];    
    int zone_duration[16][7];
    GPIO_DEFAULT_T gpio_default[29];
    int thermostat_enable;
    int heating_gpio;
    int cooling_gpio;
    int fan_gpio;
    int heating_to_cooling_lockout_mins;
    int minimum_heating_on_mins;
    int minimum_cooling_on_mins;
    int minimum_heating_off_mins;
    int minimum_cooling_off_mins;
    int thermostat_mode;   
    int max_cycles_per_hour;
    int setpoint_number;
    char setpoint_name[16][32];     // obsolete
    int setpoint_temperaturex10[32];  
    int thermostat_hysteresis; 
    int setpoint_start_mow[32];  
    int setpoint_mode[32];  
    char powerwall_ip[32];
    char powerwall_hostname[32];  
    char powerwall_password[32];
    int grid_down_heating_setpoint_decrease;
    int grid_down_cooling_setpoint_increase;
    int grid_down_heating_disable_battery_level;
    int grid_down_heating_enable_battery_level;
    int grid_down_cooling_disable_battery_level;
    int grid_down_cooling_enable_battery_level;    
    char temperature_sensor_remote_ip[6][32]; 
    int thermostat_mode_button_gpio;
    int thermostat_increase_button_gpio;
    int thermostat_decrease_button_gpio;
    int thermostat_temperature_sensor_clock_gpio;
    int thermostat_temperature_sensor_data_gpio;
    int thermostat_seven_segment_display_clock_gpio;
    int thermostat_seven_segment_display_data_gpio; 
    int outside_temperature_threshold;
    int thermostat_display_brightness;
    int thermostat_display_num_digits;
    int setpoint_heating_temperaturex10[32]; 
    int setpoint_cooling_temperaturex10[32];     
    uint16_t crc;
} NON_VOL_VARIABLES_T;


// previous non-volatile data stuctures -- used when upgrading
typedef struct
{
    int version;
    PERSONALITY_E personality;
    char wifi_ssid[32];
    char wifi_password[32];
    char wifi_country[32];
    char dhcp_enable;
    char ip_address[32];
    char network_mask[32];    
    char gateway[32];      
    char irrigation_enable;
    char day_schedule_enable[7];
    int day_start[7];
    int day_duration[7];
    int day_start_alternate[7];
    int day_duration_alternate[7];    
    char schedule_opportunity_start[32];
    char schedule_opportunity_duration[32];
    int timezone_offset;
    char daylightsaving_enable;
    char daylightsaving_start[32];
    char daylightsaving_end[32];
    char time_server[4][32];
    int weather_station_enable;
    char weather_station_ip[32];
    int wind_threshold;
    int rain_week_threshold;
    int rain_day_threshold;
    int relay_normally_open;
    int gpio_number;
    int led_pattern;
    int led_speed;
    int led_number;
    int led_pin;
    int led_rgbw;
    int use_led_strip_to_indicate_irrigation_status;
    int led_pattern_when_irrigation_active;
    int led_pattern_when_irrigation_terminated;
    int led_sustain_duration; 
    int led_strip_remote_enable;  
    char led_strip_remote_ip[6][32];  
    char govee_light_ip[32]; 
    int use_govee_to_indicate_irrigation_status;
    int govee_irrigation_active_red;
    int govee_irrigation_active_green; 
    int govee_irrigation_active_blue;    
    int govee_irrigation_usurped_red;
    int govee_irrigation_usurped_green;
    int govee_irrigation_usurped_blue;
    int govee_sustain_duration;
    int syslog_enable;
    char syslog_server_ip[32];    
    int use_archaic_units; 
    int use_simplified_english;
    int use_monday_as_week_start;       
    uint16_t crc;
} NON_VOL_VARIABLES_T_VERSION_1;

typedef struct
{
    int version;
    PERSONALITY_E personality;
    char wifi_ssid[32];
    char wifi_password[32];
    char wifi_country[32];
    char dhcp_enable;
    char ip_address[32];
    char network_mask[32];    
    char gateway[32];      
    char irrigation_enable;
    char day_schedule_enable[7];
    int day_start[7];
    int day_duration[7];
    int day_start_alternate[7];
    int day_duration_alternate[7];    
    char schedule_opportunity_start[32];
    char schedule_opportunity_duration[32];
    int timezone_offset;
    char daylightsaving_enable;
    char daylightsaving_start[32];
    char daylightsaving_end[32];
    char time_server[4][32];
    int weather_station_enable;
    char weather_station_ip[32];
    int wind_threshold;
    int rain_week_threshold;
    int rain_day_threshold;
    int relay_normally_open;
    int gpio_number;
    int led_pattern;
    int led_speed;
    int led_number;
    int led_pin;
    int led_rgbw;
    int use_led_strip_to_indicate_irrigation_status;
    int led_pattern_when_irrigation_active;
    int led_pattern_when_irrigation_terminated;
    int led_sustain_duration; 
    int led_strip_remote_enable;  
    char led_strip_remote_ip[6][32];  
    char govee_light_ip[32]; 
    int use_govee_to_indicate_irrigation_status;
    int govee_irrigation_active_red;
    int govee_irrigation_active_green; 
    int govee_irrigation_active_blue;    
    int govee_irrigation_usurped_red;
    int govee_irrigation_usurped_green;
    int govee_irrigation_usurped_blue;
    int govee_sustain_duration;
    int syslog_enable;
    char syslog_server_ip[32];    
    int use_archaic_units; 
    int use_simplified_english;
    int use_monday_as_week_start; 
    int soil_moisture_threshold[16];      
    uint16_t crc;
} NON_VOL_VARIABLES_T_VERSION_2;

typedef struct
{
    int version;
    PERSONALITY_E personality;
    char wifi_ssid[32];
    char wifi_password[32];
    char wifi_country[32];
    char dhcp_enable;
    char ip_address[32];
    char network_mask[32];    
    char gateway[32];      
    char irrigation_enable;
    char day_schedule_enable[7];
    int day_start[7];
    int day_duration[7];
    int day_start_alternate[7];
    int day_duration_alternate[7];    
    char schedule_opportunity_start[32];
    char schedule_opportunity_duration[32];
    int timezone_offset;
    char daylightsaving_enable;
    char daylightsaving_start[32];
    char daylightsaving_end[32];
    char time_server[4][32];
    int weather_station_enable;
    char weather_station_ip[32];
    int wind_threshold;
    int rain_week_threshold;
    int rain_day_threshold;
    int relay_normally_open;
    int gpio_number;
    int led_pattern;
    int led_speed;
    int led_number;
    int led_pin;
    int led_rgbw;
    int use_led_strip_to_indicate_irrigation_status;
    int led_pattern_when_irrigation_active;
    int led_pattern_when_irrigation_terminated;
    int led_sustain_duration; 
    int led_strip_remote_enable;  
    char led_strip_remote_ip[6][32];  
    char govee_light_ip[32]; 
    int use_govee_to_indicate_irrigation_status;
    int govee_irrigation_active_red;
    int govee_irrigation_active_green; 
    int govee_irrigation_active_blue;    
    int govee_irrigation_usurped_red;
    int govee_irrigation_usurped_green;
    int govee_irrigation_usurped_blue;
    int govee_sustain_duration;
    int syslog_enable;
    char syslog_server_ip[32];    
    int use_archaic_units; 
    int use_simplified_english;
    int use_monday_as_week_start; 
    int soil_moisture_threshold[16];
    int zone_max;
    int zone_gpio[16];
    char zone_name[16][32];
    char zone_enable[16];    
    int zone_duration[16][7];
    uint16_t crc;
} NON_VOL_VARIABLES_T_VERSION_3;

typedef struct
{
    int version;
    PERSONALITY_E personality;
    char wifi_ssid[32];
    char wifi_password[32];
    char wifi_country[32];
    char dhcp_enable;
    char ip_address[32];
    char network_mask[32];    
    char gateway[32];      
    char irrigation_enable;
    char day_schedule_enable[7];
    int day_start[7];
    int day_duration[7];
    int day_start_alternate[7];
    int day_duration_alternate[7];    
    char schedule_opportunity_start[32];
    char schedule_opportunity_duration[32];
    int timezone_offset;
    char daylightsaving_enable;
    char daylightsaving_start[32];
    char daylightsaving_end[32];
    char time_server[4][32];
    int weather_station_enable;
    char weather_station_ip[32];
    int wind_threshold;
    int rain_week_threshold;
    int rain_day_threshold;
    int relay_normally_open;
    int gpio_number;
    int led_pattern;
    int led_speed;
    int led_number;
    int led_pin;
    int led_rgbw;
    int use_led_strip_to_indicate_irrigation_status;
    int led_pattern_when_irrigation_active;
    int led_pattern_when_irrigation_terminated;
    int led_sustain_duration; 
    int led_strip_remote_enable;  
    char led_strip_remote_ip[6][32];  
    char govee_light_ip[32]; 
    int use_govee_to_indicate_irrigation_status;
    int govee_irrigation_active_red;
    int govee_irrigation_active_green; 
    int govee_irrigation_active_blue;    
    int govee_irrigation_usurped_red;
    int govee_irrigation_usurped_green;
    int govee_irrigation_usurped_blue;
    int govee_sustain_duration;
    int syslog_enable;
    char syslog_server_ip[32];    
    int use_archaic_units; 
    int use_simplified_english;
    int use_monday_as_week_start; 
    int soil_moisture_threshold[16];
    int zone_max;
    int zone_gpio[16];
    char zone_name[16][32];
    char zone_enable[16];    
    int zone_duration[16][7];
    uint16_t crc;
} NON_VOL_VARIABLES_T_VERSION_4;

typedef struct
{
    int version;
    PERSONALITY_E personality;
    char wifi_ssid[32];
    char wifi_password[32];
    char wifi_country[32];
    char dhcp_enable;
    char ip_address[32];
    char network_mask[32];    
    char gateway[32];      
    char irrigation_enable;
    char day_schedule_enable[7];
    int day_start[7];
    int day_duration[7];
    int day_start_alternate[7];
    int day_duration_alternate[7];    
    char schedule_opportunity_start[32];
    char schedule_opportunity_duration[32];
    int timezone_offset;
    char daylightsaving_enable;
    char daylightsaving_start[32];
    char daylightsaving_end[32];
    char time_server[4][32];
    int weather_station_enable;
    char weather_station_ip[32];
    int wind_threshold;
    int rain_week_threshold;
    int rain_day_threshold;
    int relay_normally_open;
    int gpio_number;
    int led_pattern;
    int led_speed;
    int led_number;
    int led_pin;
    int led_rgbw;
    int use_led_strip_to_indicate_irrigation_status;
    int led_pattern_when_irrigation_active;
    int led_pattern_when_irrigation_terminated;
    int led_sustain_duration; 
    int led_strip_remote_enable;  
    char led_strip_remote_ip[6][32];  
    char govee_light_ip[32]; 
    int use_govee_to_indicate_irrigation_status;
    int govee_irrigation_active_red;
    int govee_irrigation_active_green; 
    int govee_irrigation_active_blue;    
    int govee_irrigation_usurped_red;
    int govee_irrigation_usurped_green;
    int govee_irrigation_usurped_blue;
    int govee_sustain_duration;
    int syslog_enable;
    char syslog_server_ip[32];    
    int use_archaic_units; 
    int use_simplified_english;
    int use_monday_as_week_start; 
    int soil_moisture_threshold[16];
    int zone_max;
    int zone_gpio[16];
    char zone_name[16][32];
    char zone_enable[16];    
    int zone_duration[16][7];
    int thermostat_enable;
    int heating_gpio;
    int cooling_gpio;
    int fan_gpio;
    int heating_to_cooling_lockout_mins;
    int minimum_heating_on_mins;
    int minimum_cooling_on_mins;
    int minimum_heating_off_mins;
    int minimum_cooling_off_mins;
    int thermostat_mode;
    int max_cycles_per_hour;
    int setpoint_number;
    char setpoint_name[16][32];
    int setpoint_temperaturex10[16];  
    int thermostat_period_number;
    int thermostat_period_start_mow[16];
    int thermostat_period_end_mow[16];
    int thermostat_period_setpoint_index[16];
    char powerwall_ip[32];
    char powerwall_hostname[32];  // for sni may differ from dns
    char powerwall_password[32];
    int grid_down_heating_setpoint_decrease;
    int grid_down_cooling_setpoint_increase;
    int grid_down_heating_disable_battery_level;
    int grid_down_heating_enable_battery_level;
    int grid_down_cooling_disable_battery_level;
    int grid_down_cooling_enable_battery_level;
    GPIO_DEFAULT_T gpio_default[29];
    uint16_t crc;
} NON_VOL_VARIABLES_T_VERSION_5;

typedef struct
{
    int version;
    PERSONALITY_E personality;
    char wifi_ssid[32];
    char wifi_password[32];
    char wifi_country[32];
    char dhcp_enable;
    char ip_address[32];
    char network_mask[32];    
    char gateway[32];      
    char irrigation_enable;
    char day_schedule_enable[7];
    int day_start[7];
    int day_duration[7];
    int day_start_alternate[7];
    int day_duration_alternate[7];    
    char schedule_opportunity_start[32];
    char schedule_opportunity_duration[32];
    int timezone_offset;
    char daylightsaving_enable;
    char daylightsaving_start[32];
    char daylightsaving_end[32];
    char time_server[4][32];
    int weather_station_enable;
    char weather_station_ip[32];
    int wind_threshold;
    int rain_week_threshold;
    int rain_day_threshold;
    int relay_normally_open;
    int gpio_number;
    int led_pattern;
    int led_speed;
    int led_number;
    int led_pin;
    int led_rgbw;
    int use_led_strip_to_indicate_irrigation_status;
    int led_pattern_when_irrigation_active;
    int led_pattern_when_irrigation_terminated;
    int led_sustain_duration; 
    int led_strip_remote_enable;  
    char led_strip_remote_ip[6][32];  
    char govee_light_ip[32]; 
    int use_govee_to_indicate_irrigation_status;
    int govee_irrigation_active_red;
    int govee_irrigation_active_green; 
    int govee_irrigation_active_blue;    
    int govee_irrigation_usurped_red;
    int govee_irrigation_usurped_green;
    int govee_irrigation_usurped_blue;
    int govee_sustain_duration;
    int syslog_enable;
    char syslog_server_ip[32];    
    int use_archaic_units; 
    int use_simplified_english;
    int use_monday_as_week_start; 
    int soil_moisture_threshold[16];
    int zone_max;
    int zone_gpio[16];
    char zone_name[16][32];
    char zone_enable[16];    
    int zone_duration[16][7];
    GPIO_DEFAULT_T gpio_default[29];
    int thermostat_enable;
    int heating_gpio;
    int cooling_gpio;
    int fan_gpio;
    int heating_to_cooling_lockout_mins;
    int minimum_heating_on_mins;
    int minimum_cooling_on_mins;
    int minimum_heating_off_mins;
    int minimum_cooling_off_mins;
    int thermostat_mode;   //?
    int max_cycles_per_hour;
    int setpoint_number;
    char setpoint_name[16][32];     // obsolete
    int setpoint_temperaturex10[32];  // <== increased from 16 to 32
    int thermostat_period_number;   // obsolete
    int setpoint_start_mow[32];  // <== increased from 16 to 32
    int thermostat_period_end_mow[16]; // obsolete
    int thermostat_period_setpoint_index[16]; // obsolete
    char powerwall_ip[32];
    char powerwall_hostname[32];  // for sni may differ from dns
    char powerwall_password[32];
    int grid_down_heating_setpoint_decrease;
    int grid_down_cooling_setpoint_increase;
    int grid_down_heating_disable_battery_level;
    int grid_down_heating_enable_battery_level;
    int grid_down_cooling_disable_battery_level;
    int grid_down_cooling_enable_battery_level;    
    char temperature_sensor_remote_ip[6][32]; 
    uint16_t crc;
} NON_VOL_VARIABLES_T_VERSION_6;

typedef struct
{
    int version;
    PERSONALITY_E personality;
    char wifi_ssid[32];
    char wifi_password[32];
    char wifi_country[32];
    char dhcp_enable;
    char ip_address[32];
    char network_mask[32];    
    char gateway[32];      
    char irrigation_enable;
    char day_schedule_enable[7];
    int day_start[7];
    int day_duration[7];
    int day_start_alternate[7];
    int day_duration_alternate[7];    
    char schedule_opportunity_start[32];
    char schedule_opportunity_duration[32];
    int timezone_offset;
    char daylightsaving_enable;
    char daylightsaving_start[32];
    char daylightsaving_end[32];
    char time_server[4][32];
    int weather_station_enable;
    char weather_station_ip[32];
    int wind_threshold;
    int rain_week_threshold;
    int rain_day_threshold;
    int relay_normally_open;
    int gpio_number;
    int led_pattern;
    int led_speed;
    int led_number;
    int led_pin;
    int led_rgbw;
    int use_led_strip_to_indicate_irrigation_status;
    int led_pattern_when_irrigation_active;
    int led_pattern_when_irrigation_terminated;
    int led_sustain_duration; 
    int led_strip_remote_enable;  
    char led_strip_remote_ip[6][32];  
    char govee_light_ip[32]; 
    int use_govee_to_indicate_irrigation_status;
    int govee_irrigation_active_red;
    int govee_irrigation_active_green; 
    int govee_irrigation_active_blue;    
    int govee_irrigation_usurped_red;
    int govee_irrigation_usurped_green;
    int govee_irrigation_usurped_blue;
    int govee_sustain_duration;
    int syslog_enable;
    char syslog_server_ip[32];    
    int use_archaic_units; 
    int use_simplified_english;
    int use_monday_as_week_start; 
    int soil_moisture_threshold[16];
    int zone_max;
    int zone_gpio[16];
    char zone_name[16][32];
    char zone_enable[16];    
    int zone_duration[16][7];
    GPIO_DEFAULT_T gpio_default[29];
    int thermostat_enable;
    int heating_gpio;
    int cooling_gpio;
    int fan_gpio;
    int heating_to_cooling_lockout_mins;
    int minimum_heating_on_mins;
    int minimum_cooling_on_mins;
    int minimum_heating_off_mins;
    int minimum_cooling_off_mins;
    int thermostat_mode;   //?
    int max_cycles_per_hour;
    int setpoint_number;
    char setpoint_name[16][32];     // obsolete
    int setpoint_temperaturex10[32];  // <== increased from 16 to 32
    int thermostat_hysteresis; 
    int setpoint_start_mow[32];  // <== increased from 16 to 32
    //int thermostat_period_end_mow[16]; // obsolete
    //int thermostat_period_setpoint_index[16]; // obsolete
    int setpoint_mode[32];  // replaces two 16 int arrays above
    char powerwall_ip[32];
    char powerwall_hostname[32];  // for sni may differ from dns
    char powerwall_password[32];
    int grid_down_heating_setpoint_decrease;
    int grid_down_cooling_setpoint_increase;
    int grid_down_heating_disable_battery_level;
    int grid_down_heating_enable_battery_level;
    int grid_down_cooling_disable_battery_level;
    int grid_down_cooling_enable_battery_level;    
    char temperature_sensor_remote_ip[6][32]; 
    int thermostat_mode_button_gpio;
    int thermostat_increase_button_gpio;
    int thermostat_decrease_button_gpio;
    int thermostat_temperature_sensor_clock_gpio;
    int thermostat_temperature_sensor_data_gpio;
    int thermostat_seven_segment_display_clock_gpio;
    int thermostat_seven_segment_display_data_gpio;    
    uint16_t crc;
} NON_VOL_VARIABLES_T_VERSION_7;

typedef struct
{
    int version;
    PERSONALITY_E personality;
    char wifi_ssid[32];
    char wifi_password[32];
    char wifi_country[32];
    char dhcp_enable;
    char ip_address[32];
    char network_mask[32];    
    char gateway[32];      
    char irrigation_enable;
    char day_schedule_enable[7];
    int day_start[7];
    int day_duration[7];
    int day_start_alternate[7];
    int day_duration_alternate[7];    
    char schedule_opportunity_start[32];
    char schedule_opportunity_duration[32];
    int timezone_offset;
    char daylightsaving_enable;
    char daylightsaving_start[32];
    char daylightsaving_end[32];
    char time_server[4][32];
    int weather_station_enable;
    char weather_station_ip[32];
    int wind_threshold;
    int rain_week_threshold;
    int rain_day_threshold;
    int relay_normally_open;
    int gpio_number;
    int led_pattern;
    int led_speed;
    int led_number;
    int led_pin;
    int led_rgbw;
    int use_led_strip_to_indicate_irrigation_status;
    int led_pattern_when_irrigation_active;
    int led_pattern_when_irrigation_terminated;
    int led_sustain_duration; 
    int led_strip_remote_enable;  
    char led_strip_remote_ip[6][32];  
    char govee_light_ip[32]; 
    int use_govee_to_indicate_irrigation_status;
    int govee_irrigation_active_red;
    int govee_irrigation_active_green; 
    int govee_irrigation_active_blue;    
    int govee_irrigation_usurped_red;
    int govee_irrigation_usurped_green;
    int govee_irrigation_usurped_blue;
    int govee_sustain_duration;
    int syslog_enable;
    char syslog_server_ip[32];    
    int use_archaic_units; 
    int use_simplified_english;
    int use_monday_as_week_start; 
    int soil_moisture_threshold[16];
    int zone_max;
    int zone_gpio[16];
    char zone_name[16][32];
    char zone_enable[16];    
    int zone_duration[16][7];
    GPIO_DEFAULT_T gpio_default[29];
    int thermostat_enable;
    int heating_gpio;
    int cooling_gpio;
    int fan_gpio;
    int heating_to_cooling_lockout_mins;
    int minimum_heating_on_mins;
    int minimum_cooling_on_mins;
    int minimum_heating_off_mins;
    int minimum_cooling_off_mins;
    int thermostat_mode;   //?
    int max_cycles_per_hour;
    int setpoint_number;
    char setpoint_name[16][32];     // obsolete
    int setpoint_temperaturex10[32];  // <== increased from 16 to 32
    int thermostat_hysteresis; 
    int setpoint_start_mow[32];  // <== increased from 16 to 32
    //int thermostat_period_end_mow[16]; // obsolete
    //int thermostat_period_setpoint_index[16]; // obsolete
    int setpoint_mode[32];  // replaces two 16 int arrays above
    char powerwall_ip[32];
    char powerwall_hostname[32];  // for sni may differ from dns
    char powerwall_password[32];
    int grid_down_heating_setpoint_decrease;
    int grid_down_cooling_setpoint_increase;
    int grid_down_heating_disable_battery_level;
    int grid_down_heating_enable_battery_level;
    int grid_down_cooling_disable_battery_level;
    int grid_down_cooling_enable_battery_level;    
    char temperature_sensor_remote_ip[6][32]; 
    int thermostat_mode_button_gpio;
    int thermostat_increase_button_gpio;
    int thermostat_decrease_button_gpio;
    int thermostat_temperature_sensor_clock_gpio;
    int thermostat_temperature_sensor_data_gpio;
    int thermostat_seven_segment_display_clock_gpio;
    int thermostat_seven_segment_display_data_gpio; 
    int outside_temperature_threshold;
    uint16_t crc;
} NON_VOL_VARIABLES_T_VERSION_8;;

typedef struct
{
    int version;
    PERSONALITY_E personality;
    char wifi_ssid[32];
    char wifi_password[32];
    char wifi_country[32];
    char dhcp_enable;
    char ip_address[32];
    char network_mask[32];    
    char gateway[32];      
    char irrigation_enable;
    char day_schedule_enable[7];
    int day_start[7];
    int day_duration[7];
    int day_start_alternate[7];
    int day_duration_alternate[7];    
    char schedule_opportunity_start[32];
    char schedule_opportunity_duration[32];
    int timezone_offset;
    char daylightsaving_enable;
    char daylightsaving_start[32];
    char daylightsaving_end[32];
    char time_server[4][32];
    int weather_station_enable;
    char weather_station_ip[32];
    int wind_threshold;
    int rain_week_threshold;
    int rain_day_threshold;
    int relay_normally_open;
    int gpio_number;
    int led_pattern;
    int led_speed;
    int led_number;
    int led_pin;
    int led_rgbw;
    int use_led_strip_to_indicate_irrigation_status;
    int led_pattern_when_irrigation_active;
    int led_pattern_when_irrigation_terminated;
    int led_sustain_duration; 
    int led_strip_remote_enable;  
    char led_strip_remote_ip[6][32];  
    char govee_light_ip[32]; 
    int use_govee_to_indicate_irrigation_status;
    int govee_irrigation_active_red;
    int govee_irrigation_active_green; 
    int govee_irrigation_active_blue;    
    int govee_irrigation_usurped_red;
    int govee_irrigation_usurped_green;
    int govee_irrigation_usurped_blue;
    int govee_sustain_duration;
    int syslog_enable;
    char syslog_server_ip[32];    
    int use_archaic_units; 
    int use_simplified_english;
    int use_monday_as_week_start; 
    int soil_moisture_threshold[16];
    int zone_max;
    int zone_gpio[16];
    char zone_name[16][32];
    char zone_enable[16];    
    int zone_duration[16][7];
    GPIO_DEFAULT_T gpio_default[29];
    int thermostat_enable;
    int heating_gpio;
    int cooling_gpio;
    int fan_gpio;
    int heating_to_cooling_lockout_mins;
    int minimum_heating_on_mins;
    int minimum_cooling_on_mins;
    int minimum_heating_off_mins;
    int minimum_cooling_off_mins;
    int thermostat_mode;   //?
    int max_cycles_per_hour;
    int setpoint_number;
    char setpoint_name[16][32];     // obsolete
    int setpoint_temperaturex10[32];  // <== increased from 16 to 32
    int thermostat_hysteresis; 
    int setpoint_start_mow[32];  // <== increased from 16 to 32
    //int thermostat_period_end_mow[16]; // obsolete
    //int thermostat_period_setpoint_index[16]; // obsolete
    int setpoint_mode[32];  // replaces two 16 int arrays above
    char powerwall_ip[32];
    char powerwall_hostname[32];  // for sni may differ from dns
    char powerwall_password[32];
    int grid_down_heating_setpoint_decrease;
    int grid_down_cooling_setpoint_increase;
    int grid_down_heating_disable_battery_level;
    int grid_down_heating_enable_battery_level;
    int grid_down_cooling_disable_battery_level;
    int grid_down_cooling_enable_battery_level;    
    char temperature_sensor_remote_ip[6][32]; 
    int thermostat_mode_button_gpio;
    int thermostat_increase_button_gpio;
    int thermostat_decrease_button_gpio;
    int thermostat_temperature_sensor_clock_gpio;
    int thermostat_temperature_sensor_data_gpio;
    int thermostat_seven_segment_display_clock_gpio;
    int thermostat_seven_segment_display_data_gpio; 
    int outside_temperature_threshold;
    int thermostat_display_brightness;
    uint16_t crc;
} NON_VOL_VARIABLES_T_VERSION_9;

typedef struct
{
    int version;
    PERSONALITY_E personality;
    char wifi_ssid[32];
    char wifi_password[32];
    char wifi_country[32];
    char dhcp_enable;
    char ip_address[32];
    char network_mask[32];    
    char gateway[32];      
    char irrigation_enable;
    char day_schedule_enable[7];
    int day_start[7];
    int day_duration[7];
    int day_start_alternate[7];
    int day_duration_alternate[7];    
    char schedule_opportunity_start[32];
    char schedule_opportunity_duration[32];
    int timezone_offset;
    char daylightsaving_enable;
    char daylightsaving_start[32];
    char daylightsaving_end[32];
    char time_server[4][32];
    int weather_station_enable;
    char weather_station_ip[32];
    int wind_threshold;
    int rain_week_threshold;
    int rain_day_threshold;
    int relay_normally_open;
    int gpio_number;
    int led_pattern;
    int led_speed;
    int led_number;
    int led_pin;
    int led_rgbw;
    int use_led_strip_to_indicate_irrigation_status;
    int led_pattern_when_irrigation_active;
    int led_pattern_when_irrigation_terminated;
    int led_sustain_duration; 
    int led_strip_remote_enable;  
    char led_strip_remote_ip[6][32];  
    char govee_light_ip[32]; 
    int use_govee_to_indicate_irrigation_status;
    int govee_irrigation_active_red;
    int govee_irrigation_active_green; 
    int govee_irrigation_active_blue;    
    int govee_irrigation_usurped_red;
    int govee_irrigation_usurped_green;
    int govee_irrigation_usurped_blue;
    int govee_sustain_duration;
    int syslog_enable;
    char syslog_server_ip[32];    
    int use_archaic_units; 
    int use_simplified_english;
    int use_monday_as_week_start; 
    int soil_moisture_threshold[16];
    int zone_max;
    int zone_gpio[16];
    char zone_name[16][32];
    char zone_enable[16];    
    int zone_duration[16][7];
    GPIO_DEFAULT_T gpio_default[29];
    int thermostat_enable;
    int heating_gpio;
    int cooling_gpio;
    int fan_gpio;
    int heating_to_cooling_lockout_mins;
    int minimum_heating_on_mins;
    int minimum_cooling_on_mins;
    int minimum_heating_off_mins;
    int minimum_cooling_off_mins;
    int thermostat_mode;   
    int max_cycles_per_hour;
    int setpoint_number;
    char setpoint_name[16][32];     // obsolete
    int setpoint_temperaturex10[32];  
    int thermostat_hysteresis; 
    int setpoint_start_mow[32];  
    int setpoint_mode[32];  
    char powerwall_ip[32];
    char powerwall_hostname[32];  
    char powerwall_password[32];
    int grid_down_heating_setpoint_decrease;
    int grid_down_cooling_setpoint_increase;
    int grid_down_heating_disable_battery_level;
    int grid_down_heating_enable_battery_level;
    int grid_down_cooling_disable_battery_level;
    int grid_down_cooling_enable_battery_level;    
    char temperature_sensor_remote_ip[6][32]; 
    int thermostat_mode_button_gpio;
    int thermostat_increase_button_gpio;
    int thermostat_decrease_button_gpio;
    int thermostat_temperature_sensor_clock_gpio;
    int thermostat_temperature_sensor_data_gpio;
    int thermostat_seven_segment_display_clock_gpio;
    int thermostat_seven_segment_display_data_gpio; 
    int outside_temperature_threshold;
    int thermostat_display_brightness;
    int thermostat_display_num_digits;
    uint16_t crc;
} NON_VOL_VARIABLES_T_VERSION_10;

#endif