/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#define _GNU_SOURCE

#include "pico/cyw43_arch.h"
#include "pico/types.h"
#include "pico/stdlib.h"
#include <string.h>
//#include "hardware/rtc.h"
#include "pico/util/datetime.h"
#include "hardware/watchdog.h"

#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/apps/lwiperf.h"
#include "lwip/apps/sntp.h"
#include "lwip/apps/httpd.h"

#include "lwip/sockets.h"

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

#include "weather.h"
#include "calendar.h"
#include "cgi.h"

#include "flash.h"
#include "config.h"
#include "pluto.h"
#include "ssi.h"
#include "time.h"
#include "utility.h"
#include "config.h"


//#define IRRIGATION_TEST (1)

#define MINUTES_IN_WEEK (10080)
#define MINUTES_IN_DAY (1440)
#define MINUTES_IN_HOUR (60)
#define HOURS_IN_DAY (24)
#define DAYS_IN_WEEK (7)

extern NON_VOL_VARIABLES_T config;

// global variable
char current_calendar_web_page[50] = "/landscape.shtml";
uint32_t unix_time;

static int daylight_saving_start_month;
static int daylight_saving_start_day;
static int daylight_saving_end_month;
static int daylight_saving_end_day;
static int irrigation_test_start_mow = -1;


const char *weekdays[] =
{
   "Sunday",
   "Monday",
   "Tuesday",
   "Wednesday",
   "Thursday",
   "Friday",
   "Saturday",
   NULL
};

const char *months[] =
{
   "January",
   "February",
   "March",
   "April",
   "May",
   "June",
   "July",
   "August",
   "September",
   "October",
   "November",
   "Decemeber",
   NULL
};

const char *instances[] =
{
   "First",
   "Second",
   "Third",
   "Fourth",
   "Last",
   NULL
};


/*!
 * \brief Find keyword in sentence
 *
 * \param[in]  sentence   pointer to string to search for keywords
 * \param[in]  keywords   array of pointers to keywords
 * 
 * \return index of first keyword found or -1 if no keyword found
 */
int get_keyword_index(char *sentence, const char *keywords[])
{
   int i = 0;
   int found = -1;

   while(keywords[i])
   {
      if (strcasestr(sentence, keywords[i]))
      {
         found = i;
         break;
      }

      i++;
   }

   return(found);
}

/*!
 * \brief Find day of month 1-31 corresponding to the year, month, and weekday instance e.g. 2nd Tuesday in March 1970
 *
 * \param[in]  year                 year 1970-9999
 * \param[in]  month                month 0-11
 * \param[in]  weekday              weekday 0-6
 * \param[in]  instance_of_weekday  occurence of weekday 1-4
 * 
 * \return day of month or -1 if not found
 */
int get_day_of_month(int year, int month, int weekday, int instance_of_weekday)
{
   int day;
   int max_days;
   int found = 0;

   switch (month)
   {
      case 1: //February
         max_days = 29;  //TODO: normal vs. leap year
         break;

      case 3: //April
      case 5: //June
      case 8: //September
      case 10: //November
         max_days = 30;
         break;

      case 0: //January
      case 2: //March
      case 4: //May
      case 6: //July
      case 7: //August
      case 9: //October
      case 11: //December
      default:
         max_days = 31;
         break;         
   }

   // shift from zero based index to one based numbering used to compute day of week
   month++; 

   if (instance_of_weekday == 4)  // special case -- last occurence of weekday
   {
      //search backwards for last time weekday occurs in month
      for (day=max_days; day > 0; day--)
      {
         if (get_day_of_week(month, day, year) == weekday)   // weekday is zero based, 0 = sunday
         {
              found = 1;
              break;
         }
      }
   }
   else
   {
      // search forwards for first, second, third and fourth weekday occurences
      instance_of_weekday++;  // shift from zero based index to countdown

      for (day=1; day <=max_days; day++)
      {
         if (get_day_of_week(month, day, year) == weekday)   // weekday is zero based, 0 = sunday
         {
            if (--instance_of_weekday == 0)
            {
               found = 1;
               break;
            }
         }
      }
   }

   if (!found) day = -1;

   return(day);
}

