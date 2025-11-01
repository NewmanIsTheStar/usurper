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
    DISPLAY_TEMPERATURE = 0,
    DISPLAY_HVAC_OFF    = 1,
    DISPLAY_HVAC_HEAT   = 2,    
    DISPLAY_HVAC_COOL   = 3,
    DISPLAY_HVAC_FAN    = 4,
    DISPLAY_HVAC_AUTO   = 5,
    DISPLAY_SETPOINT    = 6
} DISPLAY_STATE_T;

// external variables
extern NON_VOL_VARIABLES_T config;
extern WEB_VARIABLES_T web;
extern THERMOSTAT_MODE_T mode;
extern int setpointtemperaturex10;
extern int temporary_set_point_offsetx10;

// global variables
QueueHandle_t irq_queue = NULL;
uint8_t passed_value;

/*!
 * \brief process button presses until inactivity timeout occurs
 * 
 * \return nothing
 */
bool handle_button_press_with_timeout(TickType_t timeout)
{
    bool button_pressed = false;
    int max_iterations = 400;

    do
    {
        if (xQueueReceive(irq_queue, &passed_value, 1000) == pdPASS)
        {
            if ((passed_value == config.thermostat_increase_button_gpio) ||
                (passed_value == config.thermostat_decrease_button_gpio) ||
                (passed_value == config.thermostat_mode_button_gpio))
            {
                printf("IRQ detected from GPIO%d\n", passed_value);
                enable_irq(true);
                button_pressed = true;                
            }
            else
            {
                printf("Unexpected IRQ in message: %d\n", passed_value);
                printf("WARNING: not reenabling IRQ\n");
                button_pressed = false;                
            }
        }
        else
        {
            button_pressed = false;
        }

        if (gpio_get(config.thermostat_increase_button_gpio) == false)
        {                
            temporary_set_point_offsetx10+=10;
            printf("INCREASE Button pressed. Setpoint offset = %d\n", temporary_set_point_offsetx10);                
        }

        if (gpio_get(config.thermostat_decrease_button_gpio) == false)
        {                
            temporary_set_point_offsetx10-=10;
            printf("DECREASE Button pressed. Setpoint offset = %d\n", temporary_set_point_offsetx10);                
        }

        if (gpio_get(config.thermostat_mode_button_gpio) == false)
        {                
            mode++;
            if (mode > HVAC_AUTO) mode = HVAC_OFF;

            printf("MODE Button pressed. Mode = %d\n", mode);                
        }

        // update display
        hvac_update_display(web.thermostat_temperature, mode, setpointtemperaturex10 + temporary_set_point_offsetx10);
        printf("TEMP = %d SETPOINT = %d (%d + %d) MODE = %d\n", web.thermostat_temperature, setpointtemperaturex10 + temporary_set_point_offsetx10, setpointtemperaturex10, temporary_set_point_offsetx10,mode);

        // deal with continual spurious interrupts or stuck button holding us in this loop forever
        if (--max_iterations <=0) break;

    } while (button_pressed);  

    return(button_pressed);
}


 /* \brief Control Seven Segment Display
 *
 * \param[in]   timer handle      handle of timer that expired
 * 
 * \return nothing
 */
void hvac_update_display(int temperaturex10, THERMOSTAT_MODE_T hvac_mode, int hvac_setpointx10)
{
    static DISPLAY_STATE_T display_state = DISPLAY_SETPOINT;
    static THERMOSTAT_MODE_T last_hvac_mode = 0;
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
            display_state = DISPLAY_HVAC_OFF;
            break;
        case HVAC_HEATING_ONLY:
            display_state = DISPLAY_HVAC_HEAT;
            break;
        case HVAC_COOLING_ONLY:
            display_state = DISPLAY_HVAC_COOL;
            break;
        case HVAC_FAN_ONLY:
            display_state = DISPLAY_HVAC_FAN;
            break;
        case HVAC_AUTO:
            display_state = DISPLAY_HVAC_AUTO;
            break;
        }

        last_hvac_mode = hvac_mode;
        last_display_state_change_tick = now_tick;  
    }
    else if (hvac_setpointx10 != last_hvac_setpoint)
    {
        // process setpoint change
        display_state = DISPLAY_SETPOINT;

        last_hvac_setpoint = hvac_setpointx10;
        last_display_state_change_tick = now_tick;        
    }
    else if ((now_tick-last_display_state_change_tick) > 10000)
    {
        // revert to displaying current temperature
        display_state = DISPLAY_TEMPERATURE;    
    }

    switch(display_state)
    {
        default:
        case DISPLAY_TEMPERATURE:
            tm1637_display(temperaturex10/10, false);       
            break;
        case DISPLAY_HVAC_OFF:
            tm1637_display_word("OFF", false);       
            break;  
        case DISPLAY_HVAC_HEAT:
            tm1637_display_word("HEAT", false);     
            break;             
        case DISPLAY_HVAC_COOL:
            tm1637_display_word("COOL", false);      
            break;  
        case DISPLAY_HVAC_FAN:
            tm1637_display_word("FAN", false);    
            break;             
        case DISPLAY_HVAC_AUTO:
            tm1637_display_word("AUTO", false);    
            break;                                                 
        case DISPLAY_SETPOINT:
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

int initialize_physical_buttons(int mode_button_gpio, int increase_button_gpio, int decrease_button_gpio)
{
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

    // create queue for to pass interrupt messages to task
    irq_queue = xQueueCreate(1, sizeof(uint8_t));

    enable_irq(true);

    return(0);
}