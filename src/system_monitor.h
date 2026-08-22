#pragma once

#include <stddef.h>
#include <stdint.h>

struct SystemStats {
  uint32_t boot_ms;
  uint64_t rs485_rx_bytes;
  uint64_t rs485_tx_bytes;
  uint64_t net_rx_bytes;
  uint64_t net_tx_bytes;
  uint64_t net_tx_drops;
  uint64_t net_rx_drops;
  uint64_t uart_tx_err;
  uint64_t echo_drops;
  uint64_t echo_mismatch;
  uint64_t tcp_connects;
  uint64_t tcp_disconnects;
  uint64_t tcp_reconnects;
  int reset_reason;
};

void system_monitor_begin();
SystemStats& system_monitor_stats();

void system_monitor_add_rs485_rx(size_t n);
void system_monitor_add_rs485_tx(size_t n);
void system_monitor_add_net_rx(size_t n);
void system_monitor_add_net_tx(size_t n);
void system_monitor_inc_net_tx_drop(size_t n);
void system_monitor_inc_net_rx_drop(size_t n);
void system_monitor_inc_uart_tx_err();
void system_monitor_inc_echo_drop();
void system_monitor_inc_echo_mismatch();
void system_monitor_inc_tcp_connect();
void system_monitor_inc_tcp_disconnect();
void system_monitor_inc_tcp_reconnect();

void system_monitor_loop(); // Watchdog / periodisches Log optional

// Dezimalstring für uint64 (ESP32-newlib printf hat oft kein %llu)
void system_monitor_u64_to_str(uint64_t v, char* out, size_t out_len);
