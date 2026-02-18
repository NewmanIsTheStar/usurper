/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

 #define _GNU_SOURCE
 
#include "pico/cyw43_arch.h"
#include "pico/types.h"
#include "pico/stdlib.h"
#include <string.h>

 #include "hardware/watchdog.h"

#include "lwip/apps/httpd.h"
#include "lwip/sockets.h"

#include "time.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

#include "flash.h"
#include "weather.h"
#include "calendar.h"
#include "utility.h"
#include "config.h"
#include "led_strip.h"
#include "thermostat.h"
#include "worker_tasks.h"
#include "pluto.h"


extern NON_VOL_VARIABLES_T config;
extern WEB_VARIABLES_T web;
extern WORKER_TASK_T worker_tasks[];

extern char current_calendar_web_page[50];
static bool test_end_redirect = false;


/*!
 * \brief print all the parameters passed to a cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
void dump_parameters(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    char *param = NULL;
    char *value = NULL;    
    
    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            printf("Parameter: %s has Value: %s\n", param, value);
        }

        i++;
    }
}

// moved to pluto.c

// /*!
//  * \brief convert string to integer times ten plus tenths e.g. "78.32" => 783
//  *
//  * \param[in]  value_string index of cgi handler in cgi_handlers table
//  * 
//  * \return integer = value x 10
//  */
// int get_int_with_tenths_from_string(char *value_string)
// {
//     int whole_part = 0;
//     int tenths_part = 0;
//     int new_value = 0;

//     whole_part = 0;
//     tenths_part = 0;

//     sscanf(value_string, ".%d", &tenths_part);  
//     sscanf(value_string, "%d.%d", &whole_part, &tenths_part);  

//     while (tenths_part > 10)
//     {
//         if ((tenths_part>10) && (tenths_part<100)) tenths_part += 5;  // round up
//         tenths_part /= 10;
//     }

//     CLIP(tenths_part, 0, 9);

//     new_value = whole_part*10 + tenths_part;   

//     return(new_value);
// }

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_schedule_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    // Check if a request for SCHEDULE has been made (/schedule.cgi?schedule=x)
    if (strcmp(pcParam[0] , "schedule") == 0)
    {
        // Look at the argument to check if schedule is to be turned on (x=1) or off (x=0)
        if(strcmp(pcValue[0], "0") == 0)
            config.irrigation_enable = 0;
        else if(strcmp(pcValue[0], "1") == 0)
            config.irrigation_enable = 1;
    }
    
    // Send the index page back to the user
    config_changed();
    return "/index.shtml";
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_weekday_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{   
    CLIP(iIndex, 1, 7);

    //toggle the state (assumes index 1-7 used in cgi_handlers[] for weekdays)
    config.day_schedule_enable[iIndex-1] = !config.day_schedule_enable[iIndex-1];

    config_changed();

    // Send the current page back to the user
    return(current_calendar_web_page);
}


/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_inc_duration_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    
    if (iNumParams == 1)
    {
        if (pcParam[0]!= NULL)
        {
            if (pcParam[0][0] == 'x')
            {
                i = pcValue[0][0] - '0';
                //printf("INDEX = %d\n", i);
            }
        }
    }

    CLIP(i, 0, 6);

    config.zone_duration[0][i]++;

    // Send the next page back to the user
    config_changed();
    return(current_calendar_web_page);
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_dec_duration_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    
    if (iNumParams == 1)
    {
        if (pcParam[0]!= NULL)
        {
            if (pcParam[0][0] == 'x')
            {
                i = pcValue[0][0] - '0';
                //printf("INDEX = %d\n", i);
            }
        }
    }

    CLIP(i, 0, 6);

    //toggle the state (assumes index 1-7 used in cgi_handlers[] for weekdays)
    if(config.zone_duration[0][i] > 0)
    {
        config.zone_duration[0][i]--;
    }

    // Send the next page back to the user
    config_changed();
    return(current_calendar_web_page);
}


/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_inc_hour_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    
    if (iNumParams == 1)
    {
        if (pcParam[0]!= NULL)
        {
            if (pcParam[0][0] == 'x')
            {
                i = pcValue[0][0] - '0';
                //printf("INDEX = %d\n", i);
            }
        }
    }

    CLIP(i, 0, 6);

    // add 60 minutes
    config.day_start[i] += 60;

    // wrap around at 24 hours
    if (config.day_start[i] >= (24*60))
    {
        config.day_start[i] -= (24*60);
    }

    // Send the next page back to the user
    config_changed();
    return(current_calendar_web_page);
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_dec_hour_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    
    if (iNumParams == 1)
    {
        if (pcParam[0]!= NULL)
        {
            if (pcParam[0][0] == 'x')
            {
                i = pcValue[0][0] - '0';
                //printf("INDEX = %d\n", i);
            }
        }
    }

    CLIP(i, 0, 6);

    // add 60 minutes
    config.day_start[i] -= 60;

    // wrap around at 0 hours
    if (config.day_start[i] < 0)
    {
        config.day_start[i] += (24*60);
    }

    // Send the next page back to the user
    config_changed();
    return(current_calendar_web_page);
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_inc_minute_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    int hour;
    int minute;
    
    if (iNumParams == 1)
    {
        if (pcParam[0]!= NULL)
        {
            if (pcParam[0][0] == 'x')
            {
                i = pcValue[0][0] - '0';
                //printf("INDEX = %d\n", i);
            }
        }
    }

    CLIP(i, 0, 6);

    // extract original hour and minute
    hour = config.day_start[i]/60;
    minute = config.day_start[i]%60;

    // add 1 minute
    minute++;

    // wrap at 60 minutes
    if (minute >= 60)
    {
        minute -=60;
    }

    // set adjusted minute, while retaining original hour
    config.day_start[i] = hour*60 + minute;

    // Send the next page back to the user
    config_changed();
    return(current_calendar_web_page);
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_dec_minute_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    int hour;
    int minute;    
    
    if (iNumParams == 1)
    {
        if (pcParam[0]!= NULL)
        {
            if (pcParam[0][0] == 'x')
            {
                i = pcValue[0][0] - '0';
                //printf("INDEX = %d\n", i);
            }
        }
    }

    CLIP(i, 0, 6);

    // extract original hour and minute
    hour = config.day_start[i]/60;
    minute = config.day_start[i]%60;

    // subtract 1 minute
    minute--;

    // wrap at 0 minutes
    if (minute < 0)
    {
        minute +=60;
    }

    // set adjusted minute, while retaining original hour
    config.day_start[i] = hour*60 + minute;

    // Send the next page back to the user
    config_changed();
    return(current_calendar_web_page);
}


/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_time_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    int hour = 0;
    int minute = 0;
    char *param = NULL;
    char *value = NULL;
    int new_value = 0;
       
    //dump_parameters(iIndex, iNumParams, pcParam, pcValue);

    //force daylight saving off -- I really hate that this is how it works!  We only get passed the parameter when checkbox is "on"
    config.daylightsaving_enable = 0;

    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            //printf("Parameter: %s has Value: %s\n", param, value);

            if (strcasecmp("tz", param) == 0)
            {               
                //skip escaped '+' if present  (%2B)  NB: won't work if lowercase B used
                if ((value[0] == '%') && (value[1] == '2') && (value[2] == 'B')) value += 3;

                sscanf(value, "%d", &hour);
                sscanf(value, "%d%%3A%d", &hour, &minute);

                if ((hour > -14) && (hour < 14))
                {
                    if (hour >= 0)
                    {
                        // add the minutes
                        new_value = hour*60 + minute;
                    }
                    else
                    {
                        //subtract the minutes
                        new_value = hour*60 - minute;
                    }

                    config.timezone_offset = new_value;
                }
            }

            if (strcasecmp("dsstart", param) == 0)
            {
                sanitize_daylight_saving_date(value, config.daylightsaving_start, sizeof(config.daylightsaving_start));

            }

            if (strcasecmp("dsend", param) == 0)
            {
                sanitize_daylight_saving_date(value, config.daylightsaving_end, sizeof(config.daylightsaving_end));
            }

            if (strcasecmp("ts1", param) == 0)
            {
                STRNCPY(config.time_server[0], value, sizeof(config.time_server[0]));
            }

            if (strcasecmp("ts2", param) == 0)
            {
                STRNCPY(config.time_server[1], value, sizeof(config.time_server[1]));
            }

            if (strcasecmp("ts3", param) == 0)
            {
                STRNCPY(config.time_server[2], value, sizeof(config.time_server[2]));
            }

            if (strcasecmp("ts4", param) == 0)
            {
                STRNCPY(config.time_server[3], value, sizeof(config.time_server[3]));
            }  

            if (strcasecmp("dsenable", param) == 0)
            {
                if (value[0])
                {
                    config.daylightsaving_enable = 1;
                } 
                else
                {
                    config.daylightsaving_enable = 0;
                }                              
            }
        }

        i++;
    }


    // Send the next page back to the user
    config_changed();
    return "/time.shtml";
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_ecowitt_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    char *param = NULL;
    char *value = NULL; 

    // despicable but necessary as we only receive parameter when checked
    config.weather_station_enable = 0;
       
    //dump_parameters(iIndex, iNumParams, pcParam, pcValue);
 
    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            //printf("Parameter: %s has Value: %s\n", param, value);

            if (strcasecmp("wse", param) == 0)
            {
                if (value[0])
                {
                    config.weather_station_enable = 1;
                } 
                else
                {
                    config.weather_station_enable = 0;  // should never happen
                }                             
            } 

            if (strcasecmp("ecoip", param) == 0)
            {
                STRNCPY(config.weather_station_ip, value, sizeof(config.weather_station_ip));
            }

            if (strcasecmp("wkrn", param) == 0)
            {
                config.rain_week_threshold = get_int_with_tenths_from_string(value);  
            }

            if (strcasecmp("dyrn", param) == 0)
            {
                config.rain_day_threshold = get_int_with_tenths_from_string(value); 
            }

            if (strcasecmp("soilt1", param) == 0)
            {
                sscanf(value, "%d", &config.soil_moisture_threshold[0]); 
            }                                     
    
            if (strcasecmp("wndt", param) == 0)
            {
                config.wind_threshold = get_int_with_tenths_from_string(value);                
            }     

            if (strcasecmp("tempth", param) == 0)
            {
                config.outside_temperature_threshold= get_int_with_tenths_from_string(value);                
            }               
        }

        i++;
    }

    // Send the next page back to the user
    config_changed();
    return "/weather.shtml";
}


