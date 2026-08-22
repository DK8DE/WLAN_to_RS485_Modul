#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum class WifiLinkState : uint8_t {
  DOWN = 0,
  AP_UP = 1,
  STA_CONNECTED = 2,
};

enum class WifiScanState : int8_t {
  IDLE = 0,
  RUNNING = 1,
  DONE = 2,
  FAILED = -1,
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

// Hintergrund-Scan für Web-UI (kurze HTTP-Polls statt langer Request).
bool wifi_manager_scan_start(uint8_t band, bool include_hidden = false);
WifiScanState wifi_manager_scan_state();
int wifi_manager_scan_count();
uint8_t wifi_manager_scan_band();
void wifi_manager_scan_clear();

// Blockierender Scan (AT). keepalive optional.
typedef void (*WifiScanKeepaliveFn)(void);
int wifi_manager_scan(bool include_hidden, uint8_t band,
                      WifiScanKeepaliveFn keepalive = nullptr);

void wifi_manager_apply_band(uint8_t band_mode_enum); // WifiBand
void wifi_manager_restore_softap();

// Legacy-API (no-op): SoftAP und STA laufen nie gleichzeitig.
void wifi_manager_set_client_data_mode(bool enable);
