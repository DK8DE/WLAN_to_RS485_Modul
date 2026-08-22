#include "config_udp.h"

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

void config_udp_reply(const uint8_t* data, size_t len, void* /*ctx*/) {
  if (!g_udp_ok || data == nullptr || len == 0 || g_last_peer_port == 0) {
    return;
  }
  g_udp.beginPacket(g_last_peer_ip, g_last_peer_port);
  g_udp.write(data, len);
  g_udp.endPacket();
}

void config_udp_begin() {
  g_udp_ok = false;
  g_last_peer_port = 0;
}

static void config_udp_task(void* /*arg*/) {
  // Wait until WiFi stack is up
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

    int packet = g_udp.parsePacket();
    if (packet <= 0) {
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }

    g_last_peer_ip = g_udp.remoteIP();
    g_last_peer_port = g_udp.remotePort();

    uint8_t buf[512];
    const int n = g_udp.read(buf, sizeof(buf));
    if (n <= 0) {
      continue;
    }

    uint8_t bridge_tmp[8]; // UDP config is not bridged to RS485
    (void)config_ingress_feed(ConfigTransport::Udp, buf, static_cast<size_t>(n), bridge_tmp,
                              sizeof(bridge_tmp));
  }
}

void config_udp_start_task() {
  xTaskCreate(config_udp_task, "cfg_udp", 6144, nullptr, 4, nullptr);
}
