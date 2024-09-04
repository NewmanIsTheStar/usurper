/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef CONFIG_H
#define CONFIG_H

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
    
    NO_PERSONALITY             =   4294967295,
} PERSONALITY_E;

// non-vol structure conversion info
typedef struct
{
    int version;
    size_t version_offset;
    size_t crc_offset;
    void (*upgrade_function)(void);
} NON_VOL_CONVERSION_T;

/*
* current non-volatile memory structure
* Modification Rule 1 -- duplicate this structure and append a version number before making changes
* Modification Rule 2 -- only add new fields, do not reorder or resize existing fields (except crc)
* Modification Rule 3 -- crc field must always be last
* Modification Rule 4 -- increment version and add an upgrade function to convert from previous version
*/
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
    int led_pattern_when_irrigation_usurped;
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
    int led_pattern_when_irrigation_usurped;
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


#endif