#include "system_monitor.h"

#include <Arduino.h>
#include <esp_system.h>
#include <string.h>

static SystemStats g_stats{};
static unsigned long g_last_log_ms = 0;

void system_monitor_begin() {
  memset(&g_stats, 0, sizeof(g_stats));
  g_stats.boot_ms = millis();
  g_stats.reset_reason = static_cast<int>(esp_reset_reason());
  g_last_log_ms = millis();

  // TWDT optional — Tasks melden sich nicht alle an; nur Loop füttern
  // esp_task_wdt_add(NULL) in loop wenn nötig
}

SystemStats& system_monitor_stats() { return g_stats; }

void system_monitor_add_rs485_rx(size_t n) { g_stats.rs485_rx_bytes += static_cast<uint32_t>(n); }
void system_monitor_add_rs485_tx(size_t n) { g_stats.rs485_tx_bytes += static_cast<uint32_t>(n); }
void system_monitor_add_net_rx(size_t n) { g_stats.net_rx_bytes += static_cast<uint32_t>(n); }
void system_monitor_add_net_tx(size_t n) { g_stats.net_tx_bytes += static_cast<uint32_t>(n); }
void system_monitor_inc_net_tx_drop(size_t n) {
  g_stats.net_tx_drops += static_cast<uint32_t>(n);
}
void system_monitor_inc_net_rx_drop(size_t n) {
  g_stats.net_rx_drops += static_cast<uint32_t>(n);
}
void system_monitor_inc_uart_tx_err() { g_stats.uart_tx_err++; }
void system_monitor_inc_echo_drop() { g_stats.echo_drops++; }
void system_monitor_inc_echo_mismatch() { g_stats.echo_mismatch++; }
void system_monitor_inc_tcp_connect() { g_stats.tcp_connects++; }
void system_monitor_inc_tcp_disconnect() { g_stats.tcp_disconnects++; }
void system_monitor_inc_tcp_reconnect() { g_stats.tcp_reconnects++; }

void system_monitor_loop() {
  // Periodisches Status-Log bewusst aus (würden RS485-Nutzdaten stören).
  // Bei Bedarf DEBUG_STATS aktivieren.
#if defined(DEBUG_STATS)
  const unsigned long now = millis();
  if (now - g_last_log_ms >= 30000) {
    g_last_log_ms = now;
    Serial.printf("[stats] up=%lu rs485_rx=%u tx=%u net_rx=%u tx=%u drops=%u/%u\n",
                  (now - g_stats.boot_ms) / 1000UL, g_stats.rs485_rx_bytes,
                  g_stats.rs485_tx_bytes, g_stats.net_rx_bytes, g_stats.net_tx_bytes,
                  g_stats.net_tx_drops, g_stats.net_rx_drops);
  }
#else
  (void)g_last_log_ms;
#endif
}
