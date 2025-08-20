#pragma once

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <functional> // Pievienots std::function atbalstam

/**
 * Display Manager for VVC Project - Centralizēta displeja atjaunošanas pārvaldība
 * 
 * Šis modulis pārvalda visus displeja atjauninājumus ar optimizētiem intervaliem
 * un event-driven architecture, lai samazinātu CPU slodzi un uzlabotu performance.
 * 
 * Adaptēts VVC projektam ar LVGL displeja bibliotēku.
 */

// Default update intervals (milliseconds)
#define DEFAULT_TEMP_UPDATE_INTERVAL    2000    // Temperature updates every 2 seconds
#define DEFAULT_TIME_UPDATE_INTERVAL    30000   // Time updates every 30 seconds

// Initialization functions
void display_manager_init();

// Main update function - call this from main loop
void display_manager_update();

// Force update functions (useful for debugging/testing)
void display_manager_force_update_all();
void display_manager_force_update_temperature();
void display_manager_force_update_time();

// JAUNS: Missing damper force update functions - no test22
void display_manager_force_update_damper();
void display_manager_force_update_damper_status();

// Configuration functions
void display_manager_set_update_intervals(
    uint16_t temp_interval_ms,
    uint16_t time_interval_ms
);

// Event notification functions (call when data changes)
void display_manager_notify_temperature_changed();
void display_manager_notify_target_temp_changed();
void display_manager_notify_time_synced();

// JAUNS: WiFi integrācijas funkcijas
void display_manager_set_wifi_status(bool connected); // Uzstāda WiFi statusu un sinhronizē laiku

// JAUNS: Warning popup funkcijas
void display_manager_show_warning(const char* title, const char* message);
void display_manager_hide_warning();
void display_manager_force_update_warning();

// Damper notification functions (for damper_control integration)
void display_manager_notify_damper_changed();
void display_manager_notify_damper_position_changed();

// Display manager mutex control function - RAM optimized version
bool display_manager_try_update();

// LVGL thread-safe operations helper
bool safe_lvgl_operation(std::function<void(void)> update_func);

// Statistics and debugging (RAM optimized with uint16_t)
typedef struct {
    uint16_t total_updates;      // Optimized: 65K updates is enough
    uint16_t temp_updates;       // Optimized: 65K updates is enough  
    uint16_t time_updates;       // Optimized: 65K updates is enough
    uint16_t skipped_updates;    // Optimized: 65K updates is enough
    uint32_t last_update_time;   // Keep 64-bit for precise timing
} display_stats_t;

display_stats_t display_manager_get_stats();
void display_manager_get_efficiency_stats(float* efficiency, uint16_t* total_updates, 
                                         uint16_t* skipped_updates, uint32_t* uptime_minutes);
void display_manager_reset_stats();
void display_manager_print_stats();

// Configuration info
void display_manager_print_config();
const char* display_manager_get_version();
