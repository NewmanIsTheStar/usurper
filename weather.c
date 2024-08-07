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
#include "flash.h"
#include "calendar.h"
#include "utility.h"
#include "config.h"
#include "led_strip.h"
#include "message.h"
#include "pluto.h"

#define BUF_SIZE (100)
#define RELAY_GPIO_PIN (3)
#define MAX_WINDSPEED (50)

typedef enum
{
    WEATHER_READ_SUCCESS = 0,
    WEATHER_READ_FAILED = 1,
} WEATHER_QUERY_STATUS_T;

typedef enum
{
    IRRIGATION_OFF = 0,
    IRRIGATION_IN_PROGRESS = 1,
    IRRIGATION_TERMINATED = 2,
    IRRIGATION_DISRUPTED = 3,
} IRRIGATION_STATE_T;

typedef enum
{
    LIGHT_DEFAULT = 0,
    LIGHT_IN_PROGRESS = 1,
    LIGHT_TERMINATED = 2,
    LIGHT_SUSTAIN = 3,
} LIGHT_STATE_T;

// prototypes
//WEATHER_QUERY_STATUS_T query_weather_station(int *outside_temperature, int *wind_speed, int *daily_rain, int *weekly_rain);
WEATHER_QUERY_STATUS_T query_weather_station(s16_t *outside_temperature, s16_t *wind_speed, s32_t *daily_rain, s32_t *weekly_rain);
int receive_weather_info_from_ecowitt(unsigned char *rx_bytes, int rx_len);
void hex_dump_to_string(const uint8_t *bptr, uint32_t len, char *out_string, int out_len);
int accumulate_trailing_seven_day_total_rain(int daily_rain, int weekday);
bool terminate_irrigation_due_to_weather (void);
IRRIGATION_STATE_T control_irrigation_relay(void);
int control_moodlight(IRRIGATION_STATE_T irrigation_state, bool reset);
int control_led_strip(IRRIGATION_STATE_T irrigation_state, bool reset);
int schedule_change_affecting_active_irrigation(int start_mow, int end_mow, bool reset);
int syslog_report_weather (void);
int set_led_strips(int pattern, int speed);

// external variables
extern NON_VOL_VARIABLES_T config;

// global variables
WEB_VARIABLES_T web;
int irrigation_finished_mow = 0;
int led_strip_sustain_until_mow = 0;
bool led_strip_sustain_in_progress = false;
int govee_sustain_until_mow = 0;
bool govee_sustain_in_progress = false;


//ecowitt request messages                 HDR   HDR   CMD                  LEN   CHECKSUM
const unsigned char request_live_data[] = {0xff, 0xff, CMD_GW1000_LIVEDATA, 0x03, 0x2a};
const unsigned char request_rain_data[] = {0xff, 0xff, CMD_READ_RAIN,       0x03, 0x5a};

typedef struct ECOWITT_REQUEST_STRUCT
{
    const unsigned char *request;
    const size_t request_len;
} ECOWITT_REQUEST_T;

const ECOWITT_REQUEST_T aEcowittRequests[] =
{
    {request_live_data, sizeof(request_live_data)},
    {request_rain_data, sizeof(request_rain_data)}
};


typedef struct ECOWITTT_PARAMETER_STUCT
{
    unsigned char id;
    char *name;
    char *units;
    unsigned char num_bytes;
    unsigned char *latest_value;
} ECOWITT_PARAMETER_T;

// raw bytes of relevant parameters extracted from received packets
static unsigned char raw_outsidetemp[2] = {0,0};
static unsigned char raw_windspeed[2] =   {0,0};
static unsigned char raw_dailyrain[4] =   {0,0,0,0};
static unsigned char raw_weeklyrain[4] =  {0,0,0,0};
static unsigned char raw_monthlyrain[4] = {0,0,0,0};


