# WiFi bibliotēka (ESP-IDF)

Šī bibliotēka nodrošina WiFi savienojuma, laika sinhronizācijas un OTA (Over-The-Air) atjaunināšanas funkcionalitāti ESP-IDF frameworkam.

⚠️ **SVARĪGI**: Šī bibliotēka ir speciāli projektēta ESP-IDF frameworkam, nevis Arduino frameworkam!

## Funkcijas

### WiFi savienojums
- `wifi_connect()` - izveido savienojumu ar WiFi tīklu (ar event handleriem)
- `is_wifi_connected()` - pārbauda WiFi savienojuma statusu

### Laika sinhronizācija  
- `sync_time()` - sinhronizē laiku ar NTP serveri (izmanto SNTP)
- `get_time_str()` - atgriež pašreizējo laiku kā const char* formātā "HH:MM"

### OTA atjaunināšana
- `ota_setup()` - inicializē OTA funkcionalitāti (ESP-IDF native)
- `ota_loop()` - jāizsauc loop() funkcijā

## Lietošana (ESP-IDF)

```c
#include "wifi.h"

void app_main() {
    wifi_connect();
    
    // Gaida WiFi savienojumu
    while (!is_wifi_connected()) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    
    sync_time();
    ota_setup();
    
    while (1) {
        const char* current_time = get_time_str();
        printf("Current time: %s\n", current_time);
        
        ota_loop();
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
```

## Konfigurācija

WiFi iestatījumi ir definēti wifi.cpp failā:
- `WIFI_SSID`: WiFi tīkla nosaukums
- `WIFI_PASSWORD`: WiFi tīkla parole  
- `NTP_SERVER`: NTP servera adrese
- `GMT_OFFSET_SEC`: Laika zona (GMT+2 Latvijai)
- `DAYLIGHT_OFFSET_SEC`: Vasaras laika nobīde

## Atšķirības no Arduino versijas

- Izmanto ESP-IDF native WiFi API
- Izmanto SNTP (nevis configTime)
- Event-driven WiFi savienojums
- C++ wrapper ar extern "C"
- Atgriež const char* (nevis String)
