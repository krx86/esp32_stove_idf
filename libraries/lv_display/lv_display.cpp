#include "lv_display.h"
#include "temperature.h"      // Pievienojam īstā temperatūras sensora atbalstu
#include "display_manager.h"  // Pievienojam display manager atbalstu
#include "settings_screen.h"  // JAUNS: settings screen (VVC minimal)
#include "../damper_control/damper_control.h"   // Pievienojam damper kontroli ar relatīvo ceļu
#include "../wifi/wifi.h"     // JAUNS: WiFi bibliotēka laika funkcijām
#include <string>
#include <esp_sleep.h>        // JAUNS: Deep sleep atbalsts
#include <driver/rtc_io.h>    // JAUNS: RTC GPIO atbalsts

// Forward declarations for temperature integration
extern int temperature;
extern int target_temp_c;

// Forward declarations for damper integration
extern int damper;
extern std::string messageDamp;

// JAUNS: Manual mode mainīgie - no test22
static bool manual_mode = false;
static int saved_damper = 0; // Lai saglabātu vērtību pirms manuālā režīmam

static const char *TAG = "LV_DISPLAY";

LV_FONT_DECLARE(ekstra);
LV_FONT_DECLARE(ekstra1);
LV_FONT_DECLARE(eeet);

// UI objekti no test22 (ar rolleriem)
lv_obj_t *blue_bar = NULL;
lv_obj_t *touch_point = NULL;
lv_obj_t *temp_label = NULL;
lv_obj_t *temp0 = NULL;
lv_obj_t *damper_label = NULL;
lv_obj_t *target = NULL;
lv_obj_t *damper_status_label = NULL;
lv_obj_t *time_label = NULL;

// JAUNS: Main screen roller globālais objekts
lv_obj_t *main_target_temp_roller = NULL;
// JAUNS: Damper roller globālais objekts  
lv_obj_t *damper_roller = NULL;

// Global temperature variables
int displayTemperature = 25;
int displayTargetTempC = 70;
int displayWarningTemperature = 80;

// JAUNS: Temperature mapping arrays (main screen) - no test22
static const int main_temp_values[] = {64, 66, 68, 70, 72, 74, 76, 78, 80};
static const int main_temp_count = 9;

// JAUNS: Damper mapping arrays - no test22
static const int damper_values[] = {100, 80, 60, 40, 20, 0};
static const int damper_count = 6;

// JAUNS: Atrod damper indeksu array - no test22
int get_damper_index(int damper_value)
{
    for (int i = 0; i < damper_count; i++) {
        if (damper_values[i] == damper_value) {
            return i;
        }
    }
    
    // Ja nav atrasts, atgriez tuvāko
    if (damper_value >= 100) return 0;
    if (damper_value <= 0) return damper_count - 1;
    
    // Atrod tuvāko
    for (int i = 0; i < damper_count - 1; i++) {
        if (damper_value <= damper_values[i] && damper_value > damper_values[i + 1]) {
            return i;
        }
    }
    return 0; // Default 100%
}

// JAUNS: Atrod temperature indeksu array (main screen) - no test22
int main_get_temp_index(int temperature)
{
    for (int i = 0; i < main_temp_count; i++) {
        if (main_temp_values[i] == temperature) {
            return i;
        }
    }
    // Ja nav atrasts, atgriez tuvāko
    if (temperature < 64) return 0;
    if (temperature > 80) return 8;
    
    // Atrod tuvāko
    for (int i = 0; i < main_temp_count - 1; i++) {
        if (temperature >= main_temp_values[i] && temperature < main_temp_values[i + 1]) {
            return i;
        }
    }
    return 4; // Default 70°C
}

