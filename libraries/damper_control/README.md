# Damper Control Library

## Apraksts
ESP32 damper kontroles bibliotēka ar PID regulatoru un servo vadību VVC projektam. Eksaktā kopija no test22 projekta.

## Funkcionalitāte

### Pamatfunkcijas
- **PID Regulators**: Automātiska damper pozīcijas kontrole uz bāzes temperatūras atšķirības
- **Servo Vadība**: Asinhronā servo kustība ar pakāpenisku pozīcijas maiņu
- **Temperatūras Monitorings**: Automatiska sistēma malkas papildināšanas noteikšanai
- **Deep Sleep**: Automātiska pāreja dziļajā miegā pie zemas temperatūras
- **Buzzer Brīdinājumi**: Audio signāli kritisku situāciju gadījumā

### PID Parametri
- `kP = 25`: Proporcionālais koeficients
- `tauI = 1000`: Integrālās komponentes laika konstante
- `tauD = 5`: Diferenciālās komponentes laika konstante
- `kI = kP / tauI`: Integrālais koeficients
- `kD = kP / tauD`: Diferenciālais koeficients

### Darba Režīmi
1. **AUTO**: Normāls PID regulēšanas režīms
2. **MANUAL**: Manuāla damper kontrole
3. **FILL!**: Nepieciešams papildināt malku
4. **END!**: Sistēma beigusi darboties

### Servo Konfigurācija
- GPIO: 5
- Leņķis: 35°
- Offset: 29
- Kustības ātrums: 50ms/solis

### Temperatūras Robežas
- Minimālā temperatūra: Aktivē full open režīmu
- Mērķa temperatūra: PID regulēšanas mērķis
- Maksimālā temperatūra: Aktivē damper aizvēršanu

## Izmantošana

```cpp
#include "damper_control.h"

void setup() {
    // Inicializē damper kontroli
    damperControlInit();
    
    // Sāk damper uzdevumu
    startDamperControlTask();
}

void loop() {
    // Galvenā kontroles cilpa
    damperControlLoop();
    
    // Pārbauda deep sleep nepieciešamību
    checkLowTempDeepSleep();
}
```

## Atkarības
- ESP32Servo
- FreeRTOS
- Arduino Framework

## Integrācija
Bibliotēka ir pilnībā integrēta ar:
- `temperature` bibliotēku
- `display_manager` bibliotēku
- `touch_button` bibliotēku
- `telnet` bibliotēku

## Autori
ESP32 Stove Controller Team - Eksaktā kopija no test22 projekta
