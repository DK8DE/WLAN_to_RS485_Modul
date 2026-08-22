#include "packetizer.h"

#include <string.h>

static uint8_t delim_value(PacketDelimiter d, uint8_t custom) {
  switch (d) {
    case PacketDelimiter::CR:
      return '\r';
    case PacketDelimiter::LF:
      return '\n';
    case PacketDelimiter::CUSTOM:
      return custom;
    default:
      return 0;
  }
}

void packetizer_init(Packetizer* p, const AppConfig& cfg) {
  memset(p, 0, sizeof(*p));
  packetizer_apply_config(p, cfg);
}

void packetizer_apply_config(Packetizer* p, const AppConfig& cfg) {
  p->max_size = cfg.packet_size;
  if (p->max_size > kPacketizerMax) {
    p->max_size = kPacketizerMax;
  }
  if (p->max_size < 32) {
    p->max_size = 32;
  }
  p->idle_ms = cfg.packet_timeout_ms;
  p->delim = cfg.delimiter;
  p->delim_byte = delim_value(cfg.delimiter, cfg.delimiter_custom);
}

void packetizer_reset(Packetizer* p) {
  p->len = 0;
  p->has_data = false;
}

bool packetizer_feed(Packetizer* p, uint8_t b, unsigned long now_ms) {
  if (p->len >= p->max_size) {
    return false;
  }
  p->buf[p->len++] = b;
  p->last_byte_ms = now_ms;
  p->has_data = true;

  if (p->delim != PacketDelimiter::NONE && b == p->delim_byte) {
    return true;
  }
  if (p->len >= p->max_size) {
    return true;
  }
  return false;
}

bool packetizer_poll_idle(Packetizer* p, unsigned long now_ms) {
  if (!p->has_data || p->len == 0) {
    return false;
  }
  if ((now_ms - p->last_byte_ms) >= p->idle_ms) {
    return true;
  }
  return false;
}
