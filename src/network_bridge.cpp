#include "network_bridge.h"

#include "app_config.h"
#include "rs485_uart.h"
#include "system_monitor.h"
#include "wifi_manager.h"

#include <WiFi.h>
#include <WiFiUdp.h>
#include <errno.h>
#include <lwip/sockets.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static WiFiServer* g_server = nullptr;
static WiFiClient g_client;
static WiFiUDP g_udp;
static SemaphoreHandle_t g_net_mu = nullptr;

static volatile bool g_tcp_up = false;
static volatile bool g_udp_up = false;
static volatile bool g_tcp_connecting = false;
static unsigned long g_last_client_try_ms = 0;

static IPAddress g_udp_peer_ip;
static uint16_t g_udp_peer_port = 0;
static bool g_udp_peer_valid = false;

static NetMode g_active_mode = NetMode::NET_OFF;
static uint16_t g_active_lport = 0;
static uint16_t g_active_rport = 0;
static char g_active_rip[16] = {0};
static char g_active_rhost[64] = {0};

static bool is_tcp_mode(NetMode m) {
  return m == NetMode::TCP_SERVER || m == NetMode::TCP_CLIENT;
}
static bool is_udp_mode(NetMode m) {
  return m == NetMode::UDP_SERVER || m == NetMode::UDP_CLIENT;
}

const char* network_bridge_mode_name() {
  switch (app_config_runtime_const().net_mode) {
    case NetMode::TCP_SERVER:
      return "TCP Server";
    case NetMode::TCP_CLIENT:
      return "TCP Client";
    case NetMode::UDP_SERVER:
      return "UDP Server";
    case NetMode::UDP_CLIENT:
      return "UDP Client";
    default:
      return "Disabled";
  }
}

bool network_bridge_link_up() {
  const NetMode m = app_config_runtime_const().net_mode;
  if (m == NetMode::NET_OFF || g_tcp_connecting) {
    return false;
  }
  if (is_udp_mode(m)) {
    return g_udp_up;
  }
  if (!g_tcp_up || g_net_mu == nullptr) {
    return false;
  }
  bool ok = false;
  if (xSemaphoreTake(g_net_mu, pdMS_TO_TICKS(20)) == pdTRUE) {
    ok = g_tcp_up && g_client.fd() >= 0 && g_client.connected();
    xSemaphoreGive(g_net_mu);
  }
  return ok;
}

bool network_bridge_tcp_connected() { return network_bridge_link_up(); }

static void apply_socket_opts(WiFiClient& c, const AppConfig& cfg) {
  // Längeres Timeout; NetworkClient::write() nutzt SO_SNDTIMEO + select.
  c.setTimeout(5000);
  if (cfg.tcp_nodelay) {
    c.setNoDelay(true);
  }
  // Keepalive nur sanft — aggressive Werte können Sessions unter Last abwürgen
  if (cfg.tcp_keepalive) {
    int yes = 1;
    c.setSocketOption(SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes));
#if defined(TCP_KEEPIDLE)
    int idle = 60;
    int intvl = 10;
    int cnt = 3;
    c.setSocketOption(IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
    c.setSocketOption(IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl));
    c.setSocketOption(IPPROTO_TCP, TCP_KEEPCNT, &cnt, sizeof(cnt));
#endif
  }
}

// Eigenes Send: kein MSG_DONTWAIT + kein automatisches stop() bei erstem Fehler
// (NetworkClient::write() schließt bei errno 113 den Socket und loggt auf Serial).
static size_t tcp_send_all(WiFiClient& c, const uint8_t* data, size_t len) {
  const int sock = c.fd();
  if (sock < 0 || len == 0) {
    return 0;
  }

  size_t sent = 0;
  int spins = 0;
  while (sent < len && spins < 400) {
    const int n = ::send(sock, reinterpret_cast<const char*>(data + sent), len - sent, MSG_DONTWAIT);
    if (n > 0) {
      sent += static_cast<size_t>(n);
      spins = 0;
      continue;
    }
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS)) {
      vTaskDelay(pdMS_TO_TICKS(1));
      ++spins;
      continue;
    }
    break;
  }
  return sent;
}

