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

typedef struct
{
    TimerHandle_t timer_handle;
    bool timer_expired;
} CLIMATE_TIMERS_T;

typedef enum
{
    HVAC_LOCKOUT_TIMER   = 0,
    HVAC_OVERSHOOT_TIMER = 1,   
    NUM_HVAC_TIMERS      = 2
} CLIMATE_TIMER_INDEX_T;

typedef enum
{
    SETPOINT_BIAS_UNDEFINED = 0,
    SETPOINT_BIAS_HEATING   = 1,   
    SETPOINT_BIAS_COOLING   = 2
} SETPOINT_BIAS_T;

// prototypes
int set_hvac_gpio(THERMOSTAT_STATE_T thermostat_state);
int hvac_timer_start(CLIMATE_TIMER_INDEX_T timer_index, int minutes);
int hvac_timer_stop(CLIMATE_TIMER_INDEX_T timer_index);
bool hvac_timer_expired(CLIMATE_TIMER_INDEX_T timer_index);
void vTimerCallback(TimerHandle_t xTimer);
int update_current_setpoints(THERMOSTAT_STATE_T last_active, long int temperaturex10);
long int sanitize_setpoint(long int setpoint);

// external variables
extern NON_VOL_VARIABLES_T config;
extern WEB_VARIABLES_T web;
//extern long int temperaturex10;

// gloabl variables
THERMOSTAT_MODE_T scheduled_mode = HVAC_AUTO;            // scheduled mode
int setpointtemperaturex10 = 0;                          // scheduled setpoint
int temporary_set_point_offsetx10 = 0;                   // temporary offset set using physical buttons
CLIMATE_TIMERS_T climate_timers[NUM_HVAC_TIMERS];        // set of timers used to control state
bool relay_gpio_ok = false;                              // ok to use configured gpio

/*!
 * \brief Open or close relay based on schedule and climate conditions
 * 
 * \return 0 irrigation off, 1 irrigation on, 1 irrigation usurped
 */
