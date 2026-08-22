#pragma once

#include "config_frame.h"

#include <stddef.h>
#include <stdint.h>

void config_ingress_begin();
void config_ingress_poll();

// Feed RX bytes. Config frames/AT are handled internally.
// Copies transparent leftover into bridge_out; returns bridged length.
size_t config_ingress_feed(ConfigTransport t, const uint8_t* data, size_t len, uint8_t* bridge_out,
                           size_t bridge_cap);

bool config_session_active();
uint16_t config_discovery_udp_port();