/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_network_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    //int whole_part = 0;
    //int tenths_part = 0;
    char *param = NULL;
    char *value = NULL;
    //int new_value = 0;
       
    //dump_parameters(iIndex, iNumParams, pcParam, pcValue);

    //force dhcp_enable off -- I really hate that this is how it works!  We only get passed the parameter when checkbox is "on"
    config.dhcp_enable = 0;

    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            //printf("Parameter: %s has Value: %s\n", param, value);

            
            if (strcasecmp("ssid", param) == 0)
            {
                STRNCPY(config.wifi_ssid, value, sizeof(config.wifi_ssid));
            }

            if (strcasecmp("wpass", param) == 0)
            {
                if (strcasecmp(value, "********") != 0)
                {
                    STRNCPY(config.wifi_password, value, sizeof(config.wifi_password));
                }
            }        

            if (strcasecmp("ipad", param) == 0)
            {

                if (strncasecmp(value, "automatic+via+DHCP", sizeof(config.ip_address))!=0)
                {
                    STRNCPY(config.ip_address, value, sizeof(config.ip_address));
                    if (!config.dhcp_enable)
                    {
                        STRNCPY(web.ip_address_string, value, sizeof(web.ip_address_string));
                    }                    
                }

            }

            if (strcasecmp("nmsk", param) == 0)
            {

                if (strncasecmp(value, "automatic+via+DHCP", sizeof(config.network_mask))!=0)
                {
                    STRNCPY(config.network_mask, value, sizeof(config.network_mask));
                    if (!config.dhcp_enable)
                    {
                        STRNCPY(web.network_mask_string, value, sizeof(web.network_mask_string));
                    }                     
                }                   
                
            }   

            if (strcasecmp("gatewy", param) == 0)
            {

                if (strncasecmp(value, "automatic+via+DHCP", sizeof(config.gateway))!=0)
                {
                    STRNCPY(config.gateway, value, sizeof(config.gateway));
                    if (!config.dhcp_enable)
                    {
                        STRNCPY(web.gateway_string, value, sizeof(web.gateway_string));
                    }                     
                }                   
                
            } 

            if (strcasecmp("dhcp", param) == 0)
            {
                if (value[0])
                {
                    config.dhcp_enable = 1;
                } 
                else
                {
                    config.dhcp_enable = 0;
                }                             
            }

                              
                       
        }

        i++;
    }


    // Send the next page back to the user
    config_changed();
    return "/network.shtml";
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_led_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    char *param = NULL;
    char *value = NULL;

    config.use_led_strip_to_indicate_irrigation_status = 0;   
    config.led_rgbw = 0;   

    //dump_parameters(iIndex, iNumParams, pcParam, pcValue);

    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            //printf("Parameter: %s has Value: %s\n", param, value);

            
            if (strcasecmp("lpat", param) == 0)
            {
                sscanf(value, "%d", &config.led_pattern);    

                set_led_pattern_local(config.led_pattern);         
            }

            if (strcasecmp("lspd", param) == 0)
            {
                sscanf(value, "%d", &config.led_speed);

                set_led_speed_local(config.led_speed);              
            }  

            if (strcasecmp("lpin", param) == 0)
            {
                sscanf(value, "%d", &config.led_pin);             
            }

            if (strcasecmp("lrgbw", param) == 0)
            {
                if (value[0])
                {
                    config.led_rgbw = 1;
                } 
                else
                {
                    config.led_rgbw = 0;
                }                             
            }            

            if (strcasecmp("lnum", param) == 0)
            {
                sscanf(value, "%d", &config.led_number);             
            }  

            if (strcasecmp("lie", param) == 0)
            {
                if (value[0])
                {
                    config.use_led_strip_to_indicate_irrigation_status = 1;
                } 
                else
                {
                    config.use_led_strip_to_indicate_irrigation_status = 0;
                }                             
            }

            if (strcasecmp("lia", param) == 0)
            {
                sscanf(value, "%d", &config.led_pattern_when_irrigation_active);             
            }  

            if (strcasecmp("liu", param) == 0)
            {
                sscanf(value, "%d", &config.led_pattern_when_irrigation_terminated);             
            }  

            if (strcasecmp("lis", param) == 0)
            {
                sscanf(value, "%d", &config.led_sustain_duration);             
            }                          

        }

        i++;
    }


    // Send the next page back to the user
    config_changed();
    return "/addressable_led.shtml";
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_reboot_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    printf("REBOOT requested\n");
    
    //request reboot
    application_restart(REBOOT_USER_REQUEST);

    return "/index.shtml";    
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_portrait_schedule_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    //int whole_part = 0;
    //int tenths_part = 0;
    char *param = NULL;
    char *value = NULL;
    //int new_value = 0;
       
    //dump_parameters(iIndex, iNumParams, pcParam, pcValue);

    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            //printf("Parameter: %s has Value: %s\n", param, value);

            if (strcasecmp("sunday", param) == 0)
            {
                config.day_schedule_enable[0] = 1;             
            }
            
            if (strcasecmp("strt1", param) == 0)
            {
                sscanf(value, "%d", &config.day_start[0]);             
            }

            if (strcasecmp("dur1", param) == 0)
            {
                sscanf(value, "%d", &config.day_duration[0]);             
            } 

            if (strcasecmp("monday", param) == 0)
            {
                config.day_schedule_enable[1] = 1;             
            }
            
            if (strcasecmp("strt2", param) == 0)
            {
                sscanf(value, "%d", &config.day_start[1]);             
            }

            if (strcasecmp("dur2", param) == 0)
            {
                sscanf(value, "%d", &config.day_duration[1]);             
            }              
        }

        i++;
    }


    // Send the next page back to the user
    config_changed();
    return "/portrait.shtml";
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_day_schedule_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    //int whole_part = 0;
    //int tenths_part = 0;
    char *param = NULL;
    char *value = NULL;
    //int new_value = 0;
    int day = -1;
    int dur_zone = -1;
    int dur_day = -1;
    bool checked = false;
    int start_hour = -1;
    int start_minute = -1;
    int duration = -1;
    static char weekday_page[32] = "/index.shtml";
           
    //dump_parameters(iIndex, iNumParams, pcParam, pcValue);

    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            //printf("Parameter: %s has Value: %s\n", param, value);
            
            if (strcasecmp("day", param) == 0) 
            {
                sscanf(value, "%d", &day);   

                CLIP(day, 0, 6);
                sprintf(weekday_page, "/%s.shtml", day_name(day));    
                weekday_page[1] = tolower(weekday_page[1]);      
            }

            if (strcasecmp("currentday", param) == 0)
            {
                if (strcasecmp(value, "ON") == 0)
                {
                    checked = true;
                }         
            }
            
            if (strncasecmp("strt", param, 4) == 0)
            {
                sscanf(value, "%d%%3A%d", &start_hour, &start_minute); 
                sscanf(value, "%d+%%3A+%d", &start_hour, &start_minute);       

                CLIP(start_hour, 0, 23);
                CLIP(start_minute, 0, 59);      
            }

            if (strncasecmp("dur", param, 3) == 0)
            {
                sscanf(value, "%d", &duration);             
            } 

            sscanf(param, "z%dd%dd", &dur_zone, &dur_day);
            if ((dur_zone >= 1) && (dur_zone <= 8) && (dur_day >= 1) && (dur_day <= 7))
            {
                // adjust to zero base
                dur_zone--;
                dur_day--;

                sscanf(value, "%d", &config.zone_duration[dur_zone][dur_day]);  
            }             
        }

        i++;
    }

    if ((day >=0) && (day <=6))
    {
        //printf("got valid day = %d storing new schedule parameters\n", day);

        config.day_schedule_enable[day] = checked;
        if ((start_hour != -1) && (start_minute != -1)) config.day_start[day] = start_hour*60 + start_minute;
        if (duration != -1) config.day_duration[day] = duration;
    }
    else
    {
        sprintf(weekday_page, "/index.shtml"); 
    }


    config_changed();


    // Send the current page back to the user
    //printf("Redirecting browser to : %s\n", weekday_page);
    return(weekday_page);

    //return "/index.shtml";
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_mood_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    //int whole_part = 0;
    //int tenths_part = 0;
    char *param = NULL;
    char *value = NULL;
    //int new_value = 0;
    int red = -1;
    int green = -1;
    int blue = -1;
       
    //dump_parameters(iIndex, iNumParams, pcParam, pcValue);

    config.use_govee_to_indicate_irrigation_status = 0;

    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            //printf("Parameter: %s has Value: %s\n", param, value);

            
            if (strcasecmp("gvea", param) == 0)
            {
                STRNCPY(config.govee_light_ip, value, sizeof(config.govee_light_ip));
            }  

            if (strcasecmp("gvee", param) == 0)
            {
                if (value[0])
                {
                    config.use_govee_to_indicate_irrigation_status = 1;
                } 
                else
                {
                    config.use_govee_to_indicate_irrigation_status = 0;
                }                             
            } 

            if (strcasecmp("gvei", param) == 0)
            {
                sscanf(value, "%d.%d.%d", &red, &green, &blue); 
                sscanf(value, "%d+.+%d+.+%d", &red, &green, &blue);   

                if ((red != -1) && (green != -1) && (blue != -1))
                {
                    config.govee_irrigation_active_red = CLIP(red, 0, 255);
                    config.govee_irrigation_active_green = CLIP(green, 0, 255);
                    config.govee_irrigation_active_blue = CLIP(blue, 0, 255);                                         
                }          
            }

            if (strcasecmp("gveu", param) == 0)
            {
                sscanf(value, "%d.%d.%d", &red, &green, &blue); 
                sscanf(value, "%d+.+%d+.+%d", &red, &green, &blue);   

                if ((red != -1) && (green != -1) && (blue != -1))
                {
                    config.govee_irrigation_usurped_red = CLIP(red, 0, 255);
                    config.govee_irrigation_usurped_green = CLIP(green, 0, 255);
                    config.govee_irrigation_usurped_blue = CLIP(blue, 0, 255);                                         
                }          
            }            

            if (strcasecmp("gves", param) == 0)
            {
                sscanf(value, "%d", &config.govee_sustain_duration);             
            }   

        }

        i++;
    }


    // Send the next page back to the user
    config_changed();
    return "/moodlight.shtml";
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_syslog_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    char *param = NULL;
    char *value = NULL;

       
    // vile design caused by web browser not sending unchecked parameters, they must be presumed unchecked
    config.syslog_enable = 0;       

    //dump_parameters(iIndex, iNumParams, pcParam, pcValue);

    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            //printf("Parameter: %s has Value: %s\n", param, value);
            if (strcasecmp("sloge", param) == 0)
            {
                if (value[0])
                {
                    config.syslog_enable = 1;
                } 
                else
                {
                    config.syslog_enable = 0; // unfortunately will never occur, hence unconditionally forced to zero at start of function 
                }                              
            }
            
            if (strcasecmp("slog", param) == 0)
            {
                STRNCPY(config.syslog_server_ip, value, sizeof(config.syslog_server_ip));
            }  
        }

        i++;
    }


    // Send the next page back to the user
    config_changed();
    return "/syslog.shtml";
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_units_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    char *param = NULL;
    char *value = NULL;
    int new_use_archaic_units = 0;   

    // set off by default
    config.use_simplified_english  = 0; 
    config.use_monday_as_week_start = 0; 

    //dump_parameters(iIndex, iNumParams, pcParam, pcValue);
 
    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {

            if (strcasecmp("uau", param) == 0)
            {
                if (value[0])
                {
                    new_use_archaic_units = 1;
                } 
                else
                {
                    new_use_archaic_units = 0;  // this should never happen, since the parameter is only passed if "on"
                }   
            }   

            if (strcasecmp("simpe", param) == 0)
            {
                if (value[0])
                {
                    config.use_simplified_english = 1;
                } 
                else
                {
                    config.use_simplified_english  = 0;  // this should never happen, since the parameter is only passed if "on"
                }   
            }   

            if (strcasecmp("mweek", param) == 0)
            {
                if (value[0])
                {
                    config.use_monday_as_week_start = 1;
                } 
                else
                {
                    config.use_monday_as_week_start = 0;  // this should never happen, since the parameter is only passed if "on"
                } 
            }                                                                   
        }

        i++;
    }

    // set the default week long calendar page
    set_calendar_html_page();  

    // check for change in units
    if (new_use_archaic_units != config.use_archaic_units)
    {
        config.use_archaic_units = new_use_archaic_units;

        switch (new_use_archaic_units)
        {
            case false:  // convert from archaic units to SI
                config.wind_threshold = (1000*config.wind_threshold + 1641)/3281;
                config.rain_week_threshold = (254*config.rain_week_threshold + 5)/10;
                config.rain_day_threshold = (254*config.rain_day_threshold + 5)/10;
                config.outside_temperature_threshold = ((config.outside_temperature_threshold*9)/5) + 320;

            break;
            case true:   // convert from SI to archaic units
                config.wind_threshold = (config.wind_threshold*3281 + 500)/1000;
                config.rain_week_threshold = (10*config.rain_week_threshold + 127)/254;
                config.rain_day_threshold = (10*config.rain_day_threshold + 127)/254; 
                config.outside_temperature_threshold = ((config.outside_temperature_threshold - 320)*5)/9;                   
                break;
            default:
            break;
        }
#ifdef INCORPORATE_THERMOSTAT
        // convert thermostat scheduled temperatures
        sanatize_schedule_temperatures();
        make_schedule_grid();
#endif
    }     


    // Send the next page back to the user
    config_changed();
    return "/units.shtml";
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_software_load_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    char *param = NULL;
    char *value = NULL;

    //dump_parameters(iIndex, iNumParams, pcParam, pcValue);

    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            //printf("Parameter: %s has Value: %s\n", param, value);

            
            if (strcasecmp("swlhst", param) == 0)
            {
                STRNCPY(web.software_server, value, sizeof(web.software_server));
            }  
            if (strcasecmp("swlurl", param) == 0)
            {
                STRNCPY(web.software_url, value, sizeof(web.software_url));
            }  
            if (strcasecmp("swlfle", param) == 0)
            {
                STRNCPY(web.software_file, value, sizeof(web.software_file));
            }                          
        }

        i++;
    }


    // Send the next page back to the user
    config_changed();
    return "/software_load.shtml";
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_remote_led_strips(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    char *param = NULL;
    char *value = NULL;
       
    //dump_parameters(iIndex, iNumParams, pcParam, pcValue);

    // set off by default
    config.led_strip_remote_enable  = 0; 

    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            //printf("Parameter: %s has Value: %s\n", param, value);

            if (strcasecmp("rsadr1", param) == 0)
            {
                STRNCPY(config.led_strip_remote_ip[0], value, sizeof(config.led_strip_remote_ip[0]));
            }
            if (strcasecmp("rsadr2", param) == 0)
            {
                STRNCPY(config.led_strip_remote_ip[1], value, sizeof(config.led_strip_remote_ip[1]));
            }
            if (strcasecmp("rsadr3", param) == 0)
            {
                STRNCPY(config.led_strip_remote_ip[2], value, sizeof(config.led_strip_remote_ip[2]));
            }
            if (strcasecmp("rsadr4", param) == 0)
            {
                STRNCPY(config.led_strip_remote_ip[3], value, sizeof(config.led_strip_remote_ip[3]));
            }
            if (strcasecmp("rsadr5", param) == 0)
            {
                STRNCPY(config.led_strip_remote_ip[4], value, sizeof(config.led_strip_remote_ip[4]));
            }
            if (strcasecmp("rsadr6", param) == 0)
            {
                STRNCPY(config.led_strip_remote_ip[5], value, sizeof(config.led_strip_remote_ip[5]));
            }
            
            if (strcasecmp("rse", param) == 0)
            {
                if (value[0])
                {
                    config.led_strip_remote_enable = 1;
                } 
                else
                {
                    config.led_strip_remote_enable = 0;  // this should never happen, since the parameter is only passed if "on"
                }   
            } 
        }

        i++;
    }

    // Send the next page back to the user
    config_changed();
    return "/remote_led_strips.shtml";
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_personality_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    char *param = NULL;
    char *value = NULL;
    PERSONALITY_E new_personality = NO_PERSONALITY;
       
    //dump_parameters(iIndex, iNumParams, pcParam, pcValue);

    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            //printf("Parameter: %s has Value: %s\n", param, value);

            if (strcasecmp("pertyp", param) == 0)
            {               
                sscanf(value, "%d", (int *)&new_personality);

                switch(new_personality)
                {
                    case SPRINKLER_USURPER:
                        config.personality = new_personality;
                        config.zone_max = 1;
                        set_calendar_html_page();
                        break;

                    case SPRINKLER_CONTROLLER:
                        if (config.personality != new_personality)
                        {
                            // default gpio for waveshare industrial relay module
                            config.zone_gpio[0] = 21;
                            config.zone_gpio[1] = 20;
                            config.zone_gpio[2] = 19;
                            config.zone_gpio[3] = 18;
                            config.zone_gpio[4] = 17;
                            config.zone_gpio[5] = 16;
                            config.zone_gpio[6] = 15;
                            config.zone_gpio[7] = 14;                            
                        }
                        config.personality = new_personality;
                        config.zone_max = 8;
                        set_calendar_html_page();
                        break;

                    case LED_STRIP_CONTROLLER:
                        config.personality = new_personality;
                        set_calendar_html_page();
                        break;

                    case HVAC_THERMOSTAT:
                        config.personality = new_personality;
                        break;    

                    case HOME_CONTROLLER:
                        config.personality = new_personality;
                        break;                                              
                    
                    default:
                        printf("Invalid personality\n");
                        break;
                } 
                config_changed();              
            }
        }  
        i++;
    }


    // Send the next page back to the user
    return "/personality.shtml";
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_relay_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    char *param = NULL;
    char *value = NULL;
    int new_relay_normally_open = 0; 
    int new_irrigation_test_enable = 0;      
    int new_gpio = 0;
    int gpio_zone = -1;  
    int new_zone_max = 0;
       
    //dump_parameters(iIndex, iNumParams, pcParam, pcValue);
 
    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            //printf("Parameter: %s has Value: %s\n", param, value);  

            if (strcasecmp("rly", param) == 0)
            {
                if (value[0])
                {
                    new_relay_normally_open = 1;
                } 
                else
                {
                    new_relay_normally_open = 0;  // this should never happen, since the parameter is only passed if "on"
                }   
            }

            if (strcasecmp("gpio", param) == 0)
            {
                new_gpio = atoi(value);
                if (!initialize_relay_gpio(new_gpio))
                {
                    gpio_put(new_gpio, config.relay_normally_open?0:1); 
                    config.gpio_number = new_gpio; 
                }                                
            } 

            sscanf(param, "z%dgpio", &gpio_zone);
            if ((gpio_zone >= 1) && (gpio_zone < 8))
            {
                // adjust to zero base
                gpio_zone--;

                sscanf(value, "%d", &config.zone_gpio[gpio_zone]);  
            }   

            if (strcasecmp("zmax", param) == 0)
            {
                sscanf(value, "%d", &new_zone_max);
                
                if ((new_zone_max > 0) && (new_zone_max <= 8))
                {
                    config.zone_max = new_zone_max;
                }                           
            }

            if (strcasecmp("irgnow", param) == 0)
            {
                if (value[0])
                {
                    new_irrigation_test_enable = 1;
                } 
                else
                {
                    new_irrigation_test_enable = 0;  // this should never happen, since the parameter is only passed if "on"
                }   

                if (value[0] && !web.irrigation_test_enable)
                {
                    web.irrigation_test_enable = 1;
                    snprintf(web.status_message, sizeof(web.status_message), "Preparing for irrigation test");
                } 
            }
        }
        i++;
    }

    // handle normally open checkbox
    if (config.relay_normally_open != new_relay_normally_open)
    {
        config.relay_normally_open = new_relay_normally_open;
    }

    // handle irrigation test checkbox
    if (web.irrigation_test_enable != new_irrigation_test_enable)
    {
        web.irrigation_test_enable = new_irrigation_test_enable;

        if (web.irrigation_test_enable == 1)
        {
           snprintf(web.status_message, sizeof(web.status_message), "Preparing for irrigation test"); 
        }
        else
        {
           snprintf(web.status_message, sizeof(web.status_message), "Irrigation test terminated");  
        }
    }    

    // normally open must be used in controller mode
    if (config.personality == SPRINKLER_CONTROLLER)
    {
        config.relay_normally_open = 1;
    }

    config_changed();

    // Send the next page back to the user
    if (config.personality == SPRINKLER_CONTROLLER)
    {
        if (!web.irrigation_test_enable)
        {    
            return "/z_relay.shtml";
        }
        else
        {
            return "/z_relay_test.shtml";
        }
    }
    else
    {
        return "/relay.shtml";
    }
}


