# Reddit Launch Post — SprinKlr-8

## Target subreddits (post in this order, 48 hours apart)
1. r/homeassistant — primary audience
2. r/DIY — secondary audience
3. r/homeautomation — tertiary (optional)

*Do NOT cross-post simultaneously — it looks spammy. Post to r/homeassistant first.*

---

## Post for r/homeassistant

**Title:**
I built a local-only ESP32 sprinkler controller — no cloud, no subscription, open source firmware

**Body:**

After getting burned by my Rachio losing connection during a heat wave and
discovering that "local control" still requires their cloud, I built my own.

**SprinKlr-8** — 8-zone WiFi sprinkler controller that runs entirely on your
local network. No cloud account, no app, no subscription, no data tracking.

**What it does:**
- Controls 8 zones of 24VAC irrigation valves
- Scheduling via a mobile-first web interface (any browser, no app download)
- 7-day per-zone schedule calendar
- Manual zone control and run-day sequences
- OTA firmware updates pulled from GitHub
- Works offline after initial WiFi setup — internet going down doesn't affect your schedule

**Home Assistant:** Local REST API, no cloud relay. Call `/api/run_zone` with
a JSON body directly from your HA automations.

**Tech stack for the curious:** ESP32-WROOM-32E, custom PCB, CH340C for
USB-C programming, ULN2803A relay driver, 8× SPST 5V SMD relays.
Firmware uses WebServer.h (NOT ESPAsyncWebServer — that crashes on ESP32
core 3.x if anyone else has hit that wall).

**Available on Tindie for $54:** [Tindie link — add before posting]

**Firmware on GitHub (MIT + Commons Clause):**
https://github.com/John-High-Sierra/sprinkler.board

Happy to answer questions about the hardware design, the ESP32 relay driver
circuit, the WebServer.h migration, or anything else.

---

## Adapted post for r/DIY

**Title:**
Built a $54 smart sprinkler controller — no cloud, no subscription, runs offline

**Body:**

Frustrated with smart sprinkler controllers that stop working when your
internet drops or the company shuts down their servers, I designed and built
my own.

**SprinKlr-8** — 8-zone WiFi sprinkler controller on a custom ESP32 PCB.
Runs entirely on your local network, works offline after setup.

**Features:**
- 8-zone 24VAC valve control
- Mobile-friendly web interface — any browser, no app
- 7-day scheduling per zone
- OTA firmware updates from GitHub
- USB-C programming port

**$54 assembled on Tindie:** [Tindie link — add before posting]
**Source code:** https://github.com/John-High-Sierra/sprinkler.board

For the hardware folks: custom 2-layer PCB from JLCPCB, ESP32 module,
CH340C USB-UART bridge with auto-reset, ULN2803A driving 8 SPST relays.
Happy to share the EasyEDA design process if anyone's interested.

---

## Posting tips

- Post Tuesday–Thursday, 9am–12pm US Eastern for peak visibility
- Respond to every comment in the first 24 hours — early engagement drives Reddit ranking
- Don't edit the post after submission to add links (looks like spam) — have the Tindie link ready before posting
- The technical details (WebServer.h, ESPAsyncWebServer crash) are intentional — they signal to the HA community that you know what you're doing