const ECOWITT_PARAMETER_T aEcowittParameters[] =
{
    //item tag                 name                            units              num_bytes    store
    {ITEM_INTEMP,              "Inside Temperature",           "C x 10",          2,           NULL},
    {ITEM_INHUMI,              "Inside Humidity",              "%",               1,           NULL},
    {ITEM_ABSBARO,             "Absolute Pressure",            "hpa",             2,           NULL},
    {ITEM_RELBARO,             "Relative Pressure",            "hpa",             2,           NULL},
    {ITEM_OUTTEMP,             "Outside Temperature",           "C x 10",         2,           raw_outsidetemp},
    {ITEM_OUTHUMI,             "Outside Humidity",              "%",              1,           NULL},        
    {ITEM_WINDDIRECTION,       "Wind Direction",                "360°",           2,           NULL},
    {ITEM_WINDSPEED,           "Wind Speed",                    "m/s",            2,           raw_windspeed},    
    {ITEM_GUSTSPEED,           "Gust Speed",                    "m/s",            2,           NULL},  
    {ITEM_LIGHT,               "Light",                         "m/s",            4,           NULL},  
    {ITEM_UV,                  "UV",                            "m/s",            2,           NULL},  
    {ITEM_UVI,                 "UV Index",                      "m/s",            1,           NULL},  
    {ITEM_DAYLWINDMAX,         "Wind Day Max",                  "m/s",            2,           NULL},  
    {ITEM_Piezo_Rain_Rate,     "Rain Rate",                     "mm/h",           2,           NULL},  
    {ITEM_Piezo_Event_Rain,    "Rain Event",                    "units",          2,           NULL},  
    {ITEM_Piezo_Hourly_Rain,   "Rain Hourly",                   "mm",             2,           NULL},  
    {ITEM_Piezo_Daily_Rain,    "Rain Daily",                    "mm",             4,           raw_dailyrain},  
    {ITEM_Piezo_Weekly_Rain,   "Rain Weekly",                   "mm",             4,           raw_weeklyrain},  
    {ITEM_Piezo_Monthly_Rain,  "Rain Monthly",                  "mm",             4,           raw_monthlyrain},  
    {ITEM_Piezo_yearly_Rain,   "Rain Yearly",                   "mm",             4,           NULL},  
    {ITEM_Piezo_Gain10,        "Piezo Gain",                    "gainx10",        20,          NULL},  
    {ITEM_RST_RainTime,        "Rain Time",                     "units",          3,           NULL},   
    {0x7a,                     "Rain Unknown1",                 "units",          1,           NULL},
    {0x7b,                     "Rain Unknown2",                 "units",          1,           NULL},  
    {0x6c,                     "Unknown",                       "units",          4,           NULL},  // len 4 is guess        
};



/*!
 * \brief Monitor weather and control relay based on conditions and schedule
 *
 * \param params unused garbage
 * 
 * \return nothing
 */
void weather_task(void *params)
{
    IRRIGATION_STATE_T irrigation_state = IRRIGATION_OFF;
    WEATHER_QUERY_STATUS_T weather_query_status = WEATHER_READ_SUCCESS;   
    int iNumFailedQueries = 0;    
    char log_message[200];  
    int iStatus = 0;
    int delay_seconds = 0;

    printf("weather_task started\n");

    // initialize irigation relay gpio pin
    if (!initialize_relay_gpio_(config.gpio_number))
    {
        gpio_put(config.gpio_number, config.relay_normally_open?0:1);  
    }

    while (true)
    {
        if ((config.personality == SPRINKLER_USURPER) && config.weather_station_enable)
        {
            // get weather info
            weather_query_status = query_weather_station((s16_t *)&web.outside_temperature, (s16_t *)&web.wind_speed, (s32_t *)&web.daily_rain, (s32_t *)&web.weekly_rain);

            switch(weather_query_status)
            {            
            case WEATHER_READ_SUCCESS:
                syslog_report_weather();                   
                iNumFailedQueries = 0;
                break;

            default:
            case WEATHER_READ_FAILED:
                iNumFailedQueries++;

                if (iNumFailedQueries > 3)
                {
                    invalidate_weather_variables();
                }

                if (iNumFailedQueries > 30)
                {
                    send_syslog_message("usurper", "Failed to read from weather station 30 times. ---REBOOT---.");    

                    // reboot
                    application_restart();
                }          
                break;
            }
        }

        if (config.personality == SPRINKLER_USURPER)
        {
            // control the irrigation relay
            irrigation_state = control_irrigation_relay();

            if (irrigation_state != IRRIGATION_DISRUPTED)
            {
                // control the moodlight and led strip
                control_moodlight(irrigation_state, false);
                control_led_strip(irrigation_state, false);

                // sleep until start of next minute
                delay_seconds = (60 - get_seconds_of_minute());
                CLIP(delay_seconds, 1, 60);
                //printf("weather task sleeping %d seconds\n", delay_seconds);
                sleep_ms(delay_seconds*1000); 
            }
        }
        else
        {
            sleep_ms(60000); 
        }
        
        // tell watchdog task that we are still alive
        watchdog_pulse((int *)params);            
    }
}


/*!
 * \brief Request weather information and store response
 * 
 * \param[out]   outside_temperature (global) Celcius x 10
 * 
 * \param[out]   wind_speed (global) m/s x 10    
 * 
 * \param[out]   daily_rain (global) mm x 10  
 * 
 * \param[out]   weekly_rain (global) mm x 10    
 * 
 * \return WEATHER_READ_SUCCESS or WEATHER_READ_FAILED
 */
