
/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef PLUTO_H
#define PLUTO_H

//#define PLUTO_VER "01.00.00"          // release version set in CMakeLists.txt
//#define USE_GIT_HASH_AS_VERSION       // automatically set in CMakeLists.txt for debug builds

#define STRNCPY(dst, src, size)  {strncpy((dst), (src), (size)); *((dst)+(size)-1)=0;}
//#define STRNCAT(dst, src, size)  {if ((size) > 0) {*((dst)+(size)-1)=0; strncat((dst), (src), (size)-strlen((dst))-1);};}
#define STRAPPEND(dst, src)  {*((dst)+(sizeof(dst))-1)=0; strncat((dst), (src), (sizeof(dst))-strlen((dst))-1);}
#define NUM_ROWS(x) (sizeof(x)/sizeof(x[0]))
#define MASKED_WRITE(dest,src,mask) {(dest) = (((dest) & (~(mask))) | ((src) & (mask)));}
#define CLIP(x, low, high)  (x=(((x)>(high))?(high):(((x)<(low))?(low):(x))))
#define SLEEP_MS(x) (vTaskDelay(x));

//int applet_entry_point(void);
void hex_dump(const uint8_t *bptr, uint32_t len);
int application_restart(void);
int print_gpio_pins_matching_default(char *buffer, int len, GPIO_DEFAULT_T gpio_default);

#endif 
