#include "config_udp.h"

#include "config_handlers.h"
#include "config_ingress.h"

#include <WiFi.h>
#include <errno.h>
#include <lwip/sockets.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Raw lwIP-Socket: zuverlaessiger Empfang von 255.255.255.255 als WiFiUDP.
// Antwort zusaetzlich als Broadcast (gleicher Dest-Port), falls Modul-IP falsch
// ist und Unicast-Routing nicht funktioniert.

static int g_sock = -1;
static bool g_udp_ok = false;
static IPAddress g_last_peer_ip;
static uint16_t g_last_peer_port = 0;

static void config_udp_close() {
  if (g_sock >= 0) {
    close(g_sock);
    g_sock = -1;
  }
  g_udp_ok = false;
}

static bool config_udp_open() {
  config_udp_close();
  if (WiFi.getMode() == WIFI_OFF) {
    return false;
  }

  g_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  if (g_sock < 0) {
    return false;
  }

  int opt = 1;
  setsockopt(g_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  setsockopt(g_sock, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));

  struct timeval tv {};
  tv.tv_sec = 0;
  tv.tv_usec = 200000;
  setsockopt(g_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  struct sockaddr_in bind_addr {};
  bind_addr.sin_family = AF_INET;
  bind_addr.sin_port = htons(kConfigDiscoveryUdpPort);
  bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(g_sock, reinterpret_cast<struct sockaddr*>(&bind_addr), sizeof(bind_addr)) < 0) {
    config_udp_close();
    return false;
  }

  g_udp_ok = true;
  return true;
}

static void config_udp_send_to(const uint8_t* data, size_t len, uint32_t addr, uint16_t port) {
  if (!g_udp_ok || g_sock < 0 || data == nullptr || len == 0 || port == 0) {
    return;
  }
  struct sockaddr_in dest {};
  dest.sin_family = AF_INET;
  dest.sin_port = htons(port);
  dest.sin_addr.s_addr = addr;
  (void)sendto(g_sock, data, len, 0, reinterpret_cast<struct sockaddr*>(&dest), sizeof(dest));
}

void config_udp_set_peer(const IPAddress& ip, uint16_t port) {
  g_last_peer_ip = ip;
  g_last_peer_port = port;
}

void config_udp_reply(const uint8_t* data, size_t len, void* ctx) {
  if (!g_udp_ok || g_sock < 0 || data == nullptr || len == 0) {
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

  const uint32_t peer_addr = static_cast<uint32_t>(ip);
  config_udp_send_to(data, len, peer_addr, port);
  // Duplikat per limited broadcast — wichtig bei falscher statischer Modul-IP
  config_udp_send_to(data, len, htonl(INADDR_BROADCAST), port);
}

static void config_udp_periodic_announce() {
  static unsigned long last_ms = 0;
  const unsigned long now = millis();
  if (last_ms != 0 && (now - last_ms) < 12000UL) {
    return;
  }
  if (WiFi.getMode() == WIFI_OFF) {
    return;
  }
  const bool link_up =
      (WiFi.status() == WL_CONNECTED) || ((WiFi.getMode() & WIFI_AP) != 0);
  if (!link_up) {
    return;
  }
  last_ms = now;

  char text[384];
  const size_t tn = config_build_info_text(text, sizeof(text));
  if (tn == 0) {
    return;
  }

  ConfigFrame f {};
  f.version = kConfigProtoVersion;
  f.type = ConfigMsgType::DISCOVER_RESPONSE;
  memset(f.dst_mac, 0xFF, 6);
  config_mac_copy_device(f.src_mac);
  f.seq = 0;
  f.payload_len = static_cast<uint16_t>(tn);
  memcpy(f.payload, text, tn);

  uint8_t buf[kConfigMaxFrame];
  const size_t n = config_frame_encode(f, buf, sizeof(buf));
  if (n == 0) {
    return;
  }

  config_udp_send_to(buf, n, htonl(INADDR_BROADCAST), kConfigDiscoveryClientPort);
}

void config_udp_begin() {
  config_udp_close();
  g_last_peer_port = 0;
}

static void config_udp_task(void* /*arg*/) {
  for (;;) {
    if (!g_udp_ok) {
      (void)config_udp_open();
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    config_ingress_poll();
    config_udp_periodic_announce();

    uint8_t buf[512];
    struct sockaddr_in source {};
    socklen_t slen = sizeof(source);
    const int n =
        recvfrom(g_sock, buf, sizeof(buf), 0, reinterpret_cast<struct sockaddr*>(&source), &slen);
    if (n <= 0) {
      if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        config_udp_close();
      }
      vTaskDelay(pdMS_TO_TICKS(2));
      continue;
    }

    UdpReplyPeer peer {};
    peer.ip = IPAddress(source.sin_addr.s_addr);
    peer.port = ntohs(source.sin_port);
    config_udp_set_peer(peer.ip, peer.port);

    config_ingress_feed_udp(buf, static_cast<size_t>(n), &peer);

    for (int i = 0; i < 120 && config_handlers_discover_pending(); ++i) {
      config_handlers_poll();
      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }
}

void config_udp_start_task() {
  xTaskCreate(config_udp_task, "cfg_udp", 6144, nullptr, 4, nullptr);
}
