#pragma once

#include "config_frame.h"
#include "config_udp.h"

#include <stddef.h>
#include <stdint.h>

void config_ingress_begin();
void config_ingress_poll();

size_t config_ingress_feed(ConfigTransport t, const uint8_t* data, size_t len, uint8_t* bridge_out,
                           size_t bridge_cap, void* reply_ctx);

static inline void config_ingress_feed_udp(const uint8_t* data, size_t len, const UdpReplyPeer* peer) {
  config_ingress_feed(ConfigTransport::Udp, data, len, nullptr, 0, const_cast<UdpReplyPeer*>(peer));
}

bool config_ingress_pending(ConfigTransport t);
bool config_session_active();
uint16_t config_discovery_udp_port();
