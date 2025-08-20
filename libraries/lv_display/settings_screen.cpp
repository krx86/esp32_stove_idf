#include "settings_screen.h"
#include "lv_display.h"
#include "temperature.h"
#include "display_manager.h"

// ===== Globālie mainīgie (VVC) =====
extern int target_temp_c;
extern int temperature_min;
extern uint16_t temp_read_interval_ms;
extern int displayWarningTemperature; // deprecated mirror
extern int warning_temperature;       // primārais mainīgais (kā test22)
extern int damper; // izvēles: manuāls damper slīdnis

// JAUNI: Damper un servo parametri no VVC
extern int kP;
extern float tauD;
extern float tauI;
extern float kI;
extern float kD;
extern float endTrigger;
extern unsigned long LOW_TEMP_TIMEOUT;
extern int servoAngle;
extern int servoOffset;
extern int servoStepInterval;

// Iekšējais glabājums Display cilnes laikam (test22 stilā)
static uint32_t timeUpdateIntervalMs = DEFAULT_TIME_UPDATE_INTERVAL;

// ===== Ekrāna objekti =====
static lv_obj_t * settings_screen = NULL;
static bool is_visible = false;
static lv_obj_t * tab_view = NULL;
static lv_obj_t * tab_temp = NULL;
static lv_obj_t * tab_damper = NULL;
static lv_obj_t * tab_servo = NULL;
static lv_obj_t * tab_display = NULL;
static lv_obj_t * tab_alarm = NULL;
static lv_obj_t * tab_system = NULL;

// ===== Palīgfunkcijas kā test22 =====
static lv_obj_t* create_settings_slider(lv_obj_t* parent, int x, int y, int min_val, int max_val, int current_val)
{
    lv_obj_t * slider = lv_slider_create(parent);
    lv_obj_set_width(slider, 120);
    lv_obj_set_height(slider, 20);
    lv_obj_align(slider, LV_ALIGN_TOP_LEFT, x, y);
    lv_slider_set_range(slider, min_val, max_val);
    lv_slider_set_value(slider, current_val, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x0088FF), LV_PART_INDICATOR);
    return slider;
}

static void generic_slider_event_handler(lv_event_t * e, void(*update_function)(int), const char* format)
{
    lv_obj_t * slider = lv_event_get_target(e);
    int value = lv_slider_get_value(slider);
    if (update_function) {
        update_function(value);
    }
    lv_obj_t * label = (lv_obj_t*)lv_event_get_user_data(e);
    if (label && format) {
        lv_label_set_text_fmt(label, format, value);
    }
}

static void update_slider_value(lv_obj_t* tab, int slider_index, int value, const char* format)
{
    lv_obj_t * slider = lv_obj_get_child(tab, slider_index);
    if (slider) {
        lv_slider_set_value(slider, value, LV_ANIM_OFF);
        lv_obj_t * label = lv_obj_get_child(tab, slider_index + 1);
        if (label && format) {
            lv_label_set_text_fmt(label, format, value);
        }
    }
}

// ===== Update funkcijas (pielāgotas VVC) =====
static void update_temperatureMin(int value) { temperature_min = value; }
static void update_warningTemperature(int value) {
    // primārais
    warning_temperature = value;
    // uzturam spoguļvērtību, lai cits kods vēl strādā
    displayWarningTemperature = value;
}
static void update_servoAngle(int value) { servoAngle = value; }
static void update_servoOffset(int value) { servoOffset = value; }
static void update_servoStepInterval(int value) { servoStepInterval = value; }

