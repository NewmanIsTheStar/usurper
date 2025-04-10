/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef CALENDAR_H

#define CALENDAR_H

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
int mow_to_time_string(char *string, int length, int mow);
int get_day_from_mow(int mow);
int string_to_mow(char *string, int length);
int time_string_to_mow(char *string, int length, int day);

#ifdef FAKE_RTC
int8_t rtc_update(void);
int8_t rtc_get_datetime(datetime_t *date);
int8_t rtc_set_datetime(uint32_t sec);
#endif

#endif
