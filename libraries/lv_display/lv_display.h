#pragma once

#include <lvgl.h>
#include "../../src/esp_bsp.h"
#include "../../src/lv_port.h"
#include "../../src/display.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <math.h>
#include <string>

// UI objektu deklarācijas - paplašinātas kā test22
extern lv_obj_t *blue_bar;
extern lv_obj_t *touch_point;
extern lv_obj_t *temp_label;
extern lv_obj_t *temp0;
extern lv_obj_t *damper_label;
extern lv_obj_t *target;
extern lv_obj_t *damper_status_label;
extern lv_obj_t *time_label;

// Globālo mainīgo deklarācijas (display specific)
extern int displayTargetTempC;
extern int displayWarningTemperature;

// JAUNS: Roller objektu deklarācijas (kā test22)
extern lv_obj_t *main_target_temp_roller;
extern lv_obj_t *damper_roller;

// Display inicializācijas funkcijas
void lv_display_init_hardware();
void lv_display_configure_bsp();
void lv_display_register_touch_events();

// UI izveides funkcijas
void create_main_ui();
void lv_display_init_objects();

// Touch event handleri
void touch_event_cb(lv_event_t *e);
void touch_release_cb(lv_event_t *e);

// Simulācijas funkcijas
void update_temperature_simulation();
void lv_display_start_simulation();

// LVGL task funkcijas
void lvgl_task(void *pvParameter);
void lv_display_setup_task();

// Displeja vadības funkcijas
void lv_display_update_temperature(int temp);
void lv_display_update_damper();        // Update damper percentage display
void lv_display_update_damper_status(); // Update damper status (AUTO/MANUAL/FILL!/END!)
void lv_display_show_touch_point(uint16_t x, uint16_t y, bool show);

// LVGL display wrapper functions to match display_manager naming convention
void lvgl_display_update_damper();
void lvgl_display_update_damper_status();
void lv_display_touch_update();  // JAUNS: no test22

// Damper control integration functions
bool is_manual_damper_mode();
void set_manual_damper_mode(bool enabled);  // JAUNS: papildfunkcija

// JAUNS: Funkcijas no test22
void lv_display_show_settings();
void lv_display_close_settings();
void lv_display_set_brightness(uint8_t brightness);
void lv_display_show_warning(const char* title, const char* message);
void lv_display_hide_warning();
void lv_display_set_time(const char* time_str);
void show_time_on_display();
void show_time_reset_cache();
void lv_display_update_target_temp();

// JAUNS: Roller atjaunināšanas funkcijas
void lv_display_update_main_roller();

// JAUNS: Stila funkcijas
void setup_invisible_roller_style(lv_obj_t* roller);

// JAUNS: Temperature mapping functions
int main_get_temp_index(int temperature);
int get_damper_index(int damper_value);

// JAUNS: Event handler funkcijas
void main_temp_roller_event_handler(lv_event_t * e);
void damper_roller_event_handler(lv_event_t * e);
void toggle_damper_mode(void);

// JAUNS: Brīdinājuma popup funkcijas
void lv_display_hide_warning();

// JAUNS: Deep sleep funkcijas
void enter_deep_sleep_with_touch_wakeup();
