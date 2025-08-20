#include "telegram_bot.h"
#include "../damper_control/damper_control.h"
#include "../temperature/temperature.h"
#include "../wifi/wifi.h"
#include "cJSON.h"
#include "esp_crt_bundle.h" // TLS certificate bundle
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_tls.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Konfigurācijas (BOT_TOKEN nodod ar platformio.ini build_flags
// -DBOT_TOKEN=\"xxx\")
#ifndef BOT_TOKEN
#define BOT_TOKEN "7495409709:AAFuPnpwo0RJOmQZ3qX9ZjgXHKtjrNpvNFw"
#endif
#ifndef MAX_HTTP_RECV_BUFFER
#define MAX_HTTP_RECV_BUFFER 2048
#endif

static const char *TAG = "TELEGRAM_BOT";

// Globālie (no citām bibliotēkām)
extern int target_temp_c;
extern int temperature;
extern int kP;
extern int temperature_min;
extern std::string messageDamp;

// State
static bool waiting_for_temp = false;
static bool waiting_for_kp = false;
static bool waiting_for_temp_min = false;
static int64_t last_update_id = 0;

// Buffers
static char response_buffer[MAX_HTTP_RECV_BUFFER + 1];

// Queue for outgoing HTTP requests
typedef enum { TELEGRAM_REQ_SEND, TELEGRAM_REQ_CALLBACK } telegram_req_type_t;

typedef struct {
  telegram_req_type_t type;
  char *chat_id;
  char *text;
  char *reply_markup;
  char *callback_id;
} telegram_request_t;

static QueueHandle_t telegram_queue = NULL;

// Basic HTTP helper using open/read with small timeout
static esp_err_t http_request(const char *url, const char *data,
                              esp_http_client_method_t method) {
  esp_http_client_config_t config = {};
  config.url = url;
  config.method = method;
  config.crt_bundle_attach = esp_crt_bundle_attach;
  config.timeout_ms = 3000; // a bit more tolerant

  esp_err_t err = ESP_FAIL;

  for (int attempt = 0; attempt < 3 && err != ESP_OK; ++attempt) {
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
      return ESP_FAIL;

    if (method == HTTP_METHOD_POST)
      esp_http_client_set_header(client, "Content-Type", "application/json");

    int data_len = data ? strlen(data) : 0;
    err = esp_http_client_open(client, data_len);
    if (err == ESP_OK && data_len > 0) {
      int wlen = esp_http_client_write(client, data, data_len);
      if (wlen < 0)
        err = ESP_FAIL;
    }

    int total_read = 0;
    while (err == ESP_OK) {
      int rlen = esp_http_client_read(client, response_buffer + total_read,
                                      MAX_HTTP_RECV_BUFFER - total_read);
      if (rlen < 0) {
        err = ESP_FAIL;
        break;
      }
      if (rlen == 0)
        break; // finished
      total_read += rlen;
      if (total_read >= MAX_HTTP_RECV_BUFFER)
        break;
    }
    response_buffer[total_read] = '\0';

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK)
      vTaskDelay(pdMS_TO_TICKS(100)); // brief pause before retry
  }

  return err;
}

// Forward decl
static esp_err_t send_telegram_message_now(const char *chat_id,
                                           const char *text,
                                           const char *reply_markup);
static esp_err_t answer_callback_query_now(const char *callback_query_id);
static void process_message(cJSON *message);
static void process_callback_query(cJSON *callback_query);

static esp_err_t send_telegram_message_now(const char *chat_id,
                                           const char *text,
                                           const char *reply_markup) {
  char url[160];
  snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/sendMessage",
           BOT_TOKEN);

  cJSON *json = cJSON_CreateObject();
  cJSON_AddStringToObject(json, "chat_id", chat_id);
  cJSON_AddStringToObject(json, "text", text);
  if (reply_markup && *reply_markup) {
    cJSON *markup = cJSON_Parse(reply_markup);
    if (markup)
      cJSON_AddItemToObject(json, "reply_markup", markup);
  }
  char *json_data = cJSON_PrintUnformatted(json);

  esp_err_t err = http_request(url, json_data, HTTP_METHOD_POST);

  free(json_data);
  cJSON_Delete(json);
  return err;
}

static esp_err_t answer_callback_query_now(const char *callback_query_id) {
  char url[192];
  snprintf(url, sizeof(url),
           "https://api.telegram.org/bot%s/answerCallbackQuery", BOT_TOKEN);

  cJSON *json = cJSON_CreateObject();
  cJSON_AddStringToObject(json, "callback_query_id", callback_query_id);
  char *json_data = cJSON_PrintUnformatted(json);

  esp_err_t err = http_request(url, json_data, HTTP_METHOD_POST);

  free(json_data);
  cJSON_Delete(json);
  return err;
}

