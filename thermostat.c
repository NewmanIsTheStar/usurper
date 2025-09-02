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

typedef enum
{
    HEATING_DATASET = 0,
    COOLING_DATASET = 1,
    STABLE_DATASET  = 2,    
    NUM_DATASETS    = 3
} CLIMATE_DATASET_T;

typedef enum
{
    NEGATIVE_DELTA = 0,
    POSITIVE_DELTA = 1,
    SMALL_DELATA   = 2,
    NUM_DELTAS     = 3    
} CLIMATE_DELTA_T;

typedef struct
{
    long int temperaturex10;
    long int humidityx10;
} CLIMATE_DATAPOINT_T;

typedef struct
{
    CLIMATE_DATAPOINT_T buffer[10];
    int buffer_index;
    int buffer_population;
} CLIMATE_HISTORY_T;

typedef enum
{
    TREND_DOWN    = 0,
    TREND_UP      = 1,
    TREND_STABLE  = 2,    
    NUM_TRENDS    = 3
} CLIMATE_TREND_DIRECTION_T;

typedef struct
{
    CLIMATE_DATAPOINT_T buffer[10];
    int buffer_index;
    int buffer_population;
    CLIMATE_DATAPOINT_T moving_average;
    int deltas[NUM_DELTAS];
    CLIMATE_TREND_DIRECTION_T current_trend;
    TickType_t trend_start_tick;
    TickType_t current_tick;
    TickType_t trend_length;
    int trend_up_max;
    int trend_down_min;
} CLIMATE_TREND_T;

typedef enum
{
    COOLING_MOMENTUM = 0,
    HEATING_MOMENTUM = 1,   
    NUM_MOMENTUMS   = 2
} CLIMATE_MOMENTUM_T;

typedef struct
{
    TickType_t change_tick;
    THERMOSTAT_STATE_T new_state;      
    int change_temperature;   
} HVAC_STATE_CHANGE_LOG_T;

typedef struct
{
    TimerHandle_t timer_handle;
    bool timer_expired;
} CLIMATE_TIMERS_T;

typedef enum
{
    HVAC_LOCKOUT_TIMER = 0,
    HVAC_OVERSHOOT_TIMER = 1,   
    NUM_HVAC_TIMERS   = 2
} CLIMATE_TIMER_INDEX_T;

typedef struct
{
    TickType_t hvac_off_tick[NUM_MOMENTUMS];
    long int hvac_off_temperature[NUM_MOMENTUMS];      
    bool measurement_in_progress[NUM_MOMENTUMS];
    TickType_t extrema_delay[NUM_MOMENTUMS]; 
    long int extrema_temperature[NUM_MOMENTUMS];
    TickType_t momentum_delay[NUM_MOMENTUMS];    
    long int  momentum_temperature_delta[NUM_MOMENTUMS];    
} CLIMATE_MOMENTUM_DATA_T;

// prototypes
int initialize_climate_metrics(void);
THERMOSTAT_STATE_T control_thermostat_relays(long int temperaturex10);
int accumlate_temperature_metrics(long int temperaturex10);
void mark_hvac_off(CLIMATE_MOMENTUM_T momentum_type, long int temperaturex10);
void track_hvac_extrema(CLIMATE_MOMENTUM_T momentum_type, long int temperaturex10);
void set_hvac_momentum(CLIMATE_MOMENTUM_T momentum_type);
void log_climate_change(int temperaturex10, int humidityx10);
int get_free_schedule_row(void);
bool schedule_row_valid(int row);
bool day_compare(int day1, int day2);
int set_hvac_gpio(THERMOSTAT_STATE_T thermostat_state);
int hvac_timer_start(CLIMATE_TIMER_INDEX_T timer_index, int minutes);
bool hvac_timer_expired(CLIMATE_TIMER_INDEX_T timer_index);
int update_current_setpoints(THERMOSTAT_STATE_T last_active);
void vTimerCallback(TimerHandle_t xTimer);
void hvac_log_state_change(THERMOSTAT_STATE_T new_state);
void enable_irq(bool state);
void gpio_isr(uint gpio, uint32_t events);
void hvac_update_display(int temperaturex10, THERMOSTAT_MODE_T hvac_mode, int hvac_setpoint);
int handle_button_press_with_timeout(QueueHandle_t irq_queue, TickType_t timeout);

// external variables
extern NON_VOL_VARIABLES_T config;
extern WEB_VARIABLES_T web;

// global variables
THERMOSTAT_MODE_T mode = HVAC_OFF;                       // operation mode
const uint8_t aht10_addr = 0x38;                         // i2c address of aht10 chip
const uint8_t aht10_busy_mask = 0x80;                    // aht10 busy bit in first rx byte
const uint8_t aht10_calibrated_mask = 0x08;              // aht10 calibrated bit in first rx byte
const uint ath10_i2c_timeout_us = 50000;                 // i2c timeout when reading or writing
const uint8_t aht10_initialize[]  = {0xe1, 0x08, 0x00};  // initialize, use_factory_calibration, nop
const uint8_t aht10_measurement[] = {0xac, 0x33, 0x00};  // start, measurement, nop
const uint8_t aht10_soft_reset[]  = {0xba};              // soft_reset
int temporary_set_point_offsetx10 = 0;
static int setpointtemperaturex10 = 0;

