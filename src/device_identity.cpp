#include "device_identity.h"

#include <esp_mac.h>
#include <stdio.h>
#include <string.h>

void device_identity_init(DeviceIdentity* out) {
  memset(out, 0, sizeof(*out));

  // Basis-MAC (WiFi STA)
  if (esp_read_mac(out->mac, ESP_MAC_WIFI_STA) != ESP_OK) {
    memset(out->mac, 0, 6);
  }

  snprintf(out->mac_str, sizeof(out->mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
           out->mac[0], out->mac[1], out->mac[2], out->mac[3], out->mac[4],
           out->mac[5]);

  // UID aus letzten 4 MAC-Bytes (stabil, lesbar, Spec-ähnlich C5xxxxxxxx)
  // Prefix "C5" + 6 Hex aus MAC[3..5] wäre nur 8 chars: C5 + 6 hex = 8
  snprintf(out->uid, sizeof(out->uid), "C5%02X%02X%02X", out->mac[3], out->mac[4],
           out->mac[5]);
}

uint8_t device_identity_default_bus_addr(const DeviceIdentity& id) {
  // 1..247 aus UID-Hash
  uint32_t h = 0;
  for (size_t i = 0; id.uid[i] != '\0'; ++i) {
    h = h * 131u + static_cast<uint8_t>(id.uid[i]);
  }
  return static_cast<uint8_t>((h % 247u) + 1u);
}