WEATHER_QUERY_STATUS_T query_weather_station(s16_t *outside_temperature, s16_t *wind_speed, s32_t *daily_rain, s32_t *weekly_rain)
{
    int err = WEATHER_READ_SUCCESS;
    int ret;
    int wrote_bytes;
    int read_bytes;
    static int msg_to_snd;
    int retry;
    fd_set readset;
    struct timeval tv;  
    unsigned char rx_buffer[BUF_SIZE];  
    static int ecowitt_socket = -1;
    static struct sockaddr_in ecowitt_address; 

    
    // (re)establish socket connection
    if (ecowitt_socket < 0) ecowitt_socket = establish_socket(config.weather_station_ip, &ecowitt_address, 45000, SOCK_STREAM);

    
    if(ecowitt_socket >= 0)
    {
        // send a request selected from one row of the aEcowittRequests table
        wrote_bytes = send(ecowitt_socket, aEcowittRequests[msg_to_snd%NUM_ROWS(aEcowittRequests)].request,
                            aEcowittRequests[msg_to_snd%NUM_ROWS(aEcowittRequests)].request_len, 0);
        msg_to_snd++;

        if (wrote_bytes > 0)
        {
            //printf("read live_data\n");                    
            for (retry=0; retry<5; retry++)
            {
                FD_ZERO(&readset);
                FD_SET(ecowitt_socket, &readset);
                tv.tv_sec = 2;
                tv.tv_usec = 500;

                ret = select(ecowitt_socket + 1, &readset, NULL, NULL, &tv);

                if ((ret > 0) && FD_ISSET(ecowitt_socket, &readset))
                {
                    read_bytes = recv(ecowitt_socket, rx_buffer, BUF_SIZE, 0);

                    if (read_bytes > 0)
                    {
                        //hex_dump(rx_buffer, read_bytes);
                        if (!receive_weather_info_from_ecowitt(rx_buffer, read_bytes))
                        {
                            err = WEATHER_READ_SUCCESS;
                            
                            // store parameters of interest
                            *outside_temperature = (short)ntohs(*(s16_t *)raw_outsidetemp);                            
                            *wind_speed = ntohs(*(u16_t *)raw_windspeed);                            
                            *daily_rain = ntohl(*(u32_t *)raw_dailyrain);                            
                            *weekly_rain = ntohl(*(u32_t *)raw_weeklyrain);
                            
                            // clip parameters to sane ranges
                            CLIP(*outside_temperature, -1000, 600);
                            CLIP(*wind_speed, 0, 1100);
                            CLIP(*daily_rain, 0, 2000);
                            CLIP(*weekly_rain, 0, 7*2000);                                                                                                                                          
                            break;
                        }
                    }
                    else
                    {
                        err = WEATHER_READ_FAILED;
                    }
                }
                else
                {
                    err = WEATHER_READ_FAILED;
                }  

            }         
        }
        else
        {
            // close socket
            close(ecowitt_socket);
            ecowitt_socket = -1;
            err = WEATHER_READ_FAILED;
        }
    }
    else
    {
        err = WEATHER_READ_FAILED;
    }

    return(err);
}


/*!
 * \brief Extract parameter values from ecowitt response message and store in global table
 *
 * \param[out]  rx_bytes    pointer to array containing message
 * \param[in]   len         length of message  
 * 
 * \return 0 on success, non-zero on error
 */
