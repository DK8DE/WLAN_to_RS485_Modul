#include <Arduino.h>

#include "Version.h"
#include "app_config.h"
#include "at_command.h"
#include "config_ingress.h"
#include "config_udp.h"
#include "device_identity.h"
#include "gpio_status.h"
#include "network_bridge.h"
#include "rs485_uart.h"
#include "system_monitor.h"
#include "web_server.h"
#include "wifi_manager.h"

DeviceIdentity g_identity;

void setup() {
  // UART0 zuerst (RS485 + Boot-Log auf demselben Port)
  rs485_uart_begin();
  delay(200);

  Serial.printf("\nWLAN_to_RS485_Modul v%s hw%s\n", FW_VERSION_STR, HW_VERSION_STR);

  device_identity_init(&g_identity);
  Serial.printf("UID=%s MAC=%s\n", g_identity.uid, g_identity.mac_str);

  app_config_init_runtime(g_identity);
  const AppConfig& cfg = app_config_runtime_const();
  Serial.printf("Name=%s Bus=%u NetMode=%u Port=%u\n", cfg.device_name, cfg.bus_address,
                static_cast<unsigned>(cfg.net_mode), cfg.local_port);

  gpio_status_begin();
  system_monitor_begin();
  config_ingress_begin();
  config_udp_begin();

  wifi_manager_begin();
  network_bridge_begin();
  web_server_begin();

  rs485_uart_start_tasks();
  wifi_manager_start_task();
  network_bridge_start_tasks();
  web_server_start_task();
  config_udp_start_task();

  char ip[16];
  wifi_manager_get_ip(ip, sizeof(ip));
  Serial.printf("WiFi IP=%s TCP connected=%d\n", ip[0] ? ip : "(warte auf STA…)",
                network_bridge_tcp_connected());
  Serial.printf("Discovery UDP port=%u (RS485 binary DISCOVER + AT after +++CFG)\n",
                static_cast<unsigned>(config_discovery_udp_port()));
  Serial.println("Web UI: SoftAP http://192.168.4.1/ oder STA-IP (nie beides)");
  Serial.println("Bridge running (transparent UART0 <-> TCP)");
}

void loop() {
  gpio_status_loop();
  system_monitor_loop();

  if (gpio_status_factory_requested()) {
    gpio_status_clear_factory_request();
    gpio_status_factory_confirm_blink();
    AppConfig cfg{};
    app_config_factory_reset(&cfg, g_identity);
    delay(100);
    ESP.restart();
  }

  delay(10);
}
