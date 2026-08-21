#pragma once

// Hardware-Pinbelegung WLAN_to_RS485_Modul (ESP32-C5)

static constexpr int PIN_LBLED = 6;       // Status-LED, aktiv HIGH
static constexpr int PIN_WLAN_LED = 7;    // WLAN-LED, aktiv LOW (LOW = an)
static constexpr int PIN_FACTORY_BTN = 8; // Werksreset, externer Pull-up (gedrückt = LOW)

static constexpr unsigned long LBLED_BLINK_MS = 1000;
static constexpr unsigned long FACTORY_HOLD_MS = 5000;
static constexpr unsigned long BTN_DEBOUNCE_MS = 50;
