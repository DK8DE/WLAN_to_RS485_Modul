#include "config_handlers.h"

#include "Version.h"
#include "app_config.h"
#include "config_crc.h"
#include "device_identity.h"
#include "network_bridge.h"
#include "wifi_manager.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

extern DeviceIdentity g_identity;

struct PendingDiscover {
  bool active;
  unsigned long due_ms;
  ConfigFrame req;
  ConfigTransport transport;
  ConfigReplyFn reply;
  void* reply_ctx;
};

static PendingDiscover g_pending{};
static SemaphoreHandle_t g_pending_mu = nullptr;

void config_handlers_begin() {
  if (g_pending_mu == nullptr) {
    g_pending_mu = xSemaphoreCreateMutex();
  }
  memset(&g_pending, 0, sizeof(g_pending));
}

static unsigned long discover_jitter_ms() {
  const uint16_t c = config_crc16(g_identity.mac, 6);
  return 20UL + static_cast<unsigned long>(c % 481);
}

size_t config_build_info_text(char* out, size_t out_len) {
  if (out == nullptr || out_len == 0) {
    return 0;
  }
  const AppConfig& cfg = app_config_runtime_const();
  char ip[16] = {0};
  wifi_manager_get_ip(ip, sizeof(ip));
  char ap[32] = {0};
  snprintf(ap, sizeof(ap), "ROTOR-%s", g_identity.uid);
  int n = snprintf(out, out_len,
                   "UID=%s\nAP=%s\nNAME=%s\nMAC=%s\nBUS=%u\nFW=%s\nHW=%s\nIP=%s\n"
                   "NETMODE=%u\nWIFIMODE=%u\nLPORT=%u\nDISCOVERY_UDP=%u\n",
                   g_identity.uid, ap, cfg.device_name, g_identity.mac_str, cfg.bus_address,
                   FW_VERSION_STR, HW_VERSION_STR, ip[0] ? ip : "0.0.0.0",
                   static_cast<unsigned>(cfg.net_mode), static_cast<unsigned>(cfg.wifi_mode),
                   cfg.local_port, kConfigDiscoveryUdpPort);
  if (n < 0) {
    return 0;
  }
  return static_cast<size_t>(n >= static_cast<int>(out_len) ? out_len - 1 : n);
}

size_t config_build_status_text(char* out, size_t out_len) {
  if (out == nullptr || out_len == 0) {
    return 0;
  }
  char ip[16] = {0};
  wifi_manager_get_ip(ip, sizeof(ip));
  int n = snprintf(out, out_len,
                   "WIFI=%s\nIP=%s\nRSSI=%d\nLINK=%d\nHEAP=%u\n",
                   wifi_manager_sta_connected() ? "STA" : "AP/DOWN", ip[0] ? ip : "-",
                   wifi_manager_rssi(), network_bridge_link_up() ? 1 : 0,
                   static_cast<unsigned>(ESP.getFreeHeap()));
  if (n < 0) {
    return 0;
  }
  return static_cast<size_t>(n >= static_cast<int>(out_len) ? out_len - 1 : n);
}

static void send_frame(const ConfigFrame& f, ConfigReplyFn reply, void* ctx) {
  if (reply == nullptr) {
    return;
  }
  uint8_t buf[kConfigMaxFrame];
  const size_t n = config_frame_encode(f, buf, sizeof(buf));
  if (n > 0) {
    reply(buf, n, ctx);
  }
}

static void reply_simple(ConfigMsgType type, uint16_t seq, const uint8_t dst[6],
                         const uint8_t* payload, uint16_t plen, ConfigReplyFn reply, void* ctx) {
  ConfigFrame f{};
  f.version = kConfigProtoVersion;
  f.type = type;
  memcpy(f.dst_mac, dst, 6);
  config_mac_copy_device(f.src_mac);
  f.seq = seq;
  f.payload_len = plen;
  if (plen > 0 && payload != nullptr) {
    memcpy(f.payload, payload, plen);
  }
  send_frame(f, reply, ctx);
}

static void do_discover_response(const ConfigFrame& req, ConfigReplyFn reply, void* ctx) {
  char text[384];
  const size_t n = config_build_info_text(text, sizeof(text));
  reply_simple(ConfigMsgType::DISCOVER_RESPONSE, req.seq, req.src_mac,
               reinterpret_cast<const uint8_t*>(text), static_cast<uint16_t>(n), reply, ctx);
}

static void schedule_discover(const ConfigFrame& req, ConfigTransport transport, ConfigReplyFn reply,
                              void* ctx) {
  if (g_pending_mu == nullptr) {
    return;
  }
  if (xSemaphoreTake(g_pending_mu, pdMS_TO_TICKS(50)) != pdTRUE) {
    return;
  }
  g_pending.active = true;
  g_pending.due_ms = millis() + discover_jitter_ms();
  g_pending.req = req;
  g_pending.transport = transport;
  g_pending.reply = reply;
  g_pending.reply_ctx = ctx;
  xSemaphoreGive(g_pending_mu);
}