// JAUNS: Setup invisible roller style funkcija - no test22
void setup_invisible_roller_style(lv_obj_t* roller)
{
    if (!roller) return;
    
    // Galvenā daļa - transparent background, bez border
    lv_obj_set_style_bg_opa(roller, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(roller, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(roller, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(roller, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(roller, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(roller, 0, LV_PART_MAIN);
    
    // Selected teksts
    lv_obj_set_style_text_font(roller, &lv_font_montserrat_28, LV_PART_SELECTED);
    lv_obj_set_style_text_color(roller, lv_color_hex(0x7997a3), LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(roller, LV_OPA_TRANSP, LV_PART_SELECTED);
    lv_obj_set_style_border_width(roller, 0, LV_PART_SELECTED);
    lv_obj_set_style_outline_width(roller, 0, LV_PART_SELECTED);
}

// JAUNS: Damper roller event handler - optimizēts no test22
void damper_roller_event_handler(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    
    lv_obj_t * roller = lv_event_get_target(e);
    if (roller != damper_roller) {
        return;
    }
    
    uint8_t selected = lv_roller_get_selected(roller);
    
    // Validē indeksu
    if (selected >= damper_count) {
        return;
    }
    
    // Uzstāda jauno vērtību
    damper = damper_values[selected];
    
    // Atjauno damper vērtību displejā - tieši
    static char buf[16];
    snprintf(buf, sizeof(buf), "%d %%", damper);
    lv_label_set_text(damper_label, buf);
}

// JAUNS: Main screen roller event handler - optimizēts no test22
void main_temp_roller_event_handler(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    
    lv_obj_t * roller = lv_event_get_target(e);
    if (roller != main_target_temp_roller) {
        return;
    }

    uint8_t selected = lv_roller_get_selected(roller);

    // Validē indeksu
    if (selected >= main_temp_count) {
        return;
    }
    
    // Uzstāda jauno mērķi caur temperature API (ģenerēs DM_TEMP_TARGET notikumu)
    set_target_temperature(main_temp_values[selected]);
}

// JAUNS: VVC versija ar manual mode - uzlabota no test22
void toggle_damper_mode(void)
{
    // Pārslēgšanās starp auto un manuālo režīmu - prioritizēta pareizi
    manual_mode = !manual_mode;
    
    if (manual_mode) {
        saved_damper = damper;  // Saglabājam pašreizējo vērtību
        
        // Uzstādam tekstu uz "MANUAL" - prioritārā darbība
        lv_label_set_text(damper_status_label, "MANUAL");
        
        // Teksta maiņa nekavējas, tāpēc mainām tieši
        lv_label_set_text(target, "Set Damper");
        
        // Paslēpjam temperatūras rolleri un parādam damper rolleri
        if (main_target_temp_roller) {
            lv_obj_add_flag(main_target_temp_roller, LV_OBJ_FLAG_HIDDEN);
        }
        if (damper_roller) {
            lv_obj_clear_flag(damper_roller, LV_OBJ_FLAG_HIDDEN);
            
            // Iestatām damper roller pozīciju uzreiz
            int damper_index = get_damper_index(damper);
            lv_roller_set_selected(damper_roller, damper_index, LV_ANIM_OFF);
        }
    } else {
        // Teksta maiņa nekavējas, tāpēc mainām tieši
        lv_label_set_text(target, "MAX temp.");
        
        // Paslēpjam damper rolleri un parādam temperatūras rolleri
        if (damper_roller) {
            lv_obj_add_flag(damper_roller, LV_OBJ_FLAG_HIDDEN);
        }
        if (main_target_temp_roller) {
            lv_obj_clear_flag(main_target_temp_roller, LV_OBJ_FLAG_HIDDEN);
        }
        
        // Atjaunojam iepriekšējo damper vērtību
        damper = saved_damper;
        
        // SVARĪGI: Atjaunojam damper vērtību un statusu ATSEVIŠĶI
        static char buf[16];
        snprintf(buf, sizeof(buf), "%d %%", damper);
        lv_label_set_text(damper_label, buf);
        
        // SVARĪGI: Tagad atjaunojam status ar AUTO režīmu
        lv_label_set_text(damper_status_label, "AUTO");
        
        // Forsa redraw
        lv_obj_invalidate(damper_status_label);
    }
    
    // Forsa atjaunināšanu
    lv_refr_now(NULL);
    
    ESP_LOGI(TAG, "Damper mode toggled: %s", manual_mode ? "MANUAL" : "AUTO");
}

// JAUNS: Update main roller function - no test22
void lv_display_update_main_roller() {
    if (!main_target_temp_roller) {
        return;
    }
    
    int current_index = main_get_temp_index(target_temp_c);
    
    // Temporarily remove event callback to prevent triggering
    lv_obj_remove_event_cb(main_target_temp_roller, main_temp_roller_event_handler);
    
    // Update roller
    lv_roller_set_selected(main_target_temp_roller, current_index, LV_ANIM_OFF);
    
    // Re-add event callback
    lv_obj_add_event_cb(main_target_temp_roller, main_temp_roller_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
}

// JAUNS: Brīdinājuma popup mainīgie - VVC versija (bez buzzer)
static lv_obj_t *warning_popup = NULL;
static bool warning_popup_visible = false;

// JAUNS: Funkcija brīdinājuma popup parādīšanai - VVC versija (bez buzzer)
void lv_display_show_warning(const char* title, const char* message) {
    // Aizveram iepriekšējo popup, ja tāds ir
    if (warning_popup) {
        lv_obj_del(warning_popup);
        warning_popup = NULL;
        warning_popup_visible = false;
    }
    
    // Izveidojam jaunu popup
    warning_popup = lv_obj_create(lv_scr_act());
    lv_obj_set_size(warning_popup, 300, 160); // Mazāks, jo nav mute pogas
    lv_obj_center(warning_popup);
    lv_obj_set_style_bg_color(warning_popup, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(warning_popup, 15, 0);
    lv_obj_set_style_shadow_width(warning_popup, 20, 0);
    lv_obj_set_style_shadow_color(warning_popup, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(warning_popup, LV_OPA_30, 0);
    
    // Virsraksts ar lielāku brīdinājuma ikonu
    lv_obj_t * title_label = lv_label_create(warning_popup);
    lv_label_set_text_fmt(title_label, LV_SYMBOL_WARNING " %s", title);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xFF0000), 0); // Spilgti sarkans
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 15);
    
    // Brīdinājuma ziņojums - lielāks un treknāks
    lv_obj_t * message_label = lv_label_create(warning_popup);
    lv_label_set_text(message_label, message);
    lv_obj_set_style_text_font(message_label, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(message_label, lv_color_hex(0x000000), 0);
    // Iestatām maksimālo platumu, lai teksts automātiski ietītos
    lv_obj_set_width(message_label, 260);
    lv_label_set_long_mode(message_label, LV_LABEL_LONG_WRAP);
    // Centrējam ziņojumu
    lv_obj_align(message_label, LV_ALIGN_CENTER, 0, 0);
    
    // Pievienojam automātiskas izslēgšanās norādi
    lv_obj_t * auto_close_label = lv_label_create(warning_popup);
    lv_label_set_text(auto_close_label, "bridinajums");
    lv_obj_set_style_text_font(auto_close_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(auto_close_label, lv_color_hex(0x888888), 0);
    lv_obj_align(auto_close_label, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    warning_popup_visible = true;
}

// JAUNS: Funkcija brīdinājuma aizvēršanai - VVC versija
void lv_display_hide_warning() {
    if (warning_popup) {
        lv_obj_del(warning_popup);
        warning_popup = NULL;
        warning_popup_visible = false;
        ESP_LOGI(TAG, "Warning popup closed");
    }
}


// JAUNS: Deep sleep funkcija ar touch interrupt wake-up
void enter_deep_sleep_with_touch_wakeup() {
    ESP_LOGI(TAG, "Preparing for deep sleep with touch interrupt wake-up on GPIO %d", EXAMPLE_PIN_NUM_QSPI_TOUCH_INT);
    
    // Konfigurējam touch interrupt pin kā external wake-up source
    // Izmantojam BSP definēto touch interrupt pin
    esp_sleep_enable_ext0_wakeup((gpio_num_t)EXAMPLE_PIN_NUM_QSPI_TOUCH_INT, 0); // Pamodas uz LOW signālu (touch nospiešana)
    
    // Alternatīvi var izmantot ext1 wake-up, ja nepieciešams vairāki pin:
    // const uint64_t ext_wakeup_pin_mask = 1ULL << EXAMPLE_PIN_NUM_QSPI_TOUCH_INT;
    // esp_sleep_enable_ext1_wakeup(ext_wakeup_pin_mask, ESP_EXT1_WAKEUP_ANY_HIGH);
    
    // Konfigurējam RTC GPIO, lai saglabātu stāvokli deep sleep laikā
    rtc_gpio_pullup_en((gpio_num_t)EXAMPLE_PIN_NUM_QSPI_TOUCH_INT);   // Pull-up resistor
    rtc_gpio_pulldown_dis((gpio_num_t)EXAMPLE_PIN_NUM_QSPI_TOUCH_INT); // Atslēdz pull-down
    
    ESP_LOGI(TAG, "GPIO %d configured as touch interrupt wake-up source", EXAMPLE_PIN_NUM_QSPI_TOUCH_INT);
    ESP_LOGI(TAG, "Entering deep sleep mode...");
    
    // Neliela pauze, lai log ziņojumi tiktu nosūtīti
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Ieslēdzam deep sleep
    esp_deep_sleep_start();
}

// JAUNS: Manual mode stāvokļa pārbaude - no test22
bool is_manual_damper_mode() {
    return manual_mode;
}

// JAUNS: Manual mode stāvokļa uzstādīšana - papildfunkcija
void set_manual_damper_mode(bool enabled) {
    if (manual_mode != enabled) {
        toggle_damper_mode();
    }
}

// Touch event handler funkcija - ATSLĒGTS
void touch_event_cb(lv_event_t *e) {
    // PIEZĪME: Touch event handler ir atslēgts - touch punkts vairs netiek rādīts
    // Ja nepieciešams atkal ieslēgt, noņemiet šo komentāru un aktivizējiet kodu zemāk:
    /*
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    lv_obj_set_pos(touch_point, p.x - 10, p.y - 10);
    lv_obj_clear_flag(touch_point, LV_OBJ_FLAG_HIDDEN);
    */
}

// Touch release event handler funkcija - ATSLĒGTS
void touch_release_cb(lv_event_t *e) {
    // PIEZĪME: Touch release handler ir atslēgts
    // Ja nepieciešams atkal ieslēgt, noņemiet šo komentāru un aktivizējiet kodu zemāk:
    /*
    lv_obj_add_flag(touch_point, LV_OBJ_FLAG_HIDDEN);
    */
}

// JAUNS: Touch update funkcija no test22 - ATSLĒGTS
void lv_display_touch_update() {
    // PIEZĪME: Touch punktu atjaunināšana ir atslēgta
    // VVC projektā touch punkti vairs netiek rādīti
    // Ja nepieciešams atkal ieslēgt, noņemiet komentārus zemāk:
    /*
    static bool prev_visible = false;
    bool currently_visible = !lv_obj_has_flag(touch_point, LV_OBJ_FLAG_HIDDEN);
    
    if (prev_visible != currently_visible) {
        prev_visible = currently_visible;
        if (currently_visible) {
            ESP_LOGD(TAG, "Touch point shown");
        } else {
            ESP_LOGD(TAG, "Touch point hidden");
        }
    }
    */
}

// Galvenā UI izveides funkcija (adaptēta no test22 ar rolleriem)
void create_main_ui() {
    // Ekrāna fona krāsa
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0xF3F4F3), 0);

    // Laika etiķete
    time_label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_26, 0);
    lv_obj_set_style_text_color(time_label, lv_color_hex(0x7997a3), 0);
    lv_obj_set_pos(time_label, 180, 25);
    lv_label_set_text(time_label, LV_SYMBOL_WIFI " 00:00");

    // Galvenā temperatūras josla
    blue_bar = lv_bar_create(lv_scr_act());
    lv_obj_set_size(blue_bar, 110, 350);
    lv_obj_set_pos(blue_bar, 40, 40);
    lv_obj_set_style_radius(blue_bar, 55, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(blue_bar, true, LV_PART_MAIN);
    lv_obj_set_style_bg_color(blue_bar, lv_color_hex(0x7DD0F2), LV_PART_MAIN);
    lv_obj_set_style_bg_color(blue_bar, lv_color_hex(0xff9090), LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_dir(blue_bar, LV_GRAD_DIR_VER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_color(blue_bar, lv_color_hex(0x7DD0F2), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(blue_bar, 225, LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_stop(blue_bar, 205, LV_PART_INDICATOR);
    lv_obj_set_style_bg_main_stop(blue_bar, 101, LV_PART_INDICATOR);
    lv_obj_set_style_radius(blue_bar, 0, LV_PART_INDICATOR);

    // Aplis apakšējā daļā
    lv_obj_t * circle = lv_obj_create(lv_scr_act());
    lv_obj_set_size(circle, 150, 150);
    lv_obj_set_pos(circle, 20, 520 - 200);
    lv_obj_set_style_radius(circle, 75, 0);
    lv_obj_set_style_bg_color(circle, lv_color_hex(0x7DD0F2), 0);
    lv_obj_set_style_border_width(circle, 0, 0);
    // B) Aplim arī atslēdzam hit-testu un klikus
    lv_obj_clear_flag(circle, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(circle, [](lv_event_t * e){
        if(lv_event_get_code(e) == LV_EVENT_HIT_TEST) {
            lv_hit_test_info_t* info = (lv_hit_test_info_t*)lv_event_get_param(e);
            info->res = false;
        }
    }, LV_EVENT_HIT_TEST, NULL);

    // Touch punkts (sarkanais) - ATSLĒGTS
    // PIEZĪME: Touch punkts tiek izveidots, bet vienmēr paliek paslēpts
    touch_point = lv_obj_create(lv_scr_act());
    lv_obj_set_size(touch_point, 20, 20);
    lv_obj_set_style_radius(touch_point, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(touch_point, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_bg_opa(touch_point, LV_OPA_COVER, 0);
    lv_obj_clear_flag(touch_point, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(touch_point, LV_OBJ_FLAG_HIDDEN);  // Vienmēr paslēpts

    // Temperatūras teksts
    temp_label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(temp_label, &ekstra, 0);
    lv_obj_set_style_text_color(temp_label, lv_color_hex(0xF3F4F3), 0);
    lv_obj_set_pos(temp_label, 45, 360);
    lv_label_set_text(temp_label, "25");

    // Grādu simbols
    temp0 = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(temp0, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(temp0, lv_color_hex(0xF3F4F3), 0);
    lv_obj_set_style_bg_color(temp0, lv_color_hex(0xF3F4F3), 0);
    lv_obj_set_pos(temp0, 140, 360);
    lv_label_set_text(temp0, "°C");

    // Target etiķete
    target = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(target, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(target, lv_color_hex(0x7997a3), 0);
    lv_obj_set_style_bg_color(target, lv_color_hex(0x7997a3), 0);
    lv_obj_set_pos(target, 170, 110);
    lv_label_set_text(target, "MAX temp.");

    // Damper procenti
    damper_label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(damper_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(damper_label, lv_color_hex(0x7997a3), 0);
    lv_obj_set_style_bg_color(damper_label, lv_color_hex(0x7997a3), 0);
    lv_obj_set_pos(damper_label, 200, 290);
    lv_label_set_text(damper_label, "100 %");

    // Settings poga (label_one) - S burts
    lv_obj_t *label_one = lv_label_create(lv_scr_act());
    lv_label_set_text(label_one, "2"); // No test22 - cipars 2
    lv_obj_set_style_text_font(label_one, &eeet, 0);
    lv_obj_set_style_text_color(label_one, lv_color_hex(0x7997a3), 0);
    lv_obj_set_style_bg_color(label_one, lv_color_hex(0x7997a3), 0);
    lv_obj_set_pos(label_one, 240, 390);

    // Padarām label_one klikojamu un saglabājam ātru reakciju (LV_EVENT_PRESSED)
    lv_obj_add_flag(label_one, LV_OBJ_FLAG_CLICKABLE);
    // Paplašinām trāpījuma zonu ap label ~25px (bez atsevišķas pogas)
    lv_obj_add_flag(label_one, LV_OBJ_FLAG_ADV_HITTEST);

    // Ātra atvēršana uz nospiešanu
    lv_obj_add_event_cb(label_one, [](lv_event_t * e) {
        if (lv_event_get_code(e) == LV_EVENT_PRESSED) {
            settings_screen_show();
        }
    }, LV_EVENT_PRESSED, NULL);

    // Custom hit-test: ļaujam trāpīt nedaudz garām pašam tekstam
    lv_obj_add_event_cb(label_one, [](lv_event_t * e){
        if(lv_event_get_code(e) == LV_EVENT_HIT_TEST) {
            lv_hit_test_info_t* info = (lv_hit_test_info_t*)lv_event_get_param(e);
            lv_area_t a; lv_obj_get_coords(lv_event_get_target(e), &a);
            a.x1 -= 25; a.y1 -= 25; a.x2 += 25; a.y2 += 25; // paplašināta zona
            const lv_point_t p = *info->point;
            bool in_rect = (p.x >= a.x1 && p.x <= a.x2 && p.y >= a.y1 && p.y <= a.y2);
            info->res = in_rect;
        }
    }, LV_EVENT_HIT_TEST, NULL);

    // Damper statuss
    damper_status_label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(damper_status_label, &ekstra1, 0);
    lv_obj_set_style_text_color(damper_status_label, lv_color_hex(0x7997a3), 0);
    lv_obj_set_style_bg_color(damper_status_label, lv_color_hex(0x7997a3), 0);
    lv_obj_set_pos(damper_status_label, 180, 230);
    lv_label_set_text(damper_status_label, "--");

    // A) Lielāka un precīzāka klikzona damper statusam: caurspīdīga poga virsū
    // Noņemam clickable no pašas label, eventus liksim uz pogas
    lv_obj_clear_flag(damper_status_label, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *damper_btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(damper_btn, 120, 50);              // lielāks hit-area
    lv_obj_set_pos(damper_btn, 170, 220);              // aptver label apgabalu
    lv_obj_set_style_bg_opa(damper_btn, LV_OPA_TRANSP, LV_PART_MAIN);
    // No pressed-state styling needed; keep fully transparent
    lv_obj_set_style_border_width(damper_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_border_opa(damper_btn, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_outline_width(damper_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(damper_btn, 0, LV_PART_MAIN);
    // (pēc vajadzības) noapaļojumu arī uz 0:
    lv_obj_set_style_radius(damper_btn, 0, LV_PART_MAIN);

    // Pārceļam label uz pogas bērniem un centrējam
    lv_obj_set_parent(damper_status_label, damper_btn);
    lv_obj_center(damper_status_label);

    // Event uz pogas (ātrs LV_EVENT_PRESSED)
    lv_obj_add_event_cb(damper_btn, [](lv_event_t * e) {
        if (lv_event_get_code(e) == LV_EVENT_PRESSED) {
            toggle_damper_mode();
        }
    }, LV_EVENT_PRESSED, NULL);

    // JAUNS: Main screen temperature roller - aizstāj target_temp_display
    main_target_temp_roller = lv_roller_create(lv_scr_act());
    if (main_target_temp_roller) {
        // Set roller options
        lv_roller_set_options(main_target_temp_roller, 
            "64°C\n66°C\n68°C\n70°C\n72°C\n74°C\n76°C\n78°C\n80°C", 
            LV_ROLLER_MODE_NORMAL);
        
        // Pozīcija
        lv_obj_set_pos(main_target_temp_roller, 210, 140);
        lv_obj_set_size(main_target_temp_roller, 100, 35);
        lv_roller_set_visible_row_count(main_target_temp_roller, 1);
        
        // Stils - invisible
        setup_invisible_roller_style(main_target_temp_roller);
        
        // Sākotnēji sinhronizē ar programmas mērķi (nevis displayTargetTempC)
        int current_index = main_get_temp_index(target_temp_c);
        lv_roller_set_selected(main_target_temp_roller, current_index, LV_ANIM_OFF);
        
        // Add event callback
        lv_obj_add_event_cb(main_target_temp_roller, main_temp_roller_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
        
        ESP_LOGI("LV_DISPLAY", "Init target UI=%dC SW=%dC", displayTargetTempC, target_temp_c);
    }
    
    // JAUNS: Damper roller (sākotnēji paslēpts) - no test22
    damper_roller = lv_roller_create(lv_scr_act());
    if (damper_roller) {
        // Set roller options
        lv_roller_set_options(damper_roller, 
            "100%\n80%\n60%\n40%\n20%\n0%", 
            LV_ROLLER_MODE_NORMAL);
        
        // Tāda pati pozīcija kā main_target_temp_roller
        lv_obj_set_pos(damper_roller, 210, 140);
        lv_obj_set_size(damper_roller, 100, 35);
        lv_roller_set_visible_row_count(damper_roller, 1);
        
        // Paslēpjam sākotnēji
        lv_obj_add_flag(damper_roller, LV_OBJ_FLAG_HIDDEN);
        
        // Stils - invisible  
        setup_invisible_roller_style(damper_roller);
        
        // Set initial value to 100%
        lv_roller_set_selected(damper_roller, 0, LV_ANIM_OFF);
        
        // Add event callback
        lv_obj_add_event_cb(damper_roller, damper_roller_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    } 

    // Līnija (adaptēta no test22)
    static lv_point_t v_line_points[] = {
       {0, 0},
       {110, 0},
       {
           110 + (int)(80 * cosf(M_PI - M_PI * 115.0f / 180.0f)),
           (int)(80 * sinf(M_PI - M_PI * 115.0f / 180.0f))
       },
       {
           110 + (int)(80 * cosf(M_PI - M_PI * 115.0f / 180.0f)) + 100,
           (int)(80 * sinf(M_PI - M_PI * 115.0f / 180.0f))
       }
    };
    lv_obj_t *v_dash_line = lv_line_create(lv_scr_act());
    lv_line_set_points(v_dash_line, v_line_points, 4);
    lv_obj_set_pos(v_dash_line, 40, 100);

    static lv_style_t style_v_dash;
    lv_style_init(&style_v_dash);
    lv_style_set_line_width(&style_v_dash, 2);
    lv_style_set_line_color(&style_v_dash, lv_color_hex(0x7997a3));
    lv_style_set_line_dash_gap(&style_v_dash, 7);
    lv_style_set_line_dash_width(&style_v_dash, 7);
    lv_obj_add_style(v_dash_line, &style_v_dash, LV_PART_MAIN);

    // B) Dekoratīvajai līnijai atslēdzam hit-testu un klikus
    lv_obj_clear_flag(v_dash_line, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(v_dash_line, [](lv_event_t * e){
        if(lv_event_get_code(e) == LV_EVENT_HIT_TEST) {
            lv_hit_test_info_t* info = (lv_hit_test_info_t*)lv_event_get_param(e);
            info->res = false; // nekad netrāpa
        }
    }, LV_EVENT_HIT_TEST, NULL);
}

// LVGL apkalpošanas task ar īsto temperatūras sensoru
void lvgl_task(void *pvParameter) {
    ESP_LOGI(TAG, "LVGL task started with temperature monitoring");
    
    
    
    while (1) {
        lv_timer_handler();
        
        // Atjauninām temperatūru no īstā sensora
        update_temperature();
        
        // NOVECOJIS: Šis kods ir atslēgts, jo tagad izmanto display_manager!
        // Ja temperatūra mainījusies, atjauninām displeju caur display_manager
        if (has_temperature_changed()) {
            // PAZIŅOJAM display_manager par izmaiņām, nevis atjauninam tieši!m
            display_manager_notify_temperature_changed();
            
                    }
        

        
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// UI objektu inicializācija
void lv_display_init_objects() {
    create_main_ui();
    ESP_LOGI(TAG, "LV Display objects initialized");
}

// Simulācijas sākšana
void lv_display_start_simulation() {
    ESP_LOGI(TAG, "Temperature simulation started");
}

// Task iestatīšana
void lv_display_setup_task() {
    xTaskCreate(lvgl_task, "lvgl_task", 16384, NULL, 5, NULL);
    ESP_LOGI(TAG, "LVGL task created");
}

// Temperatūras atjaunināšana
void lv_display_update_temperature(int temp) {
    displayTemperature = temp;
    if (temp_label) {
        static char buf[32];
        snprintf(buf, sizeof(buf), "%d", temp);
        lv_label_set_text(temp_label, buf);
    }
    
    // Šeit iekopēts saturs no lv_display_update_bars() funkcijas
    // JAUNS: Pārbaudam vai temperatūra ir kritiska - no test22
    static bool warning_shown = false;
    static bool high_temp_warning = false; // Vai brīdinājums ir par augstu temperatūru
    static bool low_temp_warning = false;  // Vai brīdinājums ir par zemu temperatūru
    
    if (temperature > displayWarningTemperature && !warning_shown) {
        // Ja temperatūra ir par augstu un brīdinājums vēl nav parādīts
        static char warning_message[40];
        snprintf(warning_message, sizeof(warning_message), "Temp. parsniedz %d!", displayWarningTemperature);
        display_manager_show_warning("Bridinajums!", warning_message);
        warning_shown = true;
        high_temp_warning = true;
        low_temp_warning = false;
    } else if (temperature <= 3 && !warning_shown) {
        // Sensora kļūda un brīdinājums vēl nav parādīts
        display_manager_show_warning("Bridinajums!", "Sensor error!");
        warning_shown = true;
        high_temp_warning = false;
        low_temp_warning = true;
    } else if ((high_temp_warning && temperature <= displayWarningTemperature) || 
                (low_temp_warning && temperature > 3)) {
        // Temperatūra ir normalizējusies - aizveram caur display_manager
        warning_shown = false;
        high_temp_warning = false;
        low_temp_warning = false;
        
        display_manager_hide_warning();
    }
    
    if (blue_bar) {
        lv_bar_set_range(blue_bar, 0, displayTargetTempC * 1.22);
        lv_bar_set_value(blue_bar, displayTemperature, LV_ANIM_OFF);
    }
}

// Touch punkta rādīšana
void lv_display_show_touch_point(uint16_t x, uint16_t y, bool show) {
    if (touch_point) {
        if (show) {
            lv_obj_set_pos(touch_point, x - 10, y - 10);
            lv_obj_clear_flag(touch_point, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(touch_point, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// Display hardware initialization and configuration
void lv_display_init_hardware() {
    // Displeja konfigurācija
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = EXAMPLE_LCD_QSPI_H_RES * EXAMPLE_LCD_QSPI_V_RES,
#if LVGL_PORT_ROTATION_DEGREE == 90
        .rotate = LV_DISP_ROT_90,
#elif LVGL_PORT_ROTATION_DEGREE == 270
        .rotate = LV_DISP_ROT_270,
#elif LVGL_PORT_ROTATION_DEGREE == 180
        .rotate = LV_DISP_ROT_180,
#elif LVGL_PORT_ROTATION_DEGREE == 0
        .rotate = LV_DISP_ROT_NONE,
#endif
    };

    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();
    ESP_LOGI(TAG, "Display hardware initialized");
}

// Configure BSP display
void lv_display_configure_bsp() {
    bsp_display_lock(0);
    ESP_LOGI(TAG, "BSP display configured and locked");
}

// Register touch event handlers
void lv_display_register_touch_events() {
    lv_obj_add_event_cb(lv_scr_act(), touch_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(lv_scr_act(), touch_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(lv_scr_act(), touch_release_cb, LV_EVENT_RELEASED, NULL);
    ESP_LOGI(TAG, "Touch event handlers registered");
}

// Update damper percentage display - TIEŠI kā test22
void lv_display_update_damper() {
    if (damper_label) {
        static char buf[16];
        snprintf(buf, sizeof(buf), "%d %%", damper);
        lv_label_set_text(damper_label, buf);
    }
}

// Update damper status display - LABOTS kā test22
void lv_display_update_damper_status() {
    if (damper_status_label) {
        // LABOTS: Pārbaudām manual mode kā test22
        if (!manual_mode) {
            // Tikai AUTO režīmā atjauninām no messageDamp
            lv_label_set_text(damper_status_label, messageDamp.c_str());
        }
        // Manuālajā režīmā nedarām neko - teksts paliek "MANUAL"
    }
}

// JAUNS: Update target temperature roller - no test22
void lv_display_update_target_temp() {
    lv_display_update_main_roller();
    // Izlīdzinām UI mainīgo ar programmas mērķi, lai logi rādītu korekti
    displayTargetTempC = target_temp_c;
}

// JAUNS: Laika rādīšanas funkcijas - no test22
void lv_display_set_time(const char* time_str) {
    // Šī funkcija tiek izsaukta no display_manager, kas jau darbojas LVGL kontekstā
    
    if (time_label) {
        static char buf[32];
        snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI " %s", time_str);
        lv_label_set_text(time_label, buf);
    }
}

void show_time_on_display() {
    // ATJAUNOTS: Tagad izmanto WiFi bibliotēku laika iegūšanai (ESP-IDF versija)
    static char last_time_displayed[8] = "";
    static uint32_t last_check = 0;
    if (esp_timer_get_time() / 1000000 - last_check < 10) {
        return;
    }
    last_check = esp_timer_get_time() / 1000000;
    
    // JAUNS: Izmanto WiFi bibliotēkas get_time_str() funkciju (ESP-IDF versija)
    const char* current_time = get_time_str();  // No wifi bibliotēkas
    
    if (strcmp(current_time, last_time_displayed) != 0) {
        strcpy(last_time_displayed, current_time);
        lv_display_set_time(current_time);
    }
}

void show_time_reset_cache() {
    // ATJAUNOTS: Tagad sinhronizē laiku no WiFi bibliotēkas (ESP-IDF versija)
    ESP_LOGI(TAG, "Resetting time cache and syncing from WiFi");
    
    // Iegūst jauno laiku no WiFi bibliotēkas (ESP-IDF versija)
    const char* current_time = get_time_str();
    lv_display_set_time(current_time);
    
    ESP_LOGI(TAG, "Time cache reset, current time: %s", current_time);
}

// JAUNS: Aizvērt iestatījumu ekrānu (lietots Back pogā)
void lv_display_close_settings() {
    settings_screen_hide();
}





