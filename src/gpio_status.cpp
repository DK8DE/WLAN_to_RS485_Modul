#include "gpio_status.h"

#include "board_pins.h"

#include <Arduino.h>

static WlanLedMode g_wlan_led_mode = WlanLedMode::OFF;
static bool g_factory_req = false;
static bool g_lbled_on = false;
static bool g_wlan_blink_on = false;
static unsigned long g_lbled_last = 0;
static unsigned long g_wlan_blink_last = 0;

static bool g_btn_stable = true; // true = losgelassen (HIGH)
static bool g_btn_last_raw = true;
static unsigned long g_btn_change_ms = 0;

static void wlan_led_write(bool on) {
  digitalWrite(PIN_WLAN_LED, on ? LOW : HIGH); // aktiv LOW
}

void gpio_status_begin() {
  pinMode(PIN_LBLED, OUTPUT);
  digitalWrite(PIN_LBLED, LOW);

  pinMode(PIN_WLAN_LED, OUTPUT);
  wlan_led_write(false);

  pinMode(PIN_FACTORY_BTN, INPUT); // externer Pull-up
  g_btn_last_raw = digitalRead(PIN_FACTORY_BTN) != LOW;
  g_btn_stable = g_btn_last_raw;
  g_btn_change_ms = millis();
  g_lbled_last = millis();
  g_wlan_blink_last = millis();
}

void gpio_status_set_wlan_led(WlanLedMode mode) { g_wlan_led_mode = mode; }

bool gpio_status_factory_requested() { return g_factory_req; }

void gpio_status_clear_factory_request() { g_factory_req = false; }

void gpio_status_factory_confirm_blink() {
  for (int i = 0; i < 3; ++i) {
    digitalWrite(PIN_LBLED, HIGH);
    delay(40);
    digitalWrite(PIN_LBLED, LOW);
    delay(40);
  }
}

void gpio_status_loop() {
  const unsigned long now = millis();

  if (g_factory_req) {
    digitalWrite(PIN_LBLED, HIGH);
  } else if ((int32_t)(now - g_lbled_last) >= static_cast<int32_t>(LBLED_BLINK_MS)) {
    g_lbled_last = now;
    g_lbled_on = !g_lbled_on;
    digitalWrite(PIN_LBLED, g_lbled_on ? HIGH : LOW);
  }

  switch (g_wlan_led_mode) {
    case WlanLedMode::STA_ON:
      wlan_led_write(true);
      break;
    case WlanLedMode::AP_BLINK:
      if ((int32_t)(now - g_wlan_blink_last) >= static_cast<int32_t>(WLAN_LED_BLINK_MS)) {
        g_wlan_blink_last = now;
        g_wlan_blink_on = !g_wlan_blink_on;
      }
      wlan_led_write(g_wlan_blink_on);
      break;
    default:
      wlan_led_write(false);
      break;
  }

  const bool raw_released = digitalRead(PIN_FACTORY_BTN) != LOW;
  if (raw_released != g_btn_last_raw) {
    g_btn_last_raw = raw_released;
    g_btn_change_ms = now;
  }
  if ((int32_t)(now - g_btn_change_ms) >= static_cast<int32_t>(BTN_DEBOUNCE_MS) &&
      raw_released != g_btn_stable) {
    const bool was_released = g_btn_stable;
    g_btn_stable = raw_released;
    if (was_released && !g_btn_stable && !g_factory_req) {
      g_factory_req = true;
      digitalWrite(PIN_LBLED, HIGH);
    }
  }
}