// ===== Ekrāna izveide (test22 izkārtojums) =====
void settings_screen_create(void)
{
    if (settings_screen) {
        return;
    }

    // Main screen
    settings_screen = lv_obj_create(lv_scr_act());
    lv_obj_set_size(settings_screen, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(settings_screen, 0, 0);
    lv_obj_add_flag(settings_screen, LV_OBJ_FLAG_HIDDEN);

    // Back button
    lv_obj_t * back_btn = lv_btn_create(settings_screen);
    lv_obj_set_size(back_btn, 80, 40);
    lv_obj_set_pos(back_btn, 20, 20);
    lv_obj_t * back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_center(back_label);
    lv_obj_add_event_cb(back_btn, [](lv_event_t * e) {
        (void)e;
        lv_display_close_settings();
    }, LV_EVENT_CLICKED, NULL);

    // Piezīme: NAV Save pogas (pēc prasības)

    // Create tabview (tieši tādi izmēri kā test22)
    tab_view = lv_tabview_create(settings_screen, LV_DIR_LEFT, 80);
    lv_obj_set_size(tab_view, LV_HOR_RES - 30 , LV_VER_RES - 100);
    lv_obj_set_pos(tab_view, 0, 70);

    // Tabs ar identiskām nosaukumu virkām
    tab_temp    = lv_tabview_add_tab(tab_view, "Temp.");
    tab_damper  = lv_tabview_add_tab(tab_view, "Damper");
    tab_servo   = lv_tabview_add_tab(tab_view, "Servo");
    tab_display = lv_tabview_add_tab(tab_view, "Display");
    tab_alarm   = lv_tabview_add_tab(tab_view, "Alarm");
    tab_system  = lv_tabview_add_tab(tab_view, "System");

    // --- Temperature Tab Content ---
    // Target Temp
    lv_obj_t * target_temp_label = lv_label_create(tab_temp);
    lv_label_set_text(target_temp_label, "Target Temp");
    lv_obj_align(target_temp_label, LV_ALIGN_TOP_LEFT, 10, 10);

    // Diapazons pielāgots VVC galvenajam rollerim (64..80, solis 2)
    if (target_temp_c < 62) target_temp_c = 62;
    if (target_temp_c > 80) target_temp_c = 80;
    target_temp_c = ((target_temp_c + 1) / 2) * 2;

    lv_obj_t * target_temp_slider = create_settings_slider(tab_temp, 10, 40, 62, 80, target_temp_c);
    lv_slider_set_mode(target_temp_slider, LV_SLIDER_MODE_NORMAL);

    // Uzvedība: solis 2 grādi (reāllaikā) un atjauno galveno ekrānu caur set_target_temperature
    lv_obj_add_event_cb(target_temp_slider, [](lv_event_t * e) {
        lv_obj_t * slider = lv_event_get_target(e);
        int value = lv_slider_get_value(slider);
        value = ((value + 1) / 2) * 2; // tuvākais pāra skaitlis
        if (value < 62) value = 62;
        if (value > 80) value = 80;
        lv_slider_set_value(slider, value, LV_ANIM_OFF);
        set_target_temperature(value);
    }, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t * target_temp_value = lv_label_create(tab_temp);
    lv_label_set_text_fmt(target_temp_value, "%d°C", target_temp_c);
    lv_obj_align(target_temp_value, LV_ALIGN_TOP_LEFT, 140, 40);

    // Min Temp
    lv_obj_t * min_temp_label = lv_label_create(tab_temp);
    lv_label_set_text(min_temp_label, "Min Temp");
    lv_obj_align(min_temp_label, LV_ALIGN_TOP_LEFT, 10, 80);

    lv_obj_t * min_temp_slider = create_settings_slider(tab_temp, 10, 110, 40, 55, temperature_min);
    lv_obj_t * min_temp_value = lv_label_create(tab_temp);
    lv_label_set_text_fmt(min_temp_value, "%d°C", temperature_min);
    lv_obj_align(min_temp_value, LV_ALIGN_TOP_LEFT, 140, 110);
    lv_obj_add_event_cb(min_temp_slider, [](lv_event_t * e) {
        generic_slider_event_handler(e, update_temperatureMin, "%d°C");
    }, LV_EVENT_VALUE_CHANGED, min_temp_value);

    // Read Interval (sekundēs)
    lv_obj_t * read_interval_label = lv_label_create(tab_temp);
    lv_label_set_text(read_interval_label, "Read Interval");
    lv_obj_align(read_interval_label, LV_ALIGN_TOP_LEFT, 10, 150);

    // korekcija 2s solī
    if (temp_read_interval_ms < 2000) temp_read_interval_ms = 2000;
    if (temp_read_interval_ms > 10000) temp_read_interval_ms = 10000;
    temp_read_interval_ms = ((temp_read_interval_ms + 1000) / 2000) * 2000;

    lv_obj_t * read_interval_slider = create_settings_slider(tab_temp, 10, 180, 2000, 10000, temp_read_interval_ms);
    lv_obj_t * read_interval_value = lv_label_create(tab_temp);
    lv_label_set_text_fmt(read_interval_value, "%d.0 s", temp_read_interval_ms/1000);
    lv_obj_align(read_interval_value, LV_ALIGN_TOP_LEFT, 140, 180);
    lv_obj_add_event_cb(read_interval_slider, [](lv_event_t * e) {
        lv_obj_t * slider = lv_event_get_target(e);
        int value = lv_slider_get_value(slider);
        value = ((value + 1000) / 2000) * 2000; // 2s solis
        if (value < 2000) value = 2000;
        if (value > 10000) value = 10000;
        lv_slider_set_value(slider, value, LV_ANIM_OFF);
        temp_read_interval_ms = (uint16_t)value;
        lv_obj_t * label = (lv_obj_t*)lv_event_get_user_data(e);
        if (label) lv_label_set_text_fmt(label, "%d.0 s", value/1000);
        set_temperature_read_interval((uint16_t)value);
    }, LV_EVENT_VALUE_CHANGED, read_interval_value);

    // --- Damper Tab Content ---
    // kP slider - PID proporcionalitātes koeficients
    lv_obj_t * kp_label = lv_label_create(tab_damper);
    lv_label_set_text(kp_label, "kP Value");
    lv_obj_align(kp_label, LV_ALIGN_TOP_LEFT, 10, 10);
    
    lv_obj_t * kp_slider = create_settings_slider(tab_damper, 10, 40, 1, 100, kP);
    
    lv_obj_t * kp_value = lv_label_create(tab_damper);
    lv_label_set_text_fmt(kp_value, "%d", kP);
    lv_obj_align(kp_value, LV_ALIGN_TOP_LEFT, 140, 40);
    
    lv_obj_add_event_cb(kp_slider, [](lv_event_t * e) {
        lv_obj_t * slider = lv_event_get_target(e);
        int value = lv_slider_get_value(slider);
        kP = value;
        // Aprēķinam atkarīgās vērtības
        kI = kP / tauI;
        kD = kP * tauD;
        
        lv_obj_t * label = (lv_obj_t*)lv_event_get_user_data(e);
        lv_label_set_text_fmt(label, "%d", value);
    }, LV_EVENT_VALUE_CHANGED, kp_value);
    
    // tauD slider - PID atvasināšanas laika konstante
    lv_obj_t * taud_label = lv_label_create(tab_damper);
    lv_label_set_text(taud_label, "tauD Value");
    lv_obj_align(taud_label, LV_ALIGN_TOP_LEFT, 10, 80);
    
    lv_obj_t * taud_slider = create_settings_slider(tab_damper, 10, 110, 1, 100, (int)tauD);
    
    lv_obj_t * taud_value = lv_label_create(tab_damper);
    lv_label_set_text_fmt(taud_value, "%d", (int)tauD);
    lv_obj_align(taud_value, LV_ALIGN_TOP_LEFT, 140, 110);
    
    lv_obj_add_event_cb(taud_slider, [](lv_event_t * e) {
        lv_obj_t * slider = lv_event_get_target(e);
        int value = lv_slider_get_value(slider);
        tauD = value;
        // Aprēķinam atkarīgās vērtības
        kD = kP * tauD;
        
        lv_obj_t * label = (lv_obj_t*)lv_event_get_user_data(e);
        lv_label_set_text_fmt(label, "%d", value);
    }, LV_EVENT_VALUE_CHANGED, taud_value);
    
    // endTrigger slider - beigu slieksnis
    lv_obj_t * end_trigger_label = lv_label_create(tab_damper);
    lv_label_set_text(end_trigger_label, "End Trigger");
    lv_obj_align(end_trigger_label, LV_ALIGN_TOP_LEFT, 10, 150);
    
    lv_obj_t * end_trigger_slider = create_settings_slider(tab_damper, 10, 180, 5000, 30000, (int)endTrigger);
    
    lv_obj_t * end_trigger_value = lv_label_create(tab_damper);
    lv_label_set_text_fmt(end_trigger_value, "%d", (int)endTrigger);
    lv_obj_align(end_trigger_value, LV_ALIGN_TOP_LEFT, 140, 180);
    
    lv_obj_add_event_cb(end_trigger_slider, [](lv_event_t * e) {
        lv_obj_t * slider = lv_event_get_target(e);
        int value = lv_slider_get_value(slider);
        endTrigger = value;
        
        lv_obj_t * label = (lv_obj_t*)lv_event_get_user_data(e);
        lv_label_set_text_fmt(label, "%d", value);
    }, LV_EVENT_VALUE_CHANGED, end_trigger_value);
    
    // LOW_TEMP_TIMEOUT slider - zemas temperatūras taimeris
    lv_obj_t * low_temp_timeout_label = lv_label_create(tab_damper);
    lv_label_set_text(low_temp_timeout_label, "Low Temp Timeout");
    lv_obj_align(low_temp_timeout_label, LV_ALIGN_TOP_LEFT, 10, 220);
    
    // Pārbaudam vai LOW_TEMP_TIMEOUT ir pieņemamajā diapazonā
    if (LOW_TEMP_TIMEOUT < 60000) LOW_TEMP_TIMEOUT = 60000; // Min 1 minūte
    if (LOW_TEMP_TIMEOUT > 600000) LOW_TEMP_TIMEOUT = 600000; // Max 10 minūtes
    // Noapaļojam uz tuvāko minūti (60000ms)
    LOW_TEMP_TIMEOUT = ((LOW_TEMP_TIMEOUT + 30000) / 60000) * 60000;
    
    lv_obj_t * low_temp_timeout_slider = create_settings_slider(tab_damper, 10, 250, 60000, 600000, (int)LOW_TEMP_TIMEOUT);
    
    lv_obj_t * low_temp_timeout_value = lv_label_create(tab_damper);
    lv_label_set_text_fmt(low_temp_timeout_value, "%d min", (int)(LOW_TEMP_TIMEOUT/60000));
    lv_obj_align(low_temp_timeout_value, LV_ALIGN_TOP_LEFT, 140, 250);
    
    lv_obj_add_event_cb(low_temp_timeout_slider, [](lv_event_t * e) {
        lv_obj_t * slider = lv_event_get_target(e);
        int value = lv_slider_get_value(slider);
        
        // Noapaļojam uz tuvāko minūti (60000ms)
        value = ((value + 30000) / 60000) * 60000;
        lv_slider_set_value(slider, value, LV_ANIM_OFF);
        
        // Atjauninām globālo mainīgo
        LOW_TEMP_TIMEOUT = (unsigned long)value;
        
        lv_obj_t * label = (lv_obj_t*)lv_event_get_user_data(e);
        lv_label_set_text_fmt(label, "%d min", value/60000);
    }, LV_EVENT_VALUE_CHANGED, low_temp_timeout_value);

    // --- Servo Tab Content --- (test22: Servo Angle, Servo Offset)
    lv_obj_t * servo_angle_label = lv_label_create(tab_servo);
    lv_label_set_text(servo_angle_label, "Servo Angle");
    lv_obj_align(servo_angle_label, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_t * servo_angle_slider = create_settings_slider(tab_servo, 10, 40, 10, 100, servoAngle);
    lv_obj_t * servo_angle_value = lv_label_create(tab_servo);
    lv_label_set_text_fmt(servo_angle_value, "%d°", servoAngle);
    lv_obj_align(servo_angle_value, LV_ALIGN_TOP_LEFT, 140, 40);
    lv_obj_add_event_cb(servo_angle_slider, [](lv_event_t * e) {
        generic_slider_event_handler(e, update_servoAngle, "%d°");
    }, LV_EVENT_VALUE_CHANGED, servo_angle_value);

    lv_obj_t * servo_offset_label = lv_label_create(tab_servo);
    lv_label_set_text(servo_offset_label, "Servo Offset");
    lv_obj_align(servo_offset_label, LV_ALIGN_TOP_LEFT, 10, 80);
    lv_obj_t * servo_offset_slider = create_settings_slider(tab_servo, 10, 110, 0, 90, servoOffset);
    lv_obj_t * servo_offset_value = lv_label_create(tab_servo);
    lv_label_set_text_fmt(servo_offset_value, "%d°", servoOffset);
    lv_obj_align(servo_offset_value, LV_ALIGN_TOP_LEFT, 140, 110);
    lv_obj_add_event_cb(servo_offset_slider, [](lv_event_t * e) {
        generic_slider_event_handler(e, update_servoOffset, "%d°");
    }, LV_EVENT_VALUE_CHANGED, servo_offset_value);

    // Servo Step Interval (ms)
    lv_obj_t * servo_step_interval_label = lv_label_create(tab_servo);
    lv_label_set_text(servo_step_interval_label, "Servo Step Interval");
    lv_obj_align(servo_step_interval_label, LV_ALIGN_TOP_LEFT, 10, 150);
    lv_obj_t * servo_step_interval_slider = create_settings_slider(tab_servo, 10, 180, 10, 200, servoStepInterval);
    lv_obj_t * servo_step_interval_value = lv_label_create(tab_servo);
    lv_label_set_text_fmt(servo_step_interval_value, "%d ms", servoStepInterval);
    lv_obj_align(servo_step_interval_value, LV_ALIGN_TOP_LEFT, 140, 180);
    lv_obj_add_event_cb(servo_step_interval_slider, [](lv_event_t * e) {
        generic_slider_event_handler(e, update_servoStepInterval, "%d ms");
    }, LV_EVENT_VALUE_CHANGED, servo_step_interval_value);

    // --- Display Tab Content ---
    // Time Update
    lv_obj_t * time_update_label = lv_label_create(tab_display);
    lv_label_set_text(time_update_label, "Time Update");
    lv_obj_align(time_update_label, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_t * time_update_slider = create_settings_slider(tab_display, 10, 40, 10000, 60000, (int)timeUpdateIntervalMs);
    lv_obj_t * time_update_value = lv_label_create(tab_display);
    lv_label_set_text_fmt(time_update_value, "%d s", (int)(timeUpdateIntervalMs/1000));
    lv_obj_align(time_update_value, LV_ALIGN_TOP_LEFT, 140, 40);
    lv_obj_add_event_cb(time_update_slider, [](lv_event_t * e){
        lv_obj_t * slider = lv_event_get_target(e);
        int value = lv_slider_get_value(slider);
        // 10s solis
        value = ((value + 5000) / 10000) * 10000;
        if (value < 10000) value = 10000;
        if (value > 60000) value = 60000;
        lv_slider_set_value(slider, value, LV_ANIM_OFF);
        timeUpdateIntervalMs = (uint32_t)value;
        display_manager_set_update_intervals(0, (uint16_t)value);
        lv_obj_t * label = (lv_obj_t*)lv_event_get_user_data(e);
        if (label) lv_label_set_text_fmt(label, "%d s", value/1000);
    }, LV_EVENT_VALUE_CHANGED, time_update_value);

    // --- Alarm Tab Content ---
    lv_obj_t * warning_temp_label = lv_label_create(tab_alarm);
    lv_label_set_text(warning_temp_label, "Warning Temp");
    lv_obj_align(warning_temp_label, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_t * warning_temp_slider = create_settings_slider(tab_alarm, 10, 40, 80, 95, warning_temperature);
    lv_obj_t * warning_temp_value = lv_label_create(tab_alarm);
    lv_label_set_text_fmt(warning_temp_value, "%d°C", warning_temperature);
    lv_obj_align(warning_temp_value, LV_ALIGN_TOP_LEFT, 140, 40);
    lv_obj_add_event_cb(warning_temp_slider, [](lv_event_t * e){
        generic_slider_event_handler(e, update_warningTemperature, "%d°C");
        // pārzīmē brīdinājumu stāvokli
        display_manager_force_update_temperature();
    }, LV_EVENT_VALUE_CHANGED, warning_temp_value);

    // --- System Tab Content --- (nav specifisku elementu VVC)
}

// ===== Rādīšana/paslēpšana (test22 stils) =====
void settings_screen_show(void)
{
    if (!settings_screen) {
        settings_screen_create();
    }
    is_visible = true;
    lv_obj_clear_flag(settings_screen, LV_OBJ_FLAG_HIDDEN);

    // Atjauno redzamās vērtības no globālajiem mainīgajiem
    if (tab_temp) {
        // Target
        if (target_temp_c < 62) target_temp_c = 62;
        if (target_temp_c > 80) target_temp_c = 80;
        target_temp_c = ((target_temp_c + 1) / 2) * 2;
        update_slider_value(tab_temp, 1, target_temp_c, "%d°C");
        // Min temp
        update_slider_value(tab_temp, 4, temperature_min, "%d°C");
        // Read interval
        if (temp_read_interval_ms < 2000) temp_read_interval_ms = 2000;
        if (temp_read_interval_ms > 10000) temp_read_interval_ms = 10000;
        temp_read_interval_ms = ((temp_read_interval_ms + 1000) / 2000) * 2000;
        lv_obj_t * slider = lv_obj_get_child(tab_temp, 7);
        if (slider) lv_slider_set_value(slider, temp_read_interval_ms, LV_ANIM_OFF);
        lv_obj_t * label = lv_obj_get_child(tab_temp, 8);
        if (label) lv_label_set_text_fmt(label, "%d.0 s", temp_read_interval_ms/1000);
    }

    // Atjauninām damper cilnes vērtības (test22 kārtībā)
    if (tab_damper) {
        // kP slider
        update_slider_value(tab_damper, 1, kP, "%d");
        
        // tauD slider
        update_slider_value(tab_damper, 4, (int)tauD, "%d");
        
        // endTrigger slider
        update_slider_value(tab_damper, 7, (int)endTrigger, "%d");
        
        // LOW_TEMP_TIMEOUT slider
        lv_obj_t * low_temp_timeout_slider = lv_obj_get_child(tab_damper, 10); // LOW_TEMP_TIMEOUT slider
        if (low_temp_timeout_slider) {
            // Pārbaudam vai LOW_TEMP_TIMEOUT ir pieņemamajā diapazonā
            if (LOW_TEMP_TIMEOUT < 60000) LOW_TEMP_TIMEOUT = 60000; // Min 1 minūte
            if (LOW_TEMP_TIMEOUT > 600000) LOW_TEMP_TIMEOUT = 600000; // Max 10 minūtes
            
            // Noapaļojam uz tuvāko minūti (60000ms)
            LOW_TEMP_TIMEOUT = ((LOW_TEMP_TIMEOUT + 30000) / 60000) * 60000;
            
            lv_slider_set_value(low_temp_timeout_slider, (int)LOW_TEMP_TIMEOUT, LV_ANIM_OFF);
            
            lv_obj_t * low_temp_timeout_value = lv_obj_get_child(tab_damper, 11); // LOW_TEMP_TIMEOUT value label
            if (low_temp_timeout_value) {
                lv_label_set_text_fmt(low_temp_timeout_value, "%d min", (int)(LOW_TEMP_TIMEOUT/60000));
            }
        }
    }

    if (tab_servo) {
        // Atjauninām servo cilnes vērtības
        update_slider_value(tab_servo, 1, servoAngle, "%d°");
        update_slider_value(tab_servo, 4, servoOffset, "%d°");
        update_slider_value(tab_servo, 7, servoStepInterval, "%d ms");
    }

    if (tab_display) {
        // Slider satur ms; vērtību labelā rādām sekundēs (kā test22)
        lv_obj_t * s = lv_obj_get_child(tab_display, 1);
        if (s) lv_slider_set_value(s, (int)timeUpdateIntervalMs, LV_ANIM_OFF);
        lv_obj_t * v = lv_obj_get_child(tab_display, 2);
        if (v) lv_label_set_text_fmt(v, "%d s", (int)(timeUpdateIntervalMs/1000));
    }

    if (tab_alarm) {
        // Nodrošinām test22 diapazonu 80..95
        if (warning_temperature < 80) warning_temperature = 80;
        if (warning_temperature > 95) warning_temperature = 95;
        // sinhronizējam spoguli
        displayWarningTemperature = warning_temperature;
        update_slider_value(tab_alarm, 1, warning_temperature, "%d°C");
    }
}

void settings_screen_hide(void)
{
    if (settings_screen) {
        lv_obj_add_flag(settings_screen, LV_OBJ_FLAG_HIDDEN);
        is_visible = false;
        // sinhronizē galveno rolleri
        lv_display_update_target_temp();
    }
}

bool settings_screen_is_visible(void)
{
    return is_visible;
}
