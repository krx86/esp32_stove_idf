#include "display_manager.h"
#include "lv_display.h"
#include "temperature.h"
#include "../wifi/wifi.h"
#include <atomic>

static const char *TAG = "DISPLAY_MANAGER";

// Bitu maskas vienkāršam pieprasījumu koalējumam
enum DM_Bits : uint32_t {
    DM_TEMP_CUR      = 1u << 0,  // mērītā temperatūra
    DM_TIME          = 1u << 1,
    DM_DAMPER        = 1u << 2,
    DM_DAMPER_STATUS = 1u << 3,
    DM_WARN_SHOW     = 1u << 4,
    DM_WARN_HIDE     = 1u << 5,
    DM_TEMP_TARGET   = 1u << 6,  // mērķa temperatūra (roller)
    DM_ALL           = 0xFFFFFFFFu
};

static lv_timer_t* dm_timer = nullptr;
static std::atomic<uint32_t> req_mask{0};
static std::atomic<bool> wifi_offline{false};

// Tikai laika periodam (pārējais – tikai uz izmaiņām)
static uint32_t time_interval_ms = DEFAULT_TIME_UPDATE_INTERVAL; // no header
static uint32_t last_time_tick = 0;

// Warning teksti (pointeri, bez kopēšanas)
static const char* pending_warning_title = nullptr;
static const char* pending_warning_message = nullptr;

static inline void request(uint32_t bits) {
    req_mask.fetch_or(bits, std::memory_order_relaxed);
    if (dm_timer) lv_timer_ready(dm_timer); // pamodina, neko nekrāj rindā
}

static void dm_timer_cb(lv_timer_t*) {
    uint32_t now = lv_tick_get();
    const bool time_due = (time_interval_ms > 0) && (lv_tick_elaps(last_time_tick) >= time_interval_ms);

    // Nolasa un notīra visus pieprasījumus vienā reizē; pievieno laiku, ja termiņš iztecējis
    uint32_t req = req_mask.exchange(0, std::memory_order_acq_rel) | (time_due ? DM_TIME : 0);
    if (!req) return;

    // 1) Mērītā temperatūra: atjauno gan tekstu, gan joslu
    if (req & DM_TEMP_CUR) {
        lv_display_update_temperature(temperature);
    }

    // 2) Mērķa temperatūra (roller) + joslas diapazons
    if (req & DM_TEMP_TARGET) {
        // Atjaunojam UI mērķi (roller) uz programmā esošo
        lv_display_update_target_temp();
        // Pārzīmējam joslu ar jaunu diapazonu pret aktuālo mērījumu
        lv_display_update_temperature(temperature);
        ESP_LOGI(TAG, "TARGET update: UI=%dC, SW=%dC", displayTargetTempC, target_temp_c);
    }

    // 3) Damper procenti
    if (req & DM_DAMPER) {
        lv_display_update_damper();
    }

    // 4) Damper statuss
    if (req & DM_DAMPER_STATUS) {
        lv_display_update_damper_status();
    }

    // 5) Brīdinājumi
    if (req & DM_WARN_SHOW) {
        if (pending_warning_title && pending_warning_message) {
            lv_display_show_warning(pending_warning_title, pending_warning_message);
            pending_warning_title = nullptr;
            pending_warning_message = nullptr;
        }
    }
    if (req & DM_WARN_HIDE) {
        lv_display_hide_warning();
    }

    // 6) Laiks
    if (req & DM_TIME) {
        if (wifi_offline.load(std::memory_order_relaxed) && !is_wifi_connected()) {
            lv_display_set_time("--:--");
        } else {
            wifi_offline.store(false, std::memory_order_relaxed);
            // show_time_on_display pati neko nepārzīmē, ja teksts nav mainījies
            show_time_on_display();
        }
        last_time_tick = now;
    }
}

// Publiskais API – saderīgs ar header
void display_manager_init() {
    if (!dm_timer) {
        dm_timer = lv_timer_create(dm_timer_cb, 20, nullptr); // ~50 Hz koalēšana
    }
    last_time_tick = lv_tick_get();
    ESP_LOGI(TAG, "Display Manager ready (time interval=%ums)", (unsigned)time_interval_ms);
}

// Saderībai – vairs neko nedara (taimeris apstrādā UI atjauninājumus)
void display_manager_update() {}

void display_manager_force_update_all()                    { request(DM_ALL); }
void display_manager_force_update_temperature()            { request(DM_TEMP_CUR); }
void display_manager_force_update_time()                   { request(DM_TIME); }
void display_manager_force_update_damper()                 { request(DM_DAMPER); }
void display_manager_force_update_damper_status()          { request(DM_DAMPER_STATUS); }

void display_manager_set_update_intervals(uint16_t /*temp_interval_ms*/, uint16_t time_interval_ms_param) {
    time_interval_ms = time_interval_ms_param;
    ESP_LOGI(TAG, "Intervals set: time=%ums (temp only-on-change)", (unsigned)time_interval_ms);
}

void display_manager_notify_temperature_changed()          { request(DM_TEMP_CUR); }
void display_manager_notify_target_temp_changed()          { request(DM_TEMP_TARGET); }
void display_manager_notify_time_synced()                  { request(DM_TIME); }
void display_manager_notify_damper_changed()               { request(DM_DAMPER_STATUS); }
void display_manager_notify_damper_position_changed()      { request(DM_DAMPER); }

bool display_manager_try_update()                          { if (dm_timer) lv_timer_ready(dm_timer); return true; }

void display_manager_set_wifi_status(bool connected) {
    static bool time_synced_once = false;
    if (connected) {
        if (!time_synced_once) { sync_time(); time_synced_once = true; }
        wifi_offline.store(false, std::memory_order_relaxed);
        request(DM_TIME);
    } else {
        wifi_offline.store(true, std::memory_order_relaxed);
        request(DM_TIME);
    }
}

void display_manager_update_time_from_wifi()               { request(DM_TIME); }

void display_manager_show_warning(const char* title, const char* message) {
    pending_warning_title = title;
    pending_warning_message = message;
    request(DM_WARN_SHOW);
}

void display_manager_hide_warning()                        { request(DM_WARN_HIDE); }
void display_manager_force_update_warning()                { request(DM_WARN_SHOW); }

// Deprecated: atstāts saderībai. Nelietot jauniem izsaukumiem.
bool safe_lvgl_operation(std::function<void(void)> update_func) {
    (void)update_func;
    if (dm_timer) lv_timer_ready(dm_timer);
    return true;
}