#pragma once

#include <Arduino.h>
#include <stdint.h>

// UID: 8 Hex-Zeichen, abgeleitet aus Hardware, stabil über Factory-Reset
static constexpr size_t kUidLen = 8;
static constexpr size_t kUidStrSize = kUidLen + 1;
static constexpr size_t kMacStrSize = 18; // "AA:BB:CC:DD:EE:FF"
static constexpr size_t kNameMax = 32;

struct DeviceIdentity {
  char uid[kUidStrSize];
  char mac_str[kMacStrSize];
  uint8_t mac[6];
};

void device_identity_init(DeviceIdentity* out);
uint8_t device_identity_default_bus_addr(const DeviceIdentity& id);
