#pragma once
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <ds18b20.h>
#include <onewire_bus.h>

// Dallas DS18B20 configuration
#define TEMPERATURE_SENSOR_GPIO     GPIO_NUM_6  // OneWire bus pin
#define MAX_DEVICES                 (8)
#define RMT_TX_CHANNEL             RMT_CHANNEL_1
#define RMT_RX_CHANNEL             RMT_CHANNEL_0

// Temperature sensor initialization and control
void init_temperature_sensor();
void update_temperature();

// Change detection functions
bool has_temperature_changed();
bool isTargetTempChanged();        // JAUNS: no test22
uint32_t get_time_since_last_temp_change();

// Temperature control functions
void set_target_temperature(int target);
void set_temperature_read_interval(uint16_t interval_ms);  // Optimized: max ~65s is enough

// Global temperature variables
extern int temperature;
extern int target_temp_c;
extern int temperature_min;
extern int temperature_max;
extern int max_temp;
extern int warning_temperature;
extern uint16_t temp_read_interval_ms;  // Optimized: max ~65s interval

// 1-Wire bus handle for DS18B20

extern onewire_bus_handle_t owb0_bus_hdl;

// Callback function types
typedef void (*temperature_change_callback_t)(int new_temp);
typedef void (*warning_callback_t)(int warning_temp);

// Register callbacks
void register_temperature_change_callback(temperature_change_callback_t callback);
void register_warning_callback(warning_callback_t callback);

// Task management
void start_temperature_task();
void stop_temperature_task();

// JAUNS: Pending damper calculation check function - kā test22
void check_pending_damper_calc();

// OWB specific cleanup function
void cleanup_temperature_sensor();