THERMOSTAT_STATE_T control_thermostat_relays(long int temperaturex10)
{
    static THERMOSTAT_STATE_T thermostat_state = HEATING_AND_COOLING_OFF;
    static THERMOSTAT_STATE_T last_active = HEATING_AND_COOLING_OFF;
    static THERMOSTAT_STATE_T next_active = HEATING_AND_COOLING_OFF; 
    static THERMOSTAT_MODE_T effective_mode = HVAC_AUTO;  
    THERMOSTAT_MODE_T front_panel_mode;
    TickType_t lockout_remaining_ticks = 0;  
    bool cooling_allowed = false;
    bool heating_allowed = false;
    bool fan_allowed = false;     

    // determine current setpoints based on schedule, powerwall status and last cycle
    update_current_setpoints(last_active, temperaturex10);

    // determine effective mode
    front_panel_mode = get_front_panel_mode();

    if (front_panel_mode == HVAC_AUTO)
    {
        effective_mode = scheduled_mode;
    }
    else if (front_panel_mode != effective_mode)  
    {
        // user has set a new mode on the front panel
        effective_mode = front_panel_mode;

        // reset state machine to immediately process mode change
        hvac_timer_stop(HVAC_LOCKOUT_TIMER);
        hvac_timer_stop(HVAC_OVERSHOOT_TIMER);
        thermostat_state = HEATING_AND_COOLING_OFF;
        next_active =  HEATING_AND_COOLING_OFF;
        last_active =  HEATING_AND_COOLING_OFF;    
        
        // reset hvac gpio
        set_hvac_gpio(thermostat_state);
    }

    // determine permitted operations
    switch(effective_mode)
    {
        default:
        case HVAC_OFF:
            cooling_allowed = false;
            heating_allowed = false;
            fan_allowed = false;
            break;
        case HVAC_HEATING_ONLY:
            cooling_allowed = false;
            heating_allowed = true;
            fan_allowed = true;
            break;            
        case HVAC_COOLING_ONLY:
            cooling_allowed = true;
            heating_allowed = false;
            fan_allowed = true;
            break;
        case HVAC_FAN_ONLY:
            cooling_allowed = false;
            heating_allowed = false;
            fan_allowed = true;
            break;        
        case HVAC_AUTO: 
        case HVAC_HEAT_AND_COOL:      
            cooling_allowed = true;
            heating_allowed = true;
            fan_allowed = true;
            break;        
    }   

    // update thermostat state
    switch(thermostat_state)
    {
    default:    
    case HEATING_AND_COOLING_OFF:
    case FAN_ONLY_IN_PROGRESS:
        if (cooling_allowed & (temperaturex10 > (web.thermostat_cooling_set_point + config.thermostat_hysteresis)))
        {
            if (last_active != HEATING_IN_PROGRESS)
            {
                send_syslog_message("thermostat", "Cooling commenced");            
                snprintf(web.status_message, sizeof(web.status_message), "Cooling in progress");            

                set_hvac_gpio(COOLING_IN_PROGRESS);

                // lockout state changes
                hvac_timer_start(HVAC_LOCKOUT_TIMER, config.minimum_cooling_on_mins);                 
                thermostat_state = THERMOSTAT_LOCKOUT;                 
                next_active =  COOLING_IN_PROGRESS; 

                set_hvac_lag(COOLING_LAG);
            }
            else
            {
                send_syslog_message("thermostat", "Transition from heating to cooling! Lockout commenced!");
                snprintf(web.status_message, sizeof(web.status_message), "Transition from heating to cooling");                

                set_hvac_gpio(HEATING_AND_COOLING_OFF);

                // lockout state changes
                hvac_timer_start(HVAC_LOCKOUT_TIMER, config.heating_to_cooling_lockout_mins);                 
                thermostat_state = THERMOSTAT_LOCKOUT;                 
                next_active =  HEATING_AND_COOLING_OFF;
                last_active =  HEATING_AND_COOLING_OFF;                                   
            }
        } else if (heating_allowed && (temperaturex10 < (web.thermostat_heating_set_point - config.thermostat_hysteresis)))
        {
            if (last_active != COOLING_IN_PROGRESS)
            {
                send_syslog_message("thermostat", "Heating commenced");            
                snprintf(web.status_message, sizeof(web.status_message), "Heating in progress");            

                
                set_hvac_gpio(HEATING_IN_PROGRESS);
                
                // lockout state changes
                hvac_timer_start(HVAC_LOCKOUT_TIMER, config.minimum_heating_on_mins);
                thermostat_state = THERMOSTAT_LOCKOUT;                
                next_active =  HEATING_IN_PROGRESS;

                set_hvac_lag(HEATING_LAG);  
            }
            else
            {
                send_syslog_message("thermostat", "Transition from cooling to heating! Lockout commenced!");
                snprintf(web.status_message, sizeof(web.status_message), "Transition from cooling to heating"); 

                set_hvac_gpio(HEATING_AND_COOLING_OFF);

                // lockout state changes
                hvac_timer_start(HVAC_LOCKOUT_TIMER, config.heating_to_cooling_lockout_mins);
                thermostat_state = THERMOSTAT_LOCKOUT;                
                next_active =  HEATING_AND_COOLING_OFF;
                last_active =  HEATING_AND_COOLING_OFF;                                                      
            }                        
        }
        else if ((effective_mode == HVAC_FAN_ONLY) && (thermostat_state != FAN_ONLY_IN_PROGRESS))
        {           
            set_hvac_gpio(FAN_ONLY_IN_PROGRESS);  
            thermostat_state = FAN_ONLY_IN_PROGRESS;     
        }        
        break;
    case HEATING_IN_PROGRESS:
        if (temperaturex10 > (web.thermostat_heating_set_point + config.thermostat_hysteresis))
        {
            send_syslog_message("thermostat", "Heating completed");            
            snprintf(web.status_message, sizeof(web.status_message), "Heating completed");  
            mark_hvac_off(HEATING_LAG, temperaturex10);         

            set_hvac_gpio(HEATING_AND_COOLING_OFF);

            // check for excessive overshoot that could trigger cooling
            if (temperaturex10 > (web.thermostat_cooling_set_point - config.thermostat_hysteresis))
            {
                send_syslog_message("thermostat", "Excessive overshoot. Suspending operation.");            
                snprintf(web.status_message, sizeof(web.status_message), "Excessive overshoot. Suspending operation."); 
                
                // lockout state changes
                hvac_timer_start(HVAC_LOCKOUT_TIMER, config.minimum_heating_off_mins);             
                thermostat_state = THERMOSTAT_LOCKOUT;                     
                next_active =  EXCESSIVE_OVERSHOOT;
                last_active = HEATING_IN_PROGRESS;                
            }
            else
            {
                // lockout state changes
                hvac_timer_start(HVAC_LOCKOUT_TIMER, config.minimum_heating_off_mins);             
                thermostat_state = THERMOSTAT_LOCKOUT;                       
                next_active =  HEATING_AND_COOLING_OFF;
                last_active = HEATING_IN_PROGRESS;
            }
        } 
        else
        {
                predicted_time_to_temperature(web.thermostat_heating_set_point + config.thermostat_hysteresis);
        }
        break;
    case COOLING_IN_PROGRESS:
        if (temperaturex10 < (web.thermostat_cooling_set_point - config.thermostat_hysteresis))
        {
            send_syslog_message("thermostat", "Cooling completed");            
            snprintf(web.status_message, sizeof(web.status_message), "Cooling completed"); 
            mark_hvac_off(COOLING_LAG, temperaturex10);

            set_hvac_gpio(HEATING_AND_COOLING_OFF);

            // check for excessive overshoot that could trigger heating
            if (temperaturex10 < (web.thermostat_heating_set_point + config.thermostat_hysteresis))
            {
                send_syslog_message("thermostat", "Excessive overshoot. Suspending operation.");            
                snprintf(web.status_message, sizeof(web.status_message), "Excessive overshoot. Suspending operation."); 

                // lockout state changes
                hvac_timer_start(HVAC_LOCKOUT_TIMER, config.minimum_cooling_off_mins);            
                thermostat_state = THERMOSTAT_LOCKOUT;            
                next_active =  EXCESSIVE_OVERSHOOT;    
                last_active = COOLING_IN_PROGRESS;  
            }
            else
            {
                // lockout state changes
                hvac_timer_start(HVAC_LOCKOUT_TIMER, config.minimum_cooling_off_mins);            
                thermostat_state = THERMOSTAT_LOCKOUT;            
                next_active =  HEATING_AND_COOLING_OFF;    
                last_active = COOLING_IN_PROGRESS;  
            }          
        }
        else
        {
             predicted_time_to_temperature(web.thermostat_cooling_set_point - config.thermostat_hysteresis);
        } 
        break;
    case DUCT_PURGE:
        if (0 /*time expired*/)
        {
            send_syslog_message("thermostat", "Duct purge complete");            
            snprintf(web.status_message, sizeof(web.status_message), "Duct purge completed"); 

            // disable lockout
            hvac_timer_start(HVAC_LOCKOUT_TIMER, 0);
            thermostat_state = HEATING_AND_COOLING_OFF;
        } 
        break;   
    case THERMOSTAT_LOCKOUT:
        // check if lockout complete
        if (hvac_timer_expired(HVAC_LOCKOUT_TIMER))
        {
            // change to next active state
            thermostat_state = next_active;
            if (thermostat_state == EXCESSIVE_OVERSHOOT)
            {
                hvac_timer_start(HVAC_OVERSHOOT_TIMER, 2*60*60*1000); 
            }
        }
        else
        {
            //printf("HVAC CONTROL DAMPING LOCKOUT -- no control changed permitted at present\n");
        }               
        break;    
    case EXCESSIVE_OVERSHOOT:    
        if ((last_active == HEATING_IN_PROGRESS) &&
            (temperaturex10 < (web.thermostat_heating_set_point + config.thermostat_hysteresis)))
        {
            send_syslog_message("thermostat", "Temperature has fallen to target range. Resuming operation");            
            snprintf(web.status_message, sizeof(web.status_message), "Resuming operation"); 

            thermostat_state = HEATING_AND_COOLING_OFF;
        }

        if ((last_active == COOLING_IN_PROGRESS) &&
            (temperaturex10 > (web.thermostat_cooling_set_point - config.thermostat_hysteresis)))
        {
            send_syslog_message("thermostat", "Temperature has risen to target range. Resuming operation");            
            snprintf(web.status_message, sizeof(web.status_message), "Resuming operation"); 

            thermostat_state = HEATING_AND_COOLING_OFF;
        }    
        
        if (hvac_timer_expired(HVAC_OVERSHOOT_TIMER))
        {
            send_syslog_message("thermostat", "Overshoot timeout.  Resuming operation");            
            snprintf(web.status_message, sizeof(web.status_message), "Resuming operation");  
            
            thermostat_state = HEATING_AND_COOLING_OFF;
        }

        break;                        
    }
                               
    return(thermostat_state);
}

