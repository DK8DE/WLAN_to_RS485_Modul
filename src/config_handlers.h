#pragma once

#include "config_frame.h"

#include <stddef.h>
#include <stdint.h>

// Reply sink for a transport
typedef void (*ConfigReplyFn)(const uint8_t* data, size_t len, void* ctx);

struct ConfigReplyCtx {
  ConfigReplyFn fn;
  void* user;
  ConfigTransport transport;
  // For UDP: peer filled by caller before handle
  uint32_t peer_ip; // network order or host - use IPAddress later
  uint16_t peer_port;
};

void config_handlers_begin();

// Process a validated frame addressed to us (or DISCOVER). May schedule delayed reply.
void config_handle_frame(const ConfigFrame& req, ConfigTransport transport, ConfigReplyFn reply,
                         void* reply_ctx);

// Poll delayed DISCOVER replies (call from tasks ~1ms)
void config_handlers_poll();

// Build ASCII info blob used by DISCOVER_RESPONSE / GET_INFO / AT
size_t config_build_info_text(char* out, size_t out_len);
size_t config_build_status_text(char* out, size_t out_len);
