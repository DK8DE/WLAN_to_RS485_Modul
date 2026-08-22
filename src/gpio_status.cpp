#include "gpio_status.h"

#include "board_pins.h"

#include <Arduino.h>

static bool g_wlan_up = false;
static bool g_factory_req = false;
static bool g_lbled_on = false;
static unsigned long g_lbled_last = 0;

static bool g_btn_stable = true; // true = losgelassen (HIGH)
static bool g_btn_last_raw = true;
static unsigned long g_btn_change_ms = 0;
static unsigned long g_btn_press_start = 0;
static bool g_btn_holding = false;

void gpio_status_begin() {
  pinMode(PIN_LBLED, OUTPUT);
  digitalWrite(PIN_LBLED, LOW);

  pinMode(PIN_WLAN_LED, OUTPUT);
  digitalWrite(PIN_WLAN_LED, HIGH); // aus (aktiv LOW)

  pinMode(PIN_FACTORY_BTN, INPUT); // externer Pull-up
  g_btn_last_raw = digitalRead(PIN_FACTORY_BTN) != LOW;
  g_btn_stable = g_btn_last_raw;
  g_btn_change_ms = millis();
  g_lbled_last = millis();
}

void gpio_status_set_wlan_link(bool up) {
  g_wlan_up = up;
  digitalWrite(PIN_WLAN_LED, up ? LOW : HIGH);
}

bool gpio_status_factory_requested() { return g_factory_req; }

void gpio_status_clear_factory_request() { g_factory_req = false; }

void gpio_status_factory_confirm_blink() {
  for (int i = 0; i < 6; ++i) {
    digitalWrite(PIN_LBLED, HIGH);
    delay(80);
    digitalWrite(PIN_LBLED, LOW);
    delay(80);
  }
}

void gpio_status_loop() {
  const unsigned long now = millis();
  if ((int32_t)(now - g_lbled_last) >= static_cast<int32_t>(LBLED_BLINK_MS)) {
    g_lbled_last = now;
    g_lbled_on = !g_lbled_on;
    digitalWrite(PIN_LBLED, g_lbled_on ? HIGH : LOW);
  }

  // WLAN-LED nachziehen
  digitalWrite(PIN_WLAN_LED, g_wlan_up ? LOW : HIGH);

  // Taster entprellen (gedrückt = LOW)
  const bool raw_released = digitalRead(PIN_FACTORY_BTN) != LOW;
  if (raw_released != g_btn_last_raw) {
    g_btn_last_raw = raw_released;
    g_btn_change_ms = now;
  }
  if ((int32_t)(now - g_btn_change_ms) >= static_cast<int32_t>(BTN_DEBOUNCE_MS) &&
      raw_released != g_btn_stable) {
    g_btn_stable = raw_released;
    if (!g_btn_stable) {
      g_btn_holding = true;
      g_btn_press_start = now;
    } else {
      g_btn_holding = false;
    }
  }

  if (g_btn_holding && !g_factory_req) {
    if ((int32_t)(now - g_btn_press_start) >= static_cast<int32_t>(FACTORY_HOLD_MS)) {
      g_factory_req = true;
      g_btn_holding = false;
    }
  }
}
