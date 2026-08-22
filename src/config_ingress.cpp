#include "config_ingress.h"

#include "at_command.h"
#include "config_handlers.h"

#include <Arduino.h>
#include <string.h>

static constexpr size_t kAccMax = 768;

struct IngressAcc {
  uint8_t buf[kAccMax];
  size_t len;
  unsigned long last_rx_ms;
};

static IngressAcc g_uart_acc{};
static IngressAcc g_udp_acc{};

void config_udp_reply(const uint8_t* data, size_t len, void* ctx);

static void uart_reply(const uint8_t* data, size_t len, void* /*ctx*/) {
  if (data && len) {
    Serial.write(data, len);
  }
}

void config_ingress_begin() {
  memset(&g_uart_acc, 0, sizeof(g_uart_acc));
  memset(&g_udp_acc, 0, sizeof(g_udp_acc));
  config_handlers_begin();
  at_command_begin();
}

uint16_t config_discovery_udp_port() { return kConfigDiscoveryUdpPort; }
bool config_session_active() { return at_command_mode() == UartAppMode::At; }

static IngressAcc* acc_for(ConfigTransport t) {
  return t == ConfigTransport::Udp ? &g_udp_acc : &g_uart_acc;
}

static ConfigReplyFn reply_fn(ConfigTransport t) {
  return t == ConfigTransport::Udp ? config_udp_reply : uart_reply;
}

static size_t emit_bridge(uint8_t* bridge_out, size_t bridge_cap, size_t* written, const uint8_t* p,
                          size_t n) {
  if (bridge_out == nullptr || bridge_cap == 0 || n == 0) {
    return 0;
  }
  const size_t space = bridge_cap - *written;
  const size_t take = n > space ? space : n;
  memcpy(bridge_out + *written, p, take);
  *written += take;
  return take;
}

size_t config_ingress_feed(ConfigTransport t, const uint8_t* data, size_t len, uint8_t* bridge_out,
                           size_t bridge_cap) {
  size_t bridged = 0;
  if (data == nullptr || len == 0) {
    return 0;
  }

  IngressAcc* acc = acc_for(t);
  const unsigned long now = millis();
  const bool quiet = (acc->last_rx_ms == 0) || ((now - acc->last_rx_ms) >= 500UL);
  acc->last_rx_ms = now;
  at_command_note_rx(t);

  // Active AT session: no bridging
  if (at_command_mode() == UartAppMode::At) {
    (void)at_command_feed(data, len);
    return 0;
  }

  // Escape +++CFG (requires quiet on UART; UDP always tries)
  if (quiet || t == ConfigTransport::Udp) {
    const size_t esc = at_command_watch_data(data, len);
    if (esc > 0 && at_command_mode() == UartAppMode::At) {
      if (esc < len) {
        (void)at_command_feed(data + esc, len - esc);
      }
      return 0;
    }
  }

  // Append to accumulator
  if (acc->len + len > kAccMax) {
    // Spill oldest non-sync as bridge if possible, else reset
    acc->len = 0;
  }
  memcpy(acc->buf + acc->len, data, len);
  acc->len += len;

  // Drain accumulator
  while (acc->len > 0) {
    if (acc->buf[0] != kConfigSync0) {
      size_t run = 0;
      while (run < acc->len && acc->buf[run] != kConfigSync0) {
        ++run;
      }
      emit_bridge(bridge_out, bridge_cap, &bridged, acc->buf, run);
      memmove(acc->buf, acc->buf + run, acc->len - run);
      acc->len -= run;
      continue;
    }

    ConfigFrame frame{};
    bool need_more = false;
    const size_t used = config_frame_try_parse(acc->buf, acc->len, &frame, &need_more);
    if (need_more) {
      break;
    }
    if (used >= kConfigHeaderSize + 2) {
      config_handle_frame(frame, t, reply_fn(t), nullptr);
      memmove(acc->buf, acc->buf + used, acc->len - used);
      acc->len -= used;
      continue;
    }
    // Bad sync/CRC: skip one byte — bridge the skipped 0xAA as data (safe for transparency)
    emit_bridge(bridge_out, bridge_cap, &bridged, acc->buf, 1);
    memmove(acc->buf, acc->buf + 1, acc->len - 1);
    acc->len -= 1;
  }

  return bridged;
}

void config_ingress_poll() {
  config_handlers_poll();
  at_command_poll();
}
