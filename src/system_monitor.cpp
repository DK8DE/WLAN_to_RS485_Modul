#include "system_monitor.h"

#include <Arduino.h>
#include <esp_system.h>
#include <inttypes.h>
#include <string.h>

static SystemStats g_stats{};
static unsigned long g_last_log_ms = 0;

static void add_u64(uint64_t* counter, size_t n) {
  if (counter == nullptr || n == 0) {
    return;
  }
  const uint64_t add = static_cast<uint64_t>(n);
  if (*counter > UINT64_MAX - add) {
    *counter = UINT64_MAX;
  } else {
    *counter += add;
  }
}

static void inc_u64(uint64_t* counter) {
  if (counter != nullptr && *counter < UINT64_MAX) {
    (*counter)++;
  }
}

void system_monitor_begin() {
  memset(&g_stats, 0, sizeof(g_stats));
  g_stats.boot_ms = millis();
  g_stats.reset_reason = static_cast<int>(esp_reset_reason());
  g_last_log_ms = millis();
}

SystemStats& system_monitor_stats() { return g_stats; }

void system_monitor_add_rs485_rx(size_t n) { add_u64(&g_stats.rs485_rx_bytes, n); }
void system_monitor_add_rs485_tx(size_t n) { add_u64(&g_stats.rs485_tx_bytes, n); }
void system_monitor_add_net_rx(size_t n) { add_u64(&g_stats.net_rx_bytes, n); }
void system_monitor_add_net_tx(size_t n) { add_u64(&g_stats.net_tx_bytes, n); }
void system_monitor_inc_net_tx_drop(size_t n) { add_u64(&g_stats.net_tx_drops, n); }
void system_monitor_inc_net_rx_drop(size_t n) { add_u64(&g_stats.net_rx_drops, n); }
void system_monitor_inc_uart_tx_err() { inc_u64(&g_stats.uart_tx_err); }
void system_monitor_inc_echo_drop() { inc_u64(&g_stats.echo_drops); }
void system_monitor_inc_echo_mismatch() { inc_u64(&g_stats.echo_mismatch); }
void system_monitor_inc_tcp_connect() { inc_u64(&g_stats.tcp_connects); }
void system_monitor_inc_tcp_disconnect() { inc_u64(&g_stats.tcp_disconnects); }
void system_monitor_inc_tcp_reconnect() { inc_u64(&g_stats.tcp_reconnects); }

void system_monitor_u64_to_str(uint64_t v, char* out, size_t out_len) {
  if (out == nullptr || out_len == 0) {
    return;
  }
  out[0] = '\0';
  if (v == 0) {
    if (out_len >= 2) {
      out[0] = '0';
      out[1] = '\0';
    }
    return;
  }
  char rev[24];
  size_t n = 0;
  while (v > 0 && n + 1 < sizeof(rev)) {
    rev[n++] = static_cast<char>('0' + (v % 10ULL));
    v /= 10ULL;
  }
  size_t w = 0;
  while (n > 0 && w + 1 < out_len) {
    out[w++] = rev[--n];
  }
  out[w] = '\0';
}

void system_monitor_loop() {
#if defined(DEBUG_STATS)
  const unsigned long now = millis();
  if ((int32_t)(now - g_last_log_ms) >= 30000) {
    g_last_log_ms = now;
    Serial.printf("[stats] up=%lu rs485_rx=%" PRIu64 " tx=%" PRIu64 " net_rx=%" PRIu64 " tx=%" PRIu64
                  " drops=%" PRIu64 "/%" PRIu64 "\n",
                  static_cast<unsigned long>((now - g_stats.boot_ms) / 1000UL),
                  g_stats.rs485_rx_bytes, g_stats.rs485_tx_bytes, g_stats.net_rx_bytes,
                  g_stats.net_tx_bytes, g_stats.net_tx_drops, g_stats.net_rx_drops);
  }
#else
  (void)g_last_log_ms;
#endif
}
