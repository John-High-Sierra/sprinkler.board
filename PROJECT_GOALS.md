# SprinKlr-8 Project Goals

## What We Are Building

An ESP32-based 8-zone sprinkler controller with a web dashboard, designed for home use and eventually sold as a commercial product.

---

## Hardware

**Current test board:** ACEIRMC B0DTK2PB26
- ESP32-WROOM-32E with 8 relay outputs
- Programs via CP2102 USB-TTL adapter (manual IO0/EN button press required)
- Relay pins: GPIO 32, 33, 25, 26, 27, 14, 12, 13 (zones 1-8)
- Status LED: GPIO23
- Active HIGH relay logic

**Future:** Custom PCB (SprinKlr-8) manufactured via JLCPCB, ~$7/board at 1,000 units, retail $49-69.

---

## What the Firmware Must Do

1. **Connect to home WiFi** — via a captive portal (phone connects to SprinklerSetup hotspot, opens browser, enters home WiFi credentials)
2. **Serve a web dashboard** at `http://sprinkler.local` for scheduling and control
3. **Run 8 zones** on a per-day schedule with configurable durations per zone
4. **Manual control** — run any zone or day sequence from the dashboard
5. **NTP time sync** — accurate scheduling based on real time
6. **OTA updates** — firmware updateable over WiFi without USB

---

## Web Dashboard

- Served directly from the ESP32 (no cloud required)
- Shows zone status, run controls, weekly schedule
- Timezone configurable
- Accessible at `http://sprinkler.local` on the home network

---

## Setup Experience (Goal)

A new user should be able to:
1. Plug in the board
2. Connect phone to `SprinklerSetup` hotspot
3. Enter home WiFi credentials in the browser
4. Access the dashboard at `http://sprinkler.local`

No app, no cloud account, no complicated steps.

---

## Web Flash Page (Goal)

A GitHub Pages website at `https://john-high-sierra.github.io/sprinkler.board/` that lets users flash the firmware directly from Chrome/Edge via USB — no Arduino IDE needed.

- Uses ESP Web Tools (`esp-web-install-button`)
- Flashes firmware + web UI filesystem in one operation
- Must erase flash completely before writing to avoid LittleFS corruption

---

## Current Blocking Problem

The LittleFS filesystem partition on the ESP32 keeps getting corrupted, causing a crash loop on boot:
```
assert failed: lfs_fs_grow_ lfs.c:5263
```

**Root cause:** Arduino IDE uploads only overwrite the firmware binary. The LittleFS partition at flash offset 0x290000 is left untouched, so old/incompatible data persists.

**Fix that works:** Arduino IDE → Tools → Erase All Flash Before Sketch Upload → Enabled. This wipes everything before uploading.

**Longer term fix:** Embed the HTML dashboard directly in the firmware binary (no LittleFS needed for the UI), so a standard upload is always clean. LittleFS is still used for saving the schedule and config.

---

## Current Firmware State

- Version 1.4.0 (in progress)
- HTML dashboard embedded in firmware via PROGMEM (`html_content.h`)
- WiFiManager captive portal for WiFi setup, portal timeout disabled for testing
- No Improv WiFi (removed — caused complications)
- Partition scheme must be set to **No OTA (2MB APP/2MB SPIFFS)** due to firmware size

---

## Repository

- GitHub: https://github.com/John-High-Sierra/sprinkler.board
- Branch: `custom-board`
- Setup page: https://john-high-sierra.github.io/sprinkler.board/
