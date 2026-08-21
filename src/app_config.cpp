#include "app_config.h"

#include <Preferences.h>
#include <stdio.h>
#include <string.h>

static Preferences g_prefs;
static AppConfig g_runtime;

static void clamp_packet_params(AppConfig* cfg) {
  if (cfg->packet_timeout_ms < 1) {
    cfg->packet_timeout_ms = 1;
  }
  if (cfg->packet_timeout_ms > 100) {
    cfg->packet_timeout_ms = 100;
  }
  if (cfg->packet_size < 32) {
    cfg->packet_size = 32;
  }
  if (cfg->packet_size > 1460) {
    cfg->packet_size = 1460;
  }
  if (cfg->bus_address < 1) {
    cfg->bus_address = 1;
  }
  if (cfg->bus_address > 247) {
    cfg->bus_address = 247;
  }
}

void app_config_set_factory_defaults(AppConfig* cfg, const DeviceIdentity& id) {
  memset(cfg, 0, sizeof(*cfg));
  cfg->schema_version = kConfigSchemaVersion;

  snprintf(cfg->device_name, sizeof(cfg->device_name), "ROTOR-WIFI-%s", id.uid);
  cfg->bus_address = device_identity_default_bus_addr(id);

  cfg->wifi_mode = WifiMode::AP;
  cfg->wifi_band = WifiBand::AUTO;
  cfg->wifi_ssid[0] = '\0';
  cfg->wifi_pass[0] = '\0';
  cfg->wifi_dhcp = true;
  strncpy(cfg->wifi_ip, "192.168.1.80", sizeof(cfg->wifi_ip) - 1);
  strncpy(cfg->wifi_mask, "255.255.255.0", sizeof(cfg->wifi_mask) - 1);
  strncpy(cfg->wifi_gw, "192.168.1.1", sizeof(cfg->wifi_gw) - 1);
  strncpy(cfg->wifi_dns, "192.168.1.1", sizeof(cfg->wifi_dns) - 1);
  cfg->fallback_ap = true;
  cfg->sta_timeout_ms = 60000;

  cfg->net_mode = NetMode::TCP_SERVER;
  cfg->local_port = 8886;
  strncpy(cfg->remote_ip, "192.168.1.100", sizeof(cfg->remote_ip) - 1);
  cfg->remote_host[0] = '\0';
  cfg->remote_port = 8886;
  cfg->reconnect_ms = 5000;
  cfg->tcp_timeout_ms = 60000;
  cfg->tcp_nodelay = true;
  cfg->tcp_keepalive = true;

  cfg->rs485_baud = 115200;
  cfg->packet_timeout_ms = 2;
  cfg->packet_size = 1024;
  cfg->delimiter = PacketDelimiter::NONE;
  cfg->delimiter_custom = 0;
  cfg->echo_suppress = true;
  cfg->bridge_enabled = true;
  cfg->rs485_tx_allowed = true;
  cfg->rs485_rx_allowed = true;
}

bool app_config_load(AppConfig* cfg, const DeviceIdentity& id) {
  app_config_set_factory_defaults(cfg, id);

  if (!g_prefs.begin("w2r485", true)) {
    return false;
  }

  const uint16_t ver = g_prefs.getUShort("schema", 0);
  if (ver != kConfigSchemaVersion) {
    g_prefs.end();
    return false;
  }

  cfg->schema_version = ver;
  g_prefs.getString("name", cfg->device_name, sizeof(cfg->device_name));
  cfg->bus_address = g_prefs.getUChar("bus", cfg->bus_address);
  cfg->wifi_mode = static_cast<WifiMode>(g_prefs.getUChar("wmode", static_cast<uint8_t>(cfg->wifi_mode)));
  cfg->wifi_band = static_cast<WifiBand>(g_prefs.getUChar("wband", static_cast<uint8_t>(cfg->wifi_band)));
  g_prefs.getString("ssid", cfg->wifi_ssid, sizeof(cfg->wifi_ssid));
  g_prefs.getString("pass", cfg->wifi_pass, sizeof(cfg->wifi_pass));
  cfg->wifi_dhcp = g_prefs.getBool("dhcp", cfg->wifi_dhcp);
  g_prefs.getString("ip", cfg->wifi_ip, sizeof(cfg->wifi_ip));
  g_prefs.getString("mask", cfg->wifi_mask, sizeof(cfg->wifi_mask));
  g_prefs.getString("gw", cfg->wifi_gw, sizeof(cfg->wifi_gw));
  g_prefs.getString("dns", cfg->wifi_dns, sizeof(cfg->wifi_dns));
  cfg->fallback_ap = g_prefs.getBool("fbap", cfg->fallback_ap);
  cfg->sta_timeout_ms = g_prefs.getUInt("statimo", cfg->sta_timeout_ms);

  cfg->net_mode = static_cast<NetMode>(g_prefs.getUChar("nmode", static_cast<uint8_t>(cfg->net_mode)));
  cfg->local_port = g_prefs.getUShort("lport", cfg->local_port);
  g_prefs.getString("rip", cfg->remote_ip, sizeof(cfg->remote_ip));
  g_prefs.getString("rhost", cfg->remote_host, sizeof(cfg->remote_host));
  cfg->remote_port = g_prefs.getUShort("rport", cfg->remote_port);
  cfg->reconnect_ms = g_prefs.getUInt("recon", cfg->reconnect_ms);
  cfg->tcp_timeout_ms = g_prefs.getUInt("tcpto", cfg->tcp_timeout_ms);
  cfg->tcp_nodelay = g_prefs.getBool("nodelay", cfg->tcp_nodelay);
  cfg->tcp_keepalive = g_prefs.getBool("keepalive", cfg->tcp_keepalive);

  cfg->packet_timeout_ms = g_prefs.getUShort("pktto", cfg->packet_timeout_ms);
  cfg->packet_size = g_prefs.getUShort("pktsz", cfg->packet_size);
  cfg->delimiter = static_cast<PacketDelimiter>(
      g_prefs.getUChar("delim", static_cast<uint8_t>(cfg->delimiter)));
  cfg->delimiter_custom = g_prefs.getUChar("delimc", cfg->delimiter_custom);
  cfg->echo_suppress = g_prefs.getBool("echo", cfg->echo_suppress);
  cfg->bridge_enabled = g_prefs.getBool("bridge", cfg->bridge_enabled);
  cfg->rs485_tx_allowed = g_prefs.getBool("txok", cfg->rs485_tx_allowed);
  cfg->rs485_rx_allowed = g_prefs.getBool("rxok", cfg->rs485_rx_allowed);

  g_prefs.end();
  clamp_packet_params(cfg);
  return true;
}

