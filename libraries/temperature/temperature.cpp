#include "temperature.h"
#include <math.h>
#include <ds18b20.h>
#include "display_manager.h"
#include "../damper_control/damper_control.h"

#include <onewire_bus.h>
onewire_bus_handle_t owb0_bus_hdl = NULL;

// 1-Wire kopnes handle no cita faila
extern onewire_bus_handle_t owb0_bus_hdl;

static const char *TAG = "TEMPERATURE";

// Global temperature variables
int temperature = 24;
int target_temp_c = 68;
int temperature_min = 40;
int temperature_max = 100;
int max_temp = 85;
int warning_temperature = 81;
uint16_t temp_read_interval_ms = 5000; // Optimized: 5 seconds default (max ~65s)

// Internal variables
static uint32_t last_temp_read = 0;
static TaskHandle_t temperature_task_handle = NULL;

// Temperature validation variables
static int last_valid_temperature = 5; // Sākotnējā vērtība

// Change detection variables
static int last_displayed_temperature = -999;  // Force first update
static uint32_t last_change_time = 0;
static bool temperature_changed = false;
static bool target_temp_changed = false;       // JAUNS: no test22

// JAUNS: Pending damper calculation variable kā test22
static bool damperCalcPending = false;

// Callback variables
static temperature_change_callback_t temp_change_callback = NULL;
static warning_callback_t warning_callback = NULL;

// DS18B20 sensor state

// DS18B20 new library handles
static ds18b20_handle_t ds18b20_dev_hdl = NULL;
// Removed unused variable ds18b20_dev
static bool sensor_initialized = false;

// Initialize temperature sensor system

void init_temperature_sensor() {
    // Inicializējam 1-Wire kopni uz GPIO 6, ja nav inicializēta
    if (owb0_bus_hdl == NULL) {
        onewire_bus_config_t bus_config = {
            .bus_gpio_num = 6,
            .flags = { .en_pull_up = 1 }
        };
        onewire_bus_rmt_config_t rmt_config = {
            .max_rx_bytes = 10
        };
        ESP_ERROR_CHECK(onewire_new_bus_rmt(&bus_config, &rmt_config, &owb0_bus_hdl));
    }
    ESP_LOGI(TAG, "Initializing DS18B20 temperature sensor");

    // 1. Detect DS18B20 devices (as in your example)
    onewire_device_t devices[1];
    uint8_t devices_count = 0;
    
    esp_err_t ret = ds18b20_detect(owb0_bus_hdl, devices, 1, &devices_count);
    if (ret != ESP_OK || devices_count == 0) {
        ESP_LOGE(TAG, "No DS18B20 devices found: %s", esp_err_to_name(ret));
        return;
    }

    // 2. Create device iterator (as in your example)
    onewire_device_iter_handle_t dev_iter_hdl;
    onewire_device_t dev;
    
    ESP_ERROR_CHECK(onewire_new_device_iter(owb0_bus_hdl, &dev_iter_hdl));
    
    // 3. Initialize first found device (as in your example)
    if (onewire_device_iter_get_next(dev_iter_hdl, &dev) == ESP_OK) {
        ds18b20_config_t dev_cfg = OWB_DS18B20_CONFIG_DEFAULT;
        if (ds18b20_init(&dev, &dev_cfg, &ds18b20_dev_hdl) == ESP_OK) {
            ESP_LOGI(TAG, "DS18B20 initialized, address: %016llX", dev.address);
        }
    }
    
    ESP_ERROR_CHECK(onewire_del_device_iter(dev_iter_hdl));

    // 4. Set resolution to 9-bit
    ESP_ERROR_CHECK(ds18b20_set_resolution(ds18b20_dev_hdl, DS18B20_RESOLUTION_9BIT));
    
    sensor_initialized = true;
    last_temp_read = esp_timer_get_time() / 1000;
    ESP_LOGI(TAG, "DS18B20 sensor ready");
}