int receive_weather_info_from_ecowitt(unsigned char *rx_bytes, int rx_len)
{
    int ecowitt_msg_len = 0;
    int computed_checksum = 0;
    int i;
    int j;
    int found_id;
    int complete_msg_received = 0;
    int error = 0;
    char log_message[200];


    // check if we have recieved an entire well formed message (simplistic approach -- assume one msg per buffer)
    if (rx_len > 4)
    {
        if ((rx_bytes[0] == 0xff) && (rx_bytes[1] == 0xff))
        {
           //printf("Receieved header 0xff 0xff\n"); 
        }

        ecowitt_msg_len = rx_bytes[3]*256 + rx_bytes[4]; //try shifting <<8 instead
        //printf("Receieved ecowitt message length %d  [bytes %x %x]\n", ecowitt_msg_len, rx_bytes[3], rx_bytes[4]);

        if (rx_len >= ecowitt_msg_len+2)
        {
            //printf("Read bytes conatin complete Ecowitt message.  ecowitt_len = %d  read_bytes = %d\n", ecowitt_msg_len, rx_len); 
            complete_msg_received = 1;

            //compute checksum and compare
            for (i=0; i<(ecowitt_msg_len-1); i++)
            {
                computed_checksum +=rx_bytes[i+2]; 
            }
            if (computed_checksum%256 != rx_bytes[ecowitt_msg_len+1])
            {
                printf("Ecowitt checksum error.  computed = %d  received = %d\n", computed_checksum%256, rx_bytes[ecowitt_msg_len-1]);
                error = 2;
            }
        }
        else
        {
            printf("Ecowitt message ***does not*** fit within current buffer of read bytes.  ecowitt_len = %d  buffer_len = %d\n", ecowitt_msg_len, rx_len);
            error = 3; 
        }
    }

    if (!error && complete_msg_received)
    {
        //record when packet received
        web.us_last_rx_packet = time_us_32();
        
        // parse response parameters
        i = 5;  //offset to first parameter following 0xff 0xff CMD LEN1 LEN2

        while (!error && (i < (ecowitt_msg_len-1)))  // parse all bytes except the last which is the checksum
        {            
            found_id = 0;

            // search for parameter id
            for (j=0; j < sizeof(aEcowittParameters)/sizeof(aEcowittParameters[0]); j++)
            {
                if (rx_bytes[i] == aEcowittParameters[j].id)
                {
                    found_id = 1;

                    //printf("Parameter: %s ", aEcowittParameters[j].name);


                    // check sufficient bytes remain in buffer to extract valid parameter value
                    if ((ecowitt_msg_len - i) >= aEcowittParameters[j].num_bytes)
                    {
                        // index first byte of parameter value
                        i += 1;

                        // check if parameter is one we wish to store
                        if (aEcowittParameters[j].latest_value)
                        {
                            switch (aEcowittParameters[j].num_bytes)
                            {
                                case 1:
                                    aEcowittParameters[j].latest_value[0] = rx_bytes[i];
                                    break;
                                case 2:
                                    aEcowittParameters[j].latest_value[0] = rx_bytes[i];
                                    aEcowittParameters[j].latest_value[1] = rx_bytes[i+1];
                                    break;
                                case 4:
                                    aEcowittParameters[j].latest_value[0] = rx_bytes[i];
                                    aEcowittParameters[j].latest_value[1] = rx_bytes[i+1]; 
                                    aEcowittParameters[j].latest_value[2] = rx_bytes[i+2];
                                    aEcowittParameters[j].latest_value[3] = rx_bytes[i+3];
                                    break;
                                default:
                                    printf("Ecowitt parameter has unhandled number of bytes.  Ignoring.");
                                    break;
                            }
                        }

                        // move index to next parameter
                        i += aEcowittParameters[j].num_bytes; 
                    }
                    else
                    {
                        printf("Ecowitt message parsing stopped @ byte %d due to insufficient bytes remaining.  Parameter ID = 0x%x Parameter length = %d Bytes remaning = %d\n", i, rx_bytes[i], aEcowittParameters[j].num_bytes, ecowitt_msg_len - 1 - i);
                        hex_dump(rx_bytes, rx_len);
                        error = 5;
                        break;
                    }                      
                }       
            }

            if (!found_id)
            {
                printf("Ecowitt message parsing stopped @ byte %d.  Parameter ID not recognized: 0x%x\n", i, rx_bytes[i]);
                error = 4;
                break;
            }  
        }
    }

    return (error);
}


int accumulate_trailing_seven_day_total_rain(int daily_rain, int weekday)
{
    static int day = 0;
    static int max_daily_rain = 0;
    static int daily_rain_accumulator[7] = {0,0,0,0,0,0,0};
    static int current_weekday = 0;
    int seven_day_rain_total = 0;
    int i;
    
    // check if day has changed
    if (weekday != current_weekday)
    {
        // remember current day
        current_weekday = weekday;

        // move to next accumulator bucket and zero it
        day = (day + 1)%7;
        daily_rain_accumulator[day] = 0;
    }
    
    // record credible rain readings 0 - 2m
    if ((daily_rain > 0) && (daily_rain < 2000))
    {
        daily_rain_accumulator[day] = daily_rain;
    }
    
    // sum the accumulator buckets
    for(i=0; i < 7; i++)
    {
        seven_day_rain_total += daily_rain_accumulator[i];
    }

    // if we have been running less than 7 days the calendar week total is more useful
    if (seven_day_rain_total < web.weekly_rain)
    {
        seven_day_rain_total = web.weekly_rain;
    }

    return(seven_day_rain_total);
}


/*!
 * \brief Initialize web interface variables
 *
 * \return 0 on success
 */