/*!
 * \brief Start timer
 * 
 * \return non-zero on error
 */
int hvac_timer_start(CLIMATE_TIMER_INDEX_T timer_index, int minutes)
{
    int err = 0;          

    if (climate_timers[HVAC_LOCKOUT_TIMER].timer_handle)
    {
        if (minutes > 0)
        {
            // begin new lockout period  TODO error checking
            climate_timers[HVAC_LOCKOUT_TIMER].timer_expired = false;
            xTimerChangePeriod(climate_timers[HVAC_LOCKOUT_TIMER].timer_handle, minutes*60*1000, 1000);
            xTimerStart(climate_timers[HVAC_LOCKOUT_TIMER].timer_handle, 1000);
        }
    }
    else
    {
        err = 1;
    }

    return(err);
}

/*!
 * \brief Stop timer
 * 
 * \return true if timer expired
 */
int hvac_timer_stop(CLIMATE_TIMER_INDEX_T timer_index)
{
    int err = 0;         
 
    xTimerStart(climate_timers[HVAC_LOCKOUT_TIMER].timer_handle, 0);
    climate_timers[timer_index].timer_expired = true;
    
    return(err);
}

/*!
 * \brief Check timer
 * 
 * \return true if timer expired
 */
bool hvac_timer_expired(CLIMATE_TIMER_INDEX_T timer_index)
{
    bool expired = false;          
 
    expired = climate_timers[timer_index].timer_expired;
    
    return(expired);
}

