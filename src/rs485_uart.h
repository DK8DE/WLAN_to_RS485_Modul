#pragma once

#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static constexpr size_t kBridgeChunkMax = 1024;
static constexpr size_t kUartBufSize = 4096;

struct BridgeChunk {
  uint16_t len;
  uint8_t data[kBridgeChunkMax];
};

// Queues: Netzwerk→UART und UART/Packetizer→Netzwerk
QueueHandle_t rs485_tx_queue();
QueueHandle_t net_tx_queue();

void rs485_uart_begin();
void rs485_uart_start_tasks();

// Echo: zuletzt gesendete Bytes (Ring) für Unterdrückung
void rs485_echo_note_tx(const uint8_t* data, size_t len);
bool rs485_echo_should_drop(uint8_t b);
