
#ifndef WS2812_H
#define WS2812_H

void led_strip_task(__unused void *params);
int get_pattern(void);
int get_speed(void);
//void set_led_pattern(int pattern);  // remove
//void set_led_speed(int speed);      // remove
void set_led_pattern_local(int pattern); 
void set_led_speed_local(int speed);
 int aled_pattern_web_selection(char *buf, int len, int selected_row);

#endif