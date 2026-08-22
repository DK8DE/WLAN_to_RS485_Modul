#pragma once

#include <stddef.h>
#include <stdint.h>

// CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, refin/refout false, xorout 0x0000
uint16_t config_crc16(const uint8_t* data, size_t len);