CLIMATE_HISTORY_T climate_history;
CLIMATE_MOMENTUM_DATA_T climate_momentum;
CLIMATE_TREND_T climate_trend;
CLIMATE_TIMERS_T climate_timers[NUM_HVAC_TIMERS];
int hvac_state_change_log_index = 0;
HVAC_STATE_CHANGE_LOG_T hvac_state_change_log[32];

QueueHandle_t irq_queue = NULL;
uint8_t passed_value;

/*!
 * \brief Monitor weather and control relay based on conditions and schedule
 *
 * \param params unused garbage
 * 
 * \return nothing
 */
void thermostat_task(void *params)
{
    bool i2c_error = false;
    int i2c_bytes_written = 0;
    int i2c_bytes_read = 0;
    bool aht10_initialized = false;
    bool tm1637_initialized = false;
    uint32_t temperature_native; 
    long int temperaturex10;
    uint32_t humidity_native; 
    long int humidityx10;
    uint8_t aht10_temp_humidity[7];
    int retry = 0;
    int oneshot = false;
    int i;

    irq_queue = xQueueCreate(1, sizeof(uint8_t));

     enable_irq(true);

    if (strcasecmp(APP_NAME, "Thermostat") == 0)
    {
        // force personality to match single purpose application
        config.personality = HVAC_THERMOSTAT;
    }

    config.thermostat_enable = 1;
    web.thermostat_hysteresis = 10; 

    //TODO Add web page to control these parameters
    config.heating_to_cooling_lockout_mins = 1;
    config.minimum_heating_on_mins = 1;
    config.minimum_cooling_on_mins = 1;
    config.minimum_heating_off_mins = 1;
    config.minimum_cooling_off_mins = 1;

    // configure gpio for front panel push buttons
    gpio_init(config.thermostat_mode_button_gpio);
    gpio_set_dir(config.thermostat_mode_button_gpio, false);    
    gpio_pull_up(config.thermostat_mode_button_gpio);

    gpio_init(config.thermostat_increase_button_gpio);
    gpio_set_dir(config.thermostat_increase_button_gpio, false);    
    gpio_pull_up(config.thermostat_increase_button_gpio);

    gpio_init(config.thermostat_decrease_button_gpio);
    gpio_set_dir(config.thermostat_decrease_button_gpio, false);    
    gpio_pull_up(config.thermostat_decrease_button_gpio);    

    printf("thermostat_task started!\n");

    // create hvac timers
    for (i=0; i < NUM_HVAC_TIMERS; i++)
    {
        climate_timers[i].timer_handle = xTimerCreate("Timer", 1000, pdFALSE, (void *)i, vTimerCallback);  

        printf("Created timer with handle = %p\n", climate_timers[i].timer_handle);
    }
 
    
    // initialize data structures for climate metrics
    initialize_climate_metrics();
    web.powerwall_grid_status = GRID_UNKNOWN;
    
    // check hvac gpios are valid
    if (gpio_valid(config.heating_gpio) && gpio_valid(config.cooling_gpio) && gpio_valid(config.fan_gpio))
    {    
        // check hvac gpios are unique
        if ((config.heating_gpio != config.cooling_gpio) &&
            (config.heating_gpio != config.fan_gpio) &&
            (config.cooling_gpio != config.fan_gpio))
        {
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
        }  
    }
  
    // initialize i2c for temperature sensor  TODO: set i2c block based on selected gpio pins
    i2c_init(i2c1, 100000);
    gpio_set_function(config.thermostat_temperature_sensor_data_gpio, GPIO_FUNC_I2C);
    gpio_set_function(config.thermostat_temperature_sensor_clock_gpio, GPIO_FUNC_I2C);
    gpio_pull_up(config.thermostat_temperature_sensor_data_gpio);
    gpio_pull_up(config.thermostat_temperature_sensor_clock_gpio);

    powerwall_init();

    // create the schedule grid used in web inteface
    make_schedule_grid();
    
    while (true)
    {        
        if ((config.personality == HVAC_THERMOSTAT))
        {
            if (!tm1637_initialized)
            {
                // TODO make display pins configurable
                tm1637_init(config.thermostat_seven_segment_display_clock_gpio, config.thermostat_seven_segment_display_data_gpio);
                tm1637_display(0, true);

                tm1637_initialized = true;               
            }
            i2c_error = false;

            if (!aht10_initialized)
            {
                SLEEP_MS(500);

                // initialize temperature sensor
                printf("Initializing temperature sensor...\n"); 

                // soft reset
                i2c_write_timeout_us(i2c1, aht10_addr, aht10_soft_reset, sizeof(aht10_soft_reset), false, ath10_i2c_timeout_us);
                SLEEP_MS(20);
                
                if (!i2c_error)
                {
                    // initialize
                    i2c_bytes_written = i2c_write_timeout_us(i2c1, aht10_addr, aht10_initialize, sizeof(aht10_initialize), false, ath10_i2c_timeout_us);
                    if (i2c_bytes_written < 1) // only the first byte is acknowledged by some aht10 devices
                    {    
                        i2c_error = true;                        
                    }
                } 

                if (!i2c_error)
                {                                
                    printf("Completed initialization of temperature sensor\n");
                    aht10_initialized = true;
                    SLEEP_MS(500);
                }
            }

            if (!i2c_error)
            {  
                // start measurement
                i2c_bytes_written = i2c_write_timeout_us(i2c1, aht10_addr, aht10_measurement, sizeof(aht10_measurement), true, ath10_i2c_timeout_us);
                if (i2c_bytes_written != sizeof(aht10_measurement))
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
                    temperaturex10 = ((long int)temperature_native*2000)/1048576 - 500;

                    if (config.use_archaic_units)
                    {
                        temperaturex10 = (temperaturex10*9)/5 + 320;
                    }

                    // extract 20-bit raw humidity data (2^20 steps from 0% to 100%)
                    humidity_native = (((uint32_t) aht10_temp_humidity[1] << 16) | ((uint16_t) aht10_temp_humidity[2] << 8) | (aht10_temp_humidity[3])) >> 4; 

                    // convert native humidity to percentage x 10
                    humidityx10 = ((long int)humidity_native*1000)/1048576;

                    //printf("Temperature = %ld.%ld Humidity = %ld.%ld\n", temperaturex10/10, temperaturex10%10, humidityx10/10, humidityx10%10);
                    //send_syslog_message("temperature", "Temperature = %ld.%ld Humidity = %ld.%ld\n", temperaturex10/10, temperaturex10%10, humidityx10/10, humidityx10%10);                    
                    log_climate_change(temperaturex10, humidityx10);

                    track_hvac_extrema(COOLING_MOMENTUM, temperaturex10);
                    track_hvac_extrema(HEATING_MOMENTUM, temperaturex10);                     
                    //powerwall_poll();
                    control_thermostat_relays(temperaturex10);
                    accumlate_temperature_metrics(temperaturex10);

                    // update web ui
                    web.thermostat_temperature = temperaturex10;
                    
                    // update seven segment display
                    //tm1637_display(temperaturex10, false);

                    hvac_update_display(temperaturex10, mode, setpointtemperaturex10 + temporary_set_point_offsetx10);
                }
                else
                {
                    printf("ath10: discarded measurement because it was either incomplete or uncalibrated\n");
                    i2c_error = true;
                }           
            }

            if (i2c_error)
            {
                printf("aht10: i2c error occured will attempt soft reset\n");
                aht10_initialized = false;                
            }

            //SLEEP_MS(1000);

            // Wait up to 1000ms or until an IRQ is received
            handle_button_press_with_timeout(irq_queue, 1000);

            // if (xQueueReceive(irq_queue, &passed_value, 1000) == pdPASS)
            // {
            //     switch(passed_value)
            //     {
            //         default:
            //             printf("Unexpected IRQ in message: %d\n", passed_value);
            //             printf("WARNING: not reenabling IRQ\n");
            //             break;
            //         case 16:
            //         case 17:
            //         case 22:
            //             printf("IRQ detected from GPIO%d\n", passed_value);
            //             enable_irq(true);
            //             break;
            //     }
            // }

            // if (gpio_get(config.thermostat_increase_button_gpio) == false)
            // {                
            //     temporary_set_point_offsetx10+=10;
            //     printf("Button pressed. Setpoint offset = %d\n", temporary_set_point_offsetx10);                
            // }

            // if (gpio_get(config.thermostat_decrease_button_gpio) == false)
            // {                
            //     temporary_set_point_offsetx10-=10;
            //     printf("Button pressed. Setpoint offset = %d\n", temporary_set_point_offsetx10);                
            // }            
        }  
        else
        {
            SLEEP_MS(60000); 
        }   

        // tell watchdog task that we are still alive
        watchdog_pulse((int *)params);               
    }
}

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
    TickType_t lockout_remaining_ticks = 0;       

    // check powerwall status -- TODO move up a level?
    powerwall_check();

    // determine current setpoints based on schedule and powerwall status
    update_current_setpoints(last_active);

    // update thermostat state
    switch(thermostat_state)
    {
    default:    
    case HEATING_AND_COOLING_OFF:
        if (temperaturex10 > (web.thermostat_cooling_set_point + web.thermostat_hysteresis))
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

                set_hvac_momentum(COOLING_MOMENTUM);
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
        } else if (temperaturex10 < (web.thermostat_heating_set_point - web.thermostat_hysteresis))
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

                set_hvac_momentum(HEATING_MOMENTUM);  
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
        break;
    case HEATING_IN_PROGRESS:
        if (temperaturex10 > (web.thermostat_heating_set_point + web.thermostat_hysteresis))
        {
            send_syslog_message("thermostat", "Heating completed");            
            snprintf(web.status_message, sizeof(web.status_message), "Heating completed");  
            mark_hvac_off(HEATING_MOMENTUM, temperaturex10);         

            set_hvac_gpio(HEATING_AND_COOLING_OFF);

            // check for excessive overshoot that could trigger cooling
            if (temperaturex10 > (web.thermostat_cooling_set_point - web.thermostat_hysteresis))
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
        break;
    case COOLING_IN_PROGRESS:
        if (temperaturex10 < (web.thermostat_cooling_set_point - web.thermostat_hysteresis))
        {
            send_syslog_message("thermostat", "Cooling completed");            
            snprintf(web.status_message, sizeof(web.status_message), "Cooling completed"); 
            mark_hvac_off(COOLING_MOMENTUM, temperaturex10);

            set_hvac_gpio(HEATING_AND_COOLING_OFF);

            // check for excessive overshoot that could trigger heating
            if (temperaturex10 < (web.thermostat_heating_set_point + web.thermostat_hysteresis))
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
            printf("HVAC CONTROL DAMPING LOCKOUT -- no control changed permitted at present\n");
        }               
        break;    
    case EXCESSIVE_OVERSHOOT:    
        if ((last_active == HEATING_IN_PROGRESS) &&
            (temperaturex10 < (web.thermostat_heating_set_point + web.thermostat_hysteresis)))
        {
            send_syslog_message("thermostat", "Temperature has fallen to target range. Resuming operation");            
            snprintf(web.status_message, sizeof(web.status_message), "Resuming operation"); 

            thermostat_state = HEATING_AND_COOLING_OFF;
        }

        if ((last_active == COOLING_IN_PROGRESS) &&
            (temperaturex10 > (web.thermostat_cooling_set_point - web.thermostat_hysteresis)))
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
 * \brief Check timer
 * 
 * \return true if timer expired
 */
bool hvac_timer_expired(CLIMATE_TIMER_INDEX_T timer_index)
{
    bool expired = false;          
 
    expired = climate_timers[HVAC_LOCKOUT_TIMER].timer_expired;
    
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
    if (config.thermostat_enable && gpio_valid(config.heating_gpio) && gpio_valid(config.cooling_gpio) && gpio_valid(config.fan_gpio))
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
            gpio_put(config.fan_gpio, 0);   
            break;
        case COOLING_IN_PROGRESS:
            printf("Cooling on, Heating and Fan off\n");
            gpio_put(config.heating_gpio, 0);
            gpio_put(config.cooling_gpio, 1);
            gpio_put(config.fan_gpio, 0);   
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
 * \brief Initialize Temperature Metrics
 * 
 * \return 0 irrigation off, 1 irrigation on, 1 irrigation usurped
 */
int initialize_climate_metrics(void)
{
    memset(&climate_history, 0, sizeof(climate_history));
    memset(&climate_momentum, 0, sizeof(climate_momentum));
    memset(&climate_trend, 0, sizeof(climate_trend));
    climate_trend.trend_up_max   = -500;   // -50.0  Celcius
    climate_trend.trend_down_min = -1500;  // +150.0 Celcius

    return(0);
}

/*!
 * \brief Accumulate Temperature Metrics
 * 
 * \return 0 irrigation off, 1 irrigation on, 1 irrigation usurped
 */
int accumlate_temperature_metrics(long int temperaturex10)
{
    int i;
    int gradient = 0;

    climate_trend.buffer[climate_history.buffer_index].temperaturex10 = temperaturex10 - climate_history.buffer[(climate_history.buffer_index + NUM_ROWS(climate_history.buffer) - 1)%NUM_ROWS(climate_history.buffer)].temperaturex10;
    climate_history.buffer[climate_history.buffer_index].temperaturex10 = temperaturex10;

    climate_history.buffer_index  = (climate_history.buffer_index  + 1)%NUM_ROWS(climate_history.buffer);

    if (climate_history.buffer_population < NUM_ROWS(climate_history.buffer)) climate_history.buffer_population++;
    //printf("population = %d\n", climate_history.buffer_population);

    climate_trend.moving_average.temperaturex10 = 0;
    gradient = 0;
    for(i=0; i < climate_history.buffer_population; i++)
    {
        climate_trend.moving_average.temperaturex10 += climate_history.buffer[i].temperaturex10; 
        gradient += climate_trend.buffer[i].temperaturex10;
    }
    climate_trend.moving_average.temperaturex10 = climate_trend.moving_average.temperaturex10/climate_history.buffer_population;
    gradient = gradient*100/climate_history.buffer_population;

    // zero delta counters
    climate_trend.deltas[NEGATIVE_DELTA] = 0;
    climate_trend.deltas[POSITIVE_DELTA] = 0;
    climate_trend.deltas[SMALL_DELATA] = 0;

    // count deltas
    for(i=0; i < climate_history.buffer_population; i++)
    {
        if (climate_trend.buffer[i].temperaturex10 < 0) climate_trend.deltas[NEGATIVE_DELTA]++;
        if (climate_trend.buffer[i].temperaturex10 > 0) climate_trend.deltas[POSITIVE_DELTA]++;
        if (abs(climate_trend.buffer[i].temperaturex10) < 5) climate_trend.deltas[SMALL_DELATA]++;
    }

    //printf("Temp Sample = %d\tMoving Average = %d [%d, %d, %d] ", temperaturex10, climate_trend.moving_average, climate_trend.deltas[NEGATIVE_DELTA], climate_trend.deltas[POSITIVE_DELTA], climate_trend.deltas[SMALL_DELATA]);

    if ((climate_trend.deltas[NEGATIVE_DELTA]>= 1) && (climate_trend.deltas[POSITIVE_DELTA] == 0))
    {
        //printf("Trending down @ %d per sample\n", gradient);
        if (climate_trend.current_trend != TREND_DOWN)
        {
            climate_trend.current_tick = xTaskGetTickCount();
            climate_trend.trend_length = climate_trend.current_tick - climate_trend.trend_start_tick;
            //printf("Trend changed at tick %lu.  Trend lasted %lu. Maximum = %d\n", climate_trend.current_tick, climate_trend.trend_length, climate_trend.trend_up_max);

            climate_trend.current_trend = TREND_DOWN;
            climate_trend.trend_down_min = 1500;
            climate_trend.trend_start_tick = climate_trend.current_tick;
        }
        else
        {
            if (temperaturex10 < climate_trend.trend_down_min)
            {
                climate_trend.trend_down_min = temperaturex10;
            }
        }
    } 
    else if ((climate_trend.deltas[POSITIVE_DELTA] >= 1) && (climate_trend.deltas[NEGATIVE_DELTA] == 0))
    {
        //printf("Trending up @ %d per sample\n", gradient);
        if (climate_trend.current_trend != TREND_UP)
        {
            climate_trend.current_tick = xTaskGetTickCount();
            climate_trend.trend_length = climate_trend.current_tick - climate_trend.trend_start_tick;
            //printf("Trend changed at tick %lu.  Trend lasted %lu. Minimum = %d\n", climate_trend.current_tick, climate_trend.trend_length, climate_trend.trend_down_min);

            climate_trend.current_trend  = TREND_UP;
            climate_trend.trend_up_max = -500;
            climate_trend.trend_start_tick = climate_trend.current_tick;
        }   
        else
        {
            if (temperaturex10 > climate_trend.trend_up_max)
            {
                climate_trend.trend_up_max = temperaturex10;
            }
        }          
    }
    else if ((climate_trend.deltas[SMALL_DELATA] == 10) && ((climate_trend.deltas[NEGATIVE_DELTA]-climate_trend.deltas[POSITIVE_DELTA]) < 3))
    {
        //printf("Stable\n");   // no change to trend as we allow the previous trend to continue after a plateau    
    }
    else
    {
        //printf("No trend detected\n");
    } 

    return(climate_trend.moving_average.temperaturex10);
}


/*!
 * \brief Record when hvac heating or cooling ended
 * 
 * \return nothing
 */
void mark_hvac_off(CLIMATE_MOMENTUM_T momentum_type, long int temperaturex10)
{
    climate_momentum.hvac_off_tick[momentum_type] = xTaskGetTickCount();
    climate_momentum.hvac_off_temperature[momentum_type] = temperaturex10;    
    climate_momentum.extrema_delay[momentum_type]= 0;
    climate_momentum.extrema_temperature[momentum_type]= 0;    
    climate_momentum.measurement_in_progress[momentum_type] = true;
}

/*!
 * \brief Track how long heating or cooling continued after hvac stopped
 * 
 * \return nothing
 */
void track_hvac_extrema(CLIMATE_MOMENTUM_T momentum_type, long int temperaturex10)
{
    if (climate_momentum.measurement_in_progress[momentum_type])
    {
        switch(momentum_type)
        {
        case HEATING_MOMENTUM:
            if (temperaturex10 > climate_momentum.extrema_temperature[momentum_type])
            {
                climate_momentum.extrema_delay[momentum_type] = xTaskGetTickCount() - climate_momentum.hvac_off_tick[momentum_type];
                climate_momentum.extrema_temperature[momentum_type] =  temperaturex10;
            }
            break;
        case COOLING_MOMENTUM:
            if (temperaturex10 < climate_momentum.extrema_temperature[momentum_type])
            {
                climate_momentum.extrema_delay[momentum_type] = xTaskGetTickCount() - climate_momentum.hvac_off_tick[momentum_type];
                climate_momentum.extrema_temperature[momentum_type] =  temperaturex10;
            }    
            break;
        default:
            break;
        }
    }
}


/*!
 * \brief Set momentum based on tracked extrema
 * 
 * \return nothing
 */
void set_hvac_momentum(CLIMATE_MOMENTUM_T momentum_type)
{
    if (climate_momentum.measurement_in_progress[momentum_type])
    {
        climate_momentum.momentum_delay[momentum_type] = xTaskGetTickCount() - climate_momentum.hvac_off_tick[momentum_type];
        climate_momentum.momentum_temperature_delta[momentum_type] =  climate_momentum.extrema_temperature[momentum_type] - climate_momentum.hvac_off_temperature[momentum_type]; 
        climate_momentum.measurement_in_progress[momentum_type] = false;

        // add sanity check / constraints

        //printf("MOMENTUM LAST CYCLE:  Extrema occured %lu ms after HVAC shut off and temperature change was %ld\n", climate_momentum.momentum_delay[momentum_type], climate_momentum.momentum_temperature_delta[momentum_type]);
    }
}

/*!
 * \brief Compute the current heating and cooling setpoints
 * 
 * \return nothing
 */
int update_current_setpoints(THERMOSTAT_STATE_T last_active)
{
    int i;
    int mow;
    int candidate_start_mow = 0;
    static bool cooling_disabled = false;
    static bool heating_disabled = false;
    static int current_start_mow = 0;

    // get setpoint according to schedule
    if (!get_mow_local_tz(&mow))
    {
        for(i=0; i < NUM_ROWS(config.setpoint_temperaturex10); i++)
        {
            if ((config.setpoint_start_mow[i] < mow) &&
                (config.setpoint_start_mow[i] > candidate_start_mow))
            {
                setpointtemperaturex10 = config.setpoint_temperaturex10[i] + temporary_set_point_offsetx10;
                candidate_start_mow = config.setpoint_start_mow[i];
            }
        }
    }

    // check if we've entered a new schedule period
    if (current_start_mow != candidate_start_mow)
    {
        current_start_mow = candidate_start_mow;

        // zero out temporary offset
        temporary_set_point_offsetx10 = 0;
    }
    
    // // add temporary offset -- entered by user pressing buttons on front panel
    // setpointtemperaturex10 += temporary_set_point_offsetx10;    

    // sanitize setpoint
    if (config.use_archaic_units)
    {
        // fahrenheit between 60 and 90
        if ((setpointtemperaturex10 >900) || (setpointtemperaturex10<600))
        {
            setpointtemperaturex10 = 700;
        }
    }
    else
    {
        // celcius between 21 and 32
        if ((setpointtemperaturex10 >320) || (setpointtemperaturex10<150))
        {
            setpointtemperaturex10 = 210;
        }
    }    

    // initial set points are identical
    web.thermostat_set_point = setpointtemperaturex10;
    web.thermostat_heating_set_point = setpointtemperaturex10;
    web.thermostat_cooling_set_point = setpointtemperaturex10;

    // bias set points to avoid overlapping heating and cooling targets 
    if (last_active == HEATING_IN_PROGRESS)
    {
        // increase cooling setpoint since we have recently run a heating cycle
        web.thermostat_cooling_set_point += (3*web.thermostat_hysteresis);
    } else if (last_active == COOLING_IN_PROGRESS)
    {
         // decrease heating setpoint since we have recently run a cooling cycle
         web.thermostat_heating_set_point -= (3*web.thermostat_hysteresis);       
    }

    // adjust setpoint according to powerwall status
    switch(web.powerwall_grid_status)
    {
    case GRID_DOWN:
        // grid down setpoint adjustments -- relax setpoints when grid down
        web.thermostat_heating_set_point -= config.grid_down_heating_setpoint_decrease;
        web.thermostat_cooling_set_point += config.grid_down_cooling_setpoint_increase;

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

/*!
 * \brief Create thermostat schedule grid
 * 
 * \return nothing
 */
int make_schedule_grid(void)
{
    int i, j, x, y;
    int key_mow = 0;
    int key_temp = 0;
    //int mow;
    int mod;
    int setpointtemperaturex10 = 0;
    bool found = false;
    int populated_rows = 0;
    int mow[NUM_ROWS(config.setpoint_start_mow)];
    int temp[NUM_ROWS(config.setpoint_start_mow)];

    // erase start time colum
    for(y=0;y<8; y++)
    {
        web.thermostat_grid[0][y]= -1;
    }

    // set all temperatures invalid
    for(x=1; x<8; x++)
    {
        for (y=0; y<8; y++)
        {
            web.thermostat_grid[x][y] = -300; 
        }
    }

    // copy schedule to local arrays for sorting
    for(i=0; i<NUM_ROWS(config.setpoint_start_mow); i++)
    {
        mow[i] = config.setpoint_start_mow[i];
        temp[i] = config.setpoint_temperaturex10[i];
    }

    // sort the schedule into ascending order by time of day (tod)
    for(i=1; i<NUM_ROWS(mow); i++)
    {
        key_mow = mow[i];
        key_temp = temp[i];        
        j = i - 1;

        while ((j >= 0) && ((mow[j]%(60*24)) > (key_mow%(60*24))))
        {
            mow[j+1] = mow[j];
            temp[j+1] = temp[j];            
            j = j - 1;
        }

        mow[j+1] = key_mow;
        temp[j+1] = key_temp;            
    }

    // scan list of configured setpoints
    for (i=0; i<NUM_ROWS(mow); i++)
    {
        if ((mow[i] >= 0) && (mow[i] < 60*24*7))
        {
            x = mow[i]/(60*24) + 1;  // day + 1
            mod = mow[i]%(60*24);        
            
            found = false;

            for(y=0; y < populated_rows; y++)
            {
                if (web.thermostat_grid[0][y] == mod)
                {
                    //insert into existing grid row
                    CLIP(x, 1, 7);
                    web.thermostat_grid[x][y] = temp[i];
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                if (populated_rows < 7)
                {
                    web.thermostat_grid[0][populated_rows] = mod;
                    web.thermostat_grid[x][populated_rows] = temp[i];

                    populated_rows++;
                }
            }
        }
    }

    // // dump grid
    // for(y=0; y<8; y++)
    // {
    //     printf("\nGRID: ");
    //     for (x=0; x<8; x++)
    //     {
    //          printf("[%d]", web.thermostat_grid[x][y]);
    //     }
    // } 
    // printf("\n");

    return(0);
}

/*!
 * \brief Copy daily schedule to other day(s)
 * 
 * \return nothing
 */
int copy_schedule(int source_day, int destination_day)
{
    int i, j, k, x, y;
    int key_mow = 0;
    int key_temp = 0;
    //int mow;
    int mod;
    int setpointtemperaturex10 = 0;
    bool found = false;
    int populated_rows = 0;
    int mow[NUM_ROWS(config.setpoint_start_mow)];
    int temp[NUM_ROWS(config.setpoint_start_mow)];
    int day = 0;

    CLIP(source_day, 0, 6);

    // erase existing entries on destination days
    for(i=0; i<NUM_ROWS(config.setpoint_start_mow); i++)
    {
        if ((config.setpoint_start_mow[i] >= 0) && (config.setpoint_start_mow[i] < 60*24*7))
        {
            day = config.setpoint_start_mow[i]/(60*24);
            CLIP(day, 0, 6);
        }

        // 0-6 = sunday to saturday, 7 = everyday, 8 = weekdays, 9 = weekend days
        if ((day != source_day) && (!day_compare(day, destination_day)))                                                                      
        {
            // mark unused    
            config.setpoint_start_mow[i] = -1;
            config.setpoint_temperaturex10[i] = -2000;
        }
    }


    for (day = 0; day < 7; day++)   // day to copy into
    {
        for (i=0; i < NUM_ROWS(config.setpoint_start_mow); i++)  // scan existing schedule
        {
            if (schedule_row_valid(i))
            {
                j = config.setpoint_start_mow[i]/(60*24);
                mod = config.setpoint_start_mow[i]%(60*24);
                CLIP(j, 0, 6);
                CLIP(mod, 0, 60*24);

                // 0-6 = sunday to saturday, 7 = everyday, 8 = weekdays, 9 = weekend days
                if ((day != source_day) && (j == source_day) && (!day_compare(day, destination_day)))
                {
                    k = get_free_schedule_row();

                    if ((k >= 0) && (k < NUM_ROWS(config.setpoint_start_mow)))
                    {
                        // copy schedule
                        config.setpoint_start_mow[k] = day*(60*24) + mod;
                        config.setpoint_temperaturex10[k] = config.setpoint_temperaturex10[i];

                        //printf("Copied row. New row %d [dest day = %d source day j = %d source row i = %d]\n", k, day, j, i);
                    }
                }
             }
        }
    }

    return(0);
}

/*!
 * \brief check if day matchs
 * 
 * \return return false if match, true if diffeent
 */
bool day_compare(int day1, int day2)
{
    bool diff = true;
    int multi_day = 0;
    int compare_day = 0;

    if (day1 == day2)
    {
        diff = false;
    }

    if (day1 > 6)
    {
        multi_day = day1;
        compare_day = day2;
    }
    else
    {
        multi_day = day2;
        compare_day = day1;    
    }

    switch(multi_day)
    {
        case 7: // everyday
            diff = false;
            break;
        case 8: // weekday
            if ((compare_day >=1) && (compare_day <= 5))
            {
                diff = false;
            } 
            break;
        case 9: // weekend
            if ((compare_day == 0) || (compare_day == 6))
            {
                diff = false;
            }
            break;
    }
    
    return(diff);
}

/*!
 * \brief check if temperature schedule row is valid
 * 
 * \return true if temperature schedule entry is valid
 */
bool schedule_row_valid(int row)
{
    bool valid = true;


    if ((row < 0) || (row > NUM_ROWS(config.setpoint_start_mow)))
    {
        valid = false;
    }
    else if ((config.setpoint_start_mow[row] < 0) ||  (config.setpoint_start_mow[row] > (60*24*7))) 
    {
        valid = false;
    }
    else if ((config.setpoint_temperaturex10[row] < (-1000)) ||  (config.setpoint_temperaturex10[row] > (1000))) 
    {
        valid = false;
    }    

    //printf("ROW %d mow = %d temp = %d %s\n", row, config.setpoint_start_mow[row], config.setpoint_temperaturex10[row], valid?"TRUE":"FALSE");

    return(valid);
}

/*!
 * \brief Copy daily schedule to other day(s)
 * 
 * \return nothing
 */
int get_free_schedule_row(void)
{
    int i = 0;
    bool found = false;

    for(i=0; i<NUM_ROWS(config.setpoint_start_mow); i++)
    {
        if (!schedule_row_valid(i))
        {
            found = true;
            break;
        }
    }

    if (!found) i = -1;

    return(i);
}

/*!
 * \brief Send a climate syslog message
 *
 * \param[in]   log_name      name of log file on server
 * \param[in]   format, ...   variable parameters printf style  
 * 
 * \return num bytes sent or -1 on error
 */
void log_climate_change(int temperaturex10, int humidityx10)
{
    static int sent_temperaturex10 = 0;
    static int sent_humidityx10 = 0;

    // check if values changed
    if ((temperaturex10 != sent_temperaturex10) || (humidityx10 != sent_humidityx10))
    {
        send_syslog_message("temperature", "Temperature = %ld.%ld Humidity = %ld.%ld\n", temperaturex10/10, temperaturex10%10, humidityx10/10, humidityx10%10);

        // remember what we sent
        sent_temperaturex10 = temperaturex10;
        sent_humidityx10 = humidityx10;
    }
    
    return;
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



 /*!
 * \brief Timer callback
 *
 * \param[in]   timer handle      handle of timer that expired
 * 
 * \return nothing
 */
void hvac_log_state_change(THERMOSTAT_STATE_T new_state)
{
    static THERMOSTAT_STATE_T previous_state = DUCT_PURGE;

    if (new_state != previous_state)
    {
        hvac_state_change_log[hvac_state_change_log_index].change_tick = xTaskGetTickCount();
        hvac_state_change_log[hvac_state_change_log_index].new_state = new_state;
        hvac_state_change_log[hvac_state_change_log_index].change_temperature = web.thermostat_temperature;

        hvac_state_change_log_index = (hvac_state_change_log_index + 1) % NUM_ROWS(hvac_state_change_log);
    }
}


 /*!
 * \brief Control Seven Segment Display
 *
 * \param[in]   timer handle      handle of timer that expired
 * 
 * \return nothing
 */
void hvac_update_display(int temperaturex10, THERMOSTAT_MODE_T hvac_mode, int hvac_setpointx10)
{
    static int display_state = 0;
    static int last_hvac_mode = 0;
    static int last_hvac_setpoint = 0;    
    static TickType_t last_display_state_change_tick = 0;
    TickType_t now_tick = 0;

    now_tick = xTaskGetTickCount();

    if (hvac_mode != last_hvac_mode)
    {
        // process mode change
        switch(hvac_mode)
        {
        default:
        case HVAC_OFF:
            display_state = 1;
            break;
        case HVAC_HEATING_ONLY:
            display_state = 2;
            break;
        case HVAC_COOLING_ONLY:
            display_state = 3;
            break;
        case HVAC_FAN_ONLY:
            display_state = 4;
            break;
        case HVAC_AUTO:
            display_state = 5;
            break;
        }

        last_hvac_mode = hvac_mode;
        last_display_state_change_tick = now_tick;  
    }
    else if (hvac_setpointx10 != last_hvac_setpoint)
    {
        // process setpoint change
        display_state = 6;

        last_hvac_setpoint = hvac_setpointx10;
        last_display_state_change_tick = now_tick;        
    }
    else if ((now_tick-last_display_state_change_tick) > 10000)
    {
        // revert to displaying current temperature
        display_state = 0;    
    }

    switch(display_state)
    {
        default:
        case 0:
            // update seven segment display
            tm1637_display(temperaturex10/10, false);        
            break;
        case 1:
            tm1637_display_word("OFF", false);       
            break;  
        case 2:
            tm1637_display_word("HEAT", false);     
            break;             
        case 3:
            tm1637_display_word("COOL", false);      
            break;  
        case 4:
            tm1637_display_word("FAN", false);    
            break;             
        case 5:
            tm1637_display_word("AUTO", false);    
            break;                                                 
        case 6:
            tm1637_display(hvac_setpointx10/10, false); 
            break;
    }
}

void enable_irq(bool state) 
{
    gpio_set_irq_enabled_with_callback(config.thermostat_mode_button_gpio,
                                       GPIO_IRQ_EDGE_FALL,
                                       state,
                                       &gpio_isr);

    gpio_set_irq_enabled_with_callback(config.thermostat_increase_button_gpio,
                                       GPIO_IRQ_EDGE_FALL,
                                       state,
                                       &gpio_isr);

    gpio_set_irq_enabled_with_callback(config.thermostat_decrease_button_gpio,
                                       GPIO_IRQ_EDGE_FALL,
                                       state,
                                       &gpio_isr);                                                                                                                     
}

void gpio_isr(uint gpio, uint32_t events)
{
  // Clear the URQ source
  enable_irq(false);

  static uint8_t irq_gpio_number = GP_UNINITIALIZED;

  irq_gpio_number = gpio;

  // Signal the alert clearance task
  xQueueSendToBackFromISR(irq_queue, &irq_gpio_number, 0);

}

int handle_button_press_with_timeout(QueueHandle_t irq_queue, TickType_t timeout)
{
    int err = 0;

    if (xQueueReceive(irq_queue, &passed_value, 1000) == pdPASS)
    {
        switch(passed_value)
        {
            default:
                printf("Unexpected IRQ in message: %d\n", passed_value);
                printf("WARNING: not reenabling IRQ\n");
                break;
            case 16:
            case 17:
            case 22:
                printf("IRQ detected from GPIO%d\n", passed_value);
                enable_irq(true);
                break;
        }
    }

    if (gpio_get(config.thermostat_increase_button_gpio) == false)
    {                
        temporary_set_point_offsetx10+=10;
        printf("Button pressed. Setpoint offset = %d\n", temporary_set_point_offsetx10);                
    }

    if (gpio_get(config.thermostat_decrease_button_gpio) == false)
    {                
        temporary_set_point_offsetx10-=10;
        printf("Button pressed. Setpoint offset = %d\n", temporary_set_point_offsetx10);                
    }

    if (gpio_get(config.thermostat_mode_button_gpio) == false)
    {                
        mode++;
        if (mode > HVAC_AUTO) mode = HVAC_OFF;

        printf("Button pressed. Mode = %d\n", mode);                
    }
    
    printf("TEMP = %d SETPOINT = %d MODE = %d\n", web.thermostat_temperature, setpointtemperaturex10 + temporary_set_point_offsetx10, mode);

    return(err);
}