/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_wificountry_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    char *param = NULL;
    char *value = NULL;
       
    //dump_parameters(iIndex, iNumParams, pcParam, pcValue);

    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            //printf("Parameter: %s has Value: %s\n", param, value);

            if (strcasecmp("wific", param) == 0)
            {
                STRNCPY(config.wifi_country, value, sizeof(config.wifi_country));
                deplus_string(config.wifi_country, sizeof(config.wifi_country));             
            }                                                         
        }

        i++;
    }


    // Send the next page back to the user
    config_changed();
    return "/network.shtml";
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_relay_test_stop_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    //TODO: proper intertask communication
    snprintf(web.status_message, sizeof(web.status_message), "Irrigation test terminated");  
    web.irrigation_test_enable = 0;
    set_irrigation_relay_test_zone(-1);    
    test_end_redirect = false;
    xTaskNotifyGiveIndexed(worker_tasks[0].task_handle, 0);
               
    // Send the next page back to the user
    if (config.personality == SPRINKLER_CONTROLLER)
    {
        if (!web.irrigation_test_enable)
        {    
            return "/index.shtml";
        }
        else
        {
            return "/z_relay_test.shtml";
        }
    }
    else
    {
        return "/index.shtml";
    }
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_relay_test_start_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int zone = -1;
    bool start_test = false;
    char *next_page_url = "/index.shtml";
    static int last_zone = -1;

    if (iNumParams == 1)
    {
        // get zone parameter sent by browser
        if (pcParam[0]!= NULL)
        {
            if (pcParam[0][0] == 'x')
            {
                zone = pcValue[0][0] - '0';
                CLIP(zone, 0, config.zone_max);
            }
        }

        // check if we have a valid zone
        if ((zone >=0) && (zone <config.zone_max))
        {
            // check if test in progress
            if (web.irrigation_test_enable)
            {
                // check if test zone altered
                if (zone != get_irrigation_relay_test_zone())
                {
                    snprintf(web.status_message, sizeof(web.status_message), "Changing irrigation test to Zone %d", zone+1);
                    start_test = true; 
                }
                else
                {
                    // presume this is a browser page refresh during running test
                    next_page_url = "/z_relay_test.shtml";
                }
            } 
            else 
            {
                // check if a test just ended
                if (test_end_redirect && (zone == last_zone))
                {
                    // redirect browser to main page to avoid refresh restarting the test
                    test_end_redirect = false;
                    printf("Redirecting to index due to test end.  zone = %d get_zone = %d\n", zone, get_irrigation_relay_test_zone());
                }
                else
                {
                    snprintf(web.status_message, sizeof(web.status_message), "Starting irrigation test for Zone %d", zone+1); 
                    start_test = true; 
                }
            }

            // initiate irrigation test
            if (start_test)
            {
                printf("%s\n", web.status_message);        
                set_irrigation_relay_test_zone(zone);
                web.irrigation_test_enable = 1;
                test_end_redirect = true;
                last_zone = zone;

                xTaskNotifyGiveIndexed(worker_tasks[0].task_handle, 0);

                next_page_url = "/z_relay_test.shtml";
            }
        }
    }

    return(next_page_url);
}


