/**
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
// default to pin 2 if the board doesn't have a default WS2812 pin defined
#define WS2812_PIN config.led_pin
#endif



// prototypes
//int sock_StartUdp (int iPort);

// external variables
extern NON_VOL_VARIABLES_T config;

//global variable



//static variable
static int live_pattern = -1;
static int live_speed = -1;
static int new_pattern;  // used to minimize window in which live_pattern is invalid, clipped to valid range after window closes, avoids locks
static int new_speed;    // used to minimize window in which live_pattern is invalid, clipped to valid range after window closes, avoids locks
static DOUBLE_BUF_INT local_pattern;
static DOUBLE_BUF_INT local_speed;




static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return
            ((uint32_t) (r) << 8) |
            ((uint32_t) (g) << 16) |
            (uint32_t) (b);
}

void pattern_blank(uint len, uint t) {
    for (int i = 0; i < len; ++i)
        put_pixel(urgb_u32(0, 0, 0));;
}

void pattern_scan(uint len, uint t) {
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
                    put_pixel(urgb_u32(0xff, 0, 0));
                    break;
                case 1:
                    put_pixel(urgb_u32(0, 0xff, 0));
                    break;
                case 2:
                    put_pixel(urgb_u32(0, 0, 0xff));
                    break;
                default:
                    break;  //FUBAR
            }                   
        }
        else
        {
            put_pixel(0);
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

void pattern_snakes(uint len, uint t) {
    for (uint i = 0; i < len; ++i) {
        uint x = (i + (t >> 1)) % 64;
        if (x < 10)
            put_pixel(urgb_u32(0xff, 0, 0));
        else if (x >= 15 && x < 25)
            put_pixel(urgb_u32(0, 0xff, 0));
        else if (x >= 30 && x < 40)
            put_pixel(urgb_u32(0, 0, 0xff));
        else
            put_pixel(0);
    }
}

void pattern_random(uint len, uint t) {
    if (t % 8)
        return;
    for (int i = 0; i < len; ++i)
        put_pixel(rand());
}

void pattern_sparkle(uint len, uint t) {
    if (t % 8)
        return;
    for (int i = 0; i < len; ++i)
        put_pixel(rand() % 16 ? 0 : 0xffffffff);
}

void pattern_greys(uint len, uint t) {
    int max = 100; // let's not draw too much current!
    t %= max;
    for (int i = 0; i < len; ++i) {
        put_pixel(t * 0x10101);
        if (++t >= max) t = 0;
    }
}

void pattern_police(uint len, uint t) {
    static int state = 0;
    int i;

    switch(state)
    {
        case 0: // =.==.=
            for (i=0; i<len; i++)
            {
                if ((i >= len/6*1) && (i < len/6*2) ||
                    (i >= len/6*4) && (i < len/6*5))
                {
                    put_pixel(urgb_u32(0, 0, 0));   // off
                }
                else
                {
                    if (i < len/2)
                    {
                        put_pixel(urgb_u32(0xff, 0, 0)); // red
                    }
                    else
                    {
                        put_pixel(urgb_u32(0, 0, 0xff)); // blue
                    }                    
                }
            }
            state = 1;
            break;
        case 1: // .=..=. 
            for (i=0; i<len; i++)
            {
                if ((i >= len/6*1) && (i < len/6*2) ||
                    (i >= len/6*4) && (i < len/6*5))
                {
                    if (i < len/2)
                    {
                        put_pixel(urgb_u32(0xff, 0, 0)); // red
                    }
                    else
                    {
                        put_pixel(urgb_u32(0, 0, 0xff)); // blue
                    }
                }
                else
                {
                    put_pixel(urgb_u32(0, 0, 0));   // off
                }
            }
            state = 0;
            break;  
          
    }
}

void pattern_breath(uint len, uint t) {
    static uint8_t brightness = 0;
    static uint8_t pause = 0;
    static int state = 0;
    int i;
    int step;

    for (i=0; i<len; i++)
    {
        put_pixel(urgb_u32(0, brightness, 0)); // shade of green
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

typedef void (*pattern)(uint len, uint t);
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

// NB: this doesn't work because the SSI tag value becomes too long, would need mulitple tags to make this work e.g. one per pattern
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


void led_strip_task(void *params) 
{    
    printf("led_strip_task started\n");
    //printf("Using pin %d at flash address %x\n", WS2812_PIN, &led_strip_task);    

    for(;;)
    {
        if (config.led_pin && config.led_speed)
        {
            // todo get free sm
            PIO pio = pio0;
            int sm = 0;
            uint offset = pio_add_program(pio, &ws2812_program);

            ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);

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
                    
                    pattern_table[live_pattern].pat(NUM_PIXELS, t);

                    live_speed = get_double_buf_integer(&local_speed, 0);
                    CLIP(live_speed, 0, 3000);
                    
                    sleep_ms(config.led_speed);

                    t += dir;

                    watchdog_pulse((int *)params);                    
                }                
            }
        }

        sleep_ms(15000);

        watchdog_pulse((int *)params);
    }
}

void set_led_pattern_local(int pattern) 
{
    if (pattern < 0)
    {
        pattern = config.led_pattern;
    }

    CLIP(pattern, 0, count_of(pattern_table));

    set_double_buf_integer(&local_pattern, pattern);
}

void set_led_speed_local(int speed) 
{
    if (speed < 0)
    {
        speed = config.led_speed;
    }

    CLIP(speed, 0, 3000);

    set_double_buf_integer(&local_speed, speed);
}