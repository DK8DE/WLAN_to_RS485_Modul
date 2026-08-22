#pragma once

#include "device_identity.h"

#include <stdint.h>

static constexpr uint16_t kConfigSchemaVersion = 1;

// AP = SoftAP only, STA = Infrastruktur only. APSTA nur noch NVS-Legacy (wird zu STA/AP gemappt).
enum class WifiMode : uint8_t { AP = 0, STA = 1, APSTA = 2 };
enum class WifiBand : uint8_t { AUTO = 0, BAND_2G = 1, BAND_5G = 2 };
enum class NetMode : uint8_t {
  TCP_SERVER = 0,
  TCP_CLIENT = 1,
  NET_OFF = 2,      // Schema v1: DISABLED
  UDP_SERVER = 3,   // Erweiterung gegenüber Spec v4 (UDP bewusst ergänzt)
  UDP_CLIENT = 4
};
enum class PacketDelimiter : uint8_t { NONE = 0, CR = 1, LF = 2, CUSTOM = 3 };

struct AppConfig {
  uint16_t schema_version;

  char device_name[kNameMax];
  uint8_t bus_address;

  WifiMode wifi_mode;
  WifiBand wifi_band;
  char wifi_ssid[33];
  char wifi_pass[65];
  bool wifi_dhcp;
  char wifi_ip[16];
  char wifi_mask[16];
  char wifi_gw[16];
  char wifi_dns[16];
  bool fallback_ap;
  uint32_t sta_timeout_ms; // Fallback-AP nach dieser Zeit ohne STA

  NetMode net_mode;
  uint16_t local_port;
  char remote_ip[16];
  char remote_host[64];
  uint16_t remote_port;
  uint32_t reconnect_ms;
  uint32_t tcp_timeout_ms;
  bool tcp_nodelay;
  bool tcp_keepalive;

  // RS485 fest 115200 8N1 — Baud nur zur Info
  uint32_t rs485_baud;
  uint16_t packet_timeout_ms; // 1..100, Default 2
  uint16_t packet_size;       // 32..1460, Default 1024
  PacketDelimiter delimiter;
  uint8_t delimiter_custom;
  bool echo_suppress;
  bool bridge_enabled;
  bool rs485_tx_allowed;
  bool rs485_rx_allowed;

  // Web-UI Login (HTTP Basic Auth, Benutzer "admin")
  char web_pass[65];
};

void app_config_set_factory_defaults(AppConfig* cfg, const DeviceIdentity& id);
bool app_config_load(AppConfig* cfg, const DeviceIdentity& id);
bool app_config_save(const AppConfig& cfg);
bool app_config_factory_reset(AppConfig* cfg, const DeviceIdentity& id);

// Laufzeit-Kopie (RAM); NVS erst nach save
AppConfig& app_config_runtime();
const AppConfig& app_config_runtime_const();
void app_config_init_runtime(const DeviceIdentity& id);