/*!
 * \brief Determine daylight saving start and end dates
 * 
 * \return 0 on success, -1 on error
 */
int set_daylight_saving_dates(void)
{
   int err = 0;
   int year;
   datetime_t t;   
   int ok = 0;

   // determine current year
   ok = rtc_get_datetime(&t);

    if (ok)
    {
      year = t.year;

      err = get_daylight_saving_month_and_day(year, config.daylightsaving_start, &daylight_saving_start_month, &daylight_saving_start_day);

      if (!err)
      {
         err = get_daylight_saving_month_and_day(year, config.daylightsaving_end, &daylight_saving_end_month, &daylight_saving_end_day);
      }
    }
    else
    {
      err = -1;
    }

   if(err)
   {
      // on error disable daylight saving date range
      daylight_saving_start_month = 0;
      daylight_saving_start_day = 0;
      daylight_saving_end_month = 0;
      daylight_saving_end_day = 0;
   }

   return(err);
}

/*!
 * \brief Extract daylight savings day/month from text description for the given year
 * 
 * \param[in]  year                    year 1970 - 9999
 * \param[in]  date_description        e.g. "First Sunday in March"
 * \param[out] daylight_savings_month  month 0-11
 * \param[out] daylight_savings_day    day 0-6
 * \return 0 on success, -1 on error
 */
int get_daylight_saving_month_and_day(int year, char *date_description, int *daylight_savings_month, int *daylight_savings_day)
{
   int err = -1;
   int month;
   int weekday;
   int instance_of_weekday;
   int day_of_month;  

   CLIP(year, 1970, 9999);

   // extract keywords from sentence describing start of daylight saving
   month = get_keyword_index(date_description, months);
   weekday = get_keyword_index(date_description, weekdays);
   instance_of_weekday = get_keyword_index(date_description, instances);


   if ((month >= 0) && (weekday >= 0) && (instance_of_weekday >= 0))
   {
      day_of_month = get_day_of_month(year, month, weekday, instance_of_weekday);


      if (day_of_month > 0)
      {
         // set daylight saving start
         *daylight_savings_month = month+1;
         *daylight_savings_day = day_of_month;

         err = 0;
      }      
   }

   return(err);
}

/*!
 * \brief Normalize text description of daylight savings start/end amd remove extraneous words
 *
 * \param[in]  in       daylight savings description e.g. "The first Sunday in March"
 * \param[in]  out      daylight savings description normalized e.g. "First Sunday in March"
 * \param[in]  len      maximum length of output string
 * 
 * \return 0 on success
 */
int sanitize_daylight_saving_date(char *in, char *out, int len)
{
   int month;
   int weekday;
   int instance_of_weekday;

   // extract keywords from sentence describing start of dayling saving
   month = get_keyword_index(in, months);
   weekday = get_keyword_index(in, weekdays);
   instance_of_weekday = get_keyword_index(in, instances);


   if ((month >= 0) && (weekday >= 0) && (instance_of_weekday >= 0))
   {
      snprintf(out, len, "%s %s in %s", instances[instance_of_weekday], weekdays[weekday], months[month]);
   }
   else
   {
       snprintf(out, len, "Unknown");
   }

   return(0);
}


/*!
 * \brief Determine day of week based on date
 * 
 * \param[in]  m  month 1-12
 * \param[in]  d  day 1-31
 * \param[in]  y  year 1970 - 9999
 * \return day of week, 0-6 where 0 = Sunday
 */
int get_day_of_week(int m,int d,int y)
{
    y-=m<3;
    return(y+y/4-y/100+y/400+"-bed=pen+mad."[m]+d)%7;
}

/*!
 * \brief Get day-of-week and minute-of-day in local timezone
 *
 * \param[out]   dow day of week, 0-6 where 0 = Sunday  
 * 
 * \param[out]   mod minute of day, 0 - 1439    
 * 
 * \return 0 on success or -1 on error
 */
