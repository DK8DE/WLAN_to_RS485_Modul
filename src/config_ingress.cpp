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

static void uart_reply(const uint8_t* data, size_t len, void* /*ctx*/) {
  if (data && len) {
    Serial.write(data, len);
  }
}

static ConfigReplyFn reply_fn(ConfigTransport t) {
  return t == ConfigTransport::Udp ? config_udp_reply : uart_reply;
}

static bool looks_like_config_frame(const uint8_t* data, size_t len) {
  return data != nullptr && len >= 2 && data[0] == kConfigSync0 && data[1] == kConfigSync1;
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

bool config_ingress_pending(ConfigTransport t) {
  return acc_for(t)->len > 0;
}

static size_t append_acc_safe(IngressAcc* acc, const uint8_t* data, size_t len) {
  if (acc == nullptr || data == nullptr || len == 0) {
    return 0;
  }
  if (len >= kAccMax) {
    memcpy(acc->buf, data + (len - (kAccMax - 1)), kAccMax - 1);
    acc->len = kAccMax - 1;
    return len - (kAccMax - 1);
  }
  if (acc->len + len > kAccMax) {
    acc->len = 0;
  }
  memcpy(acc->buf + acc->len, data, len);
  acc->len += len;
  return 0;
}

static size_t emit_bridge(uint8_t* bridge_out, size_t bridge_cap, size_t* written, const uint8_t* p,
                          size_t n) {
  if (bridge_out == nullptr || bridge_cap == 0 || n == 0 || written == nullptr) {
    return 0;
  }
  const size_t space = bridge_cap - *written;
  if (space == 0) {
    return 0;
  }
  const size_t take = n > space ? space : n;
  memcpy(bridge_out + *written, p, take);
  *written += take;
  return take;
}

static void drain_acc(IngressAcc* acc, ConfigTransport t, uint8_t* bridge_out, size_t bridge_cap,
                      size_t* bridged, void* reply_ctx) {
  while (acc->len > 0) {
    if (acc->buf[0] != kConfigSync0) {
      if (bridge_out == nullptr || bridge_cap == 0) {
        size_t run = 0;
        while (run < acc->len && acc->buf[run] != kConfigSync0) {
          ++run;
        }
        memmove(acc->buf, acc->buf + run, acc->len - run);
        acc->len -= run;
        continue;
      }
      size_t run = 0;
      while (run < acc->len && acc->buf[run] != kConfigSync0) {
        ++run;
      }
      const size_t emitted = emit_bridge(bridge_out, bridge_cap, bridged, acc->buf, run);
      memmove(acc->buf, acc->buf + emitted, acc->len - emitted);
      acc->len -= emitted;
      if (emitted < run) {
        break;
      }
      continue;
    }

    ConfigFrame frame{};
    bool need_more = false;
    const size_t used = config_frame_try_parse(acc->buf, acc->len, &frame, &need_more);
    if (need_more) {
      break;
    }
    if (used >= kConfigHeaderSize + 2) {
      config_handle_frame(frame, t, reply_fn(t), reply_ctx);
      memmove(acc->buf, acc->buf + used, acc->len - used);
      acc->len -= used;
      continue;
    }
    if (bridge_out != nullptr && bridge_cap > 0 && bridged != nullptr) {
      const size_t emitted = emit_bridge(bridge_out, bridge_cap, bridged, acc->buf, 1);
      if (emitted == 0) {
        break;
      }
      memmove(acc->buf, acc->buf + 1, acc->len - 1);
      acc->len -= 1;
    } else {
      memmove(acc->buf, acc->buf + 1, acc->len - 1);
      acc->len -= 1;
    }
  }
}

size_t config_ingress_feed(ConfigTransport t, const uint8_t* data, size_t len, uint8_t* bridge_out,
                           size_t bridge_cap, void* reply_ctx) {
  IngressAcc* acc = acc_for(t);
  const unsigned long now = millis();

  if (len > 0 && data != nullptr) {
    const bool quiet = (acc->last_rx_ms == 0) || ((int32_t)(now - acc->last_rx_ms) >= 500);
    acc->last_rx_ms = now;
    at_command_note_rx(t);

    if (at_command_mode() == UartAppMode::At) {
      (void)at_command_feed(data, len);
      return 0;
    }

    // Binärframes (AA 55) nicht auf +++CFG prüfen — sonst Kollision mit Payload-Bytes
    if (!looks_like_config_frame(data, len) && (quiet || t == ConfigTransport::Udp)) {
      const size_t esc = at_command_watch_data(data, len);
      if (esc > 0 && at_command_mode() == UartAppMode::At) {
        if (esc < len) {
          (void)at_command_feed(data + esc, len - esc);
        }
        return 0;
      }
    }

    size_t data_off = 0;
    const size_t spill = append_acc_safe(acc, data, len);
    if (spill > 0) {
      data_off = spill;
    }

    size_t bridged = 0;
    drain_acc(acc, t, bridge_out, bridge_cap, &bridged, reply_ctx);

    if (data_off > 0 && bridge_out != nullptr && bridge_cap > 0 && bridged < bridge_cap) {
      const size_t direct = emit_bridge(bridge_out, bridge_cap, &bridged, data, data_off);
      if (direct < data_off) {
        (void)append_acc_safe(acc, data + direct, data_off - direct);
      }
    }

    return bridged;
  }

  if (acc->len > 0) {
    size_t bridged = 0;
    drain_acc(acc, t, bridge_out, bridge_cap, &bridged, reply_ctx);
    return bridged;
  }

  return 0;
}

void config_ingress_poll() {
  config_handlers_poll();
  at_command_poll();
}
