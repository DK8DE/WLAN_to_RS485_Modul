# WLAN_to_RS485_Modul

**Firmware:** 1.5.1 · **Hardware:** 1E1

Netzwerk-Bridge für die **Rotorsteuerung**: Verlängert den **RS485-Pfad** der PC-Steuerung über **LAN/WLAN** bis zum Rotor (direkt am Schreibtisch oder in einer **Remotebox** mit Strom- und Signalversorgung). Steuerbefehle und Antworten werden transparent durchgereicht — das Rotor-Protokoll bleibt unverändert.

Repository: https://github.com/DK8DE/WLAN_to_RS485_Modul

Protokoll-Referenz: [`Spezifikationen/AT-Kommandos.md`](Spezifikationen/AT-Kommandos.md)

---

## Zweck und Einsatz

Am **PC** läuft ein **Steuermodul** (Rotorsteuerung). Es kann per **USB**, **LAN** oder **WLAN** angebunden sein — intern arbeitet die Steuerung durchgängig mit **RS485**.

Der **Rotor** kann auf zwei Wegen an die Steuerung angebunden werden:

1. **Direkt** — Rotor und Steuerung sind lokal verbunden (typisch USB/Serial oder RS485 am Schreibtisch).
2. **Über Netzwerk** — Zwischen Steuerung und Rotor sitzt eine **Netzwerkbrücke** (dieses Modul), meist in einer **Remotebox** am Rotorstandort. Die Verbindung zur Steuerung erfolgt per **LAN oder WLAN**; am Rotor hängt weiterhin **RS485**. Die Remotebox **speist den Rotor mit Strom** und führt das **RS485-Signal** zum Antrieb.

Technisch ist das Modul eine **transparente Brücke** zwischen **WLAN/LAN (TCP/UDP)** und **RS485 (UART0)**. Es wertet keine Rotorbefehle aus — Steuersoftware, Bus-Protokoll und Rotor-Controller bleiben unverändert.

### Einsatzszenarien

| Szenario | Aufbau |
|----------|--------|
| **LAN statt USB** | Die Steuerung hat nur ein **LAN-Modul** (kein direktes USB/Serial zum Rotor). Statt Kabel zum Rotor verbindet sich die Steuerung per **LAN/WLAN** mit dem Modul am Rotor — der RS485-Pfad endet physisch am Modul, logisch wie gewohnt in der Software. |
| **Remotebox am Rotor** | Steuerung am PC (USB/LAN/WLAN) → **Netzwerk** → Modul in der **Remotebox** vor Ort → **Stromversorgung + RS485** zum Rotor. So kann der Rotor entfernt vom Schreibtisch betrieben werden; die Steuerung „sieht“ weiterhin eine RS485-Verbindung, nur über die Brücke. |

```
Steuerung (PC)                    Remotebox / Modul am Rotor
┌─────────────────┐              ┌──────────────────────────┐
│ Steuermodul     │   LAN/WLAN   │ WLAN_to_RS485_Modul      │
│ (intern RS485)  │ ───────────► │  + Stromversorgung Rotor │
└─────────────────┘              │  + RS485 zum Antrieb     │
                                 └──────────────────────────┘
```

Konfiguration, Discovery und Diagnose des Moduls laufen über einen **getrennten Kanal** (Web-UI, UDP 8880, AT) — nicht über den transparenten Nutzdatenkanal.

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

### GitHub Actions / fertige Firmware

Bei jedem Push auf `main` (und bei manuellem Start) baut die [GitHub Action](.github/workflows/build.yml) die Firmware und veröffentlicht ein **GitHub Release** — Version aus `src/Version.h` (aktuell als Tag `v1.5.1`).

| Datei im Release | Inhalt |
|------------------|--------|
| `WLAN_to_RS485_v*_firmware.bin` | Web-OTA / USB-Flash (App-Partition) |
| `WLAN_to_RS485_v*_factory.bin` | Erstes Flashen (Bootloader + Partition + App) |
| `WLAN_to_RS485_v*_source.zip` | Quellcode (ZIP) |
| `WLAN_to_RS485_v*_source.tar.gz` | Quellcode (tar.gz) |
| `BUILD_INFO.txt` | Version, Git-SHA, Build-Zeit |

**Download:** [Releases](https://github.com/DK8DE/WLAN_to_RS485_Modul/releases) (immer aktuelles `v*` der Firmware-Version)  
**Zusätzlich:** [Actions](https://github.com/DK8DE/WLAN_to_RS485_Modul/actions) → Artifacts (90 Tage)

Neue Firmware-Version: `src/Version.h` anpassen und nach `main` pushen — der Tag `v*` wird automatisch gesetzt/aktualisiert.

Optional manuell taggen (gleiche Version wie in `Version.h`):

```bash
git tag v1.5.1
git push origin v1.5.1
```

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