// Queue helpers
static void queue_send_message(const char *chat_id, const char *text,
                               const char *reply_markup) {
  telegram_request_t *req =
      (telegram_request_t *)calloc(1, sizeof(telegram_request_t));
  if (!req)
    return;
  req->type = TELEGRAM_REQ_SEND;
  req->chat_id = strdup(chat_id);
  req->text = strdup(text);
  if (reply_markup)
    req->reply_markup = strdup(reply_markup);
  xQueueSend(telegram_queue, &req, portMAX_DELAY);
}

static void queue_answer_callback(const char *callback_id) {
  telegram_request_t *req =
      (telegram_request_t *)calloc(1, sizeof(telegram_request_t));
  if (!req)
    return;
  req->type = TELEGRAM_REQ_CALLBACK;
  req->callback_id = strdup(callback_id);
  xQueueSend(telegram_queue, &req, portMAX_DELAY);
}

static char *create_main_keyboard() {
  cJSON *keyboard = cJSON_CreateObject();
  cJSON *inline_keyboard = cJSON_CreateArray();

  // Row 1
  cJSON *row1 = cJSON_CreateArray();
  cJSON *btn1 = cJSON_CreateObject();
  cJSON_AddStringToObject(btn1, "text", "🔄 Info");
  cJSON_AddStringToObject(btn1, "callback_data", "refresh");
  cJSON_AddItemToArray(row1, btn1);
  cJSON_AddItemToArray(inline_keyboard, row1);

  // Row 2
  cJSON *row2 = cJSON_CreateArray();
  cJSON *bt_temp = cJSON_CreateObject();
  cJSON_AddStringToObject(bt_temp, "text", "🌡️ Set Target");
  cJSON_AddStringToObject(bt_temp, "callback_data", "change_temp");
  cJSON_AddItemToArray(row2, bt_temp);
  cJSON *bt_kp = cJSON_CreateObject();
  cJSON_AddStringToObject(bt_kp, "text", "⚙️ kP");
  cJSON_AddStringToObject(bt_kp, "callback_data", "change_kp");
  cJSON_AddItemToArray(row2, bt_kp);
  cJSON_AddItemToArray(inline_keyboard, row2);

  // Row 3
  cJSON *row3 = cJSON_CreateArray();
  cJSON *bt_min = cJSON_CreateObject();
  cJSON_AddStringToObject(bt_min, "text", "❄️ Min Temp");
  cJSON_AddStringToObject(bt_min, "callback_data", "change_temp_min");
  cJSON_AddItemToArray(row3, bt_min);
  cJSON_AddItemToArray(inline_keyboard, row3);

  cJSON_AddItemToObject(keyboard, "inline_keyboard", inline_keyboard);
  char *out = cJSON_PrintUnformatted(keyboard);
  cJSON_Delete(keyboard);
  return out;
}

static void process_message(cJSON *message) {
  cJSON *chat = cJSON_GetObjectItem(message, "chat");
  cJSON *chat_id = chat ? cJSON_GetObjectItem(chat, "id") : NULL;
  cJSON *text = cJSON_GetObjectItem(message, "text");
  if (!chat_id)
    return;

  char chat_id_str[32];
  snprintf(chat_id_str, sizeof(chat_id_str), "%lld",
           (long long)chat_id->valuedouble);

  if (text && text->valuestring) {
    ESP_LOGI(TAG, "RX: %s", text->valuestring);
    if (strcmp(text->valuestring, "/info") == 0) {
      char *kb = create_main_keyboard();
      send_telegram_message_now(chat_id_str, "🏠 Izvēlies darbību:", kb);
      free(kb);
    } else if (strcmp(text->valuestring, "/time") == 0) {
      char buf[48];
      snprintf(buf, sizeof(buf), "🕐 Laiks: %s", get_time_str());
      send_telegram_message_now(chat_id_str, buf, NULL);
    } else if (waiting_for_temp &&
               isdigit((unsigned char)text->valuestring[0])) {
      target_temp_c = atoi(text->valuestring);
      waiting_for_temp = false;
      char msg[48];
      snprintf(msg, sizeof(msg), "✅ Target: %d°C", target_temp_c);
      send_telegram_message_now(chat_id_str, msg, NULL);
    } else if (waiting_for_kp && isdigit((unsigned char)text->valuestring[0])) {
      kP = atoi(text->valuestring);
      waiting_for_kp = false;
      char msg[32];
      snprintf(msg, sizeof(msg), "✅ kP: %d", kP);
      send_telegram_message_now(chat_id_str, msg, NULL);
    } else if (waiting_for_temp_min &&
               isdigit((unsigned char)text->valuestring[0])) {
      temperature_min = atoi(text->valuestring);
      waiting_for_temp_min = false;
      char msg[40];
      snprintf(msg, sizeof(msg), "✅ Min: %d°C", temperature_min);
      send_telegram_message_now(chat_id_str, msg, NULL);
    } else {
      send_telegram_message_now(chat_id_str, "❌ Nederīga ievade.", NULL);
    }
  }
}

