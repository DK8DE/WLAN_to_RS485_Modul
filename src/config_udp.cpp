#include "config_udp.h"

#include "config_handlers.h"
#include "config_ingress.h"

#include <WiFi.h>
#include <WiFiUdp.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static WiFiUDP g_udp;
static bool g_udp_ok = false;
static IPAddress g_last_peer_ip;
static uint16_t g_last_peer_port = 0;

void config_udp_set_peer(const IPAddress& ip, uint16_t port) {
  g_last_peer_ip = ip;
  g_last_peer_port = port;
}

void config_udp_reply(const uint8_t* data, size_t len, void* ctx) {
  if (!g_udp_ok || data == nullptr || len == 0) {
    return;
  }
  IPAddress ip = g_last_peer_ip;
  uint16_t port = g_last_peer_port;
  if (ctx != nullptr) {
    const UdpReplyPeer* peer = static_cast<const UdpReplyPeer*>(ctx);
    if (peer->port != 0) {
      ip = peer->ip;
      port = peer->port;
    }
  }
  if (port == 0) {
    return;
  }
  g_udp.beginPacket(ip, port);
  g_udp.write(data, len);
  g_udp.endPacket();
}

void config_udp_begin() {
  g_udp_ok = false;
  g_last_peer_port = 0;
}

static void config_udp_task(void* /*arg*/) {
  for (;;) {
    if (!g_udp_ok) {
      if (WiFi.getMode() != WIFI_OFF) {
        if (g_udp.begin(kConfigDiscoveryUdpPort) == 1) {
          g_udp_ok = true;
        }
      }
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    config_ingress_poll();

    const int packet = g_udp.parsePacket();
    if (packet <= 0) {
      vTaskDelay(pdMS_TO_TICKS(2));
      continue;
    }

    UdpReplyPeer peer{};
    peer.ip = g_udp.remoteIP();
    peer.port = g_udp.remotePort();
    config_udp_set_peer(peer.ip, peer.port);

    uint8_t buf[512];
    const int n = g_udp.read(buf, sizeof(buf));
    if (n <= 0) {
      continue;
    }

    config_ingress_feed_udp(buf, static_cast<size_t>(n), &peer);

    // Verzögerte DISCOVER-Antworten (RS485-Jitter-Pfad) auch im UDP-Task abarbeiten
    for (int i = 0; i < 120 && config_handlers_discover_pending(); ++i) {
      config_handlers_poll();
      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }
}

void config_udp_start_task() {
  xTaskCreate(config_udp_task, "cfg_udp", 6144, nullptr, 4, nullptr);
}
