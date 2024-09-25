/**
 * Copyright (c) 2024 NewmanIsTheStar
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>

#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "generated/ws2812.pio.h"

// TODO - prune this list of includes
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/util/datetime.h"
#include "hardware/rtc.h"
#include "hardware/watchdog.h"

#include "lwip/sockets.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/apps/lwiperf.h"
#include "lwip/apps/sntp.h"
#include "lwip/apps/httpd.h"
#include "dhcpserver.h"
#include "dnsserver.h"

#include "time.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

#include "weather.h"
#include "cgi.h"
#include "ssi.h"
#include "flash.h"
#include "utility.h"
#include "config.h"

#include "watchdog.h"
#include "pluto.h"
#include "led_strip.h"

#define IS_RGBW config.led_rgbw
#define NUM_PIXELS config.led_number

#ifdef PICO_DEFAULT_WS2812_PIN
#define WS2812_PIN PICO_DEFAULT_WS2812_PIN
#else
#define WS2812_PIN config.led_pin
#endif

// prototypes
static inline void put_pixel(int sm, uint32_t pixel_grb);
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b);
void pattern_blank(int sm, uint len, uint t);
void pattern_scan(int sm, uint len, uint t);
void pattern_snakes(int sm, uint len, uint t);
void pattern_random(int sm, uint len, uint t);
void pattern_sparkle(int sm, uint len, uint t);
void pattern_greys(int sm, uint len, uint t);
void pattern_police(int sm, uint len, uint t);
void pattern_breath(int sm, uint len, uint t);

// external variables
extern NON_VOL_VARIABLES_T config;
extern WEB_VARIABLES_T web;

// types
typedef void (*pattern)(int sm, uint len, uint t);

// constants
const struct {
    pattern pat;
    const char *name;
} pattern_table[] = {
        {pattern_blank,   "Blank"},    
        {pattern_snakes,  "Snakes"},
        {pattern_scan,    "Scan"},
        {pattern_random,  "Random data"},
        {pattern_sparkle, "Sparkles"},
        {pattern_greys,   "Greys"},
        {pattern_police,  "Police"},
        {pattern_breath,  "Breath"},        
};

//static variable
static int live_pattern = -1;
static int live_speed = -1;
static DOUBLE_BUF_INT local_pattern;
static DOUBLE_BUF_INT local_speed;


/*!
 * \brief controls addressable led strip connected to an I/O pin
 *
 * \param[in]  params  alive counter that must be incremented periodically to prevent watchdog reset
 * 
 * \return nothing
 */
void led_strip_task(void *params) 
{
    int err = 0;

    printf("led_strip_task started\n");
    //printf("Using pin %d at flash address %x\n", WS2812_PIN, &led_strip_task);    

    for(;;)
    {
        if (config.led_pin && config.led_speed)
        {
            PIO pio = pio0;
            int sm = 0;
            uint offset = 0;
            
            if (pio_can_add_program(pio, &ws2812_program))
            {
                offset = pio_add_program(pio, &ws2812_program);

                sm = pio_claim_unused_sm(pio, false);

                if (sm >= 0)
                {
                    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);
                    err = 0;
                }
                else
                {
                    printf("*ERROR* Cannot obtain PIO state machine\n");
                    send_syslog_message("usurper", "*ERROR* Cannot obtain PIO state machine");
                    SLEEP_MS(10000);
                    err = 1;                    
                }
            }
            else
            {
                printf("*ERROR* Cannot add PIO program\n");
                send_syslog_message("usurper", "*ERROR* Cannot add PIO program");
                SLEEP_MS(10000);
                err = 2;

                send_syslog_message("usurper", "Clearing PIO program instruction memory");
                pio_clear_instruction_memory(pio);
            }

            if (!err)
            {
                set_led_pattern_local(config.led_pattern); 

                CLIP(config.led_pattern , 0, count_of(pattern_table));
                CLIP(config.led_speed, 0, 30000);
                CLIP(live_pattern, 0, count_of(pattern_table));

                int t = 0;
                while (1) 
                {
                    int dir = (rand() >> 30) & 1 ? 1 : -1;

                    for (int i = 0; i < 1000; ++i) 
                    {
                        live_pattern = get_double_buf_integer(&local_pattern, 0);
                        CLIP(live_pattern, 0, count_of(pattern_table));
                        
                        pattern_table[live_pattern].pat(sm, NUM_PIXELS, t);

                        live_speed = get_double_buf_integer(&local_speed, 0);
                        CLIP(live_speed, 0, 3000);
                        
                        SLEEP_MS(config.led_speed);

                        t += dir;

                        watchdog_pulse((int *)params);                    
                    }                
                }
            }
        }

        SLEEP_MS(15000);

        watchdog_pulse((int *)params);
    }
}