/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_led_pattern_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    char *param = NULL;
    char *value = NULL;
    int new_pattern = -1;
    static int pattern_type = -1;
    bool pattern_set = false;
       
    //dump_parameters(iIndex, iNumParams, pcParam, pcValue);

    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            printf("Parameter: %s has Value: %s\n", param, value);

            if (strcasecmp("ledp", param) == 0)
            {
                new_pattern = pcValue[0][0] - '0'; 
                CLIP(new_pattern, 0, 7);           
            } 

            if (strcasecmp("p", param) == 0)
            {
                pattern_type = pcValue[0][0] - '0'; 
                CLIP(pattern_type, -1, 3);           
            }                                                                   
        }

        i++;
    }

    if ((new_pattern >= 0) && (new_pattern < 8))
    {
        printf("got a valid new pattern\n");

        switch(pattern_type)
        {
        case 0:
            printf("acitve pattern = %d\n", new_pattern);
            config.led_pattern_when_irrigation_active = new_pattern;
            pattern_set = true;
            break;
        case 1:
            printf("skipped pattern = %d\n", new_pattern);
            config.led_pattern_when_irrigation_terminated = new_pattern;
            pattern_set = true;            
            break;
        case 2:
            printf("default pattern = %d\n", new_pattern);
            config.led_pattern = new_pattern;
            pattern_set = true;            
            break;
        default:
            printf("pattern type not established\n");
            break;
        }        
    }

    if (pattern_set)
    {
        pattern_type = -1;
        config_changed();
        return "/addressable_led.shtml";
    }
    else
    {
        return "/led_pattern.shtml";
    }
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_led_strip_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    char *param = NULL;
    char *value = NULL;


    config.led_rgbw = 0;       

    //dump_parameters(iIndex, iNumParams, pcParam, pcValue);

    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            //printf("Parameter: %s has Value: %s\n", param, value); 

            if (strcasecmp("lpin", param) == 0)
            {
                sscanf(value, "%d", &config.led_pin);             
            }

            if (strcasecmp("lrgbw", param) == 0)
            {
                if (value[0])
                {
                    config.led_rgbw = 1;
                } 
                else
                {
                    config.led_rgbw = 0;
                }                             
            }   

            if (strcasecmp("lnum", param) == 0)
            {
                sscanf(value, "%d", &config.led_number);             
            }                         
        }

        i++;
    }


    // Send the next page back to the user
    config_changed();
    return "/led_strip.shtml";
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_setpoints_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    char *param = NULL;
    char *value = NULL;
    int new_relay_normally_open = 0; 
    int new_irrigation_test_enable = 0;      
    int new_gpio = 0;
    int setpoint_number = -1;  
    int setpoint_index = -1;
    int new_zone_max = 0;
    int len = 0;
       
    dump_parameters(iIndex, iNumParams, pcParam, pcValue);
 
    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            printf("Parameter: %s has Value: %s\n", param, value);  

            // len = strlen(param);
            // if ((len > 5) && (param[len-1] == 'e') && (param[len-2] == 'm') && (param[len-3] == 'n'))
            // {
            //     setpoint_number = -1;
            //     sscanf(param, "sp%dnme", &setpoint_number);
            //     if ((setpoint_number >= 1) && (setpoint_number <= 12))
            //     {
            //         // adjust to zero base
            //         setpoint_number--;

            //         sscanf(value, "%s", &(config.setpoint_name[setpoint_number]));  
            //         printf("after scanf spn[%d] = %s\n", setpoint_number, config.setpoint_name[setpoint_number]);
            //     } 
            // }

            // len = strlen(param);
            // if ((len > 5) && (param[len-1] == 'p') && (param[len-2] == 'm') && (param[len-3] == 't'))
            // {            
            //     setpoint_number = -1;
            //     sscanf(param, "sp%dtmp", &setpoint_number);
            //     if ((setpoint_number >= 1) && (setpoint_number <= 12))
            //     {
            //         // adjust to zero base
            //         setpoint_number--;

            //         sscanf(value, "%d", &(config.setpoint_temperaturex10[setpoint_number]));  
            //         printf("after scanf spt[%d] = %d\n", setpoint_number, config.setpoint_temperaturex10[setpoint_number]);
            //     }
            // }             

        }
        i++;
    }

    config_changed();

    // Send the next page back to the user
    return "/ts_setpoints.shtml";
    
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_periods_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    char *param = NULL;
    char *value = NULL;
    int new_relay_normally_open = 0; 
    int new_irrigation_test_enable = 0;      
    int new_gpio = 0;
    int period_number = -1;  
    int setpoint_index = -1;
    int new_zone_max = 0;
    int len = 0;
       
    dump_parameters(iIndex, iNumParams, pcParam, pcValue);
 
    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            printf("Parameter: %s has Value: %s\n", param, value);  

            len = strlen(param);
            if ((len > 4) && (param[len-1] == 't') && (param[len-2] == 's'))
            { 
                period_number = -1;
                sscanf(param, "ts%dst", &period_number);
                if ((period_number >= 1) && (period_number <= 12))
                {
                    // adjust to zero base
                    period_number--;

                    //sscanf(value, "%s", &config.setpoint_name[period_number]);
                    config.setpoint_start_mow[period_number] = string_to_mow(value, 32);

                }
            } 

            len = strlen(param);
            if ((len > 4) && (param[len-1] == 'n') && (param[len-2] == 'e'))
            { 
                period_number = -1;
                sscanf(param, "ts%den", &period_number);
                if ((period_number >= 1) && (period_number <= 12))
                {
                    // adjust to zero base
                    period_number--;

                    //sscanf(value, "%d", &config.setpoint_temperaturex10[period_number]); 
                    //config.thermostat_period_end_mow[period_number] = string_to_mow(value, 32); 
                    printf("CGI error - thermostat_period_end not longer supported\n");
                } 
            } 
            len = strlen(param);
            if ((len > 4) && (param[len-1] == 'n') && (param[len-2] == 'i'))
            { 
                period_number = -1;
                sscanf(param, "ts%din", &period_number);
                if ((period_number >= 1) && (period_number <= 12))
                {
                    // adjust to zero base
                    period_number--;

                    //sscanf(value, "%d", &(config.thermostat_period_setpoint_index[period_number])); 
                    printf("CGI error - thermostat_period_setpoint_index not longer supported\n"); 
                }
            }    
            len = strlen(param);
            if ((len > 4) && (param[len-1] == 'p') && (param[len-2] == 'm') && (param[len-2] == 't'))
            { 
                period_number = -1;
                sscanf(param, "ts%dtmp", &period_number);
                if ((period_number >= 0) && (period_number < NUM_ROWS(config.setpoint_temperaturex10)))
                {
                    // adjust to zero base
                    period_number--;

                    sscanf(value, "%d", &(config.setpoint_temperaturex10[period_number])); 
                    config.setpoint_temperaturex10[period_number] *=10; 
                }
            }                               

        }
        i++;
    }

    config_changed();

    // Send the next page back to the user
    return "/ts_periods.shtml";
    
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_thermostat_schedule_change_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
#ifdef INCORPORATE_THERMOSTAT
    int i = 0;
    int j = 0;
    int key_mow = 0;
    int key_temp = 0;
    int key_heat_temp = 0;
    int key_cool_temp = 0;
    THERMOSTAT_MODE_T key_mode = HVAC_AUTO;
    char *param = NULL;
    char *value = NULL;
    int new_relay_normally_open = 0; 
    int new_irrigation_test_enable = 0;      
    int new_gpio = 0;
    int period_number = -1;  
    int setpoint_index = -1;
    int new_zone_max = 0;
    int len = 0;
       
    printf("Got request to change schedule\n");

    dump_parameters(iIndex, iNumParams, pcParam, pcValue);
 
    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            printf("Parameter: %s has Value: %s\n", param, value);  

            len = strlen(param);
            if ((len >= 1) && (param[0] == 'x'))
            { 
                sscanf(value, "%d", &(web.thermostat_period_row));
                CLIP(web.thermostat_period_row, 0, NUM_ROWS(config.setpoint_start_mow));  
            } 

            len = strlen(param);
            if ((len >= 4) && (param[0] == 't') && (param[1] == 'p') && (param[2] == 's') && (param[3] == 't'))
            {
                CLIP(web.thermostat_period_row, 0, NUM_ROWS(config.setpoint_start_mow)); 
                config.setpoint_start_mow[web.thermostat_period_row] = time_string_to_mow(value, 32, web.thermostat_day);
            } 

            len = strlen(param);
            if ((len >= 5) && (param[0] == 't') && (param[1] == 'p') && (param[2] == 't') && (param[3] == 'm') && (param[4] == 'p'))
            {
                CLIP(web.thermostat_period_row, 0, NUM_ROWS(config.setpoint_start_mow)); 

                if(isdigit(value[0]))
                {
                    sscanf(value, "%d", &(config.setpoint_temperaturex10[web.thermostat_period_row]));
                    config.setpoint_temperaturex10[web.thermostat_period_row] *= 10; 
                }

                if (config.setpoint_mode[web.thermostat_period_row] == HVAC_OFF)
                {
                    config.setpoint_temperaturex10[web.thermostat_period_row] = SETPOINT_TEMP_INVALID_OFF; 
                }

                if (config.setpoint_mode[web.thermostat_period_row] == HVAC_FAN_ONLY)
                {
                    config.setpoint_temperaturex10[web.thermostat_period_row] = SETPOINT_TEMP_INVALID_FAN; 
                }

            }   

            len = strlen(param);
            if ((len >= 5) && (param[0] == 't') && (param[1] == 'p') && (param[2] == 'h') && (param[3] == 't') && (param[4] == 'm') && (param[5] == 'p'))
            {
                CLIP(web.thermostat_period_row, 0, NUM_ROWS(config.setpoint_start_mow)); 

                if(isdigit(value[0]))
                {
                    sscanf(value, "%d", &(config.setpoint_heating_temperaturex10[web.thermostat_period_row]));
                    config.setpoint_heating_temperaturex10[web.thermostat_period_row] *= 10; 
                }

                if (config.setpoint_mode[web.thermostat_period_row] == HVAC_OFF)
                {
                    config.setpoint_heating_temperaturex10[web.thermostat_period_row] = SETPOINT_TEMP_INVALID_OFF; 
                }

                if (config.setpoint_mode[web.thermostat_period_row] == HVAC_FAN_ONLY)
                {
                    config.setpoint_heating_temperaturex10[web.thermostat_period_row] = SETPOINT_TEMP_INVALID_FAN; 
                }

            }  
            
            len = strlen(param);
            if ((len >= 5) && (param[0] == 't') && (param[1] == 'p') && (param[2] == 'c') && (param[3] == 't') && (param[4] == 'm') && (param[5] == 'p'))
            {
                CLIP(web.thermostat_period_row, 0, NUM_ROWS(config.setpoint_start_mow)); 

                if(isdigit(value[0]))
                {
                    sscanf(value, "%d", &(config.setpoint_cooling_temperaturex10[web.thermostat_period_row]));
                    config.setpoint_cooling_temperaturex10[web.thermostat_period_row] *= 10; 
                }

                if (config.setpoint_mode[web.thermostat_period_row] == HVAC_OFF)
                {
                    config.setpoint_cooling_temperaturex10[web.thermostat_period_row] = SETPOINT_TEMP_INVALID_OFF; 
                }

                if (config.setpoint_mode[web.thermostat_period_row] == HVAC_FAN_ONLY)
                {
                    config.setpoint_cooling_temperaturex10[web.thermostat_period_row] = SETPOINT_TEMP_INVALID_FAN; 
                }
            }              

            len = strlen(param);
            if ((len >= 4) && (param[0] == 't') && (param[1] == 'p') && (param[2] == 's') && (param[3] == 'm'))
            {
                CLIP(web.thermostat_period_row, 0, NUM_ROWS(config.setpoint_mode)); 
                sscanf(value, "%d", &(config.setpoint_mode[web.thermostat_period_row]));
                CLIP(config.setpoint_mode[web.thermostat_period_row], 0, NUM_HVAC_MODES-1);

                if (config.setpoint_mode[web.thermostat_period_row] == HVAC_OFF)
                {

                    config.setpoint_temperaturex10[web.thermostat_period_row] = SETPOINT_TEMP_INVALID_OFF; 
                }
                else
                {
                    switch(config.setpoint_temperaturex10[web.thermostat_period_row])
                    {
                    case SETPOINT_TEMP_INVALID_FAN:
                    case SETPOINT_TEMP_INVALID_OFF:
                    case SETPOINT_TEMP_UNDEFINED:
                        if (config.use_archaic_units)
                        {
                            config.setpoint_temperaturex10[web.thermostat_period_row] = SETPOINT_TEMP_DEFAULT_F;
                        }
                        else
                        {
                            config.setpoint_temperaturex10[web.thermostat_period_row] = SETPOINT_TEMP_DEFAULT_C;
                        }
                        break;
                    default:  // reject temps below absolute zero
                        if (config.use_archaic_units)
                        {
                            if (config.setpoint_temperaturex10[web.thermostat_period_row] < 4600)
                            {
                                config.setpoint_temperaturex10[web.thermostat_period_row] = SETPOINT_TEMP_DEFAULT_F;
                            }
                        }
                        else
                        {
                            if (config.setpoint_temperaturex10[web.thermostat_period_row] < 2800)
                            {                            
                                config.setpoint_temperaturex10[web.thermostat_period_row] = SETPOINT_TEMP_DEFAULT_C;
                            }
                        }                                            
                        break;
                    }
                }
            }  

            len = strlen(param);
            if ((len >= 3) && (param[0] == 'd') && (param[1] == 'a') && (param[2] == 'y'))
            { 
                sscanf(value, "%d", &(web.thermostat_day));
                CLIP(web.thermostat_day, 0, 6);  
            } 

            len = strlen(param);
            if ((len > 4) && (param[len-1] == 't') && (param[len-2] == 's'))
            { 
                period_number = -1;
                sscanf(param, "ts%dst", &period_number);
                if ((period_number >= 1) && (period_number <= NUM_ROWS(config.setpoint_start_mow)))
                {
                    // adjust to zero base
                    period_number--;

                    //sscanf(value, "%s", &config.setpoint_name[period_number]);
                    config.setpoint_start_mow[period_number] = string_to_mow(value, 32);

                }
            } 

            // len = strlen(param);
            // if ((len > 4) && (param[len-1] == 'n') && (param[len-2] == 'i'))
            // { 
            //     period_number = -1;
            //     sscanf(param, "ts%din", &period_number);
            //     if ((period_number >= 1) && (period_number <= NUM_ROWS(config.thermostat_period_setpoint_index)))
            //     {
            //         // adjust to zero base
            //         period_number--;

            //         sscanf(value, "%d", &(config.thermostat_period_setpoint_index[period_number]));  
            //     }
            // } 

            // len = strlen(param);
            // if ((len > 5) && (param[len-1] == 'p') && (param[len-2] == 'm') && (param[len-2] == 't'))
            // { 
            //     period_number = -1;
            //     sscanf(param, "ts%dtmp", &period_number);
            //     if ((period_number >= 0) && (period_number < NUM_ROWS(config.thermostat_period_setpoint_index)))
            //     {
            //         sscanf(value, "%d", &(config.thermostat_period_setpoint_index[period_number]));  
            //     }
            // }             


        }
        i++;
    }

    // check for duplicates and remove
    CLIP(web.thermostat_period_row, 0, NUM_ROWS(config.setpoint_start_mow));
    for(i=0; i<NUM_ROWS(config.setpoint_start_mow); i++)
    {
        if ((i != web.thermostat_period_row) &&
            (config.setpoint_start_mow[i] >= 0) &&
            (config.setpoint_start_mow[i] == config.setpoint_start_mow[web.thermostat_period_row]))
        {
            printf("Duplicate thermostat period deleted\n");
            config.setpoint_start_mow[i] = -1;
        }
    }    

    // sort the schedule into ascending order by mow
    for(i=1; i<NUM_ROWS(config.setpoint_start_mow); i++)
    {
        key_mow = config.setpoint_start_mow[i];
        key_temp = config.setpoint_temperaturex10[i];  
        key_heat_temp = config.setpoint_heating_temperaturex10[i]; 
        key_cool_temp = config.setpoint_cooling_temperaturex10[i];                  
        key_mode = config.setpoint_mode[i];

        j = i - 1;

        while ((j >= 0) && (config.setpoint_start_mow[j] > key_mow))
        {
            config.setpoint_start_mow[j+1] = config.setpoint_start_mow[j];
            config.setpoint_temperaturex10[j+1] = config.setpoint_temperaturex10[j]; 
            config.setpoint_heating_temperaturex10[j+1] = config.setpoint_heating_temperaturex10[j]; 
            config.setpoint_cooling_temperaturex10[j+1] = config.setpoint_cooling_temperaturex10[j];                         
            config.setpoint_mode[j+1] = config.setpoint_mode[j];            
            j = j - 1;
        }

        config.setpoint_start_mow[j+1] = key_mow;
        config.setpoint_temperaturex10[j+1] = key_temp; 
        config.setpoint_heating_temperaturex10[j+1] = key_heat_temp; 
        config.setpoint_cooling_temperaturex10[j+1] = key_cool_temp;                 
        config.setpoint_mode[j+1] = key_mode;             
    }

    // update the schedule grid
    make_schedule_grid();

    // write config changes to flash
    config_changed();
