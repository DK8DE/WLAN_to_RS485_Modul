#include "at_command.h"

#include "Version.h"
#include "app_config.h"
#include "config_handlers.h"
#include "config_udp.h"
#include "device_identity.h"
#include "network_bridge.h"
#include "wifi_manager.h"

#include <WiFi.h>
#include <Arduino.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern DeviceIdentity g_identity;

static UartAppMode g_mode = UartAppMode::Data;
static unsigned long g_session_last_ms = 0;
static constexpr unsigned long kSessionTimeoutMs = 30000;
static ConfigTransport g_session_transport = ConfigTransport::Uart;
static ConfigTransport g_last_rx_transport = ConfigTransport::Uart;

static char g_line[192];
static size_t g_line_len = 0;

static char g_esc[48];
static size_t g_esc_len = 0;

void at_command_begin() {
  g_mode = UartAppMode::Data;
  g_session_last_ms = 0;
  g_line_len = 0;
  g_esc_len = 0;
}

UartAppMode at_command_mode() { return g_mode; }

void at_command_set_mode(UartAppMode mode) {
  g_mode = mode;
  if (mode == UartAppMode::At) {
    g_session_last_ms = millis();
    g_session_transport = g_last_rx_transport;
  }
  g_line_len = 0;
  g_esc_len = 0;
}

void at_command_note_rx(ConfigTransport t) {
  g_last_rx_transport = t;
  if (g_mode == UartAppMode::At) {
    g_session_transport = t;
  }
}

void at_command_poll() {
  if (g_mode == UartAppMode::At && g_session_last_ms != 0 &&
      (millis() - g_session_last_ms) >= kSessionTimeoutMs) {
    at_command_set_mode(UartAppMode::Data);
  }
}

bool at_command_id_matches(const char* id) {
  if (id == nullptr || id[0] == '\0') {
    return false;
  }
  char ap[24];
  snprintf(ap, sizeof(ap), "ROTOR-%s", g_identity.uid);
  if (strcasecmp(id, g_identity.uid) == 0) {
    return true;
  }
  if (strcasecmp(id, ap) == 0) {
    return true;
  }
  return false;
}

static void at_write(const char* s) {
  if (s == nullptr) {
    return;
  }
  if (g_session_transport == ConfigTransport::Udp) {
    config_udp_reply(reinterpret_cast<const uint8_t*>(s), strlen(s), nullptr);
  } else {
    Serial.print(s);
  }
}

static void at_printf(const char* fmt, ...) {
  char buf[384];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  at_write(buf);
}

static void at_reply(const char* s) { at_write(s); }

static void at_reply_uid_prefix() {
  at_printf("@%s:", g_identity.uid);
}

static void touch_session() { g_session_last_ms = millis(); }

static const char* skip_ws(const char* p) {
  while (*p == ' ' || *p == '\t') {
    ++p;
  }
  return p;
}

static bool parse_quoted(const char* p, char* out, size_t out_len) {
  p = skip_ws(p);
  if (*p == '"') {
    ++p;
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < out_len) {
      out[i++] = *p++;
    }
    out[i] = '\0';
    return true;
  }
  size_t i = 0;
  while (*p && *p != '\r' && *p != '\n' && i + 1 < out_len) {
    out[i++] = *p++;
  }
  out[i] = '\0';
  return out[0] != '\0';
}

static void cmd_ok() {
  at_reply_uid_prefix();
  at_reply("OK\r\n");
}

static void cmd_err(const char* code) {
  char buf[64];
  snprintf(buf, sizeof(buf), "ERROR,%s\r\n", code ? code : "FAIL");
  at_reply_uid_prefix();
  at_write(buf);
}

