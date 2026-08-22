#pragma once

#include "config_frame.h"

#include <stddef.h>
#include <stdint.h>

void config_udp_begin();
void config_udp_start_task();
void config_udp_reply(const uint8_t* data, size_t len, void* ctx);