int init_web_variables(void)
{
    web.outside_temperature = 0;
    web.wind_speed = 0;
    web.daily_rain = 0;
    web.weekly_rain = 0;
    web.trailing_seven_days_rain = 0;
    web.us_last_rx_packet = 0;    

    STRNCPY(web.last_usurped_timestring,"never", sizeof(web.last_usurped_timestring));
    STRNCPY(web.watchdog_timestring,"never", sizeof(web.watchdog_timestring));

    web.status_message[0] = 0;
    web.stack_message[0] = 0;

    web.socket_max = 0;
    web.bind_failures = 0;
    web.connect_failures = 0;  
    web.syslog_transmit_failures = 0;
    web.govee_transmit_failures = 0;
    web.weather_station_transmit_failures = 0;
    web.bind_failures = 0;
    web.connect_failures = 0;   

    STRNCPY(web.software_server,"psycho.badnet", sizeof(web.software_server));
    STRNCPY(web.software_url,"fileserver.psycho", sizeof(web.software_url));
    STRNCPY(web.software_file,"/pluto.bin", sizeof(web.software_file));        

    // set default web page
    set_calendar_html_page();

    return(0);
}

/*!
 * \brief Invalidate weather web interface variables
 *
 * \return 0 on success
 */
int invalidate_weather_variables(void)
{
    web.outside_temperature = 0;
    web.wind_speed = 0;
    web.daily_rain = 0;
    web.weekly_rain = 0;
    //web.trailing_seven_days_rain = 0;  // actually might be useful to use for a few days if comms lost to weather station

    return(0);
}

/*!
 * \brief Open or close relay based on schedule and weather conditions
 * 
 * \return 0 irrigation off, 1 irrigation on, 1 irrigation usurped
 */
IRRIGATION_STATE_T control_irrigation_relay(void)
{
    static IRRIGATION_STATE_T irrigation_state = IRRIGATION_OFF;
    SCHEDULE_QUERY_STATUS_LT irrigation_schedule_status = SCHEDULE_FUTURE;    
    int err = 0;
    int weekday;
    int min_now;
    int mow_now;
    char log_message[200];
    int schedule_start_mow = 0;
    int schedule_end_mow = 0;

    // get time in local time zone
    get_dow_and_mod_local_tz(&weekday, &min_now);
    get_mow_local_tz(&mow_now);

    // track last seven days of rain
    web.trailing_seven_days_rain = accumulate_trailing_seven_day_total_rain(web.daily_rain, weekday);

    // find next irrigation period
    irrigation_schedule_status = find_next_irrigation_period(&schedule_start_mow, &schedule_end_mow);

    if (irrigation_schedule_status == SCHEDULE_FUTURE)
    {
        printf("Current Minute of Week = %d.  Next Irrigation begins at Minute of Week = %d.   [%d minutes from now]\n", mow_now, schedule_start_mow, schedule_start_mow - mow_now);
    }

    switch(irrigation_state)
    {
    case IRRIGATION_DISRUPTED:
        irrigation_state = IRRIGATION_OFF;
        [[fallthrough]]; 
    default:    
    case IRRIGATION_OFF:
        if (irrigation_schedule_status == SCHEDULE_NOW)
        {
            send_syslog_message("usurper", "Irrigation commenced according to schedule");            
            snprintf(web.status_message, sizeof(web.status_message), "Irrigation in progress");            
            schedule_change_affecting_active_irrigation(schedule_start_mow, schedule_end_mow, true);  // initiate schedule monitoring
            irrigation_state = IRRIGATION_IN_PROGRESS;

            if (terminate_irrigation_due_to_weather())
            {
                irrigation_state = IRRIGATION_TERMINATED;                
            }            
        } 
        break;
    case IRRIGATION_IN_PROGRESS:
        switch(irrigation_schedule_status)
        {
        case SCHEDULE_FUTURE:
            send_syslog_message("usurper", "Irrigation ended according to schedule");
            snprintf(web.status_message, sizeof(web.status_message), "Next irrigation %s at %02d:%02d", day_name(schedule_start_mow/(24*60)), (schedule_start_mow%(24*60))/60, (schedule_start_mow%(24*60))%60);                                                
            irrigation_state = IRRIGATION_OFF;
            break;
        case SCHEDULE_NOW:
            if (schedule_change_affecting_active_irrigation(schedule_start_mow, schedule_end_mow, false))
            {
                send_syslog_message("usurper", "Irrigation disrupted by schedule alteration\n");
                irrigation_state = IRRIGATION_DISRUPTED;
                control_moodlight(LIGHT_DEFAULT, true);
                control_led_strip(LIGHT_DEFAULT, true);            
            }
            break;
        default:
        case SCHEDULE_NEVER: // no irrigation scheduled
            snprintf(web.status_message, sizeof(web.status_message), "No irrigation scheduled");    
            break;
        } 
        if (terminate_irrigation_due_to_weather())
        {
            irrigation_state = IRRIGATION_TERMINATED;
        }
        break;
    case IRRIGATION_TERMINATED:
        if (irrigation_schedule_status == SCHEDULE_FUTURE)
        {
            send_syslog_message("usurper", "Previously terminated irrigation period has now reached its scheduled end");
            snprintf(web.status_message, sizeof(web.status_message), "Next irrigation %s at %02d:%02d", day_name(schedule_start_mow/(24*60)), (schedule_start_mow%(24*60))/60, (schedule_start_mow%(24*60))%60);                                                
            irrigation_state = IRRIGATION_OFF;
        }
        break;        
    }

    // set the gpio output connected to the relay
    if (config.irrigation_enable)
    {
        switch (irrigation_state)
        {
        default:
        case IRRIGATION_OFF:
        case IRRIGATION_TERMINATED:
        case IRRIGATION_DISRUPTED:
            gpio_put(config.gpio_number, config.relay_normally_open?0:1);
            //printf("IRRIGATION OFF\n"); 
            break;
        case IRRIGATION_IN_PROGRESS:
            gpio_put(config.gpio_number, config.relay_normally_open?1:0); 
            //printf("IRRIGATION ON\n"); 
            break;
        } 
    }

    return(irrigation_state);
}