#endif
    // Send the next page back to the user
    return "/t_schedule.shtml";
    
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_thermostat_period_delete_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
#ifdef INCORPORATE_THERMOSTAT
    int i = 0;
    char *param = NULL;
    char *value = NULL;
    int new_relay_normally_open = 0; 
    int new_irrigation_test_enable = 0;      
    int new_gpio = 0;
    int period_number = -1;  
    int setpoint_index = -1;
    int new_zone_max = 0;
    int len = 0;
       


    printf("Got request to delete thermostat period. row = %d\n", web.thermostat_period_row);

    dump_parameters(iIndex, iNumParams, pcParam, pcValue);
 
    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            printf("Parameter: %s has Value: %s\n", param, value);  

            len = strlen(param);
            if ((len >= 1) && (param[0] == 'x'))
            { 
                sscanf(value, "%d", &(web.thermostat_period_row));
                printf("Got request to delete thermostat period. row = %d\n", web.thermostat_period_row);
                CLIP(web.thermostat_period_row, 0, NUM_ROWS(config.setpoint_start_mow));  
                if ((web.thermostat_period_row >=0) && (web.thermostat_period_row < NUM_ROWS(config.setpoint_start_mow)))
                {
                    printf("Deleting row %d by setting mow to -1\n", web.thermostat_period_row);
                    config.setpoint_start_mow[web.thermostat_period_row] = -1;

                    // update the schedule grid
                    make_schedule_grid();

                    // write config changes to flash
                    config_changed();
                }                
            } 
        }
        i++;
    }
