/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef THERMOSTAT_H
#define THERMOSTAT_H

#define SETPOINT_DEFAULT_CELSIUS_X_10 (210)      // 21.0 C
#define SETPOINT_MAX_CELSIUS_X_10 (320)          // 32.0 C
#define SETPOINT_MIN_CELSIUS_X_10 (150)          // 15.0 C 
#define SETPOINT_DEFAULT_FAHRENHEIT_X_10 (700)   // 70.0 F
#define SETPOINT_MAX_FAHRENHEIT_X_10 (900)       // 90.0 F
#define SETPOINT_MIN_FAHRENHEIT_X_10 (600)       // 60.0 F
#define SETPOINT_TEMP_UNDEFINED   (-10001)
#define SETPOINT_TEMP_INVALID_FAN (-10002)
#define SETPOINT_TEMP_INVALID_OFF (-10003)
#define SETPOINT_TEMP_DEFAULT_C   (210)
#define SETPOINT_TEMP_DEFAULT_F   (700)

typedef enum
{
    HVAC_AUTO = 0,
    HVAC_OFF = 1,
    HVAC_HEATING_ONLY = 2,
    HVAC_COOLING_ONLY = 3,
    HVAC_FAN_ONLY = 4,
    NUM_HVAC_MODES = 5
} THERMOSTAT_MODE_T;          // set by user -- do not reorder -- html and ssi.c rely on order

typedef enum
{
    HEATING_AND_COOLING_OFF = 0,
    HEATING_IN_PROGRESS = 1,
    COOLING_IN_PROGRESS = 2,
    FAN_ONLY_IN_PROGRESS = 3,
    DUCT_PURGE = 4,
    THERMOSTAT_LOCKOUT = 5,
    EXCESSIVE_OVERSHOOT = 6
} THERMOSTAT_STATE_T;         // operational state

typedef enum
{
    COOLING_LAG = 0,
    HEATING_LAG = 1,   
    NUM_MOMENTUMS   = 2
} CLIMATE_LAG_T;

typedef struct
{
    TickType_t change_tick;
    THERMOSTAT_STATE_T new_state;      
    int change_temperature;   
} HVAC_STATE_CHANGE_LOG_T;

typedef struct
{
    uint32_t unix_time;   // TODO: should be using time_t
    long int temperaturex10;
    long int humidityx10;
} CLIMATE_DATAPOINT_T;

// thermostat_task.c
void thermostat_task(__unused void *params);
int make_schedule_grid(void);
//int update_current_setpoints(void);
int copy_schedule(int source_day, int destination_day);
void sanatize_schedule_temperatures(void);

// thermostat_metrics.c
int initialize_climate_metrics(void);
int accumlate_metrics(uint32_t unix_time, long int temperaturex10, long int humidityx10);
void mark_hvac_off(CLIMATE_LAG_T lag_type, long int temperaturex10);
void track_hvac_extrema(CLIMATE_LAG_T lag_type, long int temperaturex10);
void set_hvac_lag(CLIMATE_LAG_T lag_type);
void log_climate_change(int temperaturex10, int humidityx10);
int print_temperature_history(char *buffer, int length, int start_position, int num_data_points);
int predicted_time_to_temperature(long int target_temperature);

// thermostat_aht10.c
int aht10_initialize(int clock_gpio, int data_gpio);
int aht10_measurement(long int *temperaturex10, long int *humidityx10);
int ath10_gpio_enable(bool enable);

// thermostat_physcial_ui.c
bool handle_button_press_with_timeout(TickType_t timeout);
void enable_irq(bool state);
void gpio_isr(uint gpio, uint32_t events);
void hvac_update_display(int temperaturex10, THERMOSTAT_MODE_T hvac_mode, int hvac_setpoint);
int initialize_physical_buttons(int mode_button_gpio, int increase_button_gpio, int decrease_button_gpio);
THERMOSTAT_MODE_T get_front_panel_mode(void);
int dispay_initialize(int clock_gpio, int data_gpio);
int display_gpio_enable(bool enable);
int button_gpio_enable(bool enable);

// thermostat_web_ui.c
int get_free_schedule_row(void);
bool schedule_row_valid(int row);
bool day_compare(int day1, int day2);
void hvac_log_state_change(THERMOSTAT_STATE_T new_state);
bool schedule_setpoint_valid(int temperaturex10, int mow, THERMOSTAT_MODE_T mode);
bool schedule_mow_valid(int mow);
bool schedule_mode_valid(int mode);

// thermostat_hvac.c
int initialize_hvac_control(void);
THERMOSTAT_STATE_T control_thermostat_relays(long int temperaturex10);
int relay_gpio_enable(bool enable);



#endif
