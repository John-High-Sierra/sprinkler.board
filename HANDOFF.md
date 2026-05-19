# SprinKlr-8 Project Handoff
**Date:** 2026-05-17  
**Status:** Firmware v1.3.0 compiled and uploaded — board in crash loop due to un-erased LittleFS partition. One upload with Erase All Flash fixes it.

---

## Immediate Fix (do this first)

The board is stuck in a reboot loop showing:
```
assert failed: lfs_fs_grow_ lfs.c:5263 (block_count >= lfs->block_count)
```

**Root cause:** The v1.3.0 firmware was uploaded via Arduino IDE without "Erase All Flash" enabled. The LittleFS partition at flash offset 0x290000 was not erased, so an incompatible/corrupted image from a previous flash is still there. The firmware code is correct — only the flash needs clearing.

**Fix:**
1. Open Arduino IDE with `sprinkler_controller.ino`
2. Tools → **Erase All Flash Before Sketch Upload** → **Enabled**
3. Click Upload
4. Board will boot cleanly

---

## Project Overview

**Goal:** ESP32-based 8-zone sprinkler controller with a web dashboard and browser-based WiFi setup.

**Hardware in hand:**
- **ACEIRMC B0DTK2PB26** — 8-relay ESP32-WROOM-32E board (the actual product board)
  - Programs via CP2102 USB-TTL adapter on COM3
  - Flash mode: hold IO0, tap EN, release IO0 when "Connecting..." appears in IDE
  - NO auto-reset — cannot use ESP Web Tools directly on this board for programming
- **DORHEA CH340 ESP32 DevKit** — used for testing the web flash setup page (has auto-reset)

**Repository:** https://github.com/John-High-Sierra/sprinkler.board  
**Branch:** `custom-board`  
**Setup/Flash Website:** https://john-high-sierra.github.io/sprinkler.board/

---

## Firmware — v1.3.0

**File:** `esp32_firmware/sprinkler_controller/sprinkler_controller.ino`

**What it does:**
- 8-zone sprinkler scheduling (per-day, per-zone durations)
- Web dashboard served from LittleFS (`/index.html`)
- WiFi setup via Improv WiFi Serial (browser) → falls back to WiFiManager captive portal after 60s
- mDNS: accessible at `http://sprinkler.local`
- OTA updates via Arduino IDE (password: `sprinkler123`)
- Hold IO0 button for 3s at boot to wipe saved WiFi credentials

**Required Arduino libraries:**
- WiFiManager (tzapu)
- ArduinoJson (bblanchon) v6+
- (WebServer, LittleFS, ESPmDNS are built into ESP32 core)

**Note:** Improv WiFi Serial is implemented inline in the firmware — NO external library needed.

**Arduino IDE settings:**
- Board: ESP32 Dev Module
- Partition Scheme: Default 4MB with spiffs
- Upload Speed: 921600
- **Erase All Flash Before Sketch Upload: Enabled** (critical — must be on for every flash)

**GPIO pinout (confirmed):**
- Zone 1=GPIO32, Zone 2=GPIO33, Zone 3=GPIO25, Zone 4=GPIO26
- Zone 5=GPIO27, Zone 6=GPIO14, Zone 7=GPIO12, Zone 8=GPIO13
- Status LED=GPIO23, WiFi Reset Button=GPIO0 (IO0/BOOT button)
- Relay logic: ACTIVE HIGH

---

## Improv WiFi Serial (inline implementation)

Improv WiFi Serial is the protocol that makes ESP Web Tools show a "Configure Wi-Fi" button in the browser after flashing. It is implemented as a C++ namespace `ImprovSerial` directly in the .ino file — no library install needed.

**How it works:**
1. After flashing, ESP Web Tools waits up to 30 seconds for the board to send Improv packets
2. The firmware broadcasts its state (AUTHORIZED or PROVISIONED) every second via Serial
3. If the board has no WiFi credentials → state is AUTHORIZED → browser shows "Configure Wi-Fi"
4. User enters SSID/password in the browser dialog
5. Firmware connects, sends back `http://sprinkler.local` → browser shows "Visit Device"
6. If board already has WiFi → state is PROVISIONED → browser shows "Visit Device" directly

**If Improv times out (60s) with no WiFi configured:**  
The firmware falls back to WiFiManager — opens a hotspot `SprinklerSetup` / password `sprinkler123`. Connect phone to that hotspot and a captive portal opens for WiFi entry.

---

## Web Flash Setup Page

**URL:** https://john-high-sierra.github.io/sprinkler.board/  
**Source:** `docs/index.html`

The page uses ESP Web Tools (`esp-web-install-button`) to flash the board via USB in the browser (Chrome/Edge only).

**manifest.json** (`docs/manifest.json`) — current state:
```json
{
  "name": "SprinKlr-8 Sprinkler Controller",
  "version": "1.3.0",
  "new_install_prompt_erase": false,
  "new_install_improv_wait_time": 30,
  "builds": [{
    "chipFamily": "ESP32",
    "parts": [
      { "path": "bootloader.bin",           "offset": 4096    },
      { "path": "partitions.bin",           "offset": 32768   },
      { "path": "boot_app0.bin",            "offset": 57344   },
      { "path": "sprinkler_controller.bin", "offset": 65536   },
      { "path": "littlefs.bin",             "offset": 2686976 }
    ]
  }]
}
```

Key manifest settings:
- `new_install_prompt_erase: false` — always erases full flash before install (fixes LittleFS corruption)
- `new_install_improv_wait_time: 30` — waits 30s for Improv after flash before timing out
- `littlefs.bin` at offset 2686976 (0x290000) — writes the web UI filesystem in the same flash operation