// Direkt recv — nicht über available()/FIONREAD (auf ESP32-C5 oft dauerhaft 0 → kein RX).
static int tcp_recv_nb(WiFiClient& c, uint8_t* buf, size_t maxlen) {
  const int sock = c.fd();
  if (sock < 0 || buf == nullptr || maxlen == 0) {
    return 0;
  }
  const int n = ::recv(sock, buf, maxlen, MSG_DONTWAIT);
  if (n > 0) {
    return n;
  }
  if (n == 0) {
    return -1; // Peer hat geschlossen
  }
  if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS) {
    return 0;
  }
  if (errno == ECONNRESET || errno == ENOTCONN || errno == EPIPE || errno == ECONNABORTED) {
    return -1;
  }
  return 0;
}

static void close_tcp_locked() {
  if (g_client) {
    g_client.stop();
  }
  g_tcp_up = false;
}

static void close_tcp() {
  if (g_net_mu == nullptr) {
    return;
  }
  if (xSemaphoreTake(g_net_mu, pdMS_TO_TICKS(200)) == pdTRUE) {
    const bool was_up = g_tcp_up;
    close_tcp_locked();
    xSemaphoreGive(g_net_mu);
    if (was_up) {
      system_monitor_inc_tcp_disconnect();
    }
  }
}

static void stop_udp() {
  if (g_udp_up) {
    g_udp.stop();
    g_udp_up = false;
  }
  g_udp_peer_valid = false;
  g_udp_peer_port = 0;
}

static void teardown_all() {
  close_tcp();
  if (g_server) {
    g_server->stop();
    delete g_server;
    g_server = nullptr;
  }
  stop_udp();
}

static void ensure_tcp_server(const AppConfig& cfg) {
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

  if (xSemaphoreTake(g_net_mu, pdMS_TO_TICKS(100)) != pdTRUE) {
    incoming.stop();
    return;
  }
  if (g_tcp_up && g_client.connected()) {
    xSemaphoreGive(g_net_mu);
    incoming.stop();
    return;
  }
  g_client.stop();
  g_client = incoming;
  apply_socket_opts(g_client, cfg);
  g_tcp_up = true;
  xSemaphoreGive(g_net_mu);
  system_monitor_inc_tcp_connect();
}

// Connect direkt auf g_client — kein temporäres WiFiClient, dessen Destruktor
// den Socket nach Zuweisung wieder schließen könnte (häufiger ESP32-Bug).
static void try_tcp_client(const AppConfig& cfg) {
  if (g_tcp_connecting) {
    return;
  }

  // Ohne STA-IP keinen TCP-Connect versuchen (sonst SoftAP-Route / Abbruch)
  if (!wifi_manager_sta_connected()) {
    return;
  }

  if (xSemaphoreTake(g_net_mu, pdMS_TO_TICKS(50)) == pdTRUE) {
    if (g_tcp_up && g_client.fd() >= 0 && g_client.connected()) {
      xSemaphoreGive(g_net_mu);
      return;
    }
    if (g_tcp_up && (g_client.fd() < 0 || !g_client.connected())) {
      g_tcp_up = false;
    }
    xSemaphoreGive(g_net_mu);
  }

  const unsigned long now = millis();
  if (now - g_last_client_try_ms < cfg.reconnect_ms) {
    return;
  }
  g_last_client_try_ms = now;

  if (g_tcp_up) {
    close_tcp();
    system_monitor_inc_tcp_reconnect();
  }

  g_tcp_connecting = true;

  if (xSemaphoreTake(g_net_mu, pdMS_TO_TICKS(200)) == pdTRUE) {
    g_client.stop();
    g_tcp_up = false;
    xSemaphoreGive(g_net_mu);
  }

  bool ok = false;
  const int32_t connect_timeout_ms = 5000;
  if (cfg.remote_host[0] != '\0') {
    ok = g_client.connect(cfg.remote_host, cfg.remote_port, connect_timeout_ms);
  } else {
    IPAddress ip;
    if (ip.fromString(cfg.remote_ip)) {
      ok = g_client.connect(ip, cfg.remote_port, connect_timeout_ms);
    }
  }

  if (!ok) {
    g_client.stop();
    g_tcp_connecting = false;
    return;
  }

  apply_socket_opts(g_client, cfg);

  if (xSemaphoreTake(g_net_mu, pdMS_TO_TICKS(200)) == pdTRUE) {
    g_tcp_up = true;
    xSemaphoreGive(g_net_mu);
    system_monitor_inc_tcp_connect();
  } else {
    g_client.stop();
  }
  g_tcp_connecting = false;
}

