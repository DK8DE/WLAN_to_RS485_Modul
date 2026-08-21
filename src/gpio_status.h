#pragma once

#include <stdint.h>

void gpio_status_begin();
void gpio_status_loop(); // aus main/system_monitor aufrufen

void gpio_status_set_wlan_link(bool up);
bool gpio_status_factory_requested();
void gpio_status_clear_factory_request();

// Kurzes Blink-Muster vor Factory-Reset (blockierend, kurz)
void gpio_status_factory_confirm_blink();