/*!
 * \brief Set moodlight colour based on irrigation state
 * 
 * \param[in] irrigation_state  0 irrigation off, 1 irrigation in progress, 2 irrigation usurped
 * \return 0 on success, non-zero on failure
 */
int control_moodlight(IRRIGATION_STATE_T irrigation_state, bool reset)
{
    int err = 0;
    int mow;    
    static int moodlight_state = LIGHT_DEFAULT; 
    static int sustain_start = -1;    
    static int sustain_end = -1;


    if (reset)
    {
        moodlight_state = LIGHT_DEFAULT;
        send_govee_command(1, 0, 0, 0); 
        sustain_start = -1;     
        sustain_end = -1;        
    }

    // update moodlight state based on irrigation state
    switch(moodlight_state)
    {
    case LIGHT_DEFAULT:
        switch(irrigation_state)
        {
            default:
            case IRRIGATION_OFF:
                break;
            case IRRIGATION_IN_PROGRESS:
                moodlight_state = LIGHT_IN_PROGRESS;
                break;
            case IRRIGATION_TERMINATED:
                moodlight_state = LIGHT_TERMINATED;
                break;
        }
        break;
    case LIGHT_IN_PROGRESS:
        switch(irrigation_state)
        {
            case IRRIGATION_OFF:
                moodlight_state = LIGHT_SUSTAIN;
                break;
            default:
            case IRRIGATION_IN_PROGRESS:
                break;
            case IRRIGATION_TERMINATED:
                moodlight_state =LIGHT_TERMINATED;
                break;
        }
        break;
    case LIGHT_TERMINATED:
        switch(irrigation_state)
        {
            case IRRIGATION_OFF:
                moodlight_state = LIGHT_SUSTAIN;
                break; 
            case IRRIGATION_IN_PROGRESS:
                moodlight_state = LIGHT_IN_PROGRESS;
                break;
            default:
            case IRRIGATION_TERMINATED:
                break;                   
        }
        break;
    case LIGHT_SUSTAIN:
        switch(irrigation_state)
        {
            default:
            case IRRIGATION_OFF:
                break;
            case IRRIGATION_IN_PROGRESS:
                moodlight_state = LIGHT_IN_PROGRESS;
                break;
            case IRRIGATION_TERMINATED:
                moodlight_state = LIGHT_TERMINATED;
                break;                                
        }
        break;
    default:
        moodlight_state = LIGHT_DEFAULT;
        break;

    }

    // set moodlight colour based on current state
    switch(moodlight_state)
    {
        default:
        case LIGHT_DEFAULT:
            break;
        case LIGHT_IN_PROGRESS:
            send_govee_command(1, config.govee_irrigation_active_red, config.govee_irrigation_active_green, config.govee_irrigation_active_blue); 
            break;
        case LIGHT_TERMINATED:
            send_govee_command(1, config.govee_irrigation_usurped_red, config.govee_irrigation_usurped_green, config.govee_irrigation_usurped_blue); 
            break;
        case LIGHT_SUSTAIN:
            // check if we are starting a sustain period
            if (sustain_start == -1)
            {
                if(!get_mow_local_tz(&sustain_start))
                {
                    // set the end of the sustain period
                    sustain_end = sustain_start + config.govee_sustain_duration;
                }           
            }
            
            // check if we are ending a sustain period
            if(!get_mow_local_tz(&mow))
            {
                if (!mow_between(mow, sustain_start, sustain_end))
                {
                    moodlight_state = LIGHT_DEFAULT;
                    send_govee_command(0,0,0,0); 
                    sustain_start = -1;     
                    sustain_end = -1;                                     
                }
            }                   
            break;
    }

    return(err);
}


/*!
 * \brief Set LED strip pattern based on irrigation state
 * 
 * \param[in] irrigation_state  0 irrigation off, 1 irrigation in progress, 2 irrigation usurped
 * \return 0 on success, non-zero on failure
 */
