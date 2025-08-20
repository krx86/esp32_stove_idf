#include <lvgl.h>
#include "display.h"
#include "esp_bsp.h"
#include "lv_port.h"
#include "lv_display.h"
#include <esp_log.h>
#include <esp_chip_info.h>
#include "temperature.h"      // Pievienojam temperatūras sensora atbalstu
#include "display_manager.h"  // Pievienojam display manager

#include "damper_control.h"   // Pievienojam damper kontroli
#include "../libraries/wifi/wifi.h"  // JAUNS: WiFi bibliotēka
#include "../libraries/telegram_bot/telegram_bot.h"  // JAUNS: Telegram bots



static const char *TAG = "DEMO_LVGL";

extern "C" void app_main(void)
{
    // JAUNS: Iespējojam visus logging līmeņus, lai redzētu damper ziņojumus
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("DAMPER", ESP_LOG_INFO);
    esp_log_level_set("DAMPER_CONTROL", ESP_LOG_INFO);
    esp_log_level_set("TEMPERATURE", ESP_LOG_INFO);
    esp_log_level_set("DISPLAY_MANAGER", ESP_LOG_INFO);
    
    // Sistēmas info
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "This is %s chip with %d CPU core(s), WiFi%s%s, ",
             CONFIG_IDF_TARGET,
             chip_info.cores,
             (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
             (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    // ====== 1. SĀKAM AR WiFi INICIALIZĀCIJU ======
    ESP_LOGI(TAG, "1. Inicializējam WiFi savienojumu...");
    wifi_connect();
    
    // Gaidām WiFi savienojumu (neblokējošii)
    vTaskDelay(pdMS_TO_TICKS(3000)); // 3 sekundes gaidīšana
    ESP_LOGI(TAG, "WiFi status: %s", is_wifi_connected() ? "Connected" : "Connecting...");
    
    // JAUNS: Start Telegram bot after WiFi
    telegram_bot_start();
    
    // ====== 2. TEMPERATŪRAS SENSORA INICIALIZĀCIJA ======
    ESP_LOGI(TAG, "2. Inicializējam temperatūras sensoru...");
    init_temperature_sensor();
    
    ESP_LOGI("TEMPERATURE", "Temperature sensor initialized! Current temp: %d°C", temperature);
    
    // Startējam temperatūras task
    start_temperature_task();
    
    // ====== 3. LVGL DISPLAY INICIALIZĀCIJA (PIRMS DAMPER!) ======
    ESP_LOGI(TAG, "3. Inicializējam LVGL Display...");
    lv_display_init_hardware();
    lv_display_configure_bsp();
    lv_display_init_objects();
    lv_display_register_touch_events();
    
    bsp_display_unlock();
    
    // LVGL task izveide
    lv_display_setup_task();
    
    // ====== 4. DISPLAY MANAGER INICIALIZĀCIJA ======
    ESP_LOGI(TAG, "4. Inicializējam Display Manager...");
    display_manager_init();
    
    // Tikai uz izmaiņām temperatūrai; laikam – ik 30s, bet UI mainās tikai, ja teksts atšķiras
    display_manager_set_update_intervals(
        0,       // Temperatūra: tikai event-driven (nav fallback)
        30000    // Laiks: 30 sekundes (koalēts, bez rindas)
    );
    
    // Paziņojam Display Manager par WiFi stāvokli
    display_manager_set_wifi_status(is_wifi_connected());
    
    // ====== 5. DAMPER KONTROLES INICIALIZĀCIJA (PĒC DISPLAY!) ======
    ESP_LOGI(TAG, "5. Inicializējam Damper Control...");
    damperControlInit();
    
    ESP_LOGI("DAMPER", "Damper control initialized! Manual mode: %s", 
             is_manual_damper_mode() ? "true" : "false");

    // Tagad drošs izsaukt damperControlLoop(), jo LVGL un display manager ir gatavi
    ESP_LOGI("DAMPER", "Running initial damperControlLoop()...");
    damperControlLoop();
    ESP_LOGI("DAMPER", "Initial damper calculation complete. Damper: %d%%, Mode: %s", 
             damper, messageDamp.c_str());
    
    // Startējam damper kontroles task
    startDamperControlTask();

    // ====== 6. OTA INICIALIZĀCIJA ======
    ESP_LOGI(TAG, "6. Inicializējam OTA...");
    ota_setup();
    
    // ====== 8. GALVENAIS LOOP ======
    while (1) {
        // Display Manager vairs nav jāsauc – LVGL taimeris pats apstrādā karogus
        
        // OTA apkalpošana
        ota_loop();
        
        // Īsa pauze
        vTaskDelay(pdMS_TO_TICKS(50)); // 50ms loop
    }
}