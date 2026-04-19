# Sprinkler Controller REV20.1 — Project State (Shelf Document)

**Date shelved:** 2026-04-19  
**Board:** ACEIRMC ESP32-WROOM-32E 8-Channel Relay (Amazon B0DTK2PB26)  
**Arduino IDE:** 2.x  
**ESP32 core:** esp32 by Espressif Systems **3.3.8**

---

## Where We Are

The firmware compiles cleanly and the board is running. The web UI is deployed and works. The project is at the point where the new mobile-first UI (`index.html`) has just been rewritten and needs final review + deployment.

### What is working
- Firmware compiles with zero errors on ESP32 core 3.3.8
- Board boots, connects to WiFi, syncs NTP
- LittleFS filesystem upload works (UI served from board)
- All relay zones activate correctly via manual run
- Automatic schedule runs via FreeRTOS task
- Stop sequence works
- Web UI accessible at `http://10.110.201.40/`
- WiFi network reset via IO0 button (confirmed working — see note 8 below)

### What still needs to be done
1. **Review the new UI** — `index.html` was rewritten to match the mockup in `data/sprinkler_mockup_v2.html`. Open it in a browser to preview (`PREVIEW = true` so all buttons work with mock data).
2. **Set `PREVIEW = false`** in `data/index.html` (line ~12 — the `const PREVIEW = true;` near top of `<script>`)
3. **Upload LittleFS** — Ctrl+Shift+P → "Upload LittleFS image" in Arduino IDE to push the new `index.html` to the board
4. **Test on hardware** — navigate to `http://10.110.201.40/` and confirm new UI works end-to-end

---

## Key Files

| File | Purpose |
|------|---------|
| `esp32_firmware/sprinkler_controller/sprinkler_controller.ino` | Main firmware — DO NOT change library includes back to ESPAsyncWebServer |
| `esp32_firmware/sprinkler_controller/data/index.html` | Web UI — PREVIEW flag must be `false` before uploading to board |
| `esp32_firmware/sprinkler_controller/data/sprinkler_mockup_v2.html` | Reference mockup the new UI is based on |
| `docs/superpowers/plans/2026-04-18-webserver-migration.md` | Migration plan (all 4 tasks complete) |
| `esp32_firmware/README.md` | Hardware wiring + setup instructions (note: API table still shows old path-param routes — see below) |

---

## Critical Technical Notes (DO NOT FORGET)

### 1. WebServer.h — NOT ESPAsyncWebServer
The firmware was migrated from `ESPAsyncWebServer` to the built-in `WebServer.h` because ESPAsyncWebServer crashes on ESP32 core 3.x (`tcp_alloc` assert failure). **Never revert this.** Required libraries are now only:
- WiFiManager (tzapu)
- ArduinoJson (bblanchon) v6+
- NTPClient (Fabrice Weinberg)
- WebServer.h and LittleFS are built into the core — no install needed

### 2. mbedtls _ret functions — DO NOT FIX
`D:\JohnH\Documents\Arduino\libraries\ESPAsyncWebServer\src\WebAuthentication.cpp` still uses the old `mbedtls_md5_starts_ret` / `mbedtls_md5_update_ret` / `mbedtls_md5_finish_ret` function names. Changing them to non-`_ret` versions **causes the ESP32 to reboot continuously**. Leave them alone. ESPAsyncWebServer is no longer used by the firmware anyway.

### 3. API routes changed (path params → JSON body)
Two endpoints changed from path-parameter style to POST with JSON body:

| Old (ESPAsyncWebServer) | New (WebServer.h) |
|------------------------|-------------------|
| `POST /api/run_day/<0-6>` | `POST /api/run_day` body: `{"day": 0}` |
| `POST /api/run_zone/<0-7>/<min>` | `POST /api/run_zone` body: `{"zone": 0, "duration": 10}` |

The `index.html` already uses the new format. The `README.md` API table still shows the old routes — update it if needed.