---

## LittleFS (web UI filesystem)

The web dashboard (`index.html`) lives in LittleFS on the ESP32 flash, not in program memory.

**Partition location:** offset 0x290000, size 0x170000 (1,507,328 bytes)

**Building littlefs.bin** (only needed when index.html changes):
```
C:\Users\harri\AppData\Local\Arduino15\packages\esp32\tools\mklittlefs\4.0.2-db0513a\mklittlefs.exe -c "esp32_firmware\sprinkler_controller\data" -s 1507328 -b 4096 -p 256 "docs\littlefs.bin"
```
**IMPORTANT:** Always use `mklittlefs.exe v4.0.2` from the Arduino ESP32 core. Do NOT use Python `littlefs-python` — it produces an incompatible image that causes the same `lfs_fs_grow_` crash.

---

## LittleFS Crash — Full Explanation

The crash `assert failed: lfs_fs_grow_ lfs.c:5263` happens when:
- The LittleFS partition on flash was built with different parameters than what the firmware expects
- OR the partition was never written (blank flash after incomplete erase)
- OR an old/corrupted `littlefs.bin` is still on flash from a previous upload

Arduino IDE upload only writes the firmware binary at offset 0x10000. It does NOT touch LittleFS at 0x290000 unless "Erase All Flash Before Sketch Upload" is enabled. The web flash (ESP Web Tools) writes ALL parts including `littlefs.bin` when it erases first — this is why web flash with erase is the cleanest method.

---

## Git Status

**Branch:** custom-board  
**Remote:** https://github.com/John-High-Sierra/sprinkler.board

Recent commits:
- `ae1413e` — v1.3.0: Improv WiFi Serial inline, manifest auto-erase
- `743568e` — v1.3.0 firmware binary
- `fadc53c` — v1.3.0: Improv WiFi browser dialog + boot loop fix

**Currently uncommitted:** `docs/sprinkler_controller.bin` is modified locally but not yet pushed. After the next Arduino IDE upload (with Erase All Flash), export the bin and push:

```powershell
cd "D:\Projects_One\_Claude\Sprinkler Controller REV20.1"
git add docs/sprinkler_controller.bin
git -c user.email="harrison.john.b@gmail.com" -c user.name="John Harrison" commit -m "v1.3.0: final firmware binary"
git push origin custom-board
```

---

## Workflow: Full Update Cycle

When making firmware changes:
1. Edit `esp32_firmware/sprinkler_controller/sprinkler_controller.ino`
2. Arduino IDE → **Tools → Erase All Flash Before Sketch Upload → Enabled**
3. Upload (this also tests on the physical board)
4. If successful: **Sketch → Export Compiled Binary**
5. Copy output `.ino.bin` to `docs/sprinkler_controller.bin`
6. `git add`, `git commit`, `git push origin custom-board`
7. Test web flash at https://john-high-sierra.github.io/sprinkler.board/

When making web UI changes (index.html only):
1. Edit `esp32_firmware/sprinkler_controller/data/index.html`
2. Rebuild `littlefs.bin` with mklittlefs command above
3. Copy to `docs/littlefs.bin`
4. `git add`, `git commit`, `git push`
5. OR: use the `/api/update/ui` endpoint to push index.html directly to a connected board

---

## API Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/` | Web dashboard (served from LittleFS) |
| GET | `/api/status` | JSON: current run status |
| GET | `/api/schedule` | JSON: full 7-day schedule |
| POST | `/api/schedule` | Update schedule (JSON array of 7 days) |
| POST | `/api/run_day` | Manually trigger a day's schedule `{"day": 0-6}` |
| POST | `/api/run_zone` | Run single zone `{"zone": 0-7, "duration": 1-120}` |
| POST | `/api/stop_sequence` | Stop current run |
| POST | `/api/toggle_schedule` | Enable/disable auto-scheduling |
| GET | `/api/config` | Get timezone config |
| POST | `/api/config` | Set timezone `{"timezone": "EST5EDT,..."}` |
| GET | `/api/system_info` | IP, uptime, heap, NTP status |
| GET | `/api/version` | Current firmware version |
| POST | `/api/update/ui` | Pull new index.html from GitHub |
| POST | `/api/update/firmware` | OTA firmware update from GitHub releases |
| GET/POST | `/upload` | Browser-based index.html upload (password protected) |

---

## Custom PCB (SprinKlr-8) — Not Started in EasyEDA

Design documents exist in the workspace but the schematic has not been drawn yet.

**Design documents:**
- `SprinKlr8_Design_Spec.docx` — full manufacturing spec, BOM, FCC/CE
- `SprinKlr8_PCB_Design_Guide.docx` — step-by-step EasyEDA guide
- `SprinKlr8_Board_Design_Brief.docx` — complete AI board design prompt
- `ESP32_Relay_Board_Contractor_Brief.docx` — contractor brief

**Circuit summary:**
- Power: 24VAC barrel jack → MB10F rectifier → LM2596S-5.0 (5V) → AMS1117-3.3 (3.3V)
- ESP32-WROOM-32E module
- USB-C with CH340C auto-reset, ESD protection (USBLC6-2SC6)
- ULN2803A relay driver, 8× HF46F-G/5-HS1 SMD relays
- 8× zone indicator LEDs + 1× status LED
- BOOT + RST buttons, 4× M3 mounting holes

**Target:** 1,000 units via JLCPCB, estimated ~$7.10/board assembled, retail $49–69.
