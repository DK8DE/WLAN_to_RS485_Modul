#pragma once

#include "app_config.h"

#include <stddef.h>
#include <stdint.h>

static constexpr size_t kPacketizerMax = 1460;

struct Packetizer {
  uint8_t buf[kPacketizerMax];
  uint16_t len;
  uint16_t max_size;
  uint16_t idle_ms;
  PacketDelimiter delim;
  uint8_t delim_byte;
  unsigned long last_byte_ms;
  bool has_data;
};

void packetizer_init(Packetizer* p, const AppConfig& cfg);
void packetizer_apply_config(Packetizer* p, const AppConfig& cfg);

// Byte zuführen; liefert true wenn ein Block fertig ist (in p->buf / p->len)
bool packetizer_feed(Packetizer* p, uint8_t b, unsigned long now_ms);

// Idle-Timeout prüfen; true wenn Block wegen Ruhezeit fertig
bool packetizer_poll_idle(Packetizer* p, unsigned long now_ms);

void packetizer_reset(Packetizer* p);