### 4. LittleFS upload plugin
Plugin VSIX install location: `C:\Users\harri\.arduinoIDE\plugins\`  
(NOT `AppData\Roaming\Arduino IDE\plugins\` — that location does NOT work)  
After copying VSIX there and restarting Arduino IDE, the command appears in Ctrl+Shift+P as "Upload LittleFS image".

### 5. Flashing procedure (no USB on this board)
The board has no USB port. Needs a CP2102 USB-to-serial adapter:
- 6-pin header near IO0/EN buttons (bottom-right of board)
- Pin order: 3V3 | GND | TX | RX | IO0 | EN
- Connect: CP2102 TX→ESP RX, CP2102 RX→ESP TX, GND→GND, 3.3V→3V3
- **Flash mode:** hold IO0, tap EN, release IO0, then click Upload in Arduino IDE

### 6. GPIO / relay pinout (confirmed)
```
Relay 1 = GPIO32  |  Relay 5 = GPIO27
Relay 2 = GPIO33  |  Relay 6 = GPIO14
Relay 3 = GPIO25  |  Relay 7 = GPIO12  ← strapping pin, boots LOW — safe
Relay 4 = GPIO26  |  Relay 8 = GPIO13
Status LED = GPIO23
Logic: ACTIVE HIGH (RELAY_ACTIVE_LOW = false)
```

### 8. WiFi network reset procedure (confirmed working)
To switch the board to a new WiFi network without reflashing:

1. Press **EN** and release it (do NOT hold IO0 during this — that triggers download/flash mode)
2. You have a **2-second window** — press and hold **IO0**
3. Serial Monitor will show `IO0 detected — keep holding for 3 seconds...` and count down
4. After 3 seconds the credentials are wiped and `SprinklerSetup` hotspot opens
5. Connect from phone → pick new network → enter password → board reboots on new network

If IO0 is released before 3 seconds the reset is cancelled and the board boots normally.  
Serial Monitor (115200 baud) shows every step — use it to confirm the process is working.

### 9. Board IP
Current static assignment: **10.110.201.40**  
(Assigned by router DHCP reservation — may need updating if router changes)

---

## Arduino IDE Settings
```
Board:            ESP32 Dev Module
Partition Scheme: Default 4MB with spiffs
Upload Speed:     921600
Port:             whichever COM port the CP2102 appears on
```

---

## UI Design (new index.html)
- Dark navy: `#1a1a2e` background, `#16213e` cards, `#4fc3f7` accent (light blue), `#4caf50` active/green
- Font: Segoe UI
- Bottom navigation: Dashboard | Schedule | Manual | Settings
- Dashboard: 2×2 stat grid + zone cards with Run/Stop per zone
- Schedule: 7 expandable day cards with enable toggle, time picker, per-zone duration inputs
- Manual: zone selector + duration input + Start Zone / Stop All / Run Day Sequence
- Settings: system info display + editable zone names (stored in localStorage)
- Mobile-first, designed to match `sprinkler_mockup_v2.html`

---

## Resuming This Project

1. Open `esp32_firmware/sprinkler_controller/sprinkler_controller.ino` in Arduino IDE
2. Open `data/index.html` in a browser to review the UI (it has `PREVIEW = true`)
3. When happy with UI:
   - Set `PREVIEW = false` in `index.html`
   - Ctrl+Shift+P → "Upload LittleFS image" to push to board
4. Navigate to `http://10.110.201.40/` to verify

---

## History

| Date | Milestone |
|------|-----------|
| 2025-09-01 | Working version using ESPAsyncWebServer + ESP32 core 3.3.0 |
| 2026-04-18 | Migrated to WebServer.h (core 3.3.8) — ESPAsyncWebServer removed |
| 2026-04-18 | New mobile-first UI written to replace placeholder layout |
| 2026-04-19 | WiFi network reset via IO0 button added and confirmed working |
