# LV Display Library

Display management library for LVGL interface in VVC project.

## Purpose
This library provides a clean interface for managing LVGL display objects, handling touch events, and updating UI elements.

## Features
- Temperature display management
- Touch point visualization
- Damper control interface
- Settings screen management
- Warning popups
- Display brightness control

## Usage
Include `lv_display.h` in your main code and call the appropriate functions to manage the display.

## Functions
- `lv_display_init()` - Initialize display system
- `lv_display_update_temperature(int)` - Update temperature display
- `lv_display_show_touch_point(x, y, show)` - Show/hide touch indicator
- `lv_display_show_settings()` - Open settings screen
- `lv_display_set_brightness(uint8_t)` - Control display brightness

## Dependencies
- LVGL library
- ESP-BSP
- ESP-IDF logging system