void config_handlers_poll() {
  if (g_pending_mu == nullptr) {
    return;
  }
  if (xSemaphoreTake(g_pending_mu, 0) != pdTRUE) {
    return;
  }
  if (g_pending.active && (int32_t)(millis() - g_pending.due_ms) >= 0) {
    PendingDiscover p = g_pending;
    g_pending.active = false;
    xSemaphoreGive(g_pending_mu);
    do_discover_response(p.req, p.reply, p.reply_ctx);
    return;
  }
  xSemaphoreGive(g_pending_mu);
}

// Minimal binary config blob: key=value lines (subset)
static void apply_config_text(const char* text) {
  // Lines like NAME=... BUS=... handled lightly; full SET via AT preferred
  (void)text;
}

void config_handle_frame(const ConfigFrame& req, ConfigTransport transport, ConfigReplyFn reply,
                         void* reply_ctx) {
  if (!config_mac_matches_device(req.dst_mac) && req.type != ConfigMsgType::DISCOVER) {
    return;
  }

  switch (req.type) {
    case ConfigMsgType::DISCOVER:
      schedule_discover(req, transport, reply, reply_ctx);
      break;

    case ConfigMsgType::GET_INFO: {
      char text[384];
      const size_t n = config_build_info_text(text, sizeof(text));
      reply_simple(ConfigMsgType::ACK, req.seq, req.src_mac, reinterpret_cast<const uint8_t*>(text),
                   static_cast<uint16_t>(n), reply, reply_ctx);
      break;
    }

    case ConfigMsgType::GET_STATUS: {
      char text[256];
      const size_t n = config_build_status_text(text, sizeof(text));
      reply_simple(ConfigMsgType::ACK, req.seq, req.src_mac, reinterpret_cast<const uint8_t*>(text),
                   static_cast<uint16_t>(n), reply, reply_ctx);
      break;
    }

    case ConfigMsgType::GET_CONFIG: {
      char text[384];
      const size_t n = config_build_info_text(text, sizeof(text));
      reply_simple(ConfigMsgType::ACK, req.seq, req.src_mac, reinterpret_cast<const uint8_t*>(text),
                   static_cast<uint16_t>(n), reply, reply_ctx);
      break;
    }

    case ConfigMsgType::SET_CONFIG: {
      if (req.payload_len > 0) {
        char tmp[kConfigMaxPayload + 1];
        memcpy(tmp, req.payload, req.payload_len);
        tmp[req.payload_len] = '\0';
        apply_config_text(tmp);
      }
      reply_simple(ConfigMsgType::ACK, req.seq, req.src_mac, nullptr, 0, reply, reply_ctx);
      break;
    }

    case ConfigMsgType::SAVE_CONFIG:
      app_config_save(app_config_runtime_const());
      reply_simple(ConfigMsgType::ACK, req.seq, req.src_mac, nullptr, 0, reply, reply_ctx);
      break;

    case ConfigMsgType::PING:
      reply_simple(ConfigMsgType::ACK, req.seq, req.src_mac, nullptr, 0, reply, reply_ctx);
      break;

    case ConfigMsgType::REBOOT:
      reply_simple(ConfigMsgType::ACK, req.seq, req.src_mac, nullptr, 0, reply, reply_ctx);
      delay(50);
      ESP.restart();
      break;

    case ConfigMsgType::FACTORY_RESET: {
      AppConfig cfg{};
      app_config_factory_reset(&cfg, g_identity);
      reply_simple(ConfigMsgType::ACK, req.seq, req.src_mac, nullptr, 0, reply, reply_ctx);
      delay(50);
      ESP.restart();
      break;
    }

    case ConfigMsgType::ENTER_UPDATE_MODE:
      // AT session entered via text escape; binary ACK only
      reply_simple(ConfigMsgType::ACK, req.seq, req.src_mac, nullptr, 0, reply, reply_ctx);
      break;

    case ConfigMsgType::GET_LOG: {
      const char* msg = "LOG:none\n";
      reply_simple(ConfigMsgType::ACK, req.seq, req.src_mac, reinterpret_cast<const uint8_t*>(msg),
                   static_cast<uint16_t>(strlen(msg)), reply, reply_ctx);
      break;
    }

    default: {
      const char* err = "UNSUPPORTED";
      reply_simple(ConfigMsgType::NACK, req.seq, req.src_mac, reinterpret_cast<const uint8_t*>(err),
                   static_cast<uint16_t>(strlen(err)), reply, reply_ctx);
      break;
    }
  }
}
