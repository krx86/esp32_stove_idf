#pragma once

#include <lvgl.h>

// Create settings UI objects if not created yet
void settings_screen_create(void);

// Show/hide helpers
void settings_screen_show(void);
void settings_screen_hide(void);
bool settings_screen_is_visible(void);
