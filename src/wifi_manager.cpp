#include "wifi_manager.h"

#include "app_config.h"
#include "device_identity.h"
#include "gpio_status.h"

#include <WiFi.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern DeviceIdentity g_identity;

static volatile WifiLinkState g_wifi_state = WifiLinkState::DOWN;
static unsigned long g_sta_start_ms = 0;
static bool g_fallback_ap_active = false;

WifiLinkState wifi_manager_state() { return g_wifi_state; }

bool wifi_manager_has_ip() {
  if (g_wifi_state == WifiLinkState::STA_CONNECTED || g_wifi_state == WifiLinkState::APSTA) {
    return WiFi.status() == WL_CONNECTED;
  }
  if (g_wifi_state == WifiLinkState::AP_UP || g_fallback_ap_active) {
    return true;
  }
  return false;
}

void wifi_manager_get_ip(char* out, size_t out_len) {
  if (out == nullptr || out_len == 0) {
    return;
  }
  out[0] = '\0';
  if (WiFi.status() == WL_CONNECTED) {
    strncpy(out, WiFi.localIP().toString().c_str(), out_len - 1);
    out[out_len - 1] = '\0';
    return;
  }
  if (g_wifi_state == WifiLinkState::AP_UP || g_fallback_ap_active) {
    strncpy(out, WiFi.softAPIP().toString().c_str(), out_len - 1);
    out[out_len - 1] = '\0';
  }
}

int wifi_manager_rssi() {
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.RSSI();
  }
  return 0;
}

static void start_ap(const char* uid) {
  char ssid[32];
  snprintf(ssid, sizeof(ssid), "ROTOR-%s", uid);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1),
                    IPAddress(255, 255, 255, 0));
  WiFi.softAP(ssid);
  g_wifi_state = WifiLinkState::AP_UP;
  gpio_status_set_wlan_link(true);
}

static void apply_sta_static_ip(const AppConfig& cfg) {
  if (cfg.wifi_dhcp) {
    return;
  }
  IPAddress ip, mask, gw, dns;
  if (!ip.fromString(cfg.wifi_ip) || !mask.fromString(cfg.wifi_mask) ||
      !gw.fromString(cfg.wifi_gw)) {
    return;
  }
  dns.fromString(cfg.wifi_dns);
  WiFi.config(ip, gw, mask, dns);
}

static void wifi_apply_mode(const AppConfig& cfg) {
  g_fallback_ap_active = false;
  g_sta_start_ms = millis();

  switch (cfg.wifi_mode) {
    case WifiMode::AP:
      WiFi.mode(WIFI_AP);
      start_ap(g_identity.uid);
      break;

    case WifiMode::STA:
      WiFi.mode(WIFI_STA);
      apply_sta_static_ip(cfg);
      if (cfg.wifi_ssid[0] != '\0') {
        WiFi.begin(cfg.wifi_ssid, cfg.wifi_pass);
        g_wifi_state = WifiLinkState::DOWN;
        gpio_status_set_wlan_link(false);
      } else {
        // Keine SSID → AP als Notbetrieb
        WiFi.mode(WIFI_AP);
        start_ap(g_identity.uid);
      }
      break;

    case WifiMode::APSTA:
      WiFi.mode(WIFI_AP_STA);
      start_ap(g_identity.uid);
      apply_sta_static_ip(cfg);
      if (cfg.wifi_ssid[0] != '\0') {
        WiFi.begin(cfg.wifi_ssid, cfg.wifi_pass);
      }
      g_wifi_state = WifiLinkState::APSTA;
      gpio_status_set_wlan_link(true);
      break;
  }
}

void wifi_manager_begin() {
  WiFi.persistent(false);
  wifi_apply_mode(app_config_runtime_const());
}

static void wifi_manager_task(void* /*arg*/) {
  for (;;) {
    const AppConfig& cfg = app_config_runtime_const();
    const bool sta_connected = (WiFi.status() == WL_CONNECTED);

    if (cfg.wifi_mode == WifiMode::STA) {
      if (sta_connected) {
        g_wifi_state = WifiLinkState::STA_CONNECTED;
        g_fallback_ap_active = false;
        gpio_status_set_wlan_link(true);
      } else {
        gpio_status_set_wlan_link(false);
        if (cfg.fallback_ap && !g_fallback_ap_active && cfg.wifi_ssid[0] != '\0') {
          if ((millis() - g_sta_start_ms) >= cfg.sta_timeout_ms) {
            // Fallback-AP, STA-Versuche weiterlaufen lassen
            WiFi.mode(WIFI_AP_STA);
            start_ap(g_identity.uid);
            WiFi.begin(cfg.wifi_ssid, cfg.wifi_pass);
            g_fallback_ap_active = true;
            g_wifi_state = WifiLinkState::APSTA;
          }
        }
        // Periodisch reconnect anstoßen
        static unsigned long last_re = 0;
        if (millis() - last_re > 10000) {
          last_re = millis();
          if (cfg.wifi_ssid[0] != '\0' && WiFi.status() != WL_CONNECTED) {
            WiFi.disconnect();
            WiFi.begin(cfg.wifi_ssid, cfg.wifi_pass);
          }
        }
      }
    } else if (cfg.wifi_mode == WifiMode::APSTA) {
      gpio_status_set_wlan_link(true);
      g_wifi_state = sta_connected ? WifiLinkState::APSTA : WifiLinkState::AP_UP;
    } else {
      // AP
      gpio_status_set_wlan_link(true);
      g_wifi_state = WifiLinkState::AP_UP;
    }

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void wifi_manager_start_task() {
  xTaskCreate(wifi_manager_task, "wifi_mgr", 4096, nullptr, 3, nullptr);
}
