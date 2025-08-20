# Temperature Library

Temperature sensor management library with Dallas DS18B20 support for ESP-IDF framework.

## Purpose
This library provides temperature monitoring using Dallas DS18B20 sensor with OneWire bus, including change detection and callback system for temperature-based control systems.

## Features
- **Dallas DS18B20 sensor support** via OneWire bus
- **Automatic fallback** to simulation mode if sensor fails
- Temperature change detection
- Configurable read intervals
- Warning temperature thresholds
- Callback system for temperature changes and warnings
- FreeRTOS task-based monitoring
- Target temperature management

## Hardware Configuration
- **Sensor**: Dallas DS18B20 waterproof temperature sensor
- **GPIO Pin**: GPIO_NUM_6 (configurable in temperature.h)
- **Resolution**: 9-bit (configurable)
- **OneWire Bus**: RMT driver based

## Usage
Include `temperature.h` in your code and call the appropriate functions:

```cpp
#include "temperature.h"

// Initialize sensor (tries Dallas first, falls back to simulation)
init_temperature_sensor();

// Set callbacks
register_temperature_change_callback(my_temp_callback);
register_warning_callback(my_warning_callback);

// Start monitoring task
start_temperature_task();

// Set target temperature
set_target_temperature(70);

// Check if Dallas sensor is working
if (is_dallas_sensor_available()) {
    ESP_LOGI("MAIN", "Using Dallas DS18B20 sensor");
} else {
    ESP_LOGI("MAIN", "Using temperature simulation");
}
```

## Functions
- `init_temperature_sensor()` - Initialize temperature monitoring (Dallas + fallback)
- `init_dallas_sensor()` - Specifically initialize Dallas DS18B20
- `read_dallas_temperature()` - Read from Dallas sensor
- `is_dallas_sensor_available()` - Check sensor availability
- `start_temperature_task()` - Start FreeRTOS monitoring task
- `set_target_temperature(int)` - Set target temperature
- `register_temperature_change_callback()` - Register change callback

## Dependencies
- ESP-IDF framework
- FreeRTOS
- ESP logging system
- **onewire_bus** component
- **ds18b20** component

## Configuration
- Default GPIO: GPIO_NUM_6
- Default read interval: 2000ms
- Temperature range: 20-100°C
- Warning threshold: 80°C
- Sensor resolution: 9-bit (0.5°C precision)

## Sensor Address
The library uses a predefined sensor address. For automatic detection, modify the `init_dallas_sensor()` function to scan for available sensors on the OneWire bus.
