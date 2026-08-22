#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum class WifiLinkState : uint8_t {
  DOWN = 0,
  AP_UP = 1,
  STA_CONNECTED = 2,
};

void wifi_manager_begin();
void wifi_manager_start_task();
void wifi_manager_apply_runtime(); // Konfig aus RAM neu anwenden (ohne Reboot)

WifiLinkState wifi_manager_state();
bool wifi_manager_has_ip();
void wifi_manager_get_ip(char* out, size_t out_len);
void wifi_manager_get_sta_ip(char* out, size_t out_len);
void wifi_manager_get_ap_ip(char* out, size_t out_len);
void wifi_manager_get_ap_ssid(char* out, size_t out_len);
int wifi_manager_rssi();
bool wifi_manager_sta_connected();

// Scan (blockierend). band: WifiBand (0=Auto, 1=2.4, 2=5).
// Im SoftAP-Modus bleibt der AP während des Scans erreichbar (kurz AP+STA).
int wifi_manager_scan(bool include_hidden, uint8_t band);
void wifi_manager_apply_band(uint8_t band_mode_enum); // WifiBand
void wifi_manager_restore_softap();

// Legacy-API (no-op): SoftAP und STA laufen nie gleichzeitig.
void wifi_manager_set_client_data_mode(bool enable);
