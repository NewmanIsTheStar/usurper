/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef CALENDAR_H

#define CALENDAR_H


// porting to Pico 2W -- no RTC!
// typedef struct 
// {
//     int year;
//     int day;
//     int month;
//     int hour;
//     int min;
//     int sec;
//     int dotw;
// } datetime_t;

// from types.h  -- could also use the #ifdef from that file to know when to include this definition
// typedef struct {
//     int16_t year;    ///< 0..4095
//     int8_t month;    ///< 1..12, 1 is January
//     int8_t day;      ///< 1..28,29,30,31 depending on month
//     int8_t dotw;     ///< 0..6, 0 is Sunday
//     int8_t hour;     ///< 0..23
//     int8_t min;      ///< 0..59
//     int8_t sec;      ///< 0..59
// } datetime_t;


typedef enum
{
    SCHEDULE_FUTURE = 0,
    SCHEDULE_NOW = 1,
    SCHEDULE_NEVER = 2,
} SCHEDULE_QUERY_STATUS_LT;

int set_daylight_saving_dates(void);
int sanitize_daylight_saving_date(char *in, char *out, int len);
int get_day_of_week(int m,int d,int y);
int get_dow_and_mod_local_tz(int *dow, int *mod);
int daylight_savings_active(datetime_t date);
int get_timestamp(char *timestamp, int len, int isoformat);
int get_local_time_string(char *timestamp, int len);
SCHEDULE_QUERY_STATUS_LT get_next_irrigation_period(int *start_mow, int *end_mow, int *delay_mins, int *zone);
const char *day_name(int day);
bool mow_between(int time_mow, int start_mow, int end_mow);
int mow_future_delta(int start_mow, int end_mow);
int get_mow_local_tz(int *mow);
int get_daylight_saving_month_and_day(int year, char *date_description, int *daylight_savings_month, int *daylight_savings_day);
int set_calendar_html_page(void);
int8_t get_real_time_clock_seconds(void);
int mow_to_string(char *string, int length, int mow);
int string_to_mow(char *string, int length);

#ifdef FAKE_RTC
int8_t rtc_update(void);
int8_t rtc_get_datetime(datetime_t *date);
int8_t rtc_set_datetime(uint32_t sec);
#endif

#endif
