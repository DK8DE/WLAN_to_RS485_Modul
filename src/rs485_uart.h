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

QueueHandle_t rs485_tx_queue();
QueueHandle_t net_tx_queue();

void rs485_uart_begin();
void rs485_uart_start_tasks();
