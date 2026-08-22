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
static bool g_fallback_ap_active = false; // SoftAP-only nach STA-Timeout (kein AP+STA)
static char g_ap_ssid[32] = {0};
static volatile bool g_scan_active = false;
static volatile WifiScanState g_async_scan_state = WifiScanState::IDLE;
static volatile int g_async_scan_count = 0;
static uint8_t g_async_scan_band = 0;
static TaskHandle_t g_async_scan_task = nullptr;
static uint8_t g_sta_connect_attempts = 0;
static unsigned long g_sta_last_attempt_ms = 0;

static constexpr uint8_t kStaMaxConnectAttempts = 3;
static constexpr unsigned long kStaRetryIntervalMs = 10000UL;

static bool softap_is_up() {
  return (WiFi.getMode() & WIFI_AP) != 0 && WiFi.softAPIP() != IPAddress(0, 0, 0, 0);
}

static constexpr int kSoftApChannel2g = 6;
static constexpr int kSoftApChannel5g = 36;

WifiLinkState wifi_manager_state() { return g_wifi_state; }

bool wifi_manager_sta_connected() { return WiFi.status() == WL_CONNECTED; }

bool wifi_manager_has_ip() {
  if (wifi_manager_sta_connected()) {
    return true;
  }
  if (g_wifi_state == WifiLinkState::AP_UP || g_fallback_ap_active) {
    return true;
  }
  return false;
}

void wifi_manager_get_sta_ip(char* out, size_t out_len) {
  if (out == nullptr || out_len == 0) {
    return;
  }
  out[0] = '\0';
  if (WiFi.status() == WL_CONNECTED) {
    strncpy(out, WiFi.localIP().toString().c_str(), out_len - 1);
    out[out_len - 1] = '\0';
  }
}

void wifi_manager_get_ap_ip(char* out, size_t out_len) {
  if (out == nullptr || out_len == 0) {
    return;
  }
  out[0] = '\0';
  if ((WiFi.getMode() & WIFI_AP) != 0) {
    strncpy(out, WiFi.softAPIP().toString().c_str(), out_len - 1);
    out[out_len - 1] = '\0';
  }
}

void wifi_manager_get_ap_ssid(char* out, size_t out_len) {
  if (out == nullptr || out_len == 0) {
    return;
  }
  out[0] = '\0';
  if ((WiFi.getMode() & WIFI_AP) != 0) {
    strncpy(out, g_ap_ssid, out_len - 1);
    out[out_len - 1] = '\0';
  }
}

void wifi_manager_get_ip(char* out, size_t out_len) {
  wifi_manager_get_sta_ip(out, out_len);
  if (out[0] == '\0') {
    wifi_manager_get_ap_ip(out, out_len);
  }
}

int wifi_manager_rssi() {
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.RSSI();
  }
  return 0;
}

void wifi_manager_apply_band(uint8_t band) {
#if defined(CONFIG_SOC_WIFI_SUPPORT_5G) || defined(WIFI_BAND_MODE_AUTO)
  wifi_band_mode_t mode = WIFI_BAND_MODE_AUTO;
  switch (static_cast<WifiBand>(band)) {
    case WifiBand::BAND_2G:
      mode = WIFI_BAND_MODE_2G_ONLY;
      break;
    case WifiBand::BAND_5G:
      mode = WIFI_BAND_MODE_5G_ONLY;
      break;
    default:
      mode = WIFI_BAND_MODE_AUTO;
      break;
  }
  WiFi.setBandMode(mode);
#else
  (void)band;
#endif
}

static WifiBand softap_effective_band(WifiBand configured) {
  // SoftAP: Auto → 2,4 GHz (Auslieferungszustand)
  if (configured == WifiBand::AUTO) {
    return WifiBand::BAND_2G;
  }
  return configured;
}

static void stop_softap() {
  if ((WiFi.getMode() & WIFI_AP) != 0) {
    WiFi.softAPdisconnect(true);
  }
}

