#include "web_server.h"

#include "Version.h"
#include "app_config.h"
#include "device_identity.h"
#include "network_bridge.h"
#include "system_monitor.h"
#include "web_content.h"
#include "wifi_manager.h"
#include "config_ingress.h"

#include <WebServer.h>
#include <WiFi.h>
#include <esp_system.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern DeviceIdentity g_identity;

static WebServer* g_web = nullptr;

static const char* wifi_mode_name(WifiMode m) {
  switch (m) {
    case WifiMode::AP:
      return "SoftAP";
    case WifiMode::STA:
      return "Infrastruktur";
    case WifiMode::APSTA:
      return "Infrastruktur"; // Legacy
    default:
      return "?";
  }
}

static const char* wifi_band_name(WifiBand b) {
  switch (b) {
    case WifiBand::BAND_2G:
      return "2.4 GHz";
    case WifiBand::BAND_5G:
      return "5 GHz";
    default:
      return "Auto";
  }
}

static String json_escape(const char* s) {
  String out;
  if (!s) {
    return out;
  }
  for (const char* p = s; *p; ++p) {
    if (*p == '"' || *p == '\\') {
      out += '\\';
    }
    if (*p == '\n' || *p == '\r' || *p == '\t') {
      continue;
    }
    out += *p;
  }
  return out;
}

static bool json_get_string(const String& body, const char* key, char* out, size_t out_len) {
  if (out_len == 0) {
    return false;
  }
  out[0] = '\0';
  String pat = String("\"") + key + "\":\"";
  int i = body.indexOf(pat);
  if (i < 0) {
    return false;
  }
  i += pat.length();
  String val;
  while (i < static_cast<int>(body.length())) {
    char c = body[i++];
    if (c == '\\' && i < static_cast<int>(body.length())) {
      val += body[i++];
      continue;
    }
    if (c == '"') {
      break;
    }
    val += c;
  }
  strncpy(out, val.c_str(), out_len - 1);
  out[out_len - 1] = '\0';
  return true;
}

static bool json_get_int(const String& body, const char* key, int* out) {
  String pat = String("\"") + key + "\":";
  int i = body.indexOf(pat);
  if (i < 0) {
    return false;
  }
  i += pat.length();
  while (i < static_cast<int>(body.length()) && (body[i] == ' ')) {
    ++i;
  }
  *out = atoi(body.c_str() + i);
  return true;
}

static bool json_get_bool(const String& body, const char* key, bool* out) {
  String pat = String("\"") + key + "\":";
  int i = body.indexOf(pat);
  if (i < 0) {
    return false;
  }
  i += pat.length();
  while (i < static_cast<int>(body.length()) && body[i] == ' ') {
    ++i;
  }
  if (body.startsWith("true", i)) {
    *out = true;
    return true;
  }
  if (body.startsWith("false", i)) {
    *out = false;
    return true;
  }
  return false;
}

static String json_u64(uint64_t v) {
  char buf[24];
  snprintf(buf, sizeof(buf), "%" PRIu64, v);
  return String(buf);
}

static bool require_auth() {
  const AppConfig& cfg = app_config_runtime_const();
  const char* pass = cfg.web_pass[0] ? cfg.web_pass : "Rotorconfig";
  if (!g_web->authenticate("admin", pass)) {
    g_web->requestAuthentication(BASIC_AUTH, "ROTOR Config");
    return false;
  }
  return true;
}

static void handle_root() {
  if (!require_auth()) {
    return;
  }
  g_web->send_P(200, "text/html; charset=utf-8", WEB_INDEX_HTML);
}

