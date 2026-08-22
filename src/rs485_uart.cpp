#include "rs485_uart.h"

#include "app_config.h"
#include "at_command.h"
#include "config_ingress.h"
#include "system_monitor.h"

#include <Arduino.h>
#include <string.h>

#include "freertos/task.h"

static QueueHandle_t g_rs485_tx_q = nullptr;
static QueueHandle_t g_net_tx_q = nullptr;

QueueHandle_t rs485_tx_queue() { return g_rs485_tx_q; }
QueueHandle_t net_tx_queue() { return g_net_tx_q; }

static void bridge_push_to_net(const uint8_t* data, size_t len) {
  if (data == nullptr || len == 0) {
    return;
  }
  const AppConfig& cfg = app_config_runtime_const();
  if (!cfg.bridge_enabled || !cfg.rs485_rx_allowed) {
    return;
  }
  if (at_command_mode() == UartAppMode::At) {
    return;
  }

  size_t offset = 0;
  while (offset < len) {
    BridgeChunk chunk{};
    const size_t n = min(len - offset, kBridgeChunkMax);
    chunk.len = static_cast<uint16_t>(n);
    memcpy(chunk.data, data + offset, n);
    if (xQueueSend(g_net_tx_q, &chunk, pdMS_TO_TICKS(20)) == pdTRUE) {
      system_monitor_add_rs485_rx(n);
    } else {
      system_monitor_inc_net_tx_drop(n);
    }
    offset += n;
  }
}

static void rs485_rx_task(void* /*arg*/) {
  uint8_t raw[kBridgeChunkMax];
  uint8_t bridge_buf[kBridgeChunkMax];
  BridgeChunk chunk{};
  unsigned long last_byte_ms = 0;
  chunk.len = 0;

  Serial.setTimeout(0);

  for (;;) {
    config_ingress_poll();

    const AppConfig& cfg = app_config_runtime_const();
    const uint16_t max_chunk =
        (cfg.packet_size >= 32 && cfg.packet_size <= kBridgeChunkMax) ? cfg.packet_size
                                                                      : static_cast<uint16_t>(kBridgeChunkMax);
    const uint16_t idle_ms = (cfg.packet_timeout_ms >= 1) ? cfg.packet_timeout_ms : 1;

    const int avail = Serial.available();
    if (avail <= 0) {
      if (chunk.len > 0 && (millis() - last_byte_ms) >= idle_ms) {
        bridge_push_to_net(chunk.data, chunk.len);
        chunk.len = 0;
      }
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }

    const size_t want = min(static_cast<size_t>(avail), sizeof(raw));
    const size_t n = Serial.readBytes(reinterpret_cast<char*>(raw), want);
    if (n == 0) {
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }

    const size_t blen =
        config_ingress_feed(ConfigTransport::Uart, raw, n, bridge_buf, sizeof(bridge_buf));
    if (blen == 0) {
      continue;
    }

    size_t off = 0;
    while (off < blen) {
      const size_t space = static_cast<size_t>(max_chunk) - chunk.len;
      const size_t take = min(space, blen - off);
      memcpy(chunk.data + chunk.len, bridge_buf + off, take);
      chunk.len = static_cast<uint16_t>(chunk.len + take);
      off += take;
      last_byte_ms = millis();
      if (chunk.len >= max_chunk) {
        bridge_push_to_net(chunk.data, chunk.len);
        chunk.len = 0;
      }
    }
  }
}

static void rs485_tx_task(void* /*arg*/) {
  BridgeChunk chunk{};
  for (;;) {
    if (xQueueReceive(g_rs485_tx_q, &chunk, pdMS_TO_TICKS(50)) != pdTRUE) {
      continue;
    }
    if (at_command_mode() == UartAppMode::At) {
      continue;
    }
    const AppConfig& cfg = app_config_runtime_const();
    if (!cfg.bridge_enabled || !cfg.rs485_tx_allowed || chunk.len == 0) {
      continue;
    }
    const size_t written = Serial.write(chunk.data, chunk.len);
    system_monitor_add_rs485_tx(written);
    if (written < chunk.len) {
      system_monitor_inc_uart_tx_err();
    }
  }
}

void rs485_uart_begin() {
  Serial.setRxBufferSize(kUartBufSize);
  Serial.setTxBufferSize(kUartBufSize);
  Serial.begin(115200);
  Serial.setTimeout(0);

  g_rs485_tx_q = xQueueCreate(24, sizeof(BridgeChunk));
  g_net_tx_q = xQueueCreate(24, sizeof(BridgeChunk));
}

void rs485_uart_start_tasks() {
  xTaskCreate(rs485_rx_task, "rs485_rx", 5120, nullptr, 6, nullptr);
  xTaskCreate(rs485_tx_task, "rs485_tx", 4096, nullptr, 6, nullptr);
}
