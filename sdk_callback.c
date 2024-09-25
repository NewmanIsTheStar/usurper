/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico/cyw43_arch.h"
#include "pico/types.h"
#include "pico/stdlib.h"
#include "hardware/rtc.h"
#include "pico/util/datetime.h"
#include "hardware/watchdog.h"

#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/apps/lwiperf.h"
#include "lwip/apps/sntp.h"
#include "lwip/apps/httpd.h"
#include "dhcpserver.h"
#include "dnsserver.h"

#include "lwip/sockets.h"

#include "time.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

#include "weather.h"
#include "led_strip.h"
#include "cgi.h"
#include "ssi.h"
#include "flash.h"
#include "utility.h"
#include "config.h"


/*!
 * \brief Used by lwip sntp application to set the rtc 
 *
 * \param sec time in seconds since epoch (1970)
 *
 * \return nothing
 */
void setTimeSec(uint32_t sec)
{
	datetime_t date;
	struct tm * timeinfo;
	time_t t;
    time_t offset_hours = 0;
    time_t offset_minutes = 0;
    

    // offset from UTC
    t = sec + (60 * 60 * offset_hours) + offset_minutes* 60;

	timeinfo = gmtime(&t);

	memset(&date, 0, sizeof(date));
	date.sec = timeinfo->tm_sec;
	date.min = timeinfo->tm_min;
	date.hour = timeinfo->tm_hour;
	date.day = timeinfo->tm_mday;
	date.month = timeinfo->tm_mon + 1;
	date.year = timeinfo->tm_year + 1900;

    //printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>SETTING RTC to: year = %u month = %u day = %u hour = %u min = %u sec = %u\n", date.year, date.month, date.day, date.hour, date.min, date.sec);

	rtc_set_datetime (&date);
}
