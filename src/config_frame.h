#pragma once

#include <stddef.h>
#include <stdint.h>

static constexpr uint16_t kConfigDiscoveryUdpPort = 8880;
// Host lauscht auf diesem Port (RotorTcpBridge); periodische Ankuendigung + Broadcast-Antwort
static constexpr uint16_t kConfigDiscoveryClientPort = 8889;
static constexpr uint8_t kConfigProtoVersion = 1;
static constexpr uint8_t kConfigSync0 = 0xAA;
static constexpr uint8_t kConfigSync1 = 0x55;
static constexpr size_t kConfigHeaderSize = 20; // without CRC
static constexpr size_t kConfigMaxPayload = 512;
static constexpr size_t kConfigMaxFrame = kConfigHeaderSize + kConfigMaxPayload + 2;

enum class ConfigMsgType : uint8_t {
  DISCOVER = 0x01,
  DISCOVER_RESPONSE = 0x02,
  GET_INFO = 0x03,
  GET_STATUS = 0x04,
  GET_CONFIG = 0x05,
  SET_CONFIG = 0x06,
  SAVE_CONFIG = 0x07,
  REBOOT = 0x08,
  FACTORY_RESET = 0x09,
  PING = 0x0A,
  ACK = 0x0B,
  NACK = 0x0C,
  ENTER_UPDATE_MODE = 0x0D,
  GET_LOG = 0x0E,
};

enum class ConfigTransport : uint8_t { Uart = 0, Udp = 1 };

struct ConfigFrame {
  uint8_t version;
  ConfigMsgType type;
  uint8_t dst_mac[6];
  uint8_t src_mac[6];
  uint16_t seq;
  uint16_t payload_len;
  uint8_t payload[kConfigMaxPayload];
};

// Encode frame into out; returns total bytes or 0 on error
size_t config_frame_encode(const ConfigFrame& f, uint8_t* out, size_t out_cap);

// Try parse one frame from buf. On success returns frame length consumed.
// On need-more returns 0 and *need_more=true. On junk returns 1 (skip one byte).
size_t config_frame_try_parse(const uint8_t* buf, size_t len, ConfigFrame* out, bool* need_more);

bool config_mac_is_broadcast(const uint8_t mac[6]);
bool config_mac_matches_device(const uint8_t mac[6]);
void config_mac_copy_device(uint8_t mac[6]);
