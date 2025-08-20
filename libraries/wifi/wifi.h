#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// WiFi savienošanas funkcijass
void wifi_connect();

// Laika sinhronizācijas funkcijas
void sync_time();
const char* get_time_str();

// OTA funkcijas
void ota_setup();
void ota_loop();

// WiFi statusa pārbaude
bool is_wifi_connected();

#ifdef __cplusplus
}
#endif
