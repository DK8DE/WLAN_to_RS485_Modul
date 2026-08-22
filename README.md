# WLAN_to_RS485_Modul

**Firmware:** 1.5.1 · **Hardware:** 1E1

Netzwerk-Bridge für die **Rotorsteuerung**: Ein ESP32-C5-Modul verbindet die bestehende **RS485-Schnittstelle** des Rotors mit **WLAN/LAN**. Steuerbefehle und Antworten werden transparent durchgereicht — das Rotor-Protokoll bleibt unverändert.

Repository: https://github.com/DK8DE/WLAN_to_RS485_Modul

Protokoll-Referenz: [`Spezifikationen/AT-Kommandos.md`](Spezifikationen/AT-Kommandos.md)

---

## Zweck und Einsatz

Das Modul wird **in der Rotorsteuerung** eingebaut und macht den Rotor über das Netzwerk erreichbar, ohne die vorhandene RS485-Kommunikation zu ersetzen. Technisch ist es eine **transparente Brücke** zwischen **WLAN/TCP/UDP** und **RS485 (UART0)** — Konfiguration, Discovery und Diagnose laufen über einen getrennten Kanal (Web, UDP 8880, AT).

Typische Einsatzszenarien:

| Szenario | Beschreibung |
|----------|--------------|
| **PC über LAN** | Steuer- oder Diagnose-Software am PC verbindet sich per TCP/UDP mit dem Modul im Hausnetz und steuert den Rotor so, als säße der PC direkt am RS485-Bus. |
| **Lokal ↔ Remote** | Ein **lokales Steuergerät** am Rotor (RS485) und eine **Remote-Einspeisung** (Fernsteuerung über Netzwerk) werden über das Modul miteinander verbunden — z. B. Bedienung vor Ort und zusätzlicher Zugriff aus der Ferne über dieselbe RS485-Leitung. |

Das Modul **interpretiert keine Rotorbefehle**; es leitet Bytes nur weiter. Rotor-Controller, Steuersoftware und Bus-Protokoll bleiben unverändert — es kommt lediglich die **Netzwerkverbindung** hinzu.

---

## Hardware

| Signal | GPIO | Verhalten |
|--------|------|-----------|
| LBLED | 6 | Blinkt im Normalbetrieb (~1 s); schnelles Blinken / 3× kurz bei Werkreset |
| WLAN-LED | 7 | aktiv LOW — **blinkt** bei SoftAP, **dauerhaft an** bei STA verbunden |
| Werksreset | 8 | externer Pull-up, gedrückt = LOW → kurzer Tastendruck → Werkseinstellung |
| RS485 | UART0 | 115200 8N1, Richtungsumschaltung im THVD1406DR |

MCU: **ESP32-C5** (`esp32-c5-devkitc-1`, Umgebung `esp32-c5-n4`, 4 MB Flash, Dual-OTA) — Wi‑Fi 6, **2,4 GHz und 5 GHz**.

---

## Architektur

Zwei getrennte Kanäle:

| Kanal | Pfad | Zweck |
|-------|------|--------|
| **Nutzdaten** | WLAN → TCP/UDP → Puffer → UART0 → RS485 (und zurück) | Transparente Bridge, Daten unverändert |
| **Konfiguration** | RS485 + **UDP 8880** + Web-API | Discovery, AT, Binär-Frames, Diagnose, Reset |

- Nutzdaten-Port standardmäßig **8886** (TCP Server/Client oder UDP Server/Client).
- Discovery/Konfiguration: **UDP 8880** (Broadcast/ Unicast) und **RS485** (Binär-Sync + `+++CFG`-Session).
- AT-Befehle nutzen **kein** Web-Passwort, sondern die Geräte-UID (`+++CFG:<UID>`).

---

## Webinterface

Nach dem Flashen Modul-AP verbinden (`ROTOR-<UID>` bzw. Werkname `ROTOR-WIFI-<UID>`), Browser:

**http://192.168.4.1/**

Login: Benutzer **`admin`**, Werkspasswort **`Rotorconfig`** (Tab *Sicherheit*).

| Tab | Inhalt |
|-----|--------|
| **Status** | Gerät, UID, Uptime, WLAN, TCP/UDP, RS485-Zähler, Discovery-UDP |
| **WLAN** | Scan (Hintergrund), SSID/Passwort, Band (Auto/2,4/5 GHz), DHCP/statische IP, Speichern & Verbinden |
| **RS485** | Bridge ein/aus, Packetizer, TCP/UDP-Modus, Ports, Remote-Host |
| **Sicherheit** | Web-Passwort ändern |
| **Update** | OTA-Upload (`firmware.bin`, nicht `firmware.factory.bin`) |
| **Reset** | Neustart oder Werkseinstellung |

**WLAN-Verhalten:** Kein dauerhaftes AP+STA — entweder Infrastruktur (STA) **oder** SoftAP. SoftAP bleibt auf **2,4 GHz** (Kanal 6). Nach STA-Timeout (Default 60 s, 3 Fehlversuche) Fallback auf SoftAP-only.