static void handle_status() {
  if (!require_auth()) {
    return;
  }
  const AppConfig& cfg = app_config_runtime_const();
  const SystemStats& st = system_monitor_stats();
  char sta_ip[16] = {0};
  char ap_ip[16] = {0};
  char ap_ssid[32] = {0};
  wifi_manager_get_sta_ip(sta_ip, sizeof(sta_ip));
  wifi_manager_get_ap_ip(ap_ip, sizeof(ap_ip));
  wifi_manager_get_ap_ssid(ap_ssid, sizeof(ap_ssid));

  const bool sta = wifi_manager_sta_connected();
  const bool wifi_up = wifi_manager_has_ip();
  String link = "WLAN aus";
  if (sta) {
    link = "Infrastruktur";
  } else if ((WiFi.getMode() & WIFI_AP) != 0) {
    link = "SoftAP";
  }

  const unsigned long up_s =
      static_cast<unsigned long>((int32_t)(millis() - st.boot_ms) / 1000);

  String j = "{";
  j += "\"device_name\":\"" + json_escape(cfg.device_name) + "\",";
  j += "\"uid\":\"" + json_escape(g_identity.uid) + "\",";
  j += "\"mac\":\"" + json_escape(g_identity.mac_str) + "\",";
  j += "\"bus_address\":" + String(cfg.bus_address) + ",";
  j += "\"fw\":\"" FW_VERSION_STR "\",";
  j += "\"hw\":\"" HW_VERSION_STR "\",";
  j += "\"uptime_s\":" + String(up_s) + ",";
  j += "\"reset_reason\":" + String(st.reset_reason) + ",";
  j += "\"wifi_up\":" + String(wifi_up ? "true" : "false") + ",";
  j += "\"link_text\":\"" + json_escape(link.c_str()) + "\",";
  j += "\"wifi_mode_name\":\"" + String(wifi_mode_name(cfg.wifi_mode)) + "\",";
  j += "\"wifi_band_name\":\"" + String(wifi_band_name(cfg.wifi_band)) + "\",";
  j += "\"sta_ssid\":\"" + json_escape(cfg.wifi_ssid) + "\",";
  j += "\"rssi\":" + String(wifi_manager_rssi()) + ",";
  j += "\"sta_ip\":\"" + json_escape(sta_ip) + "\",";
  j += "\"ap_ssid\":\"" + json_escape(ap_ssid) + "\",";
  j += "\"ap_ip\":\"" + json_escape(ap_ip) + "\",";
  j += "\"tcp_connected\":" + String(network_bridge_link_up() ? "true" : "false") + ",";
  j += "\"net_mode\":" + String(static_cast<int>(cfg.net_mode)) + ",";
  j += "\"net_mode_name\":\"" + String(network_bridge_mode_name()) + "\",";
  j += "\"local_port\":" + String(cfg.local_port) + ",";
  j += "\"remote_ip\":\"" + json_escape(cfg.remote_ip) + "\",";
  j += "\"remote_port\":" + String(cfg.remote_port) + ",";
  j += "\"bridge\":" + String(cfg.bridge_enabled ? "true" : "false") + ",";
  j += "\"rs485_tx_allowed\":" + String(cfg.rs485_tx_allowed ? "true" : "false") + ",";
  j += "\"rs485_rx_allowed\":" + String(cfg.rs485_rx_allowed ? "true" : "false") + ",";
  j += "\"packet_timeout_ms\":" + String(cfg.packet_timeout_ms) + ",";
  j += "\"packet_size\":" + String(cfg.packet_size) + ",";
  j += "\"rs485_rx\":" + json_u64(st.rs485_rx_bytes) + ",";
  j += "\"rs485_tx\":" + json_u64(st.rs485_tx_bytes) + ",";
  j += "\"net_rx\":" + json_u64(st.net_rx_bytes) + ",";
  j += "\"net_tx\":" + json_u64(st.net_tx_bytes) + ",";
  j += "\"net_tx_drops\":" + json_u64(st.net_tx_drops) + ",";
  j += "\"net_rx_drops\":" + json_u64(st.net_rx_drops) + ",";
  j += "\"discovery_udp_port\":" + String(config_discovery_udp_port()) + ",";
  j += "\"config_session\":" + String(config_session_active() ? "true" : "false") + ",";
  j += "\"free_heap\":" + String(ESP.getFreeHeap());
  j += "}";
  g_web->send(200, "application/json", j);
}