static void process_callback_query(cJSON *callback_query) {
  cJSON *id = cJSON_GetObjectItem(callback_query, "id");
  cJSON *data = cJSON_GetObjectItem(callback_query, "data");
  cJSON *message = cJSON_GetObjectItem(callback_query, "message");
  if (!id || !data || !message)
    return;
  cJSON *chat = cJSON_GetObjectItem(message, "chat");
  cJSON *chat_id = chat ? cJSON_GetObjectItem(chat, "id") : NULL;
  if (!chat_id)
    return;

  char chat_id_str[32];
  snprintf(chat_id_str, sizeof(chat_id_str), "%lld",
           (long long)chat_id->valuedouble);
  answer_callback_query_now(id->valuestring);

  if (!data->valuestring)
    return;
  if (strcmp(data->valuestring, "refresh") == 0) {
    char status[256];
    snprintf(status, sizeof(status),
             "🔥 KRĀSNS STATUS 🔥\n\n"
             "🌡️ Temp: %d°C\n"
             "🎯 Target: %d°C\n"
             "⚙️ kP: %d\n"
             "❄️ Min: %d°C\n"
             "🎚️ Damper: %s",
             temperature, target_temp_c, kP, temperature_min,
             messageDamp.c_str());
    send_telegram_message_now(chat_id_str, status, NULL);
  } else if (strcmp(data->valuestring, "change_temp") == 0) {
    waiting_for_temp = true;
    waiting_for_kp = waiting_for_temp_min = false;
    send_telegram_message_now(chat_id_str,
                              "🌡️ Ievadi mērķa temperatūru:", NULL);
  } else if (strcmp(data->valuestring, "change_kp") == 0) {
    waiting_for_kp = true;
    waiting_for_temp = waiting_for_temp_min = false;
    send_telegram_message_now(chat_id_str, "⚙️ Ievadi kP vērtību:", NULL);
  } else if (strcmp(data->valuestring, "change_temp_min") == 0) {
    waiting_for_temp_min = true;
    waiting_for_temp = waiting_for_kp = false;
    send_telegram_message_now(chat_id_str, "❄️ Ievadi min. temperatūru:", NULL);
  }
}

static esp_err_t get_telegram_updates() {
  memset(response_buffer, 0, sizeof(response_buffer));
  char url[192];
  snprintf(url, sizeof(url),
           "https://api.telegram.org/bot%s/getUpdates?timeout=1&offset=%lld",
           BOT_TOKEN, (long long)(last_update_id + 1));

  esp_err_t err = http_request(url, NULL, HTTP_METHOD_GET);
  if (err == ESP_OK) {
    cJSON *json = cJSON_Parse(response_buffer);
    if (json) {
      cJSON *ok = cJSON_GetObjectItem(json, "ok");
      if (cJSON_IsTrue(ok)) {
        cJSON *result = cJSON_GetObjectItem(json, "result");
        if (cJSON_IsArray(result)) {
          cJSON *update = NULL;
          cJSON_ArrayForEach(update, result) {
            cJSON *update_id = cJSON_GetObjectItem(update, "update_id");
            if (update_id && update_id->valuedouble > last_update_id) {
              last_update_id = (int64_t)update_id->valuedouble;
              cJSON *message = cJSON_GetObjectItem(update, "message");
              cJSON *callback_query =
                  cJSON_GetObjectItem(update, "callback_query");
              if (message)
                process_message(message);
              else if (callback_query)
                process_callback_query(callback_query);
            }
          }
        }
      }
      cJSON_Delete(json);
    }
  }
  return err;
}

static void telegram_task(void *pv) {
  ESP_LOGI(TAG, "Telegram task started");
  telegram_request_t *req = NULL;
  while (1) {
    if (xQueueReceive(telegram_queue, &req, pdMS_TO_TICKS(100)) == pdPASS) {
      if (req->type == TELEGRAM_REQ_SEND) {
        send_telegram_message_now(req->chat_id, req->text, req->reply_markup);
      } else if (req->type == TELEGRAM_REQ_CALLBACK) {
        answer_callback_query_now(req->callback_id);
      }
      if (req->chat_id)
        free(req->chat_id);
      if (req->text)
        free(req->text);
      if (req->reply_markup)
        free(req->reply_markup);
      if (req->callback_id)
        free(req->callback_id);
      free(req);
    } else if (is_wifi_connected()) {
      esp_err_t err = get_telegram_updates();
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "getUpdates error: %s", esp_err_to_name(err));
      }
      vTaskDelay(pdMS_TO_TICKS(2000));
    } else {
      vTaskDelay(pdMS_TO_TICKS(500));
    }
  }
}

void telegram_bot_start() {
  telegram_queue = xQueueCreate(10, sizeof(telegram_request_t *));
  xTaskCreate(telegram_task, "telegram_task", 12288, NULL, 5, NULL);
  ESP_LOGI(TAG, "Telegram bot started");
}

void send_alert_message(const char *alert_text) {
  const char *CHAT_ID = "YOUR_CHAT_ID_HERE"; // TODO persist
  char buf[160];
  snprintf(buf, sizeof(buf), "🚨 BRĪDINĀJUMS: %s", alert_text);
  queue_send_message(CHAT_ID, buf, NULL);
}