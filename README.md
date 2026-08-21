# WLAN_to_RS485_Modul

**Version:** 1.0.0 (Phase 1 – Kernbrücke)

Firmware für ein ESP32-C5-Modul: transparente **WLAN/TCP ↔ RS485 (UART0)**-Brücke.

Remote: https://github.com/DK8DE/WLAN_to_RS485_Modul

## Hardware

| Signal | GPIO | Verhalten |
|--------|------|-----------|
| LBLED | 6 | Blinkt im Normalbetrieb (~1 s) |
| WLAN-LED | 7 | aktiv LOW (LOW = an), an bei AP/STA-Link |
| Werksreset | 8 | externer Pull-up, ≥5 s halten → Factory Reset |
| RS485 | UART0 | 115200 8N1, Richtungsumschaltung im THVD1406DR |

MCU: ESP32-C5 (`esp32-c5-devkitc-1`, Umgebung `esp32-c5-n4`).

## Phase-1-Funktionen

- Status-I/O (LEDs, Werksreset-Taster)
- Geräte-UID aus MAC (`C5` + 3 Bytes), NVS-Konfiguration Schema v1
- WLAN: AP / STA / APSTA, Fallback-AP nach STA-Timeout (Default 60 s)
- TCP Server (Port 8886, max. 1 Client, TCP_NODELAY) / TCP Client (Reconnect) / Disabled
- Packetizer RS485→TCP (Idle Default 2 ms, max. 1024 Byte)
- Echo-Unterdrückung (einfach), Byte-/Drop-Zähler

**Noch nicht in Phase 1:** Binär-Konfigprotokoll, AT-Kommandos, Web-UI/API, OTA, Windows-Tool.

## Werkseinstellungen

- WLAN: Access Point `ROTOR-<UID>`, IP `192.168.4.1`
- Netzwerk: TCP Server, Port **8886**
- Bridge aktiv, Packetizer 2 ms / 1024 Byte

## Bauen

PlatformIO + pioarduino (ESP32-C5). Empfohlen:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -t upload
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" device monitor
```

COM-Port in `platformio.ini` (`upload_port` / `monitor_port`) anpassen.

### Hinweis Windows / doppeltes Python-`pio`

Falls `ImportError: littlefs` auftritt, immer die `pio.exe` aus `.platformio\penv\Scripts\` verwenden (siehe oben).

## Struktur

```
src/
  main.cpp
  board_pins.h
  Version.h
  device_identity.*
  app_config.*
  gpio_status.*
  wifi_manager.*
  network_bridge.*
  packetizer.*
  rs485_uart.*
  system_monitor.*
```

## Kurzer Test (Phase 1)

1. Modul flashen → LBLED blinkt.
2. AP `ROTOR-…` verbinden → WLAN-LED an, IP `192.168.4.1`.
3. TCP-Client auf Port 8886 → Bytes PC↔UART0/RS485 transparent.
4. Taster ≥5 s → Factory Reset + Neustart.
