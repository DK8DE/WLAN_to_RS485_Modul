#pragma once

#include "config_frame.h"

#include <stddef.h>
#include <stdint.h>

typedef void (*ConfigReplyFn)(const uint8_t* data, size_t len, void* ctx);

void config_handlers_begin();

void config_handle_frame(const ConfigFrame& req, ConfigTransport transport, ConfigReplyFn reply,
                         void* reply_ctx);

void config_handlers_poll();
bool config_handlers_discover_pending();

size_t config_build_info_text(char* out, size_t out_len);
size_t config_build_status_text(char* out, size_t out_len);
