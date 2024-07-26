#include "hardware/watchdog.h"

#include "lwip/apps/httpd.h"
#include "pico/cyw43_arch.h"

#include "lwip/sockets.h"

#include "flash.h"
#include "weather.h"
#include "calendar.h"
#include "utility.h"
#include "config.h"
#include "led_strip.h"
#include "pluto.h"


extern NON_VOL_VARIABLES_T config;
extern WEB_VARIABLES_T web;

extern char current_calendar_web_page[50];


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


int get_int_with_tenths_from_string(char *value_string)
{
    int whole_part = 0;
    int tenths_part = 0;
    int new_value = 0;

    whole_part = 0;
    tenths_part = 0;

    sscanf(value_string, ".%d", &tenths_part);  
    sscanf(value_string, "%d.%d", &whole_part, &tenths_part);  

    while (tenths_part > 10)
    {
        if ((tenths_part>10) && (tenths_part<100)) tenths_part += 5;  // round up
        tenths_part /= 10;
    }

    CLIP(tenths_part, 0, 9);

    new_value = whole_part*10 + tenths_part;   

    return(new_value);
}

// CGI handler which is run when a request for /schedule.cgi is detected
const char * cgi_schedule_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    // Check if an request for SCHEDULE has been made (/schedule.cgi?schedule=x)
    if (strcmp(pcParam[0] , "schedule") == 0){
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

// CGI handler which is run when a request for /wed.cgi is detected
const char * cgi_weekday_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    
    CLIP(iIndex, 1, 7);

    //toggle the state (assumes index 1-7 used in cgi_handlers[] for weekdays)
    config.day_schedule_enable[iIndex-1] = !config.day_schedule_enable[iIndex-1];

    // Send the index page back to the user
    config_changed();
    return(current_calendar_web_page);
}

// CGI handler which is run when a request for /wed.cgi is detected
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

    //toggle the state (assumes index 1-7 used in cgi_handlers[] for weekdays)
    config.day_duration[i]++;

    // Send the index page back to the user
    config_changed();
    return(current_calendar_web_page);
}

// CGI handler which is run when a request for /wed.cgi is detected
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
    if(config.day_duration[i] > 0) config.day_duration[i]--;

    // Send the index page back to the user
    config_changed();
    return(current_calendar_web_page);
}


// CGI handler which is run when a request for /wed.cgi is detected
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

    // Send the index page back to the user
    config_changed();
    return(current_calendar_web_page);
}

// CGI handler which is run when a request for /wed.cgi is detected
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

    // Send the index page back to the user
    config_changed();
    return(current_calendar_web_page);
}

// CGI handler which is run when a request for /wed.cgi is detected
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

    // Send the index page back to the user
    config_changed();
    return(current_calendar_web_page);
}

// CGI handler which is run when a request for /wed.cgi is detected
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

    // Send the index page back to the user
    config_changed();
    return(current_calendar_web_page);
}


// CGI handler which is run when a request for /muppet.cgi is detected
const char * cgi_time_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    int hour = 0;
    int minute = 0;
    char *param = NULL;
    char *value = NULL;
    int new_value = 0;
       
    dump_parameters(iIndex, iNumParams, pcParam, pcValue);

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


    // Send the index page back to the user
    config_changed();
    return "/time.shtml";
}

// CGI handler which is run when a request for /muppet.cgi is detected
const char * cgi_ecowitt_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    char *param = NULL;
    char *value = NULL;
    int new_relay_normally_open = 0;  
    int new_gpio = 0;  

    // despicable but necessary as we only receive parameter when checked
    config.weather_station_enable = 0;
       
    dump_parameters(iIndex, iNumParams, pcParam, pcValue);
 
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

            if (strcasecmp("wndt", param) == 0)
            {
                config.wind_threshold = get_int_with_tenths_from_string(value);                
            }   

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
                if (!initialize_relay_gpio_(new_gpio))
                {
                    gpio_put(new_gpio, config.relay_normally_open?0:1); 
                    config.gpio_number = new_gpio; 
                }                                
            }                                     
        }

        i++;
    }

    config.relay_normally_open = new_relay_normally_open;

    // Send the index page back to the user
    config_changed();
    return "/weather.shtml";
}


// CGI handler which is run when a request for /muppet.cgi is detected
const char * cgi_network_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    int whole_part = 0;
    int tenths_part = 0;
    char *param = NULL;
    char *value = NULL;
    int new_value = 0;
       
    dump_parameters(iIndex, iNumParams, pcParam, pcValue);

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

            if (strcasecmp("wific", param) == 0)
            {
                STRNCPY(config.wifi_country, value, sizeof(config.wifi_country));
                deplus_string(config.wifi_country, sizeof(config.wifi_country));             
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


    // Send the index page back to the user
    config_changed();
    return "/network.shtml";
}

