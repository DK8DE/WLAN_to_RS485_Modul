#include "config_frame.h"

#include "config_crc.h"
#include "device_identity.h"

#include <string.h>

extern DeviceIdentity g_identity;

static uint16_t rd_be16(const uint8_t* p) {
  return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

static void wr_be16(uint8_t* p, uint16_t v) {
  p[0] = static_cast<uint8_t>((v >> 8) & 0xFF);
  p[1] = static_cast<uint8_t>(v & 0xFF);
}

bool config_mac_is_broadcast(const uint8_t mac[6]) {
  if (mac == nullptr) {
    return true;
  }
  bool all_ff = true;
  bool all_00 = true;
  for (int i = 0; i < 6; ++i) {
    if (mac[i] != 0xFF) {
      all_ff = false;
    }
    if (mac[i] != 0x00) {
      all_00 = false;
    }
  }
  return all_ff || all_00;
}

bool config_mac_matches_device(const uint8_t mac[6]) {
  if (config_mac_is_broadcast(mac)) {
    return true;
  }
  return memcmp(mac, g_identity.mac, 6) == 0;
}

void config_mac_copy_device(uint8_t mac[6]) {
  memcpy(mac, g_identity.mac, 6);
}

size_t config_frame_encode(const ConfigFrame& f, uint8_t* out, size_t out_cap) {
  if (out == nullptr || f.payload_len > kConfigMaxPayload) {
    return 0;
  }
  const size_t total = kConfigHeaderSize + f.payload_len + 2;
  if (out_cap < total) {
    return 0;
  }
  out[0] = kConfigSync0;
  out[1] = kConfigSync1;
  out[2] = f.version ? f.version : kConfigProtoVersion;
  out[3] = static_cast<uint8_t>(f.type);
  memcpy(out + 4, f.dst_mac, 6);
  memcpy(out + 10, f.src_mac, 6);
  wr_be16(out + 16, f.seq);
  wr_be16(out + 18, f.payload_len);
  if (f.payload_len > 0) {
    memcpy(out + 20, f.payload, f.payload_len);
  }
  const uint16_t crc = config_crc16(out, kConfigHeaderSize + f.payload_len);
  wr_be16(out + kConfigHeaderSize + f.payload_len, crc);
  return total;
}

size_t config_frame_try_parse(const uint8_t* buf, size_t len, ConfigFrame* out, bool* need_more) {
  if (need_more) {
    *need_more = false;
  }
  if (buf == nullptr || out == nullptr || len == 0) {
    return 0;
  }
  if (buf[0] != kConfigSync0) {
    return 1; // skip junk byte
  }
  if (len < 2) {
    if (need_more) {
      *need_more = true;
    }
    return 0;
  }
  if (buf[1] != kConfigSync1) {
    return 1;
  }
  if (len < kConfigHeaderSize) {
    if (need_more) {
      *need_more = true;
    }
    return 0;
  }
  const uint16_t plen = rd_be16(buf + 18);
  if (plen > kConfigMaxPayload) {
    return 1;
  }
  const size_t total = kConfigHeaderSize + plen + 2;
  if (len < total) {
    if (need_more) {
      *need_more = true;
    }
    return 0;
  }
  const uint16_t expect = config_crc16(buf, kConfigHeaderSize + plen);
  const uint16_t got = rd_be16(buf + kConfigHeaderSize + plen);
  if (expect != got) {
    return 1; // bad CRC — skip sync0, resync
  }
  out->version = buf[2];
  out->type = static_cast<ConfigMsgType>(buf[3]);
  memcpy(out->dst_mac, buf + 4, 6);
  memcpy(out->src_mac, buf + 10, 6);
  out->seq = rd_be16(buf + 16);
  out->payload_len = plen;
  if (plen > 0) {
    memcpy(out->payload, buf + 20, plen);
  }
  return total;
}
