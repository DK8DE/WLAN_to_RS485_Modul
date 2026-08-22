#pragma once

#include "config_frame.h"

#include <stddef.h>
#include <stdint.h>

enum class UartAppMode : uint8_t {
  Data = 0,
  At = 1,
};

void at_command_begin();
void at_command_poll();
void at_command_note_rx(ConfigTransport t);

UartAppMode at_command_mode();
void at_command_set_mode(UartAppMode mode);

size_t at_command_feed(const uint8_t* data, size_t len);
size_t at_command_watch_data(const uint8_t* data, size_t len);

bool at_command_id_matches(const char* id);
