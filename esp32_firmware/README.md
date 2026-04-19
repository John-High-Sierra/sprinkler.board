# ESP32 Sprinkler Controller Firmware

Target board: **ACEIRMC ESP32-WROOM-32E 8-Channel Relay (B0DTK2PB26)**

---

## Directory Structure

```
esp32_firmware/
├── relay_pin_finder/
│   └── relay_pin_finder.ino    ← Flash this FIRST to identify GPIO pins
├── sprinkler_controller/
│   ├── sprinkler_controller.ino  ← Main firmware
│   └── data/
│       └── index.html            ← Web UI (upload via LittleFS)
└── README.md
```

---

## Step 1 — Arduino IDE Setup

1. Install **Arduino IDE 2.x**
2. Add ESP32 board support:
   - File → Preferences → Additional Board Manager URLs:
     `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Tools → Board Manager → search "esp32" → install **esp32 by Espressif Systems**
3. Select board: **Tools → Board → ESP32 Arduino → ESP32 Dev Module**
4. Partition scheme: **Tools → Partition Scheme → Default 4MB with spiffs**

### Required Libraries (install via Tools → Manage Libraries)

| Library | Author | Version |
|---------|--------|---------|
| ESPAsyncWebServer | me-no-dev | latest |
| AsyncTCP | me-no-dev | latest |
| WiFiManager | tzapu | latest |
| ArduinoJson | bblanchon | 6.x |
| NTPClient | Fabrice Weinberg | latest |

> **Note:** ESPAsyncWebServer is not in the Library Manager. Download from:
> https://github.com/me-no-dev/ESPAsyncWebServer
> and https://github.com/me-no-dev/AsyncTCP
> Then install as ZIP: Sketch → Include Library → Add .ZIP Library

---

## Step 2 — Identify Your Relay Pins

1. Open `relay_pin_finder/relay_pin_finder.ino`
2. Flash to board
3. Open Serial Monitor (115200 baud)
4. Watch which GPIO corresponds to each relay click
5. Note: listen for ACTIVE LOW (Phase 1) vs ACTIVE HIGH (Phase 2)

**Most LC-Tech style boards use:**
- GPIO: `32, 33, 25, 26, 27, 14, 12, 13` (IN1 through IN8)
- Logic: **Active LOW** (relay energises when GPIO = LOW)

---

## Step 3 — Update Pin Config

In `sprinkler_controller.ino`, update these lines:

```cpp
const int RELAY_PINS[8] = {32, 33, 25, 26, 27, 14, 12, 13};
#define RELAY_ACTIVE_LOW true
```

Also update your timezone offset:
```cpp
#define TZ_OFFSET_SEC  -18000   // UTC-5 EST
                                // UTC-6 CST = -21600
                                // UTC-7 MST = -25200
                                // UTC-8 PST = -28800
```

---

## Step 4 — Flash the Main Firmware

1. Open `sprinkler_controller/sprinkler_controller.ino`
2. Compile and upload

---

## Step 5 — Upload the Web UI

The web interface lives in `sprinkler_controller/data/index.html` and is served
from LittleFS on the ESP32.

To upload it:
1. Install the **LittleFS Upload Tool** for Arduino IDE:
   https://github.com/lorol/arduino-esp32fs-plugin
2. Place `index.html` in the `data/` folder
3. Tools → ESP32 Sketch Data Upload

---

## Step 6 — First Boot / WiFi Setup

1. On first boot, the board creates a hotspot called **SprinklerSetup**
   (password: `sprinkler123`)
2. Connect to it with your phone or laptop
3. A captive portal opens — enter your home WiFi credentials
4. Board reboots and connects to your WiFi
5. Open a browser and go to `http://sprinkler/` (or use the IP shown in Serial Monitor)

To reset WiFi credentials later, uncomment `wm.resetSettings();` in setup() and reflash.

---

## API Reference

All endpoints match the original Raspberry Pi Flask API for easy frontend migration.

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/status` | Current run status |
| GET | `/api/schedule` | Full weekly schedule |
| POST | `/api/schedule` | Update schedule (JSON array of 7 days) |
| POST | `/api/run_day/<0-6>` | Manually run a day's schedule |
| POST | `/api/run_zone/<0-7>/<min>` | Run single zone for N minutes |
| POST | `/api/stop_sequence` | Stop any running sequence |
| POST | `/api/toggle_schedule` | Enable/disable automatic scheduling |
| GET | `/api/get_time` | Current time (NTP) |
| GET | `/api/system_info` | Board info, IP, uptime, etc. |
| POST | `/api/relay_test/<zone>/<0\|1>` | Direct relay toggle for testing |

### Schedule JSON format (same as original Pi system)

```json
[
  { "is_active": true,  "hour": 7, "minute": 0, "durations": [10,10,5,0,0,0,0,0] },
  { "is_active": false, "hour": 7, "minute": 0, "durations": [10,10,5,0,0,0,0,0] },
  ...7 days total (0=Monday, 6=Sunday)
]
```

---

## Wiring Notes

The ACEIRMC board has an onboard AC/DC converter. Power connections:

| Board terminal | Connect to |
|---------------|-----------|
| L / N / GND   | Mains 120/240V AC (or DC input — check board markings) |
| Relay COM/NO  | Zone valve wires |

> The ESP32 and relays are powered from the onboard converter — no separate USB
> power needed in the field. Use USB only for programming.

---

## Troubleshooting

**Relays click when board boots** → Board is active-low. Set `RELAY_ACTIVE_LOW true` and make sure all pins initialise to HIGH in `setupRelayPins()`. This is already handled in the firmware.

**Can't connect to WiFiManager portal** → Hold the board's BOOT/FLASH button for 3 seconds while powering on to force portal mode (some boards). Or uncomment `wm.resetSettings()`.

**NTP not syncing** → Check your router allows outbound UDP port 123. The controller falls back to showing "unsynced" time but scheduling won't fire until NTP connects.

**LittleFS upload fails** → Make sure you're using the correct plugin version for Arduino IDE 2.x vs 1.x. They are different plugins.
