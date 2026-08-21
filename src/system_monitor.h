#pragma once

#include <stddef.h>
#include <stdint.h>

struct SystemStats {
  uint32_t boot_ms;
  uint32_t rs485_rx_bytes;
  uint32_t rs485_tx_bytes;
  uint32_t net_rx_bytes;
  uint32_t net_tx_bytes;
  uint32_t net_tx_drops;
  uint32_t net_rx_drops;
  uint32_t uart_tx_err;
  uint32_t echo_drops;
  uint32_t echo_mismatch;
  uint32_t tcp_connects;
  uint32_t tcp_disconnects;
  uint32_t tcp_reconnects;
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