// Main temperature update function (EXACT like test22)
void update_temperature() {
    uint32_t current_time = esp_timer_get_time() / 1000;

    if (current_time - last_temp_read >= temp_read_interval_ms) {
        float temp_float = 0.0;
        esp_err_t result = ds18b20_get_temperature(ds18b20_dev_hdl, &temp_float);
        
        if (result == ESP_OK) {
            int new_temperature = (int)roundf(temp_float);
            
            // Validate temperature
            if (new_temperature < -50 || new_temperature > 150) {
                ESP_LOGW(TAG, "Invalid temperature: %d°C", new_temperature);
                return;
            }

            // Check for sudden changes
            if (abs(new_temperature - temperature) > 10 && temperature != 24) {
                ESP_LOGW(TAG, "Sudden temperature change: %d°C -> %d°C", 
                        temperature, new_temperature);
                return;
            }

            // Update temperature
            temperature = new_temperature;
            last_valid_temperature = new_temperature;

            if (new_temperature != last_displayed_temperature) {
                last_displayed_temperature = temperature;
                last_change_time = current_time;
                temperature_changed = true;

                // Handle warnings
                static bool warning_shown = false;
                if (temperature > warning_temperature && !warning_shown) {
                    display_manager_show_warning("WARNING!", "Temperature too high!");
                    warning_shown = true;
                } else if (temperature <= warning_temperature && warning_shown) {
                    display_manager_hide_warning();
                    warning_shown = false;
                }

                // Notify display
                display_manager_notify_temperature_changed();
                
                // Handle damper control
                if (!servoMoving) {
                    damperControlLoop();
                    damperCalcPending = false;
                } else {
                    damperCalcPending = true;
                }

                // Call callback
                if (temp_change_callback) {
                    temp_change_callback(temperature);
                }
            }
        } else {
            ESP_LOGW(TAG, "Failed to read temperature: %s", esp_err_to_name(result));
        }

        last_temp_read = current_time;
    }
}
// Change detection functions
bool has_temperature_changed() {
    if (temperature_changed) {
        temperature_changed = false; // Reset flag after reading
        return true;
    }
    return false;
}

// JAUNS: Target temperature change detection - no test22
bool isTargetTempChanged() {
    if (target_temp_changed) {
        target_temp_changed = false; // Reset flag after reading
        return true;
    }
    return false;
}

uint32_t get_time_since_last_temp_change() {
    uint32_t current_time = esp_timer_get_time() / 1000;
    return current_time - last_change_time;
}

// Temperature control functions
void set_target_temperature(int target) {
    if (target >= temperature_min && target <= max_temp) {
        // JAUNS: Check if target actually changed - no test2m2
        if (target != target_temp_c) {
            target_temp_c = target;
            target_temp_changed = true;

            // Notify display manager
            display_manager_notify_target_temp_changed();

            ESP_LOGI(TAG, "Target temperature set to: %d°C", target_temp_c);
        }
    } else {
        ESP_LOGI(TAG, "Invalid target temperature: %d°C (range: %d-%d°C)", 
                target, temperature_min, max_temp);
    }
}

void set_temperature_read_interval(uint16_t interval_ms) {
    if (interval_ms >= 100 && interval_ms <= 60000) { // 100ms to 60s range
        temp_read_interval_ms = interval_ms;
        ESP_LOGI(TAG, "Temperature read interval set to: %u ms", temp_read_interval_ms);
    } else {
        ESP_LOGI(TAG, "Invalid read interval: %u ms (range: 100-60000 ms)", interval_ms);
    }
}

// Callback registration functions
void register_temperature_change_callback(temperature_change_callback_t callback) {
    temp_change_callback = callback;
    ESP_LOGI(TAG, "Temperature change callback registered");
}

void register_warning_callback(warning_callback_t callback) {
    warning_callback = callback;
    ESP_LOGI(TAG, "Warning callback registered");
}

// Temperature monitoring task
static void temperature_task(void *pvParameter) {
    ESP_LOGI(TAG, "Temperature monitoring task started");

    // JAUNS: Periodisks damper aprēķinu counter kā test22
    static uint32_t last_periodic_damper_call = 0;

    while (1) {
        update_temperature();

        // JAUNS: Pārbaudām pending damper aprēķinus (kad servo beidz kustību) - kā test22
        if (damperCalcPending && !servoMoving) {
            damperControlLoop();
            ESP_LOGI(TAG, "SERVO_FINISHED - Executed pending damper calculation");
            damperCalcPending = false;
        }

        // JAUNS: Periodiska damper kontrole zemas temperatūras režīmā (ik 30 sek) - kā test22
        uint32_t current_time = esp_timer_get_time() / 1000;
        if (lowTempCheckActive && (current_time - last_periodic_damper_call > 30000)) {
            damperControlLoop();
            ESP_LOGI(TAG, "LOW_TEMP_MODE - Periodic damper control executed");
            last_periodic_damper_call = current_time;
        }

        vTaskDelay(pdMS_TO_TICKS(200)); // Check every 200ms (reasonable for async)
    }
}

// Task management
void start_temperature_task() {
    if (temperature_task_handle == NULL) {
        xTaskCreate(temperature_task, "temperature_task", 4096, NULL, 5, &temperature_task_handle);
        ESP_LOGI(TAG, "Temperature monitoring task created with 4KB stack");
    } else {
        ESP_LOGI(TAG, "Temperature task already running");
    }
}


void stop_temperature_task() {
    if (temperature_task_handle != NULL) {
        vTaskDelete(temperature_task_handle);
        temperature_task_handle = NULL;
        ESP_LOGI(TAG, "Temperature monitoring task stopped");
    }

    // Free DS18B20 resources (new library)
    if(ds18b20_dev_hdl != NULL) {
        ds18b20_delete(ds18b20_dev_hdl);
        ds18b20_dev_hdl = NULL;
    }
    sensor_initialized = false;
}