static void start_ap(WifiBand band) {
  snprintf(g_ap_ssid, sizeof(g_ap_ssid), "ROTOR-%s", g_identity.uid);

  const WifiBand ap_band = softap_effective_band(band);
  wifi_manager_apply_band(static_cast<uint8_t>(ap_band));

  const int channel =
      (ap_band == WifiBand::BAND_5G) ? kSoftApChannel5g : kSoftApChannel2g;

  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1),
                    IPAddress(255, 255, 255, 0));
  WiFi.softAP(g_ap_ssid, nullptr, channel, 0, 4);
  g_wifi_state = WifiLinkState::AP_UP;
  gpio_status_set_wlan_led(WlanLedMode::AP_BLINK);
}

static bool apply_sta_static_ip(const AppConfig& cfg) {
  IPAddress ip, mask, gw, dns;
  if (!ip.fromString(cfg.wifi_ip) || !mask.fromString(cfg.wifi_mask) ||
      !gw.fromString(cfg.wifi_gw)) {
    return false;
  }
  if (cfg.wifi_dns[0] != '\0') {
    if (!dns.fromString(cfg.wifi_dns)) {
      dns = gw;
    }
  } else {
    dns = gw;
  }
  return WiFi.config(ip, gw, mask, dns);
}

static void sta_begin_connect(const AppConfig& cfg) {
  wifi_manager_apply_band(static_cast<uint8_t>(cfg.wifi_band));
  WiFi.disconnect(true);
  vTaskDelay(pdMS_TO_TICKS(50));

  if (cfg.wifi_dhcp) {
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  } else {
    apply_sta_static_ip(cfg);
  }

  WiFi.begin(cfg.wifi_ssid, cfg.wifi_pass);
  g_sta_last_attempt_ms = millis();
}

// Effektiver Modus: nie AP+STA. Alte NVS-Werte APSTA → STA (mit SSID) bzw. AP.
static WifiMode effective_wifi_mode(const AppConfig& cfg) {
  if (cfg.wifi_mode == WifiMode::APSTA) {
    return (cfg.wifi_ssid[0] != '\0') ? WifiMode::STA : WifiMode::AP;
  }
  return cfg.wifi_mode;
}

static void wifi_apply_mode(const AppConfig& cfg) {
  g_fallback_ap_active = false;
  g_sta_connect_attempts = 0;
  g_sta_last_attempt_ms = 0;
  g_sta_start_ms = millis();

  const WifiMode mode = effective_wifi_mode(cfg);

  switch (mode) {
    case WifiMode::STA:
      stop_softap();
      WiFi.mode(WIFI_STA);
      if (cfg.wifi_ssid[0] != '\0') {
        g_sta_connect_attempts = 1;
        sta_begin_connect(cfg);
        g_wifi_state = WifiLinkState::DOWN;
        gpio_status_set_wlan_led(WlanLedMode::OFF);
      } else {
        // Ohne SSID kein Infrastruktur-Betrieb → SoftAP (XOR)
        WiFi.mode(WIFI_AP);
        start_ap(cfg.wifi_band);
      }
      break;

    case WifiMode::AP:
    case WifiMode::APSTA: // unreachable after effective_wifi_mode; keep for switch
    default:
      WiFi.mode(WIFI_AP);
      start_ap(cfg.wifi_band);
      break;
  }
}

void wifi_manager_begin() {
  WiFi.persistent(false);
  WiFi.setSleep(false); // Power-Save killt sonst oft TCP-Sessions (errno 113)
  wifi_apply_mode(app_config_runtime_const());
}

void wifi_manager_apply_runtime() {
  wifi_apply_mode(app_config_runtime_const());
}

struct ScanJob {
  uint8_t band;
  bool include_hidden;
  bool softap_was_up;
  bool sta_was_connected;
};