#endif
    // Send the next page back to the user
    return "/t_schedule.shtml";
    
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_thermostat_period_add_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    char *param = NULL;
    char *value = NULL;
    int new_relay_normally_open = 0; 
    int new_irrigation_test_enable = 0;      
    int new_gpio = 0;
    int period_number = -1;  
    int setpoint_index = -1;
    int new_zone_max = 0;
    int len = 0;
    char *next_page = "/t_schedule.shtml";
       


    printf("Got request to add thermostat period. row = %d\n", web.thermostat_period_row);

    dump_parameters(iIndex, iNumParams, pcParam, pcValue);

    for(i=0; i < NUM_ROWS(config.setpoint_start_mow); i++)
    {
        if (config.setpoint_start_mow[i] < 0)  // TODO: should we use the setpoint valid function? slower
        {
            web.thermostat_period_row = i;
            config.setpoint_start_mow[i] = web.thermostat_day*24*60;
            if (config.use_archaic_units)
            {
                config.setpoint_temperaturex10[i] = SETPOINT_TEMP_DEFAULT_F;
                config.setpoint_heating_temperaturex10[i] = SETPOINT_TEMP_DEFAULT_F;
                config.setpoint_cooling_temperaturex10[i] = SETPOINT_TEMP_DEFAULT_F;                                
            }
            else
            {
                config.setpoint_temperaturex10[i] = SETPOINT_TEMP_DEFAULT_C;
                config.setpoint_heating_temperaturex10[i] = SETPOINT_TEMP_DEFAULT_C;
                config.setpoint_cooling_temperaturex10[i] = SETPOINT_TEMP_DEFAULT_C;                                
            }
            config.setpoint_mode[i] = HVAC_AUTO;
            next_page = "/tp_edit.shtml";
            break;
        }
    }

    // Send the next page back to the user
    return(next_page);
    
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_thermostat_period_edit_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    char *param = NULL;
    char *value = NULL;
    int new_relay_normally_open = 0; 
    int new_irrigation_test_enable = 0;      
    int new_gpio = 0;
    int period_number = -1;  
    int setpoint_index = -1;
    int new_zone_max = 0;
    int len = 0;
       


    printf("Got request to edit thermostat period. row = %d\n", web.thermostat_period_row);

    dump_parameters(iIndex, iNumParams, pcParam, pcValue);
 
    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            printf("Parameter: %s has Value: %s\n", param, value);  

            len = strlen(param);
            if ((len >= 1) && (param[0] == 'x'))
            { 
                sscanf(value, "%d", &(web.thermostat_period_row));
                printf("Got request to edit thermostat period. row = %d\n", web.thermostat_period_row);
                CLIP(web.thermostat_period_row, 0, NUM_ROWS(config.setpoint_start_mow));                
            } 
        }
        i++;
    }

    // Send the next page back to the user
    return "/tp_edit.shtml";
    
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_thermostat_period_cancel_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    char *param = NULL;
    char *value = NULL;
    int new_relay_normally_open = 0; 
    int new_irrigation_test_enable = 0;      
    int new_gpio = 0;
    int period_number = -1;  
    int setpoint_index = -1;
    int new_zone_max = 0;
    int len = 0;
       


    printf("Got request to cancel editing thermostat period. row = %d\n", web.thermostat_period_row);

    dump_parameters(iIndex, iNumParams, pcParam, pcValue);
 
    // Send the next page back to the user
    return "/t_schedule.shtml";    
}