/*!
 * \brief Set HVAC Control GPIOs
 * 
 * \return 
 */
int set_hvac_gpio(THERMOSTAT_STATE_T thermostat_state)
{
    int err = 0;

    // set the gpio output connected to the relay    
    //if (config.thermostat_enable && gpio_valid(config.heating_gpio) && gpio_valid(config.cooling_gpio) && gpio_valid(config.fan_gpio))
    if (config.thermostat_enable && relay_gpio_ok)
    {
        switch (thermostat_state)
        {
        case EXCESSIVE_OVERSHOOT:        
        case HEATING_AND_COOLING_OFF:
            printf("Heating, Cooling and Fan off\n");
            gpio_put(config.heating_gpio, 0);
            gpio_put(config.cooling_gpio, 0);
            gpio_put(config.fan_gpio, 0);   
            break;
        case HEATING_IN_PROGRESS:
            printf("Heating on, Cooling and Fan off\n");
            gpio_put(config.heating_gpio, 1);
            gpio_put(config.cooling_gpio, 0);
            gpio_put(config.fan_gpio, 0);      // when heating the thermostat is *not* responsible for turning on the fan (to avoid blowing cold air)
            break;
        case COOLING_IN_PROGRESS:
            printf("Cooling on, Fan on and Heating off\n");
            gpio_put(config.heating_gpio, 0);
            gpio_put(config.cooling_gpio, 1);
            gpio_put(config.fan_gpio, 1);      // it is conventional for the thermostat to turn on the fan when cooling
            break;                 
        case FAN_ONLY_IN_PROGRESS:
            printf("Fan on, Cooling and Heating off\n");
            gpio_put(config.heating_gpio, 0);
            gpio_put(config.cooling_gpio, 0);
            gpio_put(config.fan_gpio, 1);      // air circulation only
            break;                    
        case DUCT_PURGE:
            printf("Heating and Cooling off and Fan on\n");
            gpio_put(config.heating_gpio, 0);
            gpio_put(config.cooling_gpio, 0);
            gpio_put(config.fan_gpio, 1);   
            break; 
        default:
        case THERMOSTAT_LOCKOUT:
            err = 1;
            break;         
        }

        hvac_log_state_change(thermostat_state);
    }
    else
    {
        err = 1;
    } 

    return(err);
}