static void handle_at_body(const char* body) {
  // body like INFO? or NAME="x" or WIFIMODE=STA
  if (body == nullptr || body[0] == '\0' || strcmp(body, "AT") == 0) {
    cmd_ok();
    return;
  }

  AppConfig& cfg = app_config_runtime();

  if (strcmp(body, "HELP?") == 0 || strcmp(body, "HELP") == 0) {
    at_reply("AT INFO STATUS UID MAC NAME SAVE REBOOT FACTORY WIFI* NET* BUSADDR PACKET* BRIDGE ECHO EXIT\r\n");
    cmd_ok();
    return;
  }
  if (strcmp(body, "EXIT") == 0) {
    cmd_ok();
    at_command_set_mode(UartAppMode::Data);
    return;
  }
  if (strcmp(body, "INFO?") == 0) {
    char buf[384];
    config_build_info_text(buf, sizeof(buf));
    at_write(buf);
    cmd_ok();
    return;
  }
  if (strcmp(body, "STATUS?") == 0) {
    char buf[256];
    config_build_status_text(buf, sizeof(buf));
    at_write(buf);
    cmd_ok();
    return;
  }
  if (strcmp(body, "UID?") == 0) {
    at_printf("+UID:%s\r\n", g_identity.uid);
    cmd_ok();
    return;
  }
  if (strcmp(body, "MAC?") == 0) {
    at_printf("+MAC:%s\r\n", g_identity.mac_str);
    cmd_ok();
    return;
  }
  if (strcmp(body, "NAME?") == 0) {
    at_printf("+NAME:%s\r\n", cfg.device_name);
    cmd_ok();
    return;
  }
  if (strncmp(body, "NAME=", 5) == 0) {
    char name[kNameMax];
    if (!parse_quoted(body + 5, name, sizeof(name))) {
      cmd_err("INVALID_PARAMETER");
      return;
    }
    strncpy(cfg.device_name, name, sizeof(cfg.device_name) - 1);
    cfg.device_name[sizeof(cfg.device_name) - 1] = '\0';
    cmd_ok();
    return;
  }
  if (strcmp(body, "SAVE") == 0) {
    app_config_save(cfg);
    cmd_ok();
    return;
  }
  if (strcmp(body, "REBOOT") == 0) {
    cmd_ok();
    delay(50);
    ESP.restart();
    return;
  }
  if (strcmp(body, "FACTORY") == 0) {
    app_config_factory_reset(&cfg, g_identity);
    cmd_ok();
    delay(50);
    ESP.restart();
    return;
  }

  // WiFi
  if (strcmp(body, "WIFIMODE?") == 0) {
    const char* m = cfg.wifi_mode == WifiMode::AP ? "AP" : (cfg.wifi_mode == WifiMode::STA ? "STA" : "APSTA");
    at_printf("+WIFIMODE:%s\r\n", m);
    cmd_ok();
    return;
  }
  if (strncmp(body, "WIFIMODE=", 9) == 0) {
    const char* v = body + 9;
    if (strcmp(v, "AP") == 0) {
      cfg.wifi_mode = WifiMode::AP;
    } else if (strcmp(v, "STA") == 0) {
      cfg.wifi_mode = WifiMode::STA;
    } else if (strcmp(v, "APSTA") == 0) {
      cfg.wifi_mode = WifiMode::APSTA;
    } else {
      cmd_err("INVALID_PARAMETER");
      return;
    }
    wifi_manager_apply_runtime();
    cmd_ok();
    return;
  }
  if (strcmp(body, "WIFIBAND?") == 0) {
    const char* b = cfg.wifi_band == WifiBand::BAND_2G ? "2G" : (cfg.wifi_band == WifiBand::BAND_5G ? "5G" : "AUTO");
    at_printf("+WIFIBAND:%s\r\n", b);
    cmd_ok();
    return;
  }
  if (strncmp(body, "WIFIBAND=", 9) == 0) {
    const char* v = body + 9;
    if (strcmp(v, "AUTO") == 0) {
      cfg.wifi_band = WifiBand::AUTO;
    } else if (strcmp(v, "2G") == 0) {
      cfg.wifi_band = WifiBand::BAND_2G;
    } else if (strcmp(v, "5G") == 0) {
      cfg.wifi_band = WifiBand::BAND_5G;
    } else {
      cmd_err("INVALID_PARAMETER");
      return;
    }
    wifi_manager_apply_runtime();
    cmd_ok();
    return;
  }
  if (strcmp(body, "SSID?") == 0) {
    at_printf("+SSID:%s\r\n", cfg.wifi_ssid);
    cmd_ok();
    return;
  }
  if (strncmp(body, "SSID=", 5) == 0) {
    if (!parse_quoted(body + 5, cfg.wifi_ssid, sizeof(cfg.wifi_ssid))) {
      cmd_err("INVALID_PARAMETER");
      return;
    }
    cmd_ok();
    return;
  }
  if (strncmp(body, "PASS=", 5) == 0) {
    if (!parse_quoted(body + 5, cfg.wifi_pass, sizeof(cfg.wifi_pass))) {
      cmd_err("INVALID_PARAMETER");
      return;
    }
    cmd_ok();
    return;
  }
  if (strncmp(body, "DHCP=", 5) == 0) {
    cfg.wifi_dhcp = (body[5] != '0');
    cmd_ok();
    return;
  }
  if (strncmp(body, "IP=", 3) == 0) {
    parse_quoted(body + 3, cfg.wifi_ip, sizeof(cfg.wifi_ip));
    cmd_ok();
    return;
  }
  if (strncmp(body, "MASK=", 5) == 0) {
    parse_quoted(body + 5, cfg.wifi_mask, sizeof(cfg.wifi_mask));
    cmd_ok();
    return;
  }
  if (strncmp(body, "GW=", 3) == 0) {
    parse_quoted(body + 3, cfg.wifi_gw, sizeof(cfg.wifi_gw));
    cmd_ok();
    return;
  }
  if (strncmp(body, "DNS=", 4) == 0) {
    parse_quoted(body + 4, cfg.wifi_dns, sizeof(cfg.wifi_dns));
    cmd_ok();
    return;
  }
  if (strcmp(body, "SCAN") == 0) {
    const int n = wifi_manager_scan(false, static_cast<uint8_t>(cfg.wifi_band));
    at_printf("+SCAN:%d\r\n", n);
    for (int i = 0; i < n && i < 20; ++i) {
      at_printf("+AP:%s,%d\r\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
    }
    WiFi.scanDelete();
    cmd_ok();
    return;
  }

  // Net
  if (strcmp(body, "NETMODE?") == 0) {
    const char* m = "DISABLED";
    switch (cfg.net_mode) {
      case NetMode::TCP_SERVER:
        m = "TCP_SERVER";
        break;
      case NetMode::TCP_CLIENT:
        m = "TCP_CLIENT";
        break;
      case NetMode::UDP_SERVER:
        m = "UDP_SERVER";
        break;
      case NetMode::UDP_CLIENT:
        m = "UDP_CLIENT";
        break;
      default:
        break;
    }
    at_printf("+NETMODE:%s\r\n", m);
    cmd_ok();
    return;
  }
  if (strncmp(body, "NETMODE=", 8) == 0) {
    const char* v = body + 8;
    if (strcmp(v, "TCP_SERVER") == 0) {
      cfg.net_mode = NetMode::TCP_SERVER;
    } else if (strcmp(v, "TCP_CLIENT") == 0) {
      cfg.net_mode = NetMode::TCP_CLIENT;
    } else if (strcmp(v, "DISABLED") == 0) {
      cfg.net_mode = NetMode::NET_OFF;
    } else if (strcmp(v, "UDP_SERVER") == 0) {
      cfg.net_mode = NetMode::UDP_SERVER;
    } else if (strcmp(v, "UDP_CLIENT") == 0) {
      cfg.net_mode = NetMode::UDP_CLIENT;
    } else {
      cmd_err("INVALID_PARAMETER");
      return;
    }
    cmd_ok();
    return;
  }
  if (strncmp(body, "LOCALPORT=", 10) == 0) {
    cfg.local_port = static_cast<uint16_t>(atoi(body + 10));
    cmd_ok();
    return;
  }
  if (strncmp(body, "REMOTEIP=", 9) == 0) {
    parse_quoted(body + 9, cfg.remote_ip, sizeof(cfg.remote_ip));
    cmd_ok();
    return;
  }
  if (strncmp(body, "REMOTEHOST=", 11) == 0) {
    parse_quoted(body + 11, cfg.remote_host, sizeof(cfg.remote_host));
    cmd_ok();
    return;
  }
  if (strncmp(body, "REMOTEPORT=", 11) == 0) {
    cfg.remote_port = static_cast<uint16_t>(atoi(body + 11));
    cmd_ok();
    return;
  }
  if (strncmp(body, "RECONNECT=", 10) == 0) {
    cfg.reconnect_ms = static_cast<uint32_t>(atoi(body + 10));
    cmd_ok();
    return;
  }

  // RS485
  if (strcmp(body, "BAUD?") == 0) {
    at_printf("+BAUD:%u,FIXED\r\n", cfg.rs485_baud);
    cmd_ok();
    return;
  }
  if (strcmp(body, "BUSADDR?") == 0) {
    at_printf("+BUSADDR:%u\r\n", cfg.bus_address);
    cmd_ok();
    return;
  }
  if (strncmp(body, "BUSADDR=", 8) == 0) {
    int v = atoi(body + 8);
    if (v < 1 || v > 247) {
      cmd_err("INVALID_PARAMETER");
      return;
    }
    cfg.bus_address = static_cast<uint8_t>(v);
    cmd_ok();
    return;
  }
  if (strcmp(body, "PACKETTIME?") == 0) {
    at_printf("+PACKETTIME:%u\r\n", cfg.packet_timeout_ms);
    cmd_ok();
    return;
  }
  if (strncmp(body, "PACKETTIME=", 11) == 0) {
    cfg.packet_timeout_ms = static_cast<uint16_t>(atoi(body + 11));
    cmd_ok();
    return;
  }
  if (strcmp(body, "PACKETSIZE?") == 0) {
    at_printf("+PACKETSIZE:%u\r\n", cfg.packet_size);
    cmd_ok();
    return;
  }
  if (strncmp(body, "PACKETSIZE=", 11) == 0) {
    cfg.packet_size = static_cast<uint16_t>(atoi(body + 11));
    cmd_ok();
    return;
  }
  if (strcmp(body, "ECHO?") == 0) {
    at_printf("+ECHO:%u\r\n", cfg.echo_suppress ? 1 : 0);
    cmd_ok();
    return;
  }
  if (strncmp(body, "ECHO=", 5) == 0) {
    cfg.echo_suppress = (body[5] != '0');
    cmd_ok();
    return;
  }
  if (strcmp(body, "BRIDGE?") == 0) {
    at_printf("+BRIDGE:%u\r\n", cfg.bridge_enabled ? 1 : 0);
    cmd_ok();
    return;
  }
  if (strncmp(body, "BRIDGE=", 7) == 0) {
    cfg.bridge_enabled = (body[7] != '0');
    cmd_ok();
    return;
  }

  cmd_err("UNKNOWN_COMMAND");
}

static void handle_line(char* line) {
  // trim CR/LF already
  touch_session();

  // AT@UID+CMD or AT+CMD or AT
  if (strncmp(line, "AT@", 3) == 0) {
    char* plus = strchr(line + 3, '+');
    if (plus == nullptr) {
      cmd_err("INVALID_PARAMETER");
      return;
    }
    *plus = '\0';
    const char* id = line + 3;
    if (!at_command_id_matches(id)) {
      return; // silent ignore — other device
    }
    handle_at_body(plus + 1);
    return;
  }
  if (strncmp(line, "AT+", 3) == 0) {
    if (g_mode != UartAppMode::At) {
      return;
    }
    handle_at_body(line + 3);
    return;
  }
  if (strcmp(line, "AT") == 0) {
    if (g_mode != UartAppMode::At) {
      return;
    }
    handle_at_body("AT");
    return;
  }
}

size_t at_command_feed(const uint8_t* data, size_t len) {
  if (data == nullptr || len == 0) {
    return 0;
  }
  for (size_t i = 0; i < len; ++i) {
    const char c = static_cast<char>(data[i]);
    if (c == '\r' || c == '\n') {
      if (g_line_len > 0) {
        g_line[g_line_len] = '\0';
        handle_line(g_line);
        g_line_len = 0;
      }
      continue;
    }
    if (g_line_len + 1 < sizeof(g_line)) {
      g_line[g_line_len++] = c;
    } else {
      g_line_len = 0; // overflow reset
    }
  }
  return len;
}

size_t at_command_watch_data(const uint8_t* data, size_t len) {
  // Collect toward +++CFG:<id>\r
  static const char kPrefix[] = "+++CFG:";
  for (size_t i = 0; i < len; ++i) {
    const char c = static_cast<char>(data[i]);
    if (g_esc_len < strlen(kPrefix)) {
      if (c == kPrefix[g_esc_len]) {
        g_esc[g_esc_len++] = c;
      } else {
        g_esc_len = (c == '+') ? 1 : 0;
        if (g_esc_len) {
          g_esc[0] = '+';
        }
      }
      continue;
    }
    // after +++CFG:
    if (c == '\r' || c == '\n') {
      g_esc[g_esc_len] = '\0';
      const char* id = g_esc + strlen(kPrefix);
      const size_t consumed = i + 1;
      g_esc_len = 0;
      if (at_command_id_matches(id)) {
        at_command_set_mode(UartAppMode::At);
        at_reply_uid_prefix();
        at_reply("CONFIG,READY\r\n");
        return consumed;
      }
      return 0;
    }
    if (g_esc_len + 1 < sizeof(g_esc)) {
      g_esc[g_esc_len++] = c;
    } else {
      g_esc_len = 0;
    }
  }
  return 0;
}
