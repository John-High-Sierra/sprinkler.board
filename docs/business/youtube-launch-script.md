# YouTube Launch Video — Script & Metadata

## Video metadata

**Title:**
I Built a $54 Sprinkler Controller That Works Without Internet (ESP32, No Cloud)

**Description (first 3 lines show before "Show more" — make them count):**
SprinKlr-8: an 8-zone WiFi sprinkler controller with local API, no cloud,
no subscription. Works with Home Assistant. Firmware on GitHub.

Buy on Tindie: [Tindie link — add before uploading]
GitHub: https://github.com/John-High-Sierra/sprinkler.board

Chapters:
0:00 Hook — works offline demo
0:45 The problem with cloud controllers
2:00 The hardware — custom ESP32 PCB
4:00 Web interface walkthrough
6:00 Home Assistant integration
7:30 Where to get it
9:00 Call to action

**Tags:**
esp32, home assistant, smart sprinkler, irrigation controller, diy smart home,
local api, no cloud, open source, rachio alternative, esp32 project, arduino,
wifi sprinkler, home automation, jlcpcb, pcb design

**Thumbnail text overlay:**
No Cloud. No Subscription. $54.
(Background: board photo + web UI screenshot side by side)

**Category:** Science & Technology

---

## Script

*Target length: 8–10 minutes. Cut ruthlessly — dead air kills retention.*

---

### 0:00 — Hook (45 seconds)

[SCREEN: Web UI open on phone. All zones visible. WiFi router visible in background — pull the ethernet cable out of the router mid-shot.]

**VO/ON CAMERA:**
"Watch this."

[Pull ethernet cable. Web UI still loads. Tap a zone — relay clicks. Zone activates.]

"Internet's gone. Sprinklers still work. Schedule still runs. This is SprinKlr-8 —
an 8-zone sprinkler controller I designed and built myself, and it never needs
the internet to do its job."

---

### 0:45 — The problem (75 seconds)

[SCREEN: Rachio app showing "Connection lost" or similar. B-roll of someone frustrated at phone.]

**VO:**
"Most smart sprinkler controllers — Rachio, B-hyve, the rest — require their
cloud servers to function. Your schedule lives on their servers. If their
internet goes down, your schedule stops. If your internet goes down during a
heat wave, your lawn dies. And Rachio wants $30 a year for the privilege.

I tried to use Rachio with Home Assistant. Turns out, even the 'local control'
option still calls out to their cloud. That was the last straw."

---

### 2:00 — The hardware (2 minutes)

[PHYSICAL: Hold up the custom PCB. Point out key components as you name them.]

**VO/ON CAMERA:**
"So I designed a custom PCB around the ESP32 — Espressif's WiFi microcontroller.
Here's what's on the board:

The ESP32-WROOM-32E module here handles WiFi and runs the web server.
This chip — the CH340C — gives you a USB-C programming port with auto-reset,
so you never need to hold a boot button to flash firmware.
The ULN2803A here is a Darlington transistor array that drives these eight
SPST relays — one per irrigation zone.
Each relay switches 24VAC to your valve solenoid.

The board was fabricated and assembled by JLCPCB. All SMD components, picked
and placed by their machines. I designed it in EasyEDA."

[Show board arriving in JLCPCB packaging. Show before/after: off-the-shelf relay board vs. custom PCB.]

---

### 4:00 — Web interface walkthrough (2 minutes)

[SCREEN CAPTURE: Open browser, navigate to board IP. Show each tab.]

**VO/ON CAMERA:**
"The web interface runs directly from the board — there's no server, no cloud,
no app to download. You just open a browser.

Dashboard — shows all 8 zones. You can run or stop any zone right here.

Schedule — 7-day calendar. For each day, you can enable it, set a start time,
and set individual durations per zone. Up to 8 zones, each can run a different
length.

Manual — run a specific zone for a specific number of minutes. Or run the
whole day's sequence at once.

Settings — zone names, system info, and firmware updates. This button checks
GitHub for a new firmware release and installs it over WiFi."

---

### 6:00 — Home Assistant integration (90 seconds)

[SCREEN CAPTURE: Home Assistant → Developer Tools → Services. Call the REST service.]

**VO/ON CAMERA:**
"Home Assistant users — this is for you. The controller has a local REST API.
No cloud relay. No Nabu Casa required.

From HA developer tools, I can call the service directly:

[Show: POST to /api/run_zone with JSON body {"zone": 0, "duration": 5}]

Zone 1 activates. I can build automations around weather data, soil moisture
sensors, or whatever logic I want — all local, all private."

[Show relay clicking on camera as the zone activates.]

---

### 7:30 — Where to get it (60 seconds)

[SCREEN: Tindie listing page]

**VO/ON CAMERA:**
"Assembled boards are available on Tindie for $54 — link in the description.
That includes the board in an enclosure and a quick start card.

If you want to build your own or contribute to the firmware, the source code
is on GitHub under an MIT license — also linked below. You can fork it, modify
it, run it on your own hardware.

I'm planning to keep improving the firmware — there are a few features I want
to add — so subscribe if you want to follow along."

---

### 9:00 — Call to action (30 seconds)

**VO/ON CAMERA:**
"If you have questions about the hardware design, the PCB layout, the
ESP32 relay circuit — drop them in the comments. I read everything.

If this solves a problem you've had with your current controller, I'd love
to hear about it.

Subscribe for the next video where I walk through the full EasyEDA PCB
design process for this board — from schematic to JLCPCB order.

Thanks for watching."

---

## Production notes

- Keep total runtime 8–10 minutes. Cut any section that runs long.
- The offline demo at 0:00 is the hook — film it cleanly, it needs to be convincing
- Relay click sound is satisfying — use a good microphone for the physical shots
- The HA integration section (6:00) is specifically for the r/homeassistant audience — don't cut it even if the video runs long
- Pin a comment on upload: "Assembled boards on Tindie — link in description. Firmware is source-available on GitHub if you want to build your own."