/*!
 * \brief Timer callback
 *
 * \param[in]   timer handle      handle of timer that expired
 * 
 * \return nothing
 */
void vTimerCallback(TimerHandle_t xTimer)
 {
    int id;

    if (xTimer)
    {
        id = (int)pvTimerGetTimerID(xTimer);

        if ((id >= 0) && (id < NUM_HVAC_TIMERS))
        {
            climate_timers[id].timer_expired = true;
        }
    }
 }

 int initialize_hvac_control(void)
{
    int err = 0;
    int i;

    if (relay_gpio_enable)
    {
        // create hvac timers
        for (i=0; i < NUM_HVAC_TIMERS; i++)
        {
            climate_timers[i].timer_handle = xTimerCreate("Timer", 1000, pdFALSE, (void *)i, vTimerCallback);  

            printf("Created timer with handle = %p\n", climate_timers[i].timer_handle);
        }

        //initialize hvac gpios
        gpio_init(config.heating_gpio);
        gpio_put(config.heating_gpio, 0);
        gpio_set_dir(config.heating_gpio, true);

        gpio_init(config.cooling_gpio);
        gpio_put(config.cooling_gpio, 0);
        gpio_set_dir(config.cooling_gpio, true);  
        
        gpio_init(config.fan_gpio);
        gpio_put(config.fan_gpio, 0);
        gpio_set_dir(config.fan_gpio, true);

        err = 0;

        // // check hvac gpios are valid
        // if (gpio_valid(config.heating_gpio) && gpio_valid(config.cooling_gpio) && gpio_valid(config.fan_gpio))
        // {    
        //     // check hvac gpios are unique
        //     if ((config.heating_gpio != config.cooling_gpio) &&
        //         (config.heating_gpio != config.fan_gpio) &&
        //         (config.cooling_gpio != config.fan_gpio))
        //     {
        //         //initialize hvac gpios
        //         gpio_init(config.heating_gpio);
        //         gpio_put(config.heating_gpio, 0);
        //         gpio_set_dir(config.heating_gpio, true);

        //         gpio_init(config.cooling_gpio);
        //         gpio_put(config.cooling_gpio, 0);
        //         gpio_set_dir(config.cooling_gpio, true);  
                
        //         gpio_init(config.fan_gpio);
        //         gpio_put(config.fan_gpio, 0);
        //         gpio_set_dir(config.fan_gpio, true);
        //     }  
        // }
    }
    else
    {
        err = 1;
    }

    return(err);
}

/*!
 * \brief Compute the current heating and cooling setpoints
 * 
 * \return nothing
 */
