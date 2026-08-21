#include "network_bridge.h"

#include "app_config.h"
#include "rs485_uart.h"
#include "system_monitor.h"

#include <WiFi.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static WiFiServer* g_server = nullptr;
static WiFiClient g_client;
static SemaphoreHandle_t g_client_mu = nullptr;
static volatile bool g_tcp_up = false;
static unsigned long g_last_client_try_ms = 0;

bool network_bridge_tcp_connected() {
  if (!g_tcp_up || g_client_mu == nullptr) {
    return false;
  }
  bool ok = false;
  if (xSemaphoreTake(g_client_mu, pdMS_TO_TICKS(20)) == pdTRUE) {
    ok = g_tcp_up && g_client.connected();
    xSemaphoreGive(g_client_mu);
  }
  return ok;
}

static void apply_socket_opts(WiFiClient& c, const AppConfig& cfg) {
  if (cfg.tcp_nodelay) {
    c.setNoDelay(true);
  }
  c.setTimeout(cfg.tcp_timeout_ms / 1000);
}

static void close_client_locked() {
  if (g_client) {
    g_client.stop();
  }
  g_tcp_up = false;
}

static void close_client() {
  if (g_client_mu == nullptr) {
    return;
  }
  if (xSemaphoreTake(g_client_mu, pdMS_TO_TICKS(200)) == pdTRUE) {
    const bool was_up = g_tcp_up;
    close_client_locked();
    xSemaphoreGive(g_client_mu);
    if (was_up) {
      system_monitor_inc_tcp_disconnect();
    }
  }
}

static void ensure_server(const AppConfig& cfg) {
  if (g_server == nullptr) {
    g_server = new WiFiServer(cfg.local_port);
    g_server->begin();
  }
}

static void accept_or_reject(const AppConfig& cfg) {
  if (g_server == nullptr) {
    return;
  }
  WiFiClient incoming = g_server->accept();
  if (!incoming) {
    return;
  }

  if (xSemaphoreTake(g_client_mu, pdMS_TO_TICKS(100)) != pdTRUE) {
    incoming.stop();
    return;
  }
  if (g_tcp_up && g_client.connected()) {
    xSemaphoreGive(g_client_mu);
    incoming.stop();
    return;
  }
  g_client.stop();
  g_client = incoming;
  apply_socket_opts(g_client, cfg);
  g_tcp_up = true;
  xSemaphoreGive(g_client_mu);
  system_monitor_inc_tcp_connect();
}

static void try_client_connect(const AppConfig& cfg) {
  if (xSemaphoreTake(g_client_mu, pdMS_TO_TICKS(100)) == pdTRUE) {
    if (g_tcp_up && g_client.connected()) {
      xSemaphoreGive(g_client_mu);
      return;
    }
    xSemaphoreGive(g_client_mu);
  }

  const unsigned long now = millis();
  if (now - g_last_client_try_ms < cfg.reconnect_ms) {
    return;
  }
  g_last_client_try_ms = now;

  if (g_tcp_up) {
    close_client();
    system_monitor_inc_tcp_reconnect();
  }

  WiFiClient c;
  bool ok = false;
  if (cfg.remote_host[0] != '\0') {
    ok = c.connect(cfg.remote_host, cfg.remote_port);
  } else {
    IPAddress ip;
    if (ip.fromString(cfg.remote_ip)) {
      ok = c.connect(ip, cfg.remote_port);
    }
  }

  if (!ok) {
    return;
  }
  apply_socket_opts(c, cfg);
  if (xSemaphoreTake(g_client_mu, pdMS_TO_TICKS(200)) == pdTRUE) {
    g_client.stop();
    g_client = c;
    g_tcp_up = true;
    xSemaphoreGive(g_client_mu);
    system_monitor_inc_tcp_connect();
  } else {
    c.stop();
  }
}

