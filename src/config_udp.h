#pragma once

#include "config_frame.h"

#include <IPAddress.h>
#include <stddef.h>
#include <stdint.h>

// UDP-Antwort-Peer (für verzögerte/binäre Antworten unabhängig von g_last_peer)
struct UdpReplyPeer {
  IPAddress ip;
  uint16_t port;
};

void config_udp_begin();
void config_udp_start_task();
void config_udp_set_peer(const IPAddress& ip, uint16_t port);
void config_udp_reply(const uint8_t* data, size_t len, void* ctx);