int get_dow_and_mod_local_tz(int *dow, int *mod)
{
    int err = 0;
    datetime_t date;

    // determine daylight savings dates for the current year
    set_daylight_saving_dates();  

    // determine current weekday in UTC
    rtc_get_datetime(&date);
    *dow = get_day_of_week(date.month, date.day, date.year);

    // standard time
    *mod = date.hour*MINUTES_IN_HOUR + date.min + config.timezone_offset; 

    // check for daylight savings
    if (config.daylightsaving_enable                                                             &&
        ((date.month*31+date.day) >= (daylight_saving_start_month*31+daylight_saving_start_day)) &&
        ((date.month*31+date.day) < (daylight_saving_end_month*31+daylight_saving_end_day)))
    {
        // daylight savings time
        *mod += MINUTES_IN_HOUR; 
    }
    
    // time zone offset means it is the previous day in local time
    if (*mod < 0)
    {
        *mod += HOURS_IN_DAY*MINUTES_IN_HOUR;

        if (--*dow < 0) *dow +=DAYS_IN_WEEK;
    }

    // time zone offset means it is the next day in local time
    if (*mod > HOURS_IN_DAY*MINUTES_IN_HOUR)
    {
        *mod -= HOURS_IN_DAY*MINUTES_IN_HOUR;

        if (++*dow > 6) *dow -=DAYS_IN_WEEK;        
    } 

    return(err);
}


/*!
 * \brief Determine if date is within daylight savings period
 *
 * \param[in]    date datetime_t structure with date to check   
 * 
 * \return true if in range otherwise false
 */
int daylight_savings_active(datetime_t date)
{
   bool daylight_savings = false;

    if (config.daylightsaving_enable                                                             &&
        ((date.month*31+date.day) >= (daylight_saving_start_month*31+daylight_saving_start_day)) &&
        ((date.month*31+date.day) < (daylight_saving_end_month*31+daylight_saving_end_day)))
   {
      daylight_savings = true;
   }

   return(daylight_savings);
}


/*!
 * \brief Generate string containing time stamp from realtime clock
 *
 * \param[out]  timestamp   pointer to string to store the timestamp 
 * \param[in]   len         max length of timestamp string  
 * \param[in]   isoformat   use iso format
 * 
 * \return 0 on success, non-zero on error
 */
int get_timestamp(char *timestamp, int len, int isoformat)
{
    int ok = 0;
    datetime_t t;

    ok = rtc_get_datetime(&t);

    if (ok)
    {
        if (isoformat)
        {
            // iso format needed for syslog
            snprintf(timestamp, len, "%04d-%02d-%02dT%02d:%02d:%02d.000Z", t.year, t.month, t.day, t.hour, t.min, t.sec);
        }
        else
        {
            // human readable format
            snprintf(timestamp, len, "%04d-%02d-%02d %02d:%02d:%02d Z", t.year, t.month, t.day, t.hour, t.min, t.sec);
        }
    }
    else
    {
        snprintf(timestamp, len, "1970-01-01T00:00:000.000Z");  //default to unix epoch
    }

    return !ok;
}

/*!
 * \brief Generate string containing local time in human readable format
 *
 * \param[out]  timestamp   pointer to string to store the timestamp 
 * \param[in]   len         max length of timestamp string  
 * 
 * \return number of characters in the timestamp
 */
int get_local_time_string(char *timestamp, int len)
{
    int printed = 0;
    int min_now = 0;
    datetime_t date;

    rtc_get_datetime(&date);

    min_now = date.hour*MINUTES_IN_HOUR + date.min + config.timezone_offset;

    // check for daylight savings
    if (daylight_savings_active(date))
    {
        min_now += MINUTES_IN_HOUR;
    }

    // correct for wrap to previous day
    if (min_now < 0)
    {
        min_now += HOURS_IN_DAY*MINUTES_IN_HOUR;
    }

    // correct for wrap to next day
    if (min_now > HOURS_IN_DAY*MINUTES_IN_HOUR)
    {
        min_now -= HOURS_IN_DAY*MINUTES_IN_HOUR;
    } 
 
    printed = snprintf(timestamp, len, "%02d:%02d", min_now/MINUTES_IN_HOUR, min_now%MINUTES_IN_HOUR);                   

    return(printed);
}