/*!
 * \brief set the current led strip pattern
 *
 * \param[in]  pattern  index into pattern_table
 * 
 * \return nothing
 */
void set_led_pattern_local(int pattern) 
{
    if (pattern < 0)
    {
        pattern = config.led_pattern;
    }

    CLIP(pattern, 0, count_of(pattern_table));

    set_double_buf_integer(&local_pattern, pattern);

    web.led_current_pattern = pattern;
}

/*!
 * \brief set the current led strip pattern transition delay
 *
 * \param[in]  speed  milliseconds to delay before next step of led sequence
 * 
 * \return nothing
 */
void set_led_speed_local(int speed) 
{
    if (speed < 0)
    {
        speed = config.led_speed;
    }

    CLIP(speed, 0, 3000);

    set_double_buf_integer(&local_speed, speed);

    web.led_current_transition_delay = speed;
}

/*!
 * \brief set led color
 *
 * \param[in]  pixel_grb  green-red-blue values packed into 32 bits
 * 
 * \return nothing
 */
static inline void put_pixel(int sm, uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, sm, pixel_grb << 8u);
}

/*!
 * \brief pack red-green-blue values into 32 bits
 *
 * \param[in]  r  red 8 bit value
 * \param[in]  g  green 8 bit value
 * \param[in]  b  blue 8 bit value
 * 
 * \return nothing
 */
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return
            ((uint32_t) (r) << 8) |
            ((uint32_t) (g) << 16) |
            (uint32_t) (b);
}

/*!
 * \brief led pattern generator 
 *
 * \param[in]  len  number of leds in strip
 * \param[in]  t    pattern sequence
 * 
 * \return nothing
 */
void pattern_blank(int sm, uint len, uint t) {
    for (int i = 0; i < len; ++i)
        put_pixel(sm, urgb_u32(0, 0, 0));;
}


/*!
 * \brief led pattern generator 
 *
 * \param[in]  len  number of leds in strip
 * \param[in]  t    pattern sequence
 * 
 * \return nothing
 */
void pattern_scan(int sm, uint len, uint t) {
    static uint position = 0;
    static uint direction = 0;
    static uint color = 0;

    for (uint i = 0; i < len; ++i) {
        //uint x = (i + (t >> 1)) % 64;

        if (i == position)
        {
            switch(color%3)
            {
                case 0:
                    put_pixel(sm, urgb_u32(0xff, 0, 0));
                    break;
                case 1:
                    put_pixel(sm, urgb_u32(0, 0xff, 0));
                    break;
                case 2:
                    put_pixel(sm, urgb_u32(0, 0, 0xff));
                    break;
                default:
                    break;  //FUBAR
            }                   
        }
        else
        {
            put_pixel(sm, 0);
        }
    }
    
    if (direction == 0)
    {
        position++;
        if (position >= (NUM_PIXELS-1))
        {
            direction = 1;
            color++;

        }
    }
    else
    {
        position--;
        if (position == 0)
        {
            direction = 0;
            color++;

        } 
    }
}

/*!
 * \brief led pattern generator 
 *
 * \param[in]  len  number of leds in strip
 * \param[in]  t    pattern sequence
 * 
 * \return nothing
 */
void pattern_snakes(int sm, uint len, uint t) {
    for (uint i = 0; i < len; ++i) {
        uint x = (i + (t >> 1)) % 64;
        if (x < 10)
            put_pixel(sm, urgb_u32(0xff, 0, 0));
        else if (x >= 15 && x < 25)
            put_pixel(sm, urgb_u32(0, 0xff, 0));
        else if (x >= 30 && x < 40)
            put_pixel(sm, urgb_u32(0, 0, 0xff));
        else
            put_pixel(sm, 0);
    }
}

/*!
 * \brief led pattern generator 
 *
 * \param[in]  len  number of leds in strip
 * \param[in]  t    pattern sequence
 * 
 * \return nothing
 */
void pattern_random(int sm, uint len, uint t) {
    if (t % 8)
        return;
    for (int i = 0; i < len; ++i)
        put_pixel(sm,rand());
}

/*!
 * \brief led pattern generator 
 *
 * \param[in]  len  number of leds in strip
 * \param[in]  t    pattern sequence
 * 
 * \return nothing
 */