static void handle_config_get() {
  if (!require_auth()) {
    return;
  }
  const AppConfig& cfg = app_config_runtime_const();
  String j = "{";
  j += "\"wifi_mode\":" + String(static_cast<int>(cfg.wifi_mode)) + ",";
  j += "\"wifi_band\":" + String(static_cast<int>(cfg.wifi_band)) + ",";
  j += "\"wifi_ssid\":\"" + json_escape(cfg.wifi_ssid) + "\",";
  j += "\"has_password\":" + String(cfg.wifi_pass[0] ? "true" : "false") + ",";
  j += "\"wifi_dhcp\":" + String(cfg.wifi_dhcp ? "true" : "false") + ",";
  j += "\"wifi_ip\":\"" + json_escape(cfg.wifi_ip) + "\",";
  j += "\"wifi_mask\":\"" + json_escape(cfg.wifi_mask) + "\",";
  j += "\"wifi_gw\":\"" + json_escape(cfg.wifi_gw) + "\",";
  j += "\"wifi_dns\":\"" + json_escape(cfg.wifi_dns) + "\",";
  j += "\"net_mode\":" + String(static_cast<int>(cfg.net_mode)) + ",";
  j += "\"local_port\":" + String(cfg.local_port) + ",";
  j += "\"remote_ip\":\"" + json_escape(cfg.remote_ip) + "\",";
  j += "\"remote_host\":\"" + json_escape(cfg.remote_host) + "\",";
  j += "\"remote_port\":" + String(cfg.remote_port) + ",";
  j += "\"reconnect_ms\":" + String(cfg.reconnect_ms) + ",";
  j += "\"tcp_nodelay\":" + String(cfg.tcp_nodelay ? "true" : "false") + ",";
  j += "\"tcp_keepalive\":" + String(cfg.tcp_keepalive ? "true" : "false") + ",";
  j += "\"rs485_baud\":" + String(cfg.rs485_baud) + ",";
  j += "\"packet_timeout_ms\":" + String(cfg.packet_timeout_ms) + ",";
  j += "\"packet_size\":" + String(cfg.packet_size) + ",";
  j += "\"delimiter\":" + String(static_cast<int>(cfg.delimiter)) + ",";
  j += "\"delimiter_custom\":" + String(cfg.delimiter_custom) + ",";
  j += "\"echo_suppress\":" + String(cfg.echo_suppress ? "true" : "false") + ",";
  j += "\"bridge_enabled\":" + String(cfg.bridge_enabled ? "true" : "false") + ",";
  j += "\"rs485_tx_allowed\":" + String(cfg.rs485_tx_allowed ? "true" : "false") + ",";
  j += "\"rs485_rx_allowed\":" + String(cfg.rs485_rx_allowed ? "true" : "false") + ",";
  j += "\"bus_address\":" + String(cfg.bus_address);
  j += "}";
  g_web->send(200, "application/json", j);
}

static void handle_config_wifi_post() {
  if (!require_auth()) {
    return;
  }
  if (!g_web->hasArg("plain")) {
    g_web->send(400, "application/json", "{\"error\":\"no body\"}");
    return;
  }
  const String body = g_web->arg("plain");
  AppConfig& cfg = app_config_runtime();

  int v = 0;
  if (json_get_int(body, "wifi_mode", &v)) {
    if (v >= 0 && v <= 2) {
      cfg.wifi_mode = static_cast<WifiMode>(v);
    }
  }
  if (json_get_int(body, "wifi_band", &v)) {
    if (v >= 0 && v <= 2) {
      cfg.wifi_band = static_cast<WifiBand>(v);
    }
  }

  char tmp[65];
  if (json_get_string(body, "wifi_ssid", tmp, sizeof(tmp))) {
    strncpy(cfg.wifi_ssid, tmp, sizeof(cfg.wifi_ssid) - 1);
    cfg.wifi_ssid[sizeof(cfg.wifi_ssid) - 1] = '\0';
  }
  if (json_get_string(body, "wifi_pass", tmp, sizeof(tmp))) {
    // leeres Passwort = nicht überschreiben (außer bewusst geleert über Open-Netz)
    if (tmp[0] != '\0') {
      strncpy(cfg.wifi_pass, tmp, sizeof(cfg.wifi_pass) - 1);
      cfg.wifi_pass[sizeof(cfg.wifi_pass) - 1] = '\0';
    }
  }

  bool b = false;
  if (json_get_bool(body, "wifi_dhcp", &b)) {
    cfg.wifi_dhcp = b;
  }
  if (json_get_string(body, "wifi_ip", tmp, sizeof(tmp))) {
    strncpy(cfg.wifi_ip, tmp, sizeof(cfg.wifi_ip) - 1);
  }
  if (json_get_string(body, "wifi_mask", tmp, sizeof(tmp))) {
    strncpy(cfg.wifi_mask, tmp, sizeof(cfg.wifi_mask) - 1);
  }
  if (json_get_string(body, "wifi_gw", tmp, sizeof(tmp))) {
    strncpy(cfg.wifi_gw, tmp, sizeof(cfg.wifi_gw) - 1);
  }
  if (json_get_string(body, "wifi_dns", tmp, sizeof(tmp))) {
    strncpy(cfg.wifi_dns, tmp, sizeof(cfg.wifi_dns) - 1);
  }

  // XOR: nie AP+STA. connect_sta + SSID → Infrastruktur; Modus AP → SoftAP.
  bool connect_sta = false;
  json_get_bool(body, "connect_sta", &connect_sta);
  if (cfg.wifi_mode == WifiMode::APSTA) {
    cfg.wifi_mode = WifiMode::STA;
  }
  if (connect_sta && cfg.wifi_ssid[0] != '\0') {
    cfg.wifi_mode = WifiMode::STA;
  } else if (cfg.wifi_mode == WifiMode::STA && cfg.wifi_ssid[0] == '\0') {
    cfg.wifi_mode = WifiMode::AP;
  }

  // Vor Moduswechsel speichern — sonst bricht SoftAP ab und /api/save kommt nicht mehr an.
  if (!app_config_save(cfg)) {
    g_web->send(500, "application/json", "{\"error\":\"save failed\"}");
    return;
  }

  wifi_manager_apply_runtime();
  g_web->send(200, "application/json", "{\"ok\":true}");
}