// CGI handler which is run when a request for /muppet.cgi is detected
const char * cgi_led_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    int whole_part = 0;
    int tenths_part = 0;
    char *param = NULL;
    char *value = NULL;
    int new_value = 0;

    config.use_led_strip_to_indicate_irrigation_status = 0;       

    dump_parameters(iIndex, iNumParams, pcParam, pcValue);

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
                sscanf(value, "%d", &config.led_rgbw);             
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
                sscanf(value, "%d", &config.led_pattern_when_irrigation_usurped);             
            }  

            if (strcasecmp("lis", param) == 0)
            {
                sscanf(value, "%d", &config.led_sustain_duration);             
            }                          

        }

        i++;
    }


    // Send the index page back to the user
    config_changed();
    return "/addressable_led.shtml";
}

//CGI handler which is run when a request for /muppet.cgi is detected
const char * cgi_reboot_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    int whole_part = 0;
    int tenths_part = 0;
    char *param = NULL;
    char *value = NULL;
    int new_value = 0;
       
    printf("REBOOT requested\n");
    
    sleep_ms(1000);

    while (config_dirty(false))
    {
        printf("Waiting for config to be written to flash before rebooting (%d seconds)\n", i++*5);
        sleep_ms(5000);

        //escape
        if (i>12) break;
    }

    //request reboot
    application_restart();

    //return "/status.shtml";

    return "/index.shtml";    
}

// CGI handler which is run when a request for /muppet.cgi is detected
const char * cgi_portrait_schedule_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    int whole_part = 0;
    int tenths_part = 0;
    char *param = NULL;
    char *value = NULL;
    int new_value = 0;
       
    dump_parameters(iIndex, iNumParams, pcParam, pcValue);

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


    // Send the index page back to the user
    config_changed();
    return "/portrait.shtml";
}

// CGI handler which is run when a request for /muppet.cgi is detected
const char * cgi_day_schedule_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    int whole_part = 0;
    int tenths_part = 0;
    char *param = NULL;
    char *value = NULL;
    int new_value = 0;
    int day = -1;
    bool checked = false;
    int start_hour = -1;
    int start_minute = -1;
    int duration = -1;
           
    dump_parameters(iIndex, iNumParams, pcParam, pcValue);

    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            //printf("Parameter: %s has Value: %s\n", param, value);
            
            if (strcasecmp("day", param) == 0)  //day is 1-7
            {
                sscanf(value, "%d", &day);             
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
            }

            if (strncasecmp("dur", param, 3) == 0)
            {
                sscanf(value, "%d", &duration);             
            }              
        }

        i++;
    }

    if ((day >0) && (day <=7))
    {
        //printf("got valid day = %d storing new schedule parameters\n", day);

        config.day_schedule_enable[day-1] = checked;
        if ((start_hour != -1) && (start_minute != -1)) config.day_start[day-1] = start_hour*60 + start_minute;
        if (duration != -1) config.day_duration[day-1] = duration;
    }


    // Send the index page back to the user
    config_changed();
    return "/index.shtml";
}

// CGI handler which is run when a request for /muppet.cgi is detected
const char * cgi_mood_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    int whole_part = 0;
    int tenths_part = 0;
    char *param = NULL;
    char *value = NULL;
    int new_value = 0;
    int red = -1;
    int green = -1;
    int blue = -1;
       
    dump_parameters(iIndex, iNumParams, pcParam, pcValue);

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
                    config.govee_enable = 1;
                } 
                else
                {
                    config.govee_enable = 0;
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


    // Send the index page back to the user
    config_changed();
    return "/moodlight.shtml";
}

// CGI handler which is run when a request for /muppet.cgi is detected
const char * cgi_syslog_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    int whole_part = 0;
    int tenths_part = 0;
    char *param = NULL;
    char *value = NULL;
    int new_value = 0;
       
    // vile design caused by web browser not sending unchecked parameters, they must be presumed unchecked
    config.syslog_enable = 0;       

    dump_parameters(iIndex, iNumParams, pcParam, pcValue);

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


    // Send the index page back to the user
    config_changed();
    return "/syslog.shtml";
}