/*!
 * \brief Get start and stop times for next irrigation period
 *
 * \param[out]  start_mow   0 - 10079 minute of week, beginning Sunday at midnight
 * \param[out]  end_mow     0 - 10079 minute of week, beginning Sunday at midnight
 * \param[out]  delay_mins  0 - 10079 minutes from now until next irrigation period
 * \param[out]  zone        0 - 15
 * 
 * \return -1 on error, 0 on success, 1 if currently in irrigation period
 */
SCHEDULE_QUERY_STATUS_LT get_next_irrigation_period(int *start_mow, int *end_mow, int *delay_mins, int *zone)
{
   int potential_zone;
   int day;      
   int now_mow = 0;
   int irrigate_now = SCHEDULE_NEVER;
   int candidate_start_mow;
   int candidate_end_mow;
   int lowest_delta = MINUTES_IN_WEEK;
   int delta = 0;
   int day_total_duration = 0;

   // get current minute of week 
   get_mow_local_tz(&now_mow);                      

    // search for next irrigation period
   for (day = 0; (day < DAYS_IN_WEEK) && (irrigate_now != SCHEDULE_NOW); day++)
   {
      if (config.day_schedule_enable[day])
      {
         day_total_duration = 0;

         for(potential_zone=0; potential_zone < config.zone_max; potential_zone++)
         {
            if (config.zone_enable[potential_zone])
            {   
               candidate_start_mow = (day*HOURS_IN_DAY*MINUTES_IN_HOUR + config.day_start[day] + day_total_duration)%MINUTES_IN_WEEK;
               candidate_end_mow =   (candidate_start_mow + config.zone_duration[potential_zone][day])%MINUTES_IN_WEEK;

               // maintain running total of duration for the day
               day_total_duration += config.zone_duration[potential_zone][day];

               // check if current time is within the candidate irrigation period
               if (mow_between(now_mow, candidate_start_mow, candidate_end_mow))
               {
                  irrigate_now = SCHEDULE_NOW;
                  *start_mow = candidate_start_mow;
                  *end_mow = candidate_end_mow;
                  *delay_mins = 0;
                  *zone = potential_zone;
                  break;
               }

               // find the lowest delta to a future irrigation
               delta = mow_future_delta(now_mow, candidate_start_mow);
               if (delta < lowest_delta)
               {
                  irrigate_now = SCHEDULE_FUTURE;                  
                  lowest_delta = delta;                  
                  *start_mow = candidate_start_mow;
                  *end_mow = candidate_end_mow;
                  *delay_mins = delta;
                  *zone = potential_zone;
               }            
            }
         }
      }   
   }

   return (irrigate_now);
}


/*!
 * \brief Check if time is between start and end dealing with wrap around 
 *
 * \param[in]  time_mow    0 - 10079 minute of week, beginning Sunday at midnight
 * \param[in]  start_mow   0 - 10079 minute of week, beginning Sunday at midnight
 * \param[in]  end_mow     0 - 10079 minute of week, beginning Sunday at midnight
 * 
 * \return true if in range
 */
bool mow_between(int time_mow, int start_mow, int end_mow)
{
   bool in_range = false;

   // normalize times
   time_mow = time_mow%MINUTES_IN_WEEK;
   start_mow = start_mow%MINUTES_IN_WEEK;
   end_mow = end_mow%MINUTES_IN_WEEK;      

 
   // check for conventional range
   if (start_mow <= end_mow)
   {
      if ((time_mow >= start_mow) && (time_mow < end_mow))
      {
         in_range = true;
      }
   }

   // check for range with wrap at week boundary
   if (start_mow > end_mow)
   {
      if (!(time_mow < end_mow) || (time_mow >= start_mow))
      {
         in_range = true;
      }   
   }

   return(in_range);
}


/*!
 * \brief Calculate future delta between mow times
 *
 * \param[in]  start_mow   0 - 10079 minute of week, beginning Sunday at midnight
 * \param[in]  end_mow     0 - 10079 minute of week, beginning Sunday at midnight
 * 
 * \return future delta i.e. when moving forward in time
 */
