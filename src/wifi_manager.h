#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum class WifiLinkState : uint8_t {
  DOWN = 0,
  AP_UP = 1,
  STA_CONNECTED = 2,
  APSTA = 3,
};

void wifi_manager_begin();
void wifi_manager_start_task();
WifiLinkState wifi_manager_state();
bool wifi_manager_has_ip();
void wifi_manager_get_ip(char* out, size_t out_len);
int wifi_manager_rssi();