static bool ensure_udp(const AppConfig& cfg) {
  if (g_udp_up) {
    return true;
  }
  if (cfg.net_mode == NetMode::UDP_SERVER) {
    if (g_udp.begin(cfg.local_port) == 1) {
      g_udp_up = true;
      return true;
    }
  } else if (cfg.net_mode == NetMode::UDP_CLIENT) {
    if (g_udp.begin(0) == 1) {
      IPAddress ip;
      if (ip.fromString(cfg.remote_ip)) {
        g_udp_peer_ip = ip;
        g_udp_peer_port = cfg.remote_port;
        g_udp_peer_valid = true;
      } else {
        g_udp_peer_valid = false;
      }
      g_udp_up = true;
      return true;
    }
  }
  return false;
}

static bool config_changed(const AppConfig& cfg) {
  return cfg.net_mode != g_active_mode || cfg.local_port != g_active_lport ||
         cfg.remote_port != g_active_rport || strcmp(cfg.remote_ip, g_active_rip) != 0 ||
         strcmp(cfg.remote_host, g_active_rhost) != 0;
}

static void remember_config(const AppConfig& cfg) {
  g_active_mode = cfg.net_mode;
  g_active_lport = cfg.local_port;
  g_active_rport = cfg.remote_port;
  strncpy(g_active_rip, cfg.remote_ip, sizeof(g_active_rip) - 1);
  g_active_rip[sizeof(g_active_rip) - 1] = '\0';
  strncpy(g_active_rhost, cfg.remote_host, sizeof(g_active_rhost) - 1);
  g_active_rhost[sizeof(g_active_rhost) - 1] = '\0';
}