bool app_config_save(const AppConfig& cfg) {
  AppConfig c = cfg;
  clamp_packet_params(&c);

  if (!g_prefs.begin("w2r485", false)) {
    return false;
  }

  g_prefs.putUShort("schema", kConfigSchemaVersion);
  g_prefs.putString("name", c.device_name);
  g_prefs.putUChar("bus", c.bus_address);
  g_prefs.putUChar("wmode", static_cast<uint8_t>(c.wifi_mode));
  g_prefs.putUChar("wband", static_cast<uint8_t>(c.wifi_band));
  g_prefs.putString("ssid", c.wifi_ssid);
  g_prefs.putString("pass", c.wifi_pass);
  g_prefs.putBool("dhcp", c.wifi_dhcp);
  g_prefs.putString("ip", c.wifi_ip);
  g_prefs.putString("mask", c.wifi_mask);
  g_prefs.putString("gw", c.wifi_gw);
  g_prefs.putString("dns", c.wifi_dns);
  g_prefs.putBool("fbap", c.fallback_ap);
  g_prefs.putUInt("statimo", c.sta_timeout_ms);

  g_prefs.putUChar("nmode", static_cast<uint8_t>(c.net_mode));
  g_prefs.putUShort("lport", c.local_port);
  g_prefs.putString("rip", c.remote_ip);
  g_prefs.putString("rhost", c.remote_host);
  g_prefs.putUShort("rport", c.remote_port);
  g_prefs.putUInt("recon", c.reconnect_ms);
  g_prefs.putUInt("tcpto", c.tcp_timeout_ms);
  g_prefs.putBool("nodelay", c.tcp_nodelay);
  g_prefs.putBool("keepalive", c.tcp_keepalive);

  g_prefs.putUShort("pktto", c.packet_timeout_ms);
  g_prefs.putUShort("pktsz", c.packet_size);
  g_prefs.putUChar("delim", static_cast<uint8_t>(c.delimiter));
  g_prefs.putUChar("delimc", c.delimiter_custom);
  g_prefs.putBool("echo", c.echo_suppress);
  g_prefs.putBool("bridge", c.bridge_enabled);
  g_prefs.putBool("txok", c.rs485_tx_allowed);
  g_prefs.putBool("rxok", c.rs485_rx_allowed);

  g_prefs.end();
  g_runtime = c;
  return true;
}

bool app_config_factory_reset(AppConfig* cfg, const DeviceIdentity& id) {
  if (g_prefs.begin("w2r485", false)) {
    g_prefs.clear();
    g_prefs.end();
  }
  app_config_set_factory_defaults(cfg, id);
  g_runtime = *cfg;
  return app_config_save(*cfg);
}

AppConfig& app_config_runtime() { return g_runtime; }
const AppConfig& app_config_runtime_const() { return g_runtime; }

void app_config_init_runtime(const DeviceIdentity& id) {
  if (!app_config_load(&g_runtime, id)) {
    app_config_set_factory_defaults(&g_runtime, id);
    app_config_save(g_runtime);
  }
}