int control_led_strip(IRRIGATION_STATE_T irrigation_state, bool reset)
{
    int err = 0;
    int mow;    
    static int led_strip_state = LIGHT_DEFAULT; 
    static int sustain_start = -1;    
    static int sustain_end = -1;


    if (reset)
    {
        led_strip_state = LIGHT_DEFAULT;
        set_led_strips(config.led_pattern, config.led_speed);  
        sustain_start = -1;     
        sustain_end = -1;        
    }

    // update moodlight state based on irrigation state
    switch(led_strip_state)
    {
    case LIGHT_DEFAULT:
        switch(irrigation_state)
        {
            default:
            case IRRIGATION_OFF:
                break;
            case IRRIGATION_IN_PROGRESS:
                led_strip_state = LIGHT_IN_PROGRESS;
                break;
            case IRRIGATION_TERMINATED:
                led_strip_state = LIGHT_TERMINATED;
                break;
        }
        break;
    case LIGHT_IN_PROGRESS:
        switch(irrigation_state)
        {
            case IRRIGATION_OFF:
                led_strip_state = LIGHT_SUSTAIN;
                break;
            default:
            case IRRIGATION_IN_PROGRESS:
                break;
            case IRRIGATION_TERMINATED:
                led_strip_state =LIGHT_TERMINATED;
                break;
        }
        break;
    case LIGHT_TERMINATED:
        switch(irrigation_state)
        {
            case IRRIGATION_OFF:
                led_strip_state = LIGHT_SUSTAIN;
                break; 
            case IRRIGATION_IN_PROGRESS:
                led_strip_state = LIGHT_IN_PROGRESS;
                break;
            default:
            case IRRIGATION_TERMINATED:
                break;                   
        }
        break;
    case LIGHT_SUSTAIN:
        switch(irrigation_state)
        {
            default:
            case IRRIGATION_OFF:
                break;
            case IRRIGATION_IN_PROGRESS:
                led_strip_state = LIGHT_IN_PROGRESS;
                break;
            case IRRIGATION_TERMINATED:
                led_strip_state = LIGHT_TERMINATED;
                break;                                
        }
        break;
    default:
        led_strip_state = LIGHT_DEFAULT;
        break;

    }

    // set led pattern based on current state
    switch(led_strip_state)
    {
        default:
        case LIGHT_DEFAULT:
            set_led_strips(config.led_pattern, config.led_speed);  
            break;
        case LIGHT_IN_PROGRESS:
            //set_led_pattern(config.led_pattern_when_irrigation_active);
            set_led_strips(config.led_pattern_when_irrigation_active, config.led_speed);
            break;
        case LIGHT_TERMINATED:
            //set_led_pattern(config.led_pattern_when_irrigation_usurped);
            set_led_strips(config.led_pattern_when_irrigation_usurped, config.led_speed);            
            break;
        case LIGHT_SUSTAIN:
            // check if we are starting a sustain period
            if (sustain_start == -1)
            {
                if(!get_mow_local_tz(&sustain_start))
                {
                    // set the end of the sustain period
                    sustain_end = sustain_start + config.led_sustain_duration;
                }           
            }
            
            // check if we are ending a sustain period
            if(!get_mow_local_tz(&mow))
            {
                if (!mow_between(mow, sustain_start, sustain_end))
                {
                    led_strip_state = LIGHT_DEFAULT;
                    //set_led_pattern(config.led_pattern);
                    set_led_strips(config.led_pattern, config.led_speed);                      
                    sustain_start = -1;     
                    sustain_end = -1;                                     
                }
            }                   
            break;
    }

    return(err);
}



/*!
 * \brief Check if we should terminate irrigation due to weather
 * 
 * \param[in] irrigation_state  0 irrigation off, 1 irrigation in progress, 2 irrigation usurped
 * \return true or false
 */