int mow_future_delta(int start_mow, int end_mow)
{
   int delta = 0;

   // check for conventional range
   if (start_mow <= end_mow)
   {
      delta = end_mow - start_mow;
   }
   else
   {
      delta = MINUTES_IN_WEEK - start_mow + end_mow;
   }   

   return(delta);
}

/*!
 * \brief Return pointer to string with weekday name
 *
 * \param[in]  day         0 - 6 day of week, beginning Sunday at midnight
 * 
 * \return pointer to string containing name of day or NULL on error
 */
const char *day_name(int day)
{
   const char *name = NULL;

   if ((day >=0) && (day <DAYS_IN_WEEK))
   {
      name = weekdays[day];
   }

   return(name);
}

/*!
 * \brief Return pointer to string with weekday name give minute of week (mow)
 *
 * \param[in]  day         0 - 6 day of week, beginning Sunday at midnight
 * 
 * \return pointer to string containing name of day or NULL on error
 */
int get_day_from_mow(int mow)
{
   int day;

   day = mow/(24*60);

   CLIP(day, 0 , 6);

   return(day);   
}


/*!
 * \brief Get minute-of-week in local timezone
 *
 * \param[out]   mow minute of week, 0-10079 where 0 = Midnight Sunday    
 * 
 * \return 0 on success or -1 on error
 */
int get_mow_local_tz(int *mow)
{
    int err = 0;
    int dow = 0;
    int mod = 0;

   err = get_dow_and_mod_local_tz(&dow, &mod);

   if(!err)
   {
      *mow = (dow*MINUTES_IN_DAY+mod)%MINUTES_IN_WEEK;
   }

    return(err);
}

/*!
 * \brief Set name of html page to use to disaplay one week calendar  
 * 
 * \return 0 on success or -1 on error
 */
int set_calendar_html_page(void)
{
   switch(config.personality)
   {
   default:
   case NO_PERSONALITY:
         STRNCPY(current_calendar_web_page, "/personality.shtml", sizeof(current_calendar_web_page));    
         break;
   case SPRINKLER_USURPER:
         if (config.use_monday_as_week_start)
         {
            STRNCPY(current_calendar_web_page, "/landscape_monday.shtml", sizeof(current_calendar_web_page));            
         }
         else
         {
            STRNCPY(current_calendar_web_page, "/landscape.shtml", sizeof(current_calendar_web_page)); 
         }
         break;
   case SPRINKLER_CONTROLLER:
         if (config.use_monday_as_week_start)
         {
            STRNCPY(current_calendar_web_page, "/zm_landscape.shtml", sizeof(current_calendar_web_page)); 
         }
         else
         {
            STRNCPY(current_calendar_web_page, "/zs_landscape.shtml", sizeof(current_calendar_web_page)); 
         }
         break;                
   case LED_STRIP_CONTROLLER:
         STRNCPY(current_calendar_web_page, "/led_controller.shtml", sizeof(current_calendar_web_page)); 
         break;
   case HVAC_THERMOSTAT:
         if (config.use_monday_as_week_start)
         {
            STRNCPY(current_calendar_web_page, "/tm_thermostat.shtml", sizeof(current_calendar_web_page));           
         }
         else
         {
            STRNCPY(current_calendar_web_page, "/ts_thermostat.shtml", sizeof(current_calendar_web_page)); 
         }         
         break;         
   }


    return(0);
}
/*!
 * \brief Get seconds part of the current real time
 *
 * \param[out]   dow day of week, 0-6 where 0 = Sunday  
 * 
 * \param[out]   mod minute of day, 0 - 1439    
 * 
 * \return 0-59 on success or 0 if unable to read rtc
 */
int8_t get_real_time_clock_seconds(void)
{
   datetime_t date;

   date.sec = 0;
   rtc_get_datetime(&date);

   return(date.sec);
}


/*!
 * \brief Update fake rtc
 *
 * This should only be called from one task!
 */
