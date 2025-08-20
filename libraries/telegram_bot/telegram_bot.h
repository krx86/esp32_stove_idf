#pragma once

#ifdef __cplusplus
extern "C" {
#endif


void telegram_bot_start();
void send_alert_message(const char* alert_text);

#ifdef __cplusplus
}
#endif