static void network_mgmt_task(void* /*arg*/) {
  NetMode last_mode = NetMode::NET_OFF;
  uint16_t last_port = 0;

  for (;;) {
    const AppConfig& cfg = app_config_runtime_const();

    if (cfg.net_mode != last_mode || cfg.local_port != last_port) {
      close_client();
      if (g_server) {
        g_server->stop();
        delete g_server;
        g_server = nullptr;
      }
      last_mode = cfg.net_mode;
      last_port = cfg.local_port;
      g_last_client_try_ms = 0;
    }

    if (cfg.net_mode == NetMode::NET_OFF) {
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }

    if (cfg.net_mode == NetMode::TCP_SERVER) {
      ensure_server(cfg);
      if (g_tcp_up) {
        bool dead = false;
        if (xSemaphoreTake(g_client_mu, pdMS_TO_TICKS(50)) == pdTRUE) {
          dead = g_tcp_up && !g_client.connected();
          xSemaphoreGive(g_client_mu);
        }
        if (dead) {
          close_client();
        }
      }
      accept_or_reject(cfg);
    } else if (cfg.net_mode == NetMode::TCP_CLIENT) {
      bool dead = false;
      if (xSemaphoreTake(g_client_mu, pdMS_TO_TICKS(50)) == pdTRUE) {
        dead = g_tcp_up && !g_client.connected();
        xSemaphoreGive(g_client_mu);
      }
      if (dead) {
        close_client();
      }
      try_client_connect(cfg);
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

static void network_rx_task(void* /*arg*/) {
  uint8_t buf[512];
  for (;;) {
    const AppConfig& cfg = app_config_runtime_const();
    if (cfg.net_mode == NetMode::NET_OFF || !cfg.bridge_enabled || !cfg.rs485_tx_allowed) {
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }

    int n = 0;
    if (xSemaphoreTake(g_client_mu, pdMS_TO_TICKS(20)) == pdTRUE) {
      if (g_tcp_up && g_client.connected() && g_client.available() > 0) {
        const int avail = g_client.available();
        const int to_read =
            avail > static_cast<int>(sizeof(buf)) ? static_cast<int>(sizeof(buf)) : avail;
        n = g_client.read(buf, to_read);
      }
      xSemaphoreGive(g_client_mu);
    }

    if (n <= 0) {
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }

    size_t offset = 0;
    while (offset < static_cast<size_t>(n)) {
      BridgeChunk chunk{};
      const size_t m = min(static_cast<size_t>(n - offset), kBridgeChunkMax);
      chunk.len = static_cast<uint16_t>(m);
      memcpy(chunk.data, buf + offset, m);
      if (xQueueSend(rs485_tx_queue(), &chunk, 0) != pdTRUE) {
        system_monitor_inc_net_rx_drop(m);
      } else {
        system_monitor_add_net_rx(m);
      }
      offset += m;
    }
  }
}

static void network_tx_task(void* /*arg*/) {
  BridgeChunk chunk{};
  for (;;) {
    if (xQueueReceive(net_tx_queue(), &chunk, pdMS_TO_TICKS(50)) != pdTRUE) {
      continue;
    }
    const AppConfig& cfg = app_config_runtime_const();
    if (cfg.net_mode == NetMode::NET_OFF || !cfg.bridge_enabled || !cfg.rs485_rx_allowed) {
      system_monitor_inc_net_tx_drop(chunk.len);
      continue;
    }

    size_t written = 0;
    bool connected = false;
    if (xSemaphoreTake(g_client_mu, pdMS_TO_TICKS(100)) == pdTRUE) {
      connected = g_tcp_up && g_client.connected();
      if (connected && chunk.len > 0) {
        written = g_client.write(chunk.data, chunk.len);
      }
      xSemaphoreGive(g_client_mu);
    }

    if (!connected || chunk.len == 0) {
      system_monitor_inc_net_tx_drop(chunk.len);
      continue;
    }
    if (written > 0) {
      system_monitor_add_net_tx(written);
    }
    if (written < chunk.len) {
      system_monitor_inc_net_tx_drop(chunk.len - written);
    }
  }
}

void network_bridge_begin() {
  g_client_mu = xSemaphoreCreateMutex();
}

void network_bridge_start_tasks() {
  xTaskCreate(network_mgmt_task, "net_mgmt", 6144, nullptr, 4, nullptr);
  xTaskCreate(network_rx_task, "net_rx", 6144, nullptr, 5, nullptr);
  xTaskCreate(network_tx_task, "net_tx", 6144, nullptr, 5, nullptr);
}