bool terminate_irrigation_due_to_weather (void)
{
    int terminate_irrigation = false;
    int wind_speed = 0;
    int rain_day = 0;
    int rain_week = 0; 
 

    // convert current measurements to archaic units if necessary
    switch(config.use_archaic_units)
    {
    case true:
        wind_speed = (web.wind_speed*3281 + 500)/1000;                 // feet per second
        rain_week = (10*web.trailing_seven_days_rain + 127)/254;       // inches
        rain_day = (10*web.daily_rain + 127)/254;                      // inches
        break;
        
    default:
    case false:
        wind_speed = web.wind_speed;                      // m/s
        rain_week = web.trailing_seven_days_rain;         // mm   
        rain_day = web.daily_rain;                        // mm
        break;
    } 

    // check if we should terminate irrigation due to weather
    if ((wind_speed > config.wind_threshold)      ||
        (rain_day > config.rain_day_threshold)    ||
        (rain_week  > config.rain_week_threshold)                        
        )                   
    {
        terminate_irrigation = true;

        switch(config.use_archaic_units)
        {
        case true:
            send_syslog_message("usurper", "Irrigation terminated due to weather.  Wind speed is %d.%d ft/s Daily rain is %d.%d inches Seven day rain is %d.%d inches",
                wind_speed/10, wind_speed%10, rain_day/10, rain_day%10, rain_week/10, rain_week%10);
            break;            
        default:
        case false:
            send_syslog_message("usurper", "Irrigation terminated due to weather.  Wind speed is %d.%d m/s Daily rain is %d.%d mm Seven day rain is %d.%d mm",
                wind_speed/10, wind_speed%10, rain_day/10, rain_day%10, rain_week/10, rain_week%10);
            break;
        }         
        get_timestamp(web.last_usurped_timestring, sizeof(web.last_usurped_timestring), 0);                 
    }

    return(terminate_irrigation);
}        


/*!
 * \brief Monitor schedule to detect changes affecting active irrigation
 * 
 * \param[in] reset  true = start new monitoring period, false = report on existing period
 * \return 0 no change, non-zero schedule changed
 */
int schedule_change_affecting_active_irrigation(int start_mow, int end_mow, bool reset)
{
    static int irrigation_start_mow = -1;
    static int irrigation_end_mow = -1; 
    int err = 0;

    if (reset)
    {
        irrigation_start_mow = start_mow;
        irrigation_end_mow = end_mow;
    }

    if ((start_mow != irrigation_start_mow) || (end_mow != irrigation_end_mow))
    {
        err = 1;

        printf("SCHEDULE ALTERATION DETECTED!\n");
        // irrigation_start_mow = -1;
        // irrigation_end_mow = -1; 
    }

    return(err);
}

/*!
 * \brief Send current weather info to syslog server
 * 
 * \return 0 on success, non-zero on failure
 */
int syslog_report_weather (void)
{
    int outside_temp = 0;
    int wind_speed = 0;
    int rain_day = 0;
    int rain_week = 0; 
    static int previous_outside_temp = 0;
    static int previous_wind_speed = 0;
    static int previous_rain_day = 0;
    static int previous_rain_week = 0;     
 
    // convert current measurements to archaic units if necessary
    switch(config.use_archaic_units)
    {
    case true:
        outside_temp = (web.outside_temperature*9)/5 + 320;     // Fahrenheit
        wind_speed = (web.wind_speed*3281 + 500)/1000;          // feet per second
        rain_week = (10*web.weekly_rain + 127)/254;             // inches
        rain_day = (10*web.daily_rain + 127)/254;               // inches
        break;
        
    default:
    case false:
        outside_temp = web.outside_temperature;                 // Celcius
        wind_speed = web.wind_speed;                            // m/s
        rain_week = web.weekly_rain;                            // mm   
        rain_day = web.daily_rain;                              // mm
        break;
    } 

    // check if anything changed since last message was sent
    // note: necessary due to different resolution of archaic units e.g. a 1 mm change may result in no change when measured in tenths of an inch
    if ((outside_temp != previous_outside_temp) ||
        (wind_speed != previous_wind_speed)     ||
        (rain_day != previous_rain_day)         ||
        (rain_week != previous_rain_week))
    {
        // remember what we log
        previous_outside_temp = outside_temp;
        previous_wind_speed = wind_speed;
        previous_rain_day = rain_day;
        previous_rain_week =  rain_week;  

        switch(config.use_archaic_units)
        {
        case true:
            send_syslog_message("usurper", "Temperature = %d.%d F Wind speed = %d.%d ft/s Daily rain = %d.%d inches Weekly rain = %d.%d inches",
                outside_temp/10, abs(outside_temp%10), wind_speed/10, wind_speed%10, rain_day/10, rain_day%10, rain_week/10, rain_week%10);
            break;            
        default:
        case false:
            send_syslog_message("usurper", "Temperature = %d.%d C Wind speed = %d.%d m/s Daily rain = %d.%d mm Weekly rain = %d.%d mm",
                outside_temp/10, abs(outside_temp%10), wind_speed/10, wind_speed%10, rain_day/10, rain_day%10, rain_week/10, rain_week%10);
            break;
        }  
    }   

    return(0);
}

/*!
 * \brief Set the state of all LED strips -- both local and remote
 * 
 * \return 0 on success, non-zero on failure
 */
int set_led_strips(int pattern, int speed)
{
    set_led_pattern_remote(pattern);
    set_led_speed_remote(speed);    
    set_led_pattern_local(pattern);
    set_led_speed_local(speed);
}