static void network_mgmt_task(void* /*arg*/) {
  for (;;) {
    const AppConfig& cfg = app_config_runtime_const();

    // TCP/UDP-Client: SoftAP abschalten sobald STA steht (weniger errno 113)
    const bool client_mode =
        cfg.net_mode == NetMode::TCP_CLIENT || cfg.net_mode == NetMode::UDP_CLIENT;
    wifi_manager_set_client_data_mode(client_mode);

    if (config_changed(cfg)) {
      teardown_all();
      remember_config(cfg);
      g_last_client_try_ms = 0;
    }

    if (cfg.net_mode == NetMode::NET_OFF) {
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }

    if (cfg.net_mode == NetMode::TCP_SERVER) {
      ensure_tcp_server(cfg);
      if (g_tcp_up) {
        bool dead = false;
        if (xSemaphoreTake(g_net_mu, pdMS_TO_TICKS(50)) == pdTRUE) {
          dead = g_tcp_up && g_client.fd() >= 0 && !g_client.connected();
          if (dead) {
            close_tcp_locked();
          }
          xSemaphoreGive(g_net_mu);
        }
        if (dead) {
          system_monitor_inc_tcp_disconnect();
        }
      }
      accept_or_reject(cfg);
    } else if (cfg.net_mode == NetMode::TCP_CLIENT) {
      bool dead = false;
      if (xSemaphoreTake(g_net_mu, pdMS_TO_TICKS(50)) == pdTRUE) {
        // Nicht available() nutzen (kann intern recv in den Arduino-Puffer ziehen)
        dead = g_tcp_up && g_client.fd() >= 0 && !g_client.connected();
        if (dead) {
          close_tcp_locked();
        }
        xSemaphoreGive(g_net_mu);
      }
      if (dead) {
        system_monitor_inc_tcp_disconnect();
        g_last_client_try_ms = 0;
      }
      try_tcp_client(cfg);
    } else if (is_udp_mode(cfg.net_mode)) {
      ensure_udp(cfg);
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

static void enqueue_to_rs485(const uint8_t* data, size_t n) {
  size_t offset = 0;
  while (offset < n) {
    BridgeChunk chunk{};
    const size_t m = min(n - offset, kBridgeChunkMax);
    chunk.len = static_cast<uint16_t>(m);
    memcpy(chunk.data, data + offset, m);
    if (xQueueSend(rs485_tx_queue(), &chunk, pdMS_TO_TICKS(20)) != pdTRUE) {
      system_monitor_inc_net_rx_drop(m);
    } else {
      system_monitor_add_net_rx(m);
    }
    offset += m;
  }
}

static void network_rx_task(void* /*arg*/) {
  uint8_t buf[1024];
  for (;;) {
    const AppConfig& cfg = app_config_runtime_const();
    if (cfg.net_mode == NetMode::NET_OFF || !cfg.bridge_enabled || !cfg.rs485_tx_allowed ||
        g_tcp_connecting) {
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }

    if (is_tcp_mode(cfg.net_mode)) {
      int n = 0;
      bool peer_dead = false;
      if (xSemaphoreTake(g_net_mu, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (g_tcp_up && g_client.fd() >= 0) {
          n = tcp_recv_nb(g_client, buf, sizeof(buf));
          if (n < 0) {
            close_tcp_locked();
            peer_dead = true;
            n = 0;
          }
        }
        xSemaphoreGive(g_net_mu);
      }
      if (peer_dead) {
        system_monitor_inc_tcp_disconnect();
        g_last_client_try_ms = 0;
      }
      if (n > 0) {
        enqueue_to_rs485(buf, static_cast<size_t>(n));
        continue;
      }
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }

    if (is_udp_mode(cfg.net_mode) && g_udp_up) {
      int packet = g_udp.parsePacket();
      if (packet > 0) {
        const int n = g_udp.read(buf, sizeof(buf));
        if (n > 0) {
          if (cfg.net_mode == NetMode::UDP_SERVER) {
            g_udp_peer_ip = g_udp.remoteIP();
            g_udp_peer_port = g_udp.remotePort();
            g_udp_peer_valid = true;
          }
          enqueue_to_rs485(buf, static_cast<size_t>(n));
          continue;
        }
      }
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

static void network_tx_task(void* /*arg*/) {
  BridgeChunk chunk{};
  for (;;) {
    if (xQueueReceive(net_tx_queue(), &chunk, pdMS_TO_TICKS(50)) != pdTRUE) {
      continue;
    }
    const AppConfig& cfg = app_config_runtime_const();
    if (cfg.net_mode == NetMode::NET_OFF || !cfg.bridge_enabled || !cfg.rs485_rx_allowed ||
        chunk.len == 0 || g_tcp_connecting) {
      system_monitor_inc_net_tx_drop(chunk.len);
      continue;
    }

    if (is_tcp_mode(cfg.net_mode)) {
      size_t written = 0;
      bool can_write = false;
      bool aborted = false;
      if (xSemaphoreTake(g_net_mu, pdMS_TO_TICKS(200)) == pdTRUE) {
        can_write = g_tcp_up && (g_client.fd() >= 0);
        if (can_write) {
          // connected() (MSG_PEEK) unter Last weglassen — verzögert nur das Senden
          written = tcp_send_all(g_client, chunk.data, chunk.len);
          if (written < chunk.len) {
            close_tcp_locked();
            aborted = true;
          }
        }
        xSemaphoreGive(g_net_mu);
      }
      if (aborted) {
        system_monitor_inc_net_tx_drop(chunk.len - written);
        system_monitor_inc_tcp_disconnect();
        g_last_client_try_ms = 0;
        continue;
      }
      if (!can_write) {
        system_monitor_inc_net_tx_drop(chunk.len);
        continue;
      }
      if (written > 0) {
        system_monitor_add_net_tx(written);
      }
      continue;
    }

    if (is_udp_mode(cfg.net_mode) && g_udp_up) {
      if (!g_udp_peer_valid) {
        system_monitor_inc_net_tx_drop(chunk.len);
        continue;
      }
      if (xSemaphoreTake(g_net_mu, pdMS_TO_TICKS(100)) == pdTRUE) {
        g_udp.beginPacket(g_udp_peer_ip, g_udp_peer_port);
        const size_t written = g_udp.write(chunk.data, chunk.len);
        const bool ok = g_udp.endPacket() == 1;
        xSemaphoreGive(g_net_mu);
        if (ok && written > 0) {
          system_monitor_add_net_tx(written);
        }
        if (!ok || written < chunk.len) {
          system_monitor_inc_net_tx_drop(chunk.len - written);
        }
      } else {
        system_monitor_inc_net_tx_drop(chunk.len);
      }
      continue;
    }

    system_monitor_inc_net_tx_drop(chunk.len);
  }
}

void network_bridge_begin() {
  g_net_mu = xSemaphoreCreateMutex();
}

void network_bridge_start_tasks() {
  xTaskCreate(network_mgmt_task, "net_mgmt", 6144, nullptr, 4, nullptr);
  xTaskCreate(network_rx_task, "net_rx", 8192, nullptr, 6, nullptr);
  xTaskCreate(network_tx_task, "net_tx", 6144, nullptr, 6, nullptr);
}
