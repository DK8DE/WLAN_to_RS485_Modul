#pragma once

#include <stdint.h>

enum class WlanLedMode : uint8_t {
  OFF = 0,
  STA_ON = 1,   // Infrastruktur verbunden — dauerhaft an
  AP_BLINK = 2, // SoftAP aktiv — blinken
};

void gpio_status_begin();
void gpio_status_start_task();
void gpio_status_loop();

void gpio_status_set_wlan_led(WlanLedMode mode);
bool gpio_status_factory_requested();
void gpio_status_clear_factory_request();

// Kurzes Blink-Muster vor Factory-Reset (blockierend, kurz)
void gpio_status_factory_confirm_blink();