static void scan_restore_after(const ScanJob& job) {
  const AppConfig& cfg = app_config_runtime_const();
  const WifiMode eff = effective_wifi_mode(cfg);

  if (job.softap_was_up && (eff == WifiMode::AP || g_fallback_ap_active)) {
    if (eff == WifiMode::AP && WiFi.getMode() != WIFI_AP) {
      WiFi.mode(WIFI_AP);
    } else if (g_fallback_ap_active) {
      wifi_manager_restore_softap();
    }
  } else if (!job.softap_was_up) {
    wifi_manager_apply_band(static_cast<uint8_t>(cfg.wifi_band));
    if (!job.sta_was_connected && cfg.wifi_ssid[0] != '\0' &&
        effective_wifi_mode(cfg) == WifiMode::STA) {
      g_sta_connect_attempts = 1;
      sta_begin_connect(cfg);
    }
  }
}

static void scan_prepare(const ScanJob& job) {
  if (job.softap_was_up) {
    if (WiFi.getMode() != WIFI_AP_STA) {
      WiFi.mode(WIFI_AP_STA);
    }
  } else if (job.sta_was_connected) {
    wifi_manager_apply_band(job.band);
  } else {
    stop_softap();
    WiFi.mode(WIFI_STA);
    wifi_manager_apply_band(job.band);
  }
}