void pattern_sparkle(int sm, uint len, uint t) {
    if (t % 8)
        return;
    for (int i = 0; i < len; ++i)
        put_pixel(sm, rand() % 16 ? 0 : 0xffffffff);
}

/*!
 * \brief led pattern generator 
 *
 * \param[in]  len  number of leds in strip
 * \param[in]  t    pattern sequence
 * 
 * \return nothing
 */
void pattern_greys(int sm, uint len, uint t) {
    int max = 100; // let's not draw too much current!
    t %= max;
    for (int i = 0; i < len; ++i) {
        put_pixel(sm, t * 0x10101);
        if (++t >= max) t = 0;
    }
}

/*!
 * \brief led pattern generator 
 *
 * \param[in]  len  number of leds in strip
 * \param[in]  t    pattern sequence
 * 
 * \return nothing
 */
void pattern_police(int sm, uint len, uint t) {
    static int state = 0;
    int i;

    switch(state)
    {
        case 0: // =.==.=
            for (i=0; i<len; i++)
            {
                if (((i >= len/6*1) && (i < len/6*2)) ||
                    ((i >= len/6*4) && (i < len/6*5)))
                {
                    put_pixel(sm, urgb_u32(0, 0, 0));   // off
                }
                else
                {
                    if (i < len/2)
                    {
                        put_pixel(sm, urgb_u32(0xff, 0, 0)); // red
                    }
                    else
                    {
                        put_pixel(sm, urgb_u32(0, 0, 0xff)); // blue
                    }                    
                }
            }
            state = 1;
            break;
        case 1: // .=..=. 
            for (i=0; i<len; i++)
            {
                if (((i >= len/6*1) && (i < len/6*2)) ||
                    ((i >= len/6*4) && (i < len/6*5)))
                {
                    if (i < len/2)
                    {
                        put_pixel(sm, urgb_u32(0xff, 0, 0)); // red
                    }
                    else
                    {
                        put_pixel(sm, urgb_u32(0, 0, 0xff)); // blue
                    }
                }
                else
                {
                    put_pixel(sm, urgb_u32(0, 0, 0));   // off
                }
            }
            state = 0;
            break;  
          
    }
}

/*!
 * \brief led pattern generator 
 *
 * \param[in]  len  number of leds in strip
 * \param[in]  t    pattern sequence
 * 
 * \return nothing
 */
void pattern_breath(int sm, uint len, uint t) {
    static uint8_t brightness = 0;
    static uint8_t pause = 0;
    static int state = 0;
    int i;
    int step;

    for (i=0; i<len; i++)
    {
        put_pixel(sm, urgb_u32(0, brightness, 0)); // shade of green
    }

    // crude attempt to compensate for non-linear perceived brightness
    step = 1;
    if (brightness > 150) step += 5;
    if (brightness > 190) step += 7;
    //if (brightness > 210) step += 10;

    switch(state)
    {
    default:
    case 0:
        if (brightness > (255-step))
        {
            brightness = 255;
            state = 1;
        }
        else
        {
            brightness += step;
        }
        break;
    case 1:
        if (brightness < step)
        {
            brightness = 0;
            state = 2;
        } 
        else
        {
            brightness -= step;
        }           
        break;
    case 2:
        if (++pause == 8)
        {
            state = 0;
            pause = 0;
        } 
        break;        
    }
}



// NB: this doesn't work because the SSI tag value becomes too long, would need mulitple tags to make this work e.g. one per pattern
/*!
 * \brief a failed attempt to dynamically generate an html drop down list of pattern names
 *
 * \param[in]  buf            buffer to receive html snippet
 * \param[in]  len            max length of buffer
 * \param[in]  selected_row   row to select in the html drop down list 
 * 
 * \return nothing
 */
 int aled_pattern_web_selection(char *buf, int len, int selected_row)  
 {
    int num_chars = 0;
    int row = 0;
    int i = 0;

    while(row < NUM_ROWS(pattern_table) && num_chars < len)
    {
        if (row == selected_row)
        {
            i = snprintf(buf+num_chars, len-num_chars, "<option value=\"%d selected\">%s</option>", row, pattern_table[row].name);
        }
        else
        {
           i = snprintf(buf+num_chars, len-num_chars, "<option value=\"%d\">%s</option>", row, pattern_table[row].name); 
        }
        if (i >= 0)
        {
            num_chars += i;
        }
        row++; 
    }

    return(0);
 }    
