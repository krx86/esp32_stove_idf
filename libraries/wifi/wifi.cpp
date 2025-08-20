#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <esp_sntp.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <string.h>
#include <time.h>
#include "wifi.h"


// Minimāla konfigurācijas
#define WIFI_SSID "HUAWEI-B525-90C8"
#define WIFI_PASSWORD "BTT6F1EA171"
#define WIFI_MAXIMUM_RETRY 3

static int wifi_retry_num = 0;
static bool wifi_connected = false;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (wifi_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            wifi_retry_num++;
        }
        wifi_connected = false;
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        wifi_retry_num = 0;
        wifi_connected = true;
    }
}

void wifi_connect() {
    // Minimāla NVS inicializācija
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Iespējojam PSRAM priekš LWIP (ja ir pieejams)
    if (heap_caps_get_total_size(MALLOC_CAP_SPIRAM) > 0) {
        heap_caps_malloc_extmem_enable(2048); // Minimāli DRAM
    }

    // Minimāla network interface inicializācija
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // WiFi inicializācija ar PSRAM optimizāciju
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    
    // Droša PSRAM optimizācija, lai izvairītos no kļūdām
    cfg.static_rx_buf_num = 4;          // Drošs minimums RX buferiem
    cfg.dynamic_rx_buf_num = 6;         // Optimāls minimums dynamic buferiem
    cfg.static_tx_buf_num = 4;          // Nepieciešamais minimums TX buferiem
    cfg.tx_buf_type = 0;                // Standarta TX buferu režīms
    cfg.cache_tx_buf_num = 8;           // Optimizēts PSRAM cache
    
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    // WiFi konfigurācija - bez buferiem
    wifi_config_t wifi_config = {};
    memcpy(wifi_config.sta.ssid, WIFI_SSID, strlen(WIFI_SSID));
    memcpy(wifi_config.sta.password, WIFI_PASSWORD, strlen(WIFI_PASSWORD));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MAX_MODEM)); // Minimāls enerģijas patēriņš
    ESP_ERROR_CHECK(esp_wifi_start());
}

void sync_time() {
    // Minimāls SNTP setup bez liekas atmiņas patēriņa
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
    
    // Kompakta timezone ar DST atbalstu (Latvijas laiks)
    setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1); // Latvijas laika zona ar vasaras laiku
    tzset();
    
    // Iestatām ilgāku sinhronizācijas intervālu, lai taupītu resurstus
    sntp_set_sync_interval(3600000); // Stundas sinhronizācija
    
    // Īsāks timeout
    int retry = 0;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < 5) {
        vTaskDelay(500 / portTICK_PERIOD_MS);  // Vēl ātrāks
    }
}

const char* get_time_str() {
    static char time_buf[6];
    time_t now;
    struct tm timeinfo;
    
    // Inicializējam visu strukturu ar nullēm
    memset(&timeinfo, 0, sizeof(timeinfo));
    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    if (timeinfo.tm_year < (2016 - 1900)) {
        return "--:--";
    } else {
        // Tiešā formatēšana bez sprintf/strftime (ietaupa RAM)
        time_buf[0] = '0' + timeinfo.tm_hour / 10;
        time_buf[1] = '0' + timeinfo.tm_hour % 10;
        time_buf[2] = ':';
        time_buf[3] = '0' + timeinfo.tm_min / 10;
        time_buf[4] = '0' + timeinfo.tm_min % 10;
        time_buf[5] = '\0';
        return time_buf;
    }
}

void ota_setup() {
    // Placeholder
}

void ota_loop() {
    // Placeholder
}

bool is_wifi_connected() {
    return wifi_connected;
}
