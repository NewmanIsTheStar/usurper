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
} THERMOSTAT_MODE_T;          // set by user -- do not reorder -- html and ssi.c rely on order

typedef enum
{
    HEATING_AND_COOLING_OFF = 0,
    HEATING_IN_PROGRESS = 1,
    COOLING_IN_PROGRESS = 2,
    DUCT_PURGE = 3,
    THERMOSTAT_LOCKOUT = 4,
    EXCESSIVE_OVERSHOOT = 5
} THERMOSTAT_STATE_T;         // operational state

typedef enum
{
    COOLING_MOMENTUM = 0,
    HEATING_MOMENTUM = 1,   
    NUM_MOMENTUMS   = 2
} CLIMATE_MOMENTUM_T;

typedef struct
{
    TickType_t change_tick;
    THERMOSTAT_STATE_T new_state;      
    int change_temperature;   
} HVAC_STATE_CHANGE_LOG_T;


//prototypes
void thermostat_task(__unused void *params);
int make_schedule_grid(void);
//int update_current_setpoints(void);
int copy_schedule(int source_day, int destination_day);
void sanatize_schedule_temperatures(void);

// thermostat_metrics.c
int initialize_climate_metrics(void);
int accumlate_temperature_metrics(long int temperaturex10);
void mark_hvac_off(CLIMATE_MOMENTUM_T momentum_type, long int temperaturex10);
void track_hvac_extrema(CLIMATE_MOMENTUM_T momentum_type, long int temperaturex10);
void set_hvac_momentum(CLIMATE_MOMENTUM_T momentum_type);
void log_climate_change(int temperaturex10, int humidityx10);

// thermostat_aht10.c
int aht10_initialize(int clock_gpio, int data_gpio);
int aht10_measurement(long int *temperaturex10, long int *humidityx10);

// thermostat_physcial_ui.c
int handle_button_press_with_timeout(TickType_t timeout);
void enable_irq(bool state);
void gpio_isr(uint gpio, uint32_t events);
void hvac_update_display(int temperaturex10, THERMOSTAT_MODE_T hvac_mode, int hvac_setpoint);
int initialize_physical_buttons(int mode_button_gpio, int increase_button_gpio, int decrease_button_gpio);

// thermostat_web_ui.c
int get_free_schedule_row(void);
bool schedule_row_valid(int row);
bool day_compare(int day1, int day2);
void hvac_log_state_change(THERMOSTAT_STATE_T new_state);

// thermostat_hvac.c
int initialize_hvac_control(void);
THERMOSTAT_STATE_T control_thermostat_relays(long int temperaturex10);

#endif
