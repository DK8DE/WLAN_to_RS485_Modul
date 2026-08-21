#include "rs485_uart.h"

#include "app_config.h"
#include "packetizer.h"
#include "system_monitor.h"

#include <Arduino.h>
#include <string.h>

#include "freertos/task.h"

static QueueHandle_t g_rs485_tx_q = nullptr;
static QueueHandle_t g_net_tx_q = nullptr;

static constexpr size_t kEchoRing = 512;
static uint8_t g_echo_ring[kEchoRing];
static size_t g_echo_head = 0;
static size_t g_echo_tail = 0;
static portMUX_TYPE g_echo_mux = portMUX_INITIALIZER_UNLOCKED;

static Packetizer g_pkt;

QueueHandle_t rs485_tx_queue() { return g_rs485_tx_q; }
QueueHandle_t net_tx_queue() { return g_net_tx_q; }

void rs485_echo_note_tx(const uint8_t* data, size_t len) {
  if (!app_config_runtime_const().echo_suppress || data == nullptr || len == 0) {
    return;
  }
  portENTER_CRITICAL(&g_echo_mux);
  for (size_t i = 0; i < len; ++i) {
    g_echo_ring[g_echo_head] = data[i];
    g_echo_head = (g_echo_head + 1) % kEchoRing;
    if (g_echo_head == g_echo_tail) {
      g_echo_tail = (g_echo_tail + 1) % kEchoRing;
    }
  }
  portEXIT_CRITICAL(&g_echo_mux);
}

bool rs485_echo_should_drop(uint8_t b) {
  if (!app_config_runtime_const().echo_suppress) {
    return false;
  }
  bool drop = false;
  portENTER_CRITICAL(&g_echo_mux);
  if (g_echo_head != g_echo_tail) {
    if (g_echo_ring[g_echo_tail] == b) {
      g_echo_tail = (g_echo_tail + 1) % kEchoRing;
      drop = true;
    } else {
      // Abweichung: kein Match — Kollision/Echo-Miss, Ring nicht verwerfen
      system_monitor_inc_echo_mismatch();
    }
  }
  portEXIT_CRITICAL(&g_echo_mux);
  return drop;
}

static void push_packet_to_net(Packetizer* p) {
  if (p->len == 0) {
    return;
  }
  const AppConfig& cfg = app_config_runtime_const();
  if (!cfg.bridge_enabled || !cfg.rs485_rx_allowed) {
    packetizer_reset(p);
    return;
  }

  size_t offset = 0;
  while (offset < p->len) {
    BridgeChunk chunk{};
    const size_t n = min(static_cast<size_t>(p->len - offset), kBridgeChunkMax);
    chunk.len = static_cast<uint16_t>(n);
    memcpy(chunk.data, p->buf + offset, n);
    if (xQueueSend(g_net_tx_q, &chunk, 0) != pdTRUE) {
      system_monitor_inc_net_tx_drop(n);
    } else {
      system_monitor_add_rs485_rx(n);
    }
    offset += n;
  }
  packetizer_reset(p);
}

static void rs485_rx_task(void* /*arg*/) {
  packetizer_init(&g_pkt, app_config_runtime_const());
  for (;;) {
    packetizer_apply_config(&g_pkt, app_config_runtime_const());
    const unsigned long now = millis();

    while (Serial.available() > 0) {
      const int v = Serial.read();
      if (v < 0) {
        break;
      }
      const uint8_t b = static_cast<uint8_t>(v);
      if (rs485_echo_should_drop(b)) {
        system_monitor_inc_echo_drop();
        continue;
      }
      if (packetizer_feed(&g_pkt, b, now)) {
        push_packet_to_net(&g_pkt);
      }
    }

    if (packetizer_poll_idle(&g_pkt, millis())) {
      push_packet_to_net(&g_pkt);
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

static void rs485_tx_task(void* /*arg*/) {
  BridgeChunk chunk{};
  for (;;) {
    if (xQueueReceive(g_rs485_tx_q, &chunk, pdMS_TO_TICKS(50)) == pdTRUE) {
      const AppConfig& cfg = app_config_runtime_const();
      if (!cfg.bridge_enabled || !cfg.rs485_tx_allowed || chunk.len == 0) {
        continue;
      }
      rs485_echo_note_tx(chunk.data, chunk.len);
      const size_t written = Serial.write(chunk.data, chunk.len);
      Serial.flush();
      system_monitor_add_rs485_tx(written);
      if (written < chunk.len) {
        system_monitor_inc_uart_tx_err();
      }
    }
  }
}

void rs485_uart_begin() {
  Serial.setRxBufferSize(kUartBufSize);
  Serial.setTxBufferSize(kUartBufSize);
  Serial.begin(115200);

  g_rs485_tx_q = xQueueCreate(8, sizeof(BridgeChunk));
  g_net_tx_q = xQueueCreate(8, sizeof(BridgeChunk));
}

void rs485_uart_start_tasks() {
  xTaskCreate(rs485_rx_task, "rs485_rx", 4096, nullptr, 5, nullptr);
  xTaskCreate(rs485_tx_task, "rs485_tx", 4096, nullptr, 5, nullptr);
}
