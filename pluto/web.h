/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef WEB_H
#define WEB_H

//#include "rmtsw.h"
// #include "thermostat.h"

typedef struct WEB_VARIABLES
{
  uint8_t mac[6];
  int access_point_mode;
  char last_completed_timestring[50];
  char last_usurped_timestring[50];
  int outside_temperature;
  int wind_speed;
  int daily_rain;
  int weekly_rain;                  // this comes from weather station based on calendar weeks and is not useful for irrigation decisions
  int trailing_seven_days_rain;     // this is accumulated from the daily totals as it is more relevant to irrigation decision
  uint8_t soil_moisture[16];
  char watchdog_timestring[50];
  uint32_t us_last_rx_packet;
  char status_message[100];
  char stack_message[256];
  char ip_address_string[50];
  char network_mask_string[50];
  char gateway_string[50];
  char mqtt_client_name[64];  
  int socket_max;
  int bind_failures;
  int connect_failures;  
  int syslog_transmit_failures;
  int govee_transmit_failures;
  int weather_station_transmit_failures;
  int pluto_transmit_failures;
  char software_server[100];
  char software_url[100];
  char software_file[100];
  int led_current_pattern;
  int led_current_transition_delay;
  char led_last_request_ip[32];
  int irrigation_test_enable;
  int thermostat_set_point;                // desired temperature
  int thermostat_heating_set_point;        // target temperature when heating
  int thermostat_cooling_set_point;        // target temperature when cooling 
  //int thermostat_hysteresis;
  int thermostat_day;              // used for rendering thermostat event web page -- making that page single user / session only
  int thermostat_period_row;
  int thermostat_day_events[7];
  int thermostat_grid[8][8];
  int thermostat_temperature;       // current temperature
  int powerwall_grid_status;
  int powerwall_battery_percentage;
  int thermostat_temperature_moving_average;
  int thermostat_temperature_gradient;
  int thermostat_temperature_prediction;
  // THERMOSTAT_MODE_T thermostat_effective_mode;
  int anemometer_wind_speed;
  int anemometer_adc_min;
  int anemometer_adc_max;  
  int rmtsw_relay_enabled[8];
  bool rmtsw_relay_desired_state[8];
  int rmtsw_relay_day;              // used for rendering relay event web page -- making that page single user / session only
  int rmtsw_relay_period_row;
  int rmtsw_relay_day_events[7];
  int rmtsw_relay_grid[8][8];
} WEB_VARIABLES_T;                  //remember to add initialization code when adding to this structure !!!


int init_web_variables(void);

#endif