static void handle_config_rs485_post() {
  if (!require_auth()) {
    return;
  }
  if (!g_web->hasArg("plain")) {
    g_web->send(400, "application/json", "{\"error\":\"no body\"}");
    return;
  }
  const String body = g_web->arg("plain");
  AppConfig& cfg = app_config_runtime();
  int v = 0;
  char tmp[65];
  bool b = false;

  if (json_get_int(body, "net_mode", &v) && v >= 0 && v <= 4) {
    cfg.net_mode = static_cast<NetMode>(v);
  }
  if (json_get_int(body, "local_port", &v) && v > 0 && v <= 65535) {
    cfg.local_port = static_cast<uint16_t>(v);
  }
  if (json_get_string(body, "remote_ip", tmp, sizeof(tmp))) {
    strncpy(cfg.remote_ip, tmp, sizeof(cfg.remote_ip) - 1);
  }
  if (json_get_string(body, "remote_host", tmp, sizeof(tmp))) {
    strncpy(cfg.remote_host, tmp, sizeof(cfg.remote_host) - 1);
  }
  if (json_get_int(body, "remote_port", &v) && v > 0 && v <= 65535) {
    cfg.remote_port = static_cast<uint16_t>(v);
  }
  if (json_get_int(body, "reconnect_ms", &v) && v >= 500) {
    cfg.reconnect_ms = static_cast<uint32_t>(v);
  }
  if (json_get_bool(body, "tcp_nodelay", &b)) {
    cfg.tcp_nodelay = b;
  }
  if (json_get_bool(body, "tcp_keepalive", &b)) {
    cfg.tcp_keepalive = b;
  }
  if (json_get_int(body, "packet_timeout_ms", &v)) {
    cfg.packet_timeout_ms = static_cast<uint16_t>(v);
  }
  if (json_get_int(body, "packet_size", &v)) {
    cfg.packet_size = static_cast<uint16_t>(v);
  }
  if (json_get_int(body, "delimiter", &v) && v >= 0 && v <= 3) {
    cfg.delimiter = static_cast<PacketDelimiter>(v);
  }
  if (json_get_int(body, "delimiter_custom", &v)) {
    cfg.delimiter_custom = static_cast<uint8_t>(v & 0xFF);
  }
  if (json_get_bool(body, "echo_suppress", &b)) {
    (void)b;
  }
  // Transparente Bridge: Echo-Filter bewusst deaktiviert
  cfg.echo_suppress = false;
  if (json_get_bool(body, "bridge_enabled", &b)) {
    cfg.bridge_enabled = b;
  }
  if (json_get_bool(body, "rs485_tx_allowed", &b)) {
    cfg.rs485_tx_allowed = b;
  }
  if (json_get_bool(body, "rs485_rx_allowed", &b)) {
    cfg.rs485_rx_allowed = b;
  }
  if (json_get_int(body, "bus_address", &v) && v >= 1 && v <= 247) {
    cfg.bus_address = static_cast<uint8_t>(v);
  }

  // Clamp wie in app_config
  if (cfg.packet_timeout_ms < 1) {
    cfg.packet_timeout_ms = 1;
  }
  if (cfg.packet_timeout_ms > 100) {
    cfg.packet_timeout_ms = 100;
  }
  if (cfg.packet_size < 32) {
    cfg.packet_size = 32;
  }
  if (cfg.packet_size > 1460) {
    cfg.packet_size = 1460;
  }

  g_web->send(200, "application/json", "{\"ok\":true}");
}