static int scan_run_blocking(const ScanJob& job, WifiScanKeepaliveFn keepalive) {
  g_scan_active = true;
  scan_prepare(job);

  const bool passive = job.sta_was_connected;
  int n = -1;
  const int16_t started = WiFi.scanNetworks(/*async=*/true, job.include_hidden, passive,
                                            /*max_ms_per_chan=*/120);
  if (started == WIFI_SCAN_FAILED) {
    g_scan_active = false;
    return -1;
  }

  const unsigned long deadline = millis() + 45000UL;
  for (;;) {
    if (keepalive) {
      keepalive();
    }
    const int16_t st = WiFi.scanComplete();
    if (st >= 0) {
      n = st;
      break;
    }
    if (st == WIFI_SCAN_FAILED) {
      break;
    }
    if ((int32_t)(millis() - deadline) >= 0) {
      WiFi.scanDelete();
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }

  scan_restore_after(job);
  g_scan_active = false;
  return n;
}

static void wifi_scan_task(void* pv) {
  ScanJob job = *static_cast<ScanJob*>(pv);
  delete static_cast<ScanJob*>(pv);

  g_async_scan_band = job.band;
  g_async_scan_count = -1;

  const int n = scan_run_blocking(job, nullptr);
  g_async_scan_count = n;
  g_async_scan_state = (n >= 0) ? WifiScanState::DONE : WifiScanState::FAILED;
  g_async_scan_task = nullptr;
  vTaskDelete(nullptr);
}

void wifi_manager_scan_clear() {
  WiFi.scanDelete();
  g_async_scan_count = 0;
  g_async_scan_state = WifiScanState::IDLE;
}

bool wifi_manager_scan_start(uint8_t band, bool include_hidden) {
  if (g_async_scan_state == WifiScanState::RUNNING) {
    return false;
  }
  if (g_async_scan_state == WifiScanState::DONE) {
    wifi_manager_scan_clear();
  }

  auto* job = new ScanJob{band, include_hidden, softap_is_up(),
                          (WiFi.status() == WL_CONNECTED)};
  g_async_scan_state = WifiScanState::RUNNING;
  g_async_scan_band = band;
  if (xTaskCreate(wifi_scan_task, "wifiscan", 8192, job, 2, &g_async_scan_task) !=
      pdPASS) {
    delete job;
    g_async_scan_state = WifiScanState::IDLE;
    return false;
  }
  return true;
}

WifiScanState wifi_manager_scan_state() { return g_async_scan_state; }

int wifi_manager_scan_count() { return g_async_scan_count; }

uint8_t wifi_manager_scan_band() { return g_async_scan_band; }

int wifi_manager_scan(bool include_hidden, uint8_t band,
                      WifiScanKeepaliveFn keepalive) {
  if (g_async_scan_state == WifiScanState::RUNNING) {
    const unsigned long deadline = millis() + 45000UL;
    while (g_async_scan_state == WifiScanState::RUNNING) {
      if (keepalive) {
        keepalive();
      }
      if ((int32_t)(millis() - deadline) >= 0) {
        return -1;
      }
      vTaskDelay(pdMS_TO_TICKS(50));
    }
    if (g_async_scan_state == WifiScanState::DONE) {
      const int n = g_async_scan_count;
      wifi_manager_scan_clear();
      return n;
    }
    return -1;
  }

  const ScanJob job{band, include_hidden, softap_is_up(),
                    (WiFi.status() == WL_CONNECTED)};
  const int n = scan_run_blocking(job, keepalive);
  return n;
}

void wifi_manager_restore_softap() {
  stop_softap();
  WiFi.mode(WIFI_AP);
  start_ap(app_config_runtime_const().wifi_band);
  g_fallback_ap_active = (effective_wifi_mode(app_config_runtime_const()) == WifiMode::STA);
}

void wifi_manager_set_client_data_mode(bool /*enable*/) {
  // Kein AP+STA mehr — SoftAP ist im STA-Modus immer aus.
}

static void wifi_manager_task(void* /*arg*/) {
  for (;;) {
    if (g_scan_active) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    const AppConfig& cfg = app_config_runtime_const();
    const WifiMode mode = effective_wifi_mode(cfg);
    const bool sta_connected = (WiFi.status() == WL_CONNECTED);

    if (mode == WifiMode::STA && !g_fallback_ap_active) {
      // Infrastruktur: SoftAP darf nie an sein
      if ((WiFi.getMode() & WIFI_AP) != 0) {
        stop_softap();
        WiFi.mode(WIFI_STA);
        wifi_manager_apply_band(static_cast<uint8_t>(cfg.wifi_band));
      }

      if (sta_connected) {
        g_wifi_state = WifiLinkState::STA_CONNECTED;
        gpio_status_set_wlan_led(WlanLedMode::STA_ON);
        WiFi.setSleep(false);
        g_sta_connect_attempts = 0;
      } else {
        g_wifi_state = WifiLinkState::DOWN;
        gpio_status_set_wlan_led(WlanLedMode::OFF);

        const unsigned long since_attempt = (unsigned long)(int32_t)(millis() - g_sta_last_attempt_ms);
        if (cfg.fallback_ap && cfg.wifi_ssid[0] != '\0' &&
            g_sta_connect_attempts >= kStaMaxConnectAttempts &&
            since_attempt >= kStaRetryIntervalMs) {
          // 3 Verbindungsversuche fehlgeschlagen → SoftAP-Fallback
          WiFi.disconnect(true);
          WiFi.mode(WIFI_AP);
          start_ap(cfg.wifi_band);
          g_fallback_ap_active = true;
        } else if (since_attempt >= kStaRetryIntervalMs &&
                   g_sta_connect_attempts < kStaMaxConnectAttempts) {
          g_sta_connect_attempts++;
          sta_begin_connect(cfg);
        }
      }
    } else if (g_fallback_ap_active) {
      // SoftAP-Fallback aktiv — bleibt SoftAP-only bis Konfig erneut STA wählt
      g_wifi_state = WifiLinkState::AP_UP;
      gpio_status_set_wlan_led(WlanLedMode::AP_BLINK);
    } else {
      // SoftAP-Modus
      if ((WiFi.getMode() & WIFI_STA) != 0 && (WiFi.getMode() & WIFI_AP) == 0) {
        WiFi.mode(WIFI_AP);
        start_ap(cfg.wifi_band);
      }
      g_wifi_state = WifiLinkState::AP_UP;
      gpio_status_set_wlan_led(WlanLedMode::AP_BLINK);
    }

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void wifi_manager_start_task() {
  xTaskCreate(wifi_manager_task, "wifi_mgr", 4096, nullptr, 3, nullptr);
}