/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_thermostat_schedule_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    char *param = NULL;
    char *value = NULL;
    int new_relay_normally_open = 0; 
    int new_irrigation_test_enable = 0;      
    int new_gpio = 0;
    int period_number = -1;  
    int setpoint_index = -1;
    int new_zone_max = 0;
    int len = 0;
       


    printf("Got request to display thermostat schedule. row = %d\n", web.thermostat_period_row);

    dump_parameters(iIndex, iNumParams, pcParam, pcValue);

    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            printf("Parameter: %s has Value: %s\n", param, value);    

            len = strlen(param);
            if ((len >= 3) && (param[0] == 'd') && (param[1] == 'a') && (param[2] == 'y'))
            { 
                sscanf(value, "%d", &(web.thermostat_day));
                CLIP(web.thermostat_day, 0, 6);  
            } 
        }
        i++;
    }

 
    // Send the next page back to the user
    return "/t_schedule.shtml";    
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_powerwall_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    char *param = NULL;
    char *value = NULL; 

    // despicable but necessary as we only receive parameter when checked
    config.weather_station_enable = 0;
       
    //dump_parameters(iIndex, iNumParams, pcParam, pcValue);
 
    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            //printf("Parameter: %s has Value: %s\n", param, value);


            if (strcasecmp("pwip", param) == 0)
            {
                STRNCPY(config.powerwall_ip, value, sizeof(config.powerwall_ip));
            }

            if (strcasecmp("pwhost", param) == 0)
            {
                STRNCPY(config.powerwall_hostname, value, sizeof(config.powerwall_hostname));
            }      
            
            if (strcasecmp("pwpass", param) == 0)
            {
                if (strcasecmp(value, "********") != 0)
                {
                    STRNCPY(config.powerwall_password, value, sizeof(config.powerwall_password));
                }
            }              
            
            if (strcasecmp("pwgdhd", param) == 0)
            {
                config.grid_down_heating_setpoint_decrease = get_int_with_tenths_from_string(value);  
                printf("CGI setting grid down heating setpoint decrease to %d\n", config.grid_down_heating_setpoint_decrease);
            }

            if (strcasecmp("pwgdci", param) == 0)
            {
                config.grid_down_cooling_setpoint_increase = get_int_with_tenths_from_string(value);  
            }

            if (strcasecmp("pwblhd", param) == 0)
            {
                config.grid_down_heating_disable_battery_level = get_int_with_tenths_from_string(value);  
            }

            if (strcasecmp("pwblhe", param) == 0)
            {
                config.grid_down_heating_enable_battery_level = get_int_with_tenths_from_string(value);  
            } 
            
            if (strcasecmp("pwblcd", param) == 0)
            {
                config.grid_down_cooling_disable_battery_level = get_int_with_tenths_from_string(value);  
            }     

            if (strcasecmp("pwblce", param) == 0)
            {
                config.grid_down_cooling_enable_battery_level = get_int_with_tenths_from_string(value);  
            }                                       
        }

        i++;
    }

    // Send the next page back to the user
    config_changed();

    return "/powerwall.shtml";
}


/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_thermostat_copy_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
#ifdef INCORPORATE_THERMOSTAT
    int i = 0;
    char *param = NULL;
    char *value = NULL;
    int new_relay_normally_open = 0; 
    int new_irrigation_test_enable = 0;      
    int new_gpio = 0;
    int period_number = -1;  
    int setpoint_index = -1;
    int new_zone_max = 0;
    int len = 0;
    int copy_destination;
       


    printf("Got request to copy thermostat schedule from day %sd\n", web.thermostat_day);

    dump_parameters(iIndex, iNumParams, pcParam, pcValue);

    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            printf("Parameter: %s has Value: %s\n", param, value);    

            len = strlen(param);
            if ((len >= 3) && (param[0] == 'd') && (param[1] == 'a') && (param[2] == 'y'))
            { 
                sscanf(value, "%d", &(web.thermostat_day));
                CLIP(web.thermostat_day, 0, 6);  
            } 

            len = strlen(param);
            if (strcasecmp("tscpy", param) == 0)
            { 
                sscanf(value, "%d", &(copy_destination));
                CLIP(copy_destination, 0, 9);  

                printf("copy destination is %d\n", copy_destination);
                copy_schedule(web.thermostat_day, copy_destination);
            }             
        }
        i++;
    }

    // update the schedule grid
    make_schedule_grid();

    // write config changes to flash
    config_changed();
 
#endif
    // Send the next page back to the user
    return "/t_schedule.shtml";    
}