static void handle_save() {
  if (!require_auth()) {
    return;
  }
  if (!app_config_save(app_config_runtime_const())) {
    g_web->send(500, "application/json", "{\"error\":\"save failed\"}");
    return;
  }
  g_web->send(200, "application/json", "{\"ok\":true}");
}

static void handle_webpass_post() {
  if (!require_auth()) {
    return;
  }
  if (!g_web->hasArg("plain")) {
    g_web->send(400, "application/json", "{\"error\":\"no body\"}");
    return;
  }
  const String body = g_web->arg("plain");
  AppConfig& cfg = app_config_runtime();
  char current[65] = {0};
  char next[65] = {0};
  if (!json_get_string(body, "current", current, sizeof(current)) ||
      !json_get_string(body, "next", next, sizeof(next))) {
    g_web->send(400, "application/json", "{\"error\":\"missing fields\"}");
    return;
  }
  if (strcmp(current, cfg.web_pass) != 0) {
    g_web->send(403, "application/json", "{\"error\":\"wrong password\"}");
    return;
  }
  if (next[0] == '\0' || strlen(next) > 64) {
    g_web->send(400, "application/json", "{\"error\":\"invalid password\"}");
    return;
  }
  strncpy(cfg.web_pass, next, sizeof(cfg.web_pass) - 1);
  cfg.web_pass[sizeof(cfg.web_pass) - 1] = '\0';
  if (!app_config_save(cfg)) {
    g_web->send(500, "application/json", "{\"error\":\"save failed\"}");
    return;
  }
  g_web->send(200, "application/json", "{\"ok\":true}");
}

static void handle_scan() {
  if (!require_auth()) {
    return;
  }
  int band = static_cast<int>(app_config_runtime_const().wifi_band);
  if (g_web->hasArg("band")) {
    band = g_web->arg("band").toInt();
  }
  if (band < 0 || band > 2) {
    band = 0;
  }

  const int n = wifi_manager_scan(false, static_cast<uint8_t>(band));

  String j = "{";
  j += "\"band\":" + String(band) + ",";
  j += "\"may_disconnect\":false,";
  j += "\"networks\":[";
  bool first = true;
  for (int i = 0; i < n; ++i) {
    const int ch = WiFi.channel(i);
    const bool is5 = ch > 14;
    // Bei reinem 2.4-Scan 5G-Einträge verwerfen; bei 5G-Scan 2.4 verwerfen
    if (band == 1 && is5) {
      continue;
    }
    if (band == 2 && !is5) {
      continue;
    }
    if (!first) {
      j += ",";
    }
    first = false;
    j += "{";
    j += "\"ssid\":\"" + json_escape(WiFi.SSID(i).c_str()) + "\",";
    j += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
    j += "\"channel\":" + String(ch) + ",";
    j += "\"band\":\"" + String(is5 ? "5G" : "2G") + "\",";
    j += "\"band_code\":" + String(is5 ? 2 : 1) + ",";
    j += "\"secure\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false");
    j += "}";
  }
  j += "]}";
  WiFi.scanDelete();
  g_web->send(200, "application/json", j);
}

static void handle_not_found() {
  g_web->send(404, "text/plain", "Not found");
}

void web_server_begin() {
  g_web = new WebServer(80);
  g_web->on("/", HTTP_GET, handle_root);
  g_web->on("/api/status", HTTP_GET, handle_status);
  g_web->on("/api/config", HTTP_GET, handle_config_get);
  g_web->on("/api/config/wifi", HTTP_POST, handle_config_wifi_post);
  g_web->on("/api/config/rs485", HTTP_POST, handle_config_rs485_post);
  g_web->on("/api/config/webpass", HTTP_POST, handle_webpass_post);
  g_web->on("/api/save", HTTP_POST, handle_save);
  g_web->on("/api/wifi/scan", HTTP_GET, handle_scan);
  g_web->onNotFound(handle_not_found);
  g_web->begin();
}

static void web_server_task(void* /*arg*/) {
  for (;;) {
    if (g_web) {
      g_web->handleClient();
    }
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

void web_server_start_task() {
  xTaskCreate(web_server_task, "web", 8192, nullptr, 3, nullptr);
}