int update_current_setpoints(THERMOSTAT_STATE_T last_active, long int temperaturex10)
{
    static SETPOINT_BIAS_T setpoint_bias = SETPOINT_BIAS_UNDEFINED;
    int i;
    int mow;
    int candidate_start_mow = 0;
    long int candidate_temperature = 0;
    long int candidate_heating_temperature = 0;
    long int candidate_cooling_temperature = 0;
    THERMOSTAT_MODE_T candidate_mode = HVAC_AUTO;
    int candidate_delta = 0;
    static bool cooling_disabled = false;
    static bool heating_disabled = false;
    static int current_start_mow = 0;
    int delta = 10079;  // number of minutes in a week

    // get setpoint according to schedule
    if (!get_mow_local_tz(&mow))
    {
        for(i=0; i < NUM_ROWS(config.setpoint_temperaturex10); i++)
        {
            
            if (schedule_setpoint_valid(config.setpoint_temperaturex10[i], config.setpoint_heating_temperaturex10[i], config.setpoint_cooling_temperaturex10[i], config.setpoint_start_mow[i], config.setpoint_mode[i]))
            {

                candidate_delta = mow_future_delta(config.setpoint_start_mow[i], mow);

                if (candidate_delta < delta)
                {
                    delta = candidate_delta;

                    candidate_temperature = config.setpoint_temperaturex10[i];
                    candidate_heating_temperature = config.setpoint_heating_temperaturex10[i];
                    candidate_cooling_temperature = config.setpoint_cooling_temperaturex10[i];
                    candidate_start_mow = config.setpoint_start_mow[i];
                    candidate_mode = config.setpoint_mode[i];
                }
            }
        }

        setpointtemperaturex10 = candidate_temperature;
        scheduled_mode = candidate_mode;
    }

    // check if we've entered a new schedule period
    if (current_start_mow != candidate_start_mow)
    {
        current_start_mow = candidate_start_mow;

        // zero out temporary offset
        temporary_set_point_offsetx10 = 0;
    }  

    // sanitize setpoints
    setpointtemperaturex10 = sanitize_setpoint(setpointtemperaturex10);
    candidate_heating_temperature = sanitize_setpoint(candidate_heating_temperature);
    candidate_cooling_temperature = sanitize_setpoint(candidate_cooling_temperature);

    if (scheduled_mode != HVAC_HEAT_AND_COOL)
    {
        // initial set points are identical
        web.thermostat_set_point = setpointtemperaturex10 + temporary_set_point_offsetx10;
        web.thermostat_heating_set_point = setpointtemperaturex10 + temporary_set_point_offsetx10;
        web.thermostat_cooling_set_point = setpointtemperaturex10 + temporary_set_point_offsetx10;

        // adjust setpoint bias based on last heating or cooling cycle run
        switch(last_active)
        {
        case HEATING_IN_PROGRESS:
            setpoint_bias = SETPOINT_BIAS_HEATING;
            break;
        case COOLING_IN_PROGRESS:
            setpoint_bias = SETPOINT_BIAS_COOLING;
            break; 
        default:
            break;       
        }
        
        // bias set points to avoid overlapping heating and cooling targets     
        switch(setpoint_bias)
        {
        case SETPOINT_BIAS_HEATING:
            web.thermostat_cooling_set_point += (3*config.thermostat_hysteresis);
            break;
        case SETPOINT_BIAS_COOLING:
            web.thermostat_heating_set_point -= (3*config.thermostat_hysteresis); 
            break;
        default:
        case SETPOINT_BIAS_UNDEFINED:
            if (temperaturex10 < web.thermostat_set_point)
            {
                web.thermostat_cooling_set_point += (3*config.thermostat_hysteresis);
            }
            else
            {
                web.thermostat_heating_set_point -= (3*config.thermostat_hysteresis);         
            }
            break;    
        }
    }
    else
    {
        web.thermostat_set_point = setpointtemperaturex10 + temporary_set_point_offsetx10;  // TODO:  not used -- what should this be?
        
        // force minimum hsyteresis
        if ((candidate_heating_temperature >= candidate_cooling_temperature) ||
            (abs(candidate_cooling_temperature - candidate_heating_temperature) < (3*config.thermostat_hysteresis)))
        {
            candidate_heating_temperature = candidate_cooling_temperature - 3*config.thermostat_hysteresis;
        }
        
        web.thermostat_heating_set_point = candidate_heating_temperature + temporary_set_point_offsetx10;
        web.thermostat_cooling_set_point = candidate_cooling_temperature + temporary_set_point_offsetx10;        
    }

    // adjust setpoint according to powerwall status
    switch(web.powerwall_grid_status)
    {
    case GRID_DOWN:
        // grid down setpoint adjustments -- relax setpoints when grid down
        web.thermostat_heating_set_point -= config.grid_down_heating_setpoint_decrease;
        web.thermostat_cooling_set_point += config.grid_down_cooling_setpoint_increase;

        // sanitize user configured battery levels
        CLIP(config.grid_down_cooling_disable_battery_level, 300, 950);   // 30% - 95%
        CLIP(config.grid_down_heating_disable_battery_level, 300, 950);   // 30% - 95%
        CLIP(config.grid_down_cooling_enable_battery_level,  350, 1000);  // 35% - 100%
        CLIP(config.grid_down_heating_enable_battery_level,  350, 1000);  // 35% - 100%

        if (config.grid_down_cooling_disable_battery_level >= config.grid_down_cooling_enable_battery_level)
        {
            config.grid_down_cooling_disable_battery_level = config.grid_down_cooling_enable_battery_level - 50;
        }

        if (config.grid_down_heating_disable_battery_level >= config.grid_down_heating_enable_battery_level)
        {
            config.grid_down_heating_disable_battery_level = config.grid_down_heating_enable_battery_level - 50;
        }        

        // disable cooling if battery level too low -- keep disabled until satisfactory level reached
        if ((web.powerwall_battery_percentage < config.grid_down_cooling_disable_battery_level) || cooling_disabled)
        {
            web.thermostat_cooling_set_point = 1500;  //max temp so that cooling is disabled
            cooling_disabled = true;
        }

        // disable heating if battery level too low -- keep disabled until satisfactory level reached
        if ((web.powerwall_battery_percentage < config.grid_down_heating_disable_battery_level) || heating_disabled)
        {
            web.thermostat_heating_set_point = -1000;  //min temp so that heating is disabled
            heating_disabled = true;
        }

        // enable cooling if battery level satisfactory
        if ((web.powerwall_battery_percentage > config.grid_down_cooling_enable_battery_level))
        {
            cooling_disabled = false;
        }    

        // enable heating if battery level satisfactory
        if ((web.powerwall_battery_percentage > config.grid_down_heating_enable_battery_level))
        {
            heating_disabled = false;
        }                  
        break;

    case GRID_UP:
        cooling_disabled = false;
        heating_disabled= false; 
        break;   
        
    case GRID_UNKNOWN:
    default:
        break;
    }

    return(setpointtemperaturex10);
}

int relay_gpio_enable(bool enable)
{
    relay_gpio_ok = enable;
}

long int sanitize_setpoint(long int setpoint)
{
    // sanitize setpoint
    if (config.use_archaic_units)
    {
        // fahrenheit between 60 and 90
        if ((setpoint > SETPOINT_MAX_FAHRENHEIT_X_10) || (setpoint < SETPOINT_MIN_FAHRENHEIT_X_10))
        {
            setpoint = SETPOINT_DEFAULT_FAHRENHEIT_X_10;
        }
    }
    else
    {
        // celsius between 15 and 32
        if ((setpoint > SETPOINT_MAX_CELSIUS_X_10) || (setpoint < SETPOINT_MIN_CELSIUS_X_10))
        {
            setpoint = SETPOINT_DEFAULT_CELSIUS_X_10;
        }
    }        

    return(setpoint);
}