/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_thermostat_gpio_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    char *param = NULL;
    char *value = NULL;
    int len = 0;
    int temp = 0;
       

    dump_parameters(iIndex, iNumParams, pcParam, pcValue);

    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            printf("Parameter: %s has Value: %s\n", param, value);    

            len = strlen(param);

            if (strcasecmp("thgpio", param) == 0)
            {
                if (!strcasestr(value, "none"))
                { 
                    sscanf(value, "%d", &temp);

                    if (gpio_valid(temp))
                    {
                        config.heating_gpio = temp;
                    }
                }                
            }     
            
            if (strcasecmp("tcgpio", param) == 0)
            { 
                if (!strcasestr(value, "none"))
                {                 
                    sscanf(value, "%d", &temp);

                    if (gpio_valid(temp))
                    {
                        config.cooling_gpio = temp;
                    } 
                }               
            }    
            
            if (strcasecmp("tfgpio", param) == 0)
            {
                if (!strcasestr(value, "none"))
                { 
                    sscanf(value, "%d", &temp);

                    if (gpio_valid(temp))
                    {
                        config.fan_gpio = temp;
                    }    
                }            
            }  


            if (strcasecmp("tacgpio", param) == 0)
            {
                if (!strcasestr(value, "none"))
                { 
                    sscanf(value, "%d", &temp);

                    if (gpio_valid(temp))
                    {
                        config.thermostat_temperature_sensor_clock_gpio = temp;
                    }    
                }            
            } 

            if (strcasecmp("tadgpio", param) == 0)
            {
                if (!strcasestr(value, "none"))
                { 
                    sscanf(value, "%d", &temp);

                    if (gpio_valid(temp))
                    {
                        config.thermostat_temperature_sensor_data_gpio = temp;
                    }    
                }            
            } 

            if (strcasecmp("tlcgpio", param) == 0)
            {
                if (!strcasestr(value, "none"))
                { 
                    sscanf(value, "%d", &temp);

                    if (gpio_valid(temp))
                    {
                        config.thermostat_seven_segment_display_clock_gpio = temp;
                    }    
                }            
            } 

            if (strcasecmp("tldgpio", param) == 0)
            {
                if (!strcasestr(value, "none"))
                { 
                    sscanf(value, "%d", &temp);

                    if (gpio_valid(temp))
                    {
                        config.thermostat_seven_segment_display_data_gpio = temp;
                    }    
                }            
            } 

            if (strcasecmp("tbugpio", param) == 0)
            {
                if (!strcasestr(value, "none"))
                { 
                    sscanf(value, "%d", &temp);

                    if (gpio_valid(temp))
                    {
                        config.thermostat_increase_button_gpio = temp;
                    }    
                }            
            } 

            if (strcasecmp("tbdgpio", param) == 0)
            {
                if (!strcasestr(value, "none"))
                { 
                    sscanf(value, "%d", &temp);

                    if (gpio_valid(temp))
                    {
                        config.thermostat_decrease_button_gpio = temp;
                    }    
                }            
            } 

            if (strcasecmp("tbmgpio", param) == 0)
            {
                if (!strcasestr(value, "none"))
                { 
                    sscanf(value, "%d", &temp);

                    if (gpio_valid(temp))
                    {
                        config.thermostat_mode_button_gpio = temp;
                    }    
                }            
            }                       
        }
        i++;
    }

    // write config changes to flash
    config_changed();
 
    // Send the next page back to the user
    return "/t_gpio.shtml";    
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_gpio_default_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    char *param = NULL;
    char *value = NULL;
    int len = 0;
    int temp = 0;
    int gpio_number = -1;
    int gpio_value = -1;
       

    dump_parameters(iIndex, iNumParams, pcParam, pcValue);

    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            printf("Parameter: %s has Value: %s\n", param, value);    

            len = strlen(param);

            if (strcasecmp("gpion", param) == 0)
            {
                sscanf(value, "%d", &gpio_number);
            }                
               
            if (strcasecmp("gpiov", param) == 0)
            {
                sscanf(value, "%d", &gpio_value);
            }                
        }
        i++;
    }

    if (gpio_valid(gpio_number))
    {
        switch(gpio_value)
        {
            default:
                break;
            case GP_UNINITIALIZED:       
            case GP_INPUT_FLOATING:          
            case GP_INPUT_PULLED_HIGH:          
            case GP_INPUT_PULLED_LOW:
            case GP_OUTPUT_HIGH:
            case GP_OUTPUT_LOW:
                config.gpio_default[gpio_number] = gpio_value;
                break;
        }
    }

    // write config changes to flash
    config_changed();
 
    // Send the next page back to the user
    return "/gpio_defaults.shtml";    
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_temperature_sensors(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    char *param = NULL;
    char *value = NULL;
       
    //dump_parameters(iIndex, iNumParams, pcParam, pcValue);

    // set off by default
    config.led_strip_remote_enable  = 0; 

    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            //printf("Parameter: %s has Value: %s\n", param, value);

            if (strcasecmp("tsadr1", param) == 0)
            {
                STRNCPY(config.temperature_sensor_remote_ip[0], value, sizeof(config.temperature_sensor_remote_ip[0]));
            }
            if (strcasecmp("tsadr2", param) == 0)
            {
                STRNCPY(config.temperature_sensor_remote_ip[1], value, sizeof(config.temperature_sensor_remote_ip[1]));
            }
            if (strcasecmp("tsadr3", param) == 0)
            {
                STRNCPY(config.temperature_sensor_remote_ip[2], value, sizeof(config.temperature_sensor_remote_ip[2]));
            }
            if (strcasecmp("tsadr4", param) == 0)
            {
                STRNCPY(config.temperature_sensor_remote_ip[3], value, sizeof(config.temperature_sensor_remote_ip[3]));
            }
            if (strcasecmp("tsadr5", param) == 0)
            {
                STRNCPY(config.temperature_sensor_remote_ip[4], value, sizeof(config.temperature_sensor_remote_ip[4]));
            }
            if (strcasecmp("tsadr6", param) == 0)
            {
                STRNCPY(config.temperature_sensor_remote_ip[5], value, sizeof(config.temperature_sensor_remote_ip[5]));
            }
        }

        i++;
    }

    // Send the next page back to the user
    config_changed();
    return "/t_sensors.shtml";
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_advanced_settings(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    char *param = NULL;
    char *value = NULL;
    int setting = 0;
       
    //dump_parameters(iIndex, iNumParams, pcParam, pcValue);

    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            //printf("Parameter: %s has Value: %s\n", param, value);

            if (strcasecmp("htclm", param) == 0)
            {
                sscanf(value, "%d", &setting);
                CLIP(setting, 1, 60);
                config.heating_to_cooling_lockout_mins = setting;
            }
            if (strcasecmp("mhonm", param) == 0)
            {
                sscanf(value, "%d", &setting);
                CLIP(setting, 1, 60);
                config.minimum_heating_on_mins = setting;
            }
            if (strcasecmp("mconm", param) == 0)
            {
                sscanf(value, "%d", &setting);
                CLIP(setting, 1, 60);
                config.minimum_cooling_on_mins = setting;
            }
            if (strcasecmp("mhoffm", param) == 0)
            {
                sscanf(value, "%d", &setting);
                CLIP(setting, 1, 60);
                config.minimum_heating_off_mins = setting;
            }
            if (strcasecmp("mcoffm", param) == 0)
            {
                sscanf(value, "%d", &setting);
                CLIP(setting, 1, 60);
                config.minimum_cooling_off_mins = setting;
            }
            if (strcasecmp("hvachys", param) == 0)
            {
                setting = get_int_with_tenths_from_string(value); 
                CLIP(setting, 10, 100);
                config.thermostat_hysteresis = setting; 
            }
            if (strcasecmp("disbri", param) == 0)
            {
                sscanf(value, "%d", &setting);
                CLIP(setting, 0, 7);
                config.thermostat_display_brightness = setting; 
            }  
            if (strcasecmp("disdig", param) == 0)
            {
                sscanf(value, "%d", &setting);
                CLIP(setting, 0, 6);
                config.thermostat_display_num_digits = setting; 
            }                       
        }

        i++;
    }

    // Send the next page back to the user
    config_changed();
    return "/t_advanced.shtml";
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_anemometer_settings(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    char *param = NULL;
    char *value = NULL;
       
    //dump_parameters(iIndex, iNumParams, pcParam, pcValue);

    // set off by default
    config.anemometer_remote_enable  = 0; 

    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            //printf("Parameter: %s has Value: %s\n", param, value);

            if (strcasecmp("anip", param) == 0)
            {
                STRNCPY(config.anemometer_remote_ip, value, sizeof(config.anemometer_remote_ip));
            }
            
            if (strcasecmp("anen", param) == 0)
            {
                if (value[0])
                {
                    config.anemometer_remote_enable = 1;
                } 
                else
                {
                    config.anemometer_remote_enable = 0;  // this should never happen, since the parameter is only passed if "on"
                }   
            } 
        }

        i++;
    }

    // Send the next page back to the user
    config_changed();
    return "/weather.shtml";
}

// CGI requests and their respective handlers  --Add new entires at bottom--
static const tCGI cgi_handlers[] = {
    {"/schedule.cgi",                   cgi_schedule_handler},
    {"/sunday.cgi",                     cgi_weekday_handler},   //-START- days of week must be consecutive AND start at index 1
    {"/monday.cgi",                     cgi_weekday_handler},
    {"/tuesday.cgi",                    cgi_weekday_handler},
    {"/wednesday.cgi",                  cgi_weekday_handler},
    {"/thursday.cgi",                   cgi_weekday_handler},
    {"/friday.cgi",                     cgi_weekday_handler},
    {"/saturday.cgi",                   cgi_weekday_handler},   //-END- days of week must be consecutive 
    {"/durinc.cgi",                     cgi_inc_duration_handler}, 
    {"/durdec.cgi",                     cgi_dec_duration_handler},    
    {"/hrinc.cgi",                      cgi_inc_hour_handler}, 
    {"/mininc.cgi",                     cgi_inc_minute_handler},  
    {"/hrdec.cgi",                      cgi_dec_hour_handler}, 
    {"/mindec.cgi",                     cgi_dec_minute_handler}, 
    {"/time.cgi",                       cgi_time_handler},      
    {"/ecowitt.cgi",                    cgi_ecowitt_handler},   
    {"/network.cgi",                    cgi_network_handler},    
    {"/reboot.cgi",                     cgi_reboot_handler},    
    {"/aled.cgi",                       cgi_led_handler},   
    {"/psched.cgi",                     cgi_portrait_schedule_handler},     
    {"/dsched.cgi",                     cgi_day_schedule_handler},   
    {"/mood.cgi",                       cgi_mood_handler},       
    {"/syslog.cgi",                     cgi_syslog_handler}, 
    {"/units.cgi",                      cgi_units_handler},   
    {"/swload.cgi",                     cgi_software_load_handler},     
    {"/remote_led_strips.cgi",          cgi_remote_led_strips},  
    {"/personality.cgi",                cgi_personality_handler},   
    {"/relay.cgi",                      cgi_relay_handler}, 
    {"/wificountry.cgi",                cgi_wificountry_handler}, 
    {"/relay_test_stop.cgi",            cgi_relay_test_stop_handler}, 
    {"/relay_test_start.cgi",           cgi_relay_test_start_handler},     
    {"/led_pattern.cgi",                cgi_led_pattern_handler},   
    {"/led_strip.cgi",                  cgi_led_strip_handler},  
    {"/setpoints.cgi",                  cgi_setpoints_handler},      
    {"/periods.cgi",                    cgi_periods_handler},      
    {"/ts_change.cgi",                  cgi_thermostat_schedule_change_handler},   
    {"/tp_delete.cgi",                  cgi_thermostat_period_delete_handler},    
    {"/tp_add.cgi",                     cgi_thermostat_period_add_handler}, 
    {"/tp_edit.cgi",                    cgi_thermostat_period_edit_handler},   
    {"/tp_cancel.cgi",                  cgi_thermostat_period_cancel_handler},    
    {"/t_schedule.cgi",                 cgi_thermostat_schedule_handler}, 
    {"/powerwall.cgi",                  cgi_powerwall_handler},   
    {"/t_copy.cgi",                     cgi_thermostat_copy_handler},
    {"/t_gpio.cgi",                     cgi_thermostat_gpio_handler},   
    {"/gpio_default.cgi",               cgi_gpio_default_handler},  
    {"/t_sensors.cgi",                  cgi_temperature_sensors},
    {"/t_advanced.cgi",                 cgi_advanced_settings},    
    {"/t_anemometer.cgi",               cgi_anemometer_settings},     
     
};

/*!
 * \brief initialize cgi handlers
 * 
 * \return nothing
 */
void cgi_init(void)
{
    http_set_cgi_handlers(cgi_handlers, NUM_ROWS(cgi_handlers));
}