int8_t rtc_update(void)
{
   TickType_t current_tick;
   TickType_t increment;
   static TickType_t last_tick = 0;
   static bool initialized = false;   


   current_tick = xTaskGetTickCount();

   if(!initialized)
   {
      last_tick = current_tick;
      initialized = true;
   }

   increment = (current_tick - last_tick)/1000;

   last_tick += increment*1000;   // avoid accumulating rounding errors
   unix_time += increment;  // TODO make atomic

   return(0);
}

/*!
 * \brief Get seconds part of the current real time
 *
 * \param[out]   dow day of week, 0-6 where 0 = Sunday  
 * 
 * \param[out]   mod minute of day, 0 - 1439    
 * 
 * \return 0-59 on success or 0 if unable to read rtc
 */
int8_t rtc_get_datetime(datetime_t *date)
{
   struct tm * timeinfo;
   time_t t;

   t = unix_time; // TODO make atomic

	timeinfo = gmtime(&t);

	memset(date, 0, sizeof(datetime_t));
	date->sec = timeinfo->tm_sec;
	date->min = timeinfo->tm_min;
	date->hour = timeinfo->tm_hour;
	date->day = timeinfo->tm_mday;
	date->month = timeinfo->tm_mon + 1;
	date->year = timeinfo->tm_year + 1900;

   date->dotw = get_day_of_week(date->month, date->day, date->year);

   return(1);
}

/*!
 * \brief Set fake RTC
 *
 */
int8_t rtc_set_datetime(uint32_t sec)
{
    
   unix_time = sec;

   return(1);
}

/*!
 * \brief print mow in human readable form e.g. "Monday 11:00"
 *
 */
int mow_to_string(char *string, int length, int mow)
{
   int day;
   int hour;
   int minute; 
   int printed = 0;
   
   day = mow / (60*24);
   hour = (mow % (60*24))/60;
   minute = (mow % (60*24))%60;

   CLIP(day, 0, 6);
   CLIP(hour, 0, 23);
   CLIP(minute, 0, 59);

   printed = snprintf(string, length, "%s %02d:%02d", weekdays[day], hour, minute);

   return(printed);
}

/*!
 * \brief print mow time in human readable form e.g. "11:00"
 *
 */
int mow_to_time_string(char *string, int length, int mow)
{
   int hour;
   int minute; 
   int printed = 0;
   
   if ((mow >= 0) && (mow < 60*24*7))
   {
      hour = (mow % (60*24))/60;
      minute = (mow % (60*24))%60;

      CLIP(hour, 0, 23);
      CLIP(minute, 0, 59);

      printed = snprintf(string, length, "%02d:%02d", hour, minute);
   }
   else
   {
      printed = snprintf(string, length, "&nbsp;");  //TODO -- don't like this webUI stuff here
   }

   return(printed);
}

/*!
 * \brief string to mow
 *
 */
int string_to_mow(char *string, int length)
{
   int mow;
   int day = 0;
   int hour = 0;
   int minute = 0; 

   for(day = 0; day < 6; day++)
   {
      if (strcasestr(string, weekdays[day]))
      {
         string += strlen(weekdays[day]);
         break;
      }
   }
   
   sscanf(string,"%d:%d", &hour, &minute);
   sscanf(string,"%d%%3A%d", &hour, &minute);     

   CLIP(day, 0, 6);
   CLIP(hour, 0, 23);
   CLIP(minute, 0, 59);

   mow = day*24*60 + hour*60 + minute;

   return(mow);
}

/*!
 * \brief time string to mow
 *
 */
int time_string_to_mow(char *string, int length, int day)
{
   int mow;
   int hour = 0;
   int minute = 0; 
   
   sscanf(string,"%d:%d", &hour, &minute);
   sscanf(string,"%d%%3A%d", &hour, &minute);   

   printf(">>>>>AFTER SCANF %d %d\n", hour, minute);

   CLIP(day, 0, 6);
   CLIP(hour, 0, 23);
   CLIP(minute, 0, 59);

   mow = day*24*60 + hour*60 + minute;

   return(mow);
}