// CGI handler which is run when a request for /muppet.cgi is detected
const char * cgi_units_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    char *param = NULL;
    char *value = NULL;
    int new_use_archaic_units = 0;   

    // set off by default
    config.use_simplified_english  = 0; 
    config.use_monday_as_week_start = 0; 

    dump_parameters(iIndex, iNumParams, pcParam, pcValue);
 
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

    //check for change in units
    if (new_use_archaic_units != config.use_archaic_units)
    {
        config.use_archaic_units = new_use_archaic_units;

        switch (new_use_archaic_units)
        {
            case false:  //convert from archaic units to SI
                config.wind_threshold = (1000*config.wind_threshold + 1641)/3281;
                config.rain_week_threshold = (254*config.rain_week_threshold + 5)/10;
                config.rain_day_threshold = (254*config.rain_day_threshold + 5)/10;                            
            break;
            case true:   //convert from SI to archaic units
                config.wind_threshold = (config.wind_threshold*3281 + 500)/1000;
                config.rain_week_threshold = (10*config.rain_week_threshold + 127)/254;
                config.rain_day_threshold = (10*config.rain_day_threshold + 127)/254;                    
                break;
            default:
            break;
        }
    }     


    // Send the index page back to the user
    config_changed();
    return "/units.shtml";
}

// CGI handler which is run when a request for /muppet.cgi is detected
const char * cgi_software_load_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    int whole_part = 0;
    int tenths_part = 0;
    char *param = NULL;
    char *value = NULL;
    int new_value = 0;

    dump_parameters(iIndex, iNumParams, pcParam, pcValue);

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


    // Send the index page back to the user
    config_changed();
    return "/software_load.shtml";
}

// CGI handler which is run when a request for /muppet.cgi is detected
const char * cgi_remote_led_strips(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    int hour = 0;
    int minute = 0;
    char *param = NULL;
    char *value = NULL;
       
    dump_parameters(iIndex, iNumParams, pcParam, pcValue);

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

    // Send the index page back to the user
    config_changed();
    return "/remote_led_strips.shtml";
}

// CGI handler which is run when a request for /muppet.cgi is detected
const char * cgi_personality_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    int hour = 0;
    int minute = 0;
    char *param = NULL;
    char *value = NULL;
    PERSONALITY_E new_personality = NO_PERSONALITY;
       
    dump_parameters(iIndex, iNumParams, pcParam, pcValue);

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
                sscanf(value, "%d", &new_personality);

                switch(new_personality)
                {
                    case SPRINKLER_USURPER:
                    case SPRINKLER_CONTROLLER:
                    case LED_STRIP_CONTROLLER:
                        config.personality = new_personality;
                        //printf("config.personality set to %d\n", config.personality);
                        break;
                    
                    default:
                        printf("Invalid personality\n");
                        break;
                }                
            }
        }  
        i++;
    }


    // Send the index page back to the user
    config_changed();
    return "/personality.shtml";
}

// tCGI Struct
// CGI requests and their respective handlers  --Add new entires at bottom--
static const tCGI cgi_handlers[] = {
    {"/schedule.cgi",           cgi_schedule_handler},
    {"/sunday.cgi",             cgi_weekday_handler},   //-START- days of week must be consecutive AND start at index 1
    {"/monday.cgi",             cgi_weekday_handler},
    {"/tuesday.cgi",            cgi_weekday_handler},
    {"/wednesday.cgi",          cgi_weekday_handler},
    {"/thursday.cgi",           cgi_weekday_handler},
    {"/friday.cgi",             cgi_weekday_handler},
    {"/saturday.cgi",           cgi_weekday_handler},   //-END- days of week must be consecutive 
    {"/durinc.cgi",             cgi_inc_duration_handler}, 
    {"/durdec.cgi",             cgi_dec_duration_handler},    
    {"/hrinc.cgi",              cgi_inc_hour_handler}, 
    {"/mininc.cgi",             cgi_inc_minute_handler},  
    {"/hrdec.cgi",              cgi_dec_hour_handler}, 
    {"/mindec.cgi",             cgi_dec_minute_handler}, 
    {"/time.cgi",               cgi_time_handler},      
    {"/ecowitt.cgi",            cgi_ecowitt_handler},   
    {"/network.cgi",            cgi_network_handler},    
    {"/reboot.cgi",             cgi_reboot_handler},    
    {"/aled.cgi",               cgi_led_handler},   
    {"/psched.cgi",             cgi_portrait_schedule_handler},     
    {"/dsched.cgi",             cgi_day_schedule_handler},   
    {"/mood.cgi",               cgi_mood_handler},       
    {"/syslog.cgi",             cgi_syslog_handler}, 
    {"/units.cgi",              cgi_units_handler},   
    {"/swload.cgi",             cgi_software_load_handler},     
    {"/remote_led_strips.cgi",  cgi_remote_led_strips},  
    {"/personality.cgi",        cgi_personality_handler},                               
};

void cgi_init(void)
{
    http_set_cgi_handlers(cgi_handlers, NUM_ROWS(cgi_handlers));
}