---

## Funktionsumfang (Stand 1.5.1)

### Bridge & Netzwerk

- TCP Server (1 Client, NODELAY, Keepalive) / TCP Client (Reconnect) / UDP Server / UDP Client / aus
- Packetizer RS485→Netz (Default 2 ms Idle, max. 1024 Byte)
- Bridge aktivieren, TX/RX-Richtung steuerbar
- Byte- und Drop-Zähler (`system_monitor`)

### WLAN

- Modi AP / STA (Legacy APSTA in NVS wird gemappt)
- Bandwahl 2,4 / 5 / Auto, Fallback-AP
- Statische IP (`WiFi.config` nach Disconnect)

### Konfigurationskanal

- Binär-Protokoll (CRC16-CCITT-FALSE): DISCOVER, INFO, STATUS, CONFIG, SAVE, REBOOT, FACTORY_RESET, …
- UDP **8880** + RS485/UART0
- AT-Session nach `+++CFG:<UID>` (Timeout 30 s), adressiert mit `AT@<UID>+…`
- Web-REST-API (`/api/status`, `/api/config`, …)

### System

- UID aus MAC (`C5` + 3 Hex-Bytes), NVS Schema v1
- Status-LEDs, Werksreset-Taster (eigener FreeRTOS-Task)
- Dual-OTA-Partitionen, Web-OTA-Update
- Werkreset: Hardware-Taster, Web, AT `FACTORY`, Binär `FACTORY_RESET`

### Noch offen / geplant

- Windows-Konfigurationstool
- Einzelne AT-/Binär-Befehle laut Spec noch Stubs (z. B. `SET_CONFIG`, `GET_LOG`, Update-Modus)
- Vollständiger Abgleich aller Spec-v4-Punkte in [`AT-Kommandos.md`](Spezifikationen/AT-Kommandos.md)

---

## Werkseinstellungen

| Parameter | Wert |
|-----------|------|
| WLAN | SoftAP `ROTOR-WIFI-<UID>`, IP `192.168.4.1`, 2,4 GHz |
| Netzwerk | TCP Server, Port **8886** |
| Bridge | aktiv, Packetizer 2 ms / 1024 Byte |
| Web | `admin` / `Rotorconfig` |

---

## Bauen & Flashen

PlatformIO + **pioarduino** (ESP32-C5). Empfohlen unter Windows:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -t upload
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" device monitor
```

COM-Port in `platformio.ini` (`upload_port` / `monitor_port`) anpassen.

### Partition / OTA

- `partitions.csv`: Dual-OTA (~1,88 MB je App-Slot)
- **Erstes Flashen** nach Partition-Wechsel: USB mit `firmware.bin` oder `firmware.factory.bin`
- **Web-OTA:** nur `.pio/build/esp32-c5-n4/firmware.bin` — **nicht** `firmware.factory.bin`

### Hinweis Windows / doppeltes Python-`pio`

Falls `ImportError: littlefs` auftritt, immer die `pio.exe` aus `.platformio\penv\Scripts\` verwenden.

### UART0 / Monitor

Boot-Logs und RS485 teilen sich **UART0**. Nach dem Start keine Debug-Ausgaben auf dem Bus — sonst Störung der Bridge-Daten.

---

## Projektstruktur

```
src/
  main.cpp
  Version.h / board_pins.h
  device_identity.*     # UID, MAC, Bus-Adresse
  app_config.*          # NVS Schema v1
  gpio_status.*         # LEDs, Werksreset-Taster
  wifi_manager.*        # AP/STA, Scan, Band, Fallback
  network_bridge.*      # TCP/UDP Nutzdatenkanal
  rs485_uart.*          # UART0-Tasks, Bridge-Queues
  packetizer.*          # Idle-Paketierung RS485→Netz
  config_frame.*        # Binär-Frame Codec + CRC
  config_handlers.*     # DISCOVER, INFO, STATUS, …
  config_ingress.*      # Demux Konfig vs. Bridge auf UART
  config_udp.*          # UDP 8880 Discovery/Config
  at_command.*          # AT-Parser (+CFG-Session)
  system_monitor.*      # Statistik-Zähler
  web_server.* / web_content.h
Spezifikationen/
  AT-Kommandos.md       # AT + Binär-Protokoll (Referenz)
partitions.csv
platformio.ini
```

---

## Kurzer Test

1. Flashen → LBLED blinkt (~1 s).
2. SoftAP `ROTOR-…` verbinden → WLAN-LED blinkt → Browser `http://192.168.4.1/`
3. Tab **WLAN** → Netz scannen → SSID/Passwort → **Speichern / Verbinden**
4. Tab **Status** → STA-IP und TCP/UDP-Status prüfen
5. TCP/UDP-Client auf Port 8886 → transparente Bytes PC ↔ RS485
6. Tab **Update** → OTA mit `firmware.bin` testen (optional)
7. **Werkreset:** Hardware-Taster kurz drücken **oder** Tab **Reset** → danach wieder SoftAP `192.168.4.1`
