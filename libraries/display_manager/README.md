# Display Manager for VVC Project

Efektīvs displeja pārvaldības modulis VVC ESP32 projektam ar LVGL bibliotēku.

## Galvenās funkcijas

- **Event-driven atjaunināšana** - displejs atjaunojas tikai kad dati mainās
- **Optimizēti intervāli** - mazāka CPU slodze
- **WiFi laika sinhronizācija** - integrēts ar wifi bibliotēku
- **Statistika un debugging** - detalizēta informācija par veiktspēju
- **VVC projekt adaptācija** - integrēts ar lv_display un temperature bibliotēkām

## Lietošana

### 1. Inicializācija

```cpp
#include "display_manager.h"

void setup() {
    // ... citas inicializācijas ...
    display_manager_init();
}
```

### 2. Galvenā loop funkcija

```cpp
void loop() {
    display_manager_update();
    // ... citas funkcijas ...
}
```

### 3. Event paziņošana

```cpp
// Kad temperatūra mainās
display_manager_notify_temperature_changed();

// Kad mērķa temperatūra mainās
display_manager_notify_target_temp_changed();

// Kad laiks tiek sinhronizēts
display_manager_notify_time_synced();
```

### 4. WiFi laika sinhronizācija

```cpp
#include "wifi.h"  // WiFi bibliotēka

void setup() {
    // WiFi savienojums
    wifi_connect();
    
    // Paziņot display manager par WiFi statusu
    display_manager_set_wifi_status(true);
    
    // Forsa laika sinhronizāciju
    display_manager_force_time_sync();
}

void loop() {
    // Regulāra laika atjaunošana (automātiski)
    display_manager_update();
    
    // OTA apkalpošana
    ota_loop();
}
```

### 5. Konfigurācijas maiņa

```cpp
// Event-driven režīms (maksimāla efektivitāte)
display_manager_set_update_intervals(
    0,      // Temperatūra: tikai event-driven
    1000,   // Progress bars: katru sekundi
    30000,  // Laiks: katras 30 sekundes
    50      // Touch: katras 50ms
);
```

## Pieejamās funkcijas

### Galvenās funkcijas
- `display_manager_init()` - Inicializācija
- `display_manager_update()` - Galvenā atjaunināšanas funkcija
- `display_manager_set_update_intervals()` - Intervālu konfigurācija

### Force update funkcijas
- `display_manager_force_update_all()` - Atjaunināt visu
- `display_manager_force_update_temperature()` - Atjaunināt temperatūru
- `display_manager_force_update_bars()` - Atjaunināt progress bars
- `display_manager_force_update_time()` - Atjaunināt laiku

### Event paziņošana
- `display_manager_notify_temperature_changed()` - Temperatūras izmaiņas
- `display_manager_notify_target_temp_changed()` - Mērķa temperatūras izmaiņas
- `display_manager_notify_time_synced()` - Laika sinhronizācija

### WiFi laika sinhronizācija
- `display_manager_wifi_time_sync()` - Sinhronizē laiku ar NTP serveri
- `display_manager_force_time_sync()` - Forsa tūlītēju laika sinhronizāciju
- `display_manager_set_wifi_status(bool connected)` - Paziņo par WiFi statusu
- `display_manager_update_time_from_wifi()` - Atjauno laiku no WiFi bibliotēkas

### Debugging un statistika
- `display_manager_print_config()` - Parādīt konfigurāciju
- `display_manager_print_stats()` - Parādīt statistiku
- `display_manager_reset_stats()` - Atiestatīt statistiku
- `display_manager_get_stats()` - Iegūt statistikas struktūru

## Konfigurācijas ieteikumi

### Maksimāla efektivitāte (Event-driven)
```cpp
display_manager_set_update_intervals(0, 0, 30000, 50);
```
- Temperatūra un bars atjaunojas tikai kad mainās
- Laiks katras 30 sekundes
- Touch atbildes katras 50ms

### Standarta režīms
```cpp
display_manager_set_update_intervals(2000, 1000, 30000, 50);
```
- Temperatūra katras 2 sekundes
- Progress bars katru sekundi  
- Laiks katras 30 sekundes
- Touch atbildes katras 50ms

## Integrācija ar VVC projektu

Display Manager izmanto:
- `lv_display.h` - LVGL displeja funkcijas
- `temperature.h` - temperatūras datu iegūšana
- `has_temperature_changed()` - izmaiņu noteikšana

Nepieciešamās funkcijas lv_display bibliotēkā:
- `lv_display_update_temperature(int temp)`
- `lv_display_update_bars()`
- `lv_display_update_time()` (izvēles)

## Debug piemērs

```cpp
// Parādīt konfigurāciju
display_manager_print_config();

// Parādīt statistiku
display_manager_print_stats();

// Manuāla atjaunināšana testēšanai
display_manager_force_update_all();
```

## Versija

VVC Display Manager v1.0.0 - Adaptēts VVC projektam 2025
