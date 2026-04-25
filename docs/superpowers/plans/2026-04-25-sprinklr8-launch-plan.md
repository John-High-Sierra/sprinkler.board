# SprinKlr-8 Launch Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Take SprinKlr-8 from working off-shelf prototype to a custom PCB product sold on Tindie, generating sustainable side income from the Home Assistant and DIY homeowner communities.

**Architecture:** Three parallel tracks — (1) custom PCB design and manufacturing, (2) business and legal foundation, (3) community and marketing launch. Tracks converge at Month 3 soft launch when boards and storefront are both ready.

**Tech Stack:** EasyEDA Standard (PCB design), JLCPCB (PCB fabrication + PCBA), Arduino IDE + ESP32 firmware (existing), GitHub (open source + OTA), Tindie (initial storefront), Shopify (owned storefront from Month 5), Reddit / YouTube / TikTok (marketing channels).

**Reference docs:**
- Business plan: `docs/business/2026-04-25-sprinklr8-business-plan.md`
- PCB design guide: `SprinKlr8_PCB_Design_Guide.docx`
- Design spec: `SprinKlr8_Design_Spec.docx`
- Firmware: `esp32_firmware/sprinkler_controller/sprinkler_controller.ino`

---

## PHASE 1 — Foundation (Months 1–2)

---

### Task 1: Register LLC

**Purpose:** Separate personal liability from business before the first sale.

- [ ] **Step 1: Choose business name**

  Decide between `SprinKlr-8 LLC` (product-specific) or a broader name like `High Sierra Electronics LLC` (allows future products). Broader name recommended — check availability at your state's Secretary of State website.

- [ ] **Step 2: Register online**

  Go to your state's Secretary of State business registration portal. File Articles of Organization for a single-member LLC. Cost: $100–200. Most states process same-day online.

  Required fields:
  - Business name
  - Registered agent (you, at your home address is fine to start)
  - Business purpose: "Design, manufacture, and sale of electronic hardware products"
  - Member name and address

- [ ] **Step 3: Get EIN (Employer Identification Number)**

  Go to [irs.gov/businesses/small-businesses-self-employed/apply-for-an-employer-identification-number-ein-online](https://www.irs.gov/businesses/small-businesses-self-employed/apply-for-an-employer-identification-number-ein-online). Apply online — takes 5 minutes, EIN issued immediately. You need this to open a business bank account.

- [ ] **Step 4: Open business bank account**

  Take LLC registration certificate + EIN to your bank (or use an online business bank — Mercury is popular for small product businesses, no monthly fees). Open a business checking account. All JLCPCB orders, Tindie/Shopify payouts, and business expenses go through this account.

- [ ] **Step 5: Confirm**

  You should now have:
  - LLC registration certificate (save PDF)
  - EIN confirmation letter (save PDF)
  - Business bank account number and routing number

---

### Task 2: Open Source the Firmware on GitHub

**Purpose:** Publishing open source firmware is your primary marketing asset for the Home Assistant community. Do this before any sales.

> **Branch note:** This task runs on `master`. Switch before starting:
> ```bash
> git checkout master
> ```
> Switch back to `custom-board` after Task 2 is complete.

**Files:**
- Modify: `esp32_firmware/sprinkler_controller/sprinkler_controller.ino`
- Modify: `esp32_firmware/README.md`
- Create: `LICENSE`
- Create: `CONTRIBUTING.md`

- [ ] **Step 1: Clean up the README**

  Open `esp32_firmware/README.md`. Update the library table to reflect the current stack (WebServer.h, not ESPAsyncWebServer):

  ```markdown
  ## Required Libraries (install via Tools → Manage Libraries)

  | Library | Author | Version |
  |---------|--------|---------|
  | WiFiManager | tzapu | latest |
  | ArduinoJson | bblanchon | 6.x |
  | NTPClient | Fabrice Weinberg | latest |

  > WebServer.h and LittleFS are built into the ESP32 core — no install needed.
  ```

  Add a "Buy assembled board" link at the top (placeholder until Tindie listing is live):
  ```markdown
  > **Buy an assembled SprinKlr-8 board:** [Tindie listing coming soon]
  ```

- [ ] **Step 2: Add MIT + Commons Clause License**

  Create `LICENSE` in the project root. This is a source-available license —
  anyone can read, use, and modify the code for personal/non-commercial purposes,
  but selling products built with this firmware requires a commercial license.

  ```
  MIT License with Commons Clause

  Copyright (c) 2026 [Your Name / LLC Name]

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

  Commons Clause License Condition v1.0

  The Software is provided to you by the Licensor under the License, as defined
  below, subject to the following condition.

  Without limiting other conditions in the License, the grant of rights under
  the License will not include, and the License does not grant to you, the right
  to Sell the Software.

  For purposes of the foregoing, "Sell" means practicing any or all of the rights
  granted to you under the License to provide to third parties, for a fee or other
  consideration (including without limitation fees for hosting or consulting/
  support services related to the Software), a product or service whose value
  derives, entirely or substantially, from the functionality of the Software.

  For commercial licensing inquiries contact: [your business email]
  ```

- [ ] **Step 3: Add CONTRIBUTING.md**

  Create `CONTRIBUTING.md` in the project root:

  ```markdown
  # Contributing to SprinKlr-8

  Pull requests welcome. For major changes, open an issue first.

  ## License
  This project uses the MIT License with Commons Clause. Contributions are accepted
  under the same terms. Personal and non-commercial use is free. Commercial use
  (selling products built with this firmware) requires a separate commercial license
  — contact [your business email].

  ## Development Setup
  1. Install Arduino IDE 2.x
  2. Add ESP32 board support (see README)
  3. Install required libraries (see README)
  4. Open `esp32_firmware/sprinkler_controller/sprinkler_controller.ino`

  ## Firmware Guidelines
  - Do not revert WebServer.h to ESPAsyncWebServer (crashes on ESP32 core 3.x)
  - GPIO12 must keep its 10kΩ pull-down (strapping pin — floats HIGH without it)
  - Test all 8 zones before submitting relay-related changes

  ## Reporting Issues
  Open a GitHub issue with: firmware version, ESP32 core version, and serial monitor output.
  ```

- [ ] **Step 4: Push to GitHub**

  ```bash
  git add LICENSE CONTRIBUTING.md esp32_firmware/README.md
  git commit -m "Add MIT license, contributing guide, update README for open source release"
  git push origin master
  ```

  Expected: push succeeds, files visible at github.com/John-High-Sierra/sprinkler.board

- [ ] **Step 5: Add repository topics on GitHub**

  Go to the repo on GitHub → click the gear icon next to "About" → add topics:
  `esp32`, `home-assistant`, `sprinkler`, `irrigation`, `smart-home`, `arduino`, `wifi`, `local-api`, `open-source`

  Topics make the repo discoverable in GitHub search — critical for the Home Assistant community.

---

### Task 3: Complete Custom PCB Design in EasyEDA

**Purpose:** Design the custom SprinKlr-8 PCB that replaces the ACEIRMC off-shelf module.

**Reference:** Follow `SprinKlr8_PCB_Design_Guide.docx` exactly. This task summarises the checkpoints — the guide is the authoritative source for pin-level detail.

**Files:** (EasyEDA project — save exports to)
- `hardware/schematics/sprinklr8-schematic.json` (EasyEDA export)
- `hardware/schematics/sprinklr8-schematic.pdf` (PDF export for review)
- `hardware/pcb/sprinklr8-pcb.json` (EasyEDA export)
- `hardware/pcb/sprinklr8-pcb.pdf` (PDF export for review)
- `hardware/bom/sprinklr8-bom.csv` (JLCPCB BOM CSV)
- `hardware/bom/sprinklr8-cpl.csv` (JLCPCB CPL CSV)

- [ ] **Step 1: Create EasyEDA account and project**

  Go to easyeda.com → Sign Up → create free account. Click New Project → name it `SprinKlr8`. Inside the project, click New Schematic. Set paper to A3 landscape, grid to 2.54mm (100mil).

- [ ] **Step 2: Place all components**

  Search by LCSC part number for each component in the guide Section 3 netlist. Place in this order:
  1. ESP32-WROOM-32E (C701341) — center
  2. CH340C (C84681) — left of ESP32
  3. AMS1117-3.3 (C6186) — top-left
  4. ULN2803A (C8652) — right of ESP32
  5. 8× HF46F-G/5-HS1 (C165255) — far right column
  6. USB-C connector (C2988369) — top-left
  7. 2× MMBT3904 (C20526) — between CH340C and ESP32
  8. 2× tact switch (C318884) — near EN and GPIO0
  9. 8× 2-pin screw terminal (C8465) — right edge
  10. Polyfuse (C210357) — on VBUS line
  11. Resistors and caps per netlist

- [ ] **Step 3: Draw net connections**

  Wire all connections using net labels per guide Section 4.3. Critical nets to double-check:
  - GPIO12 → R12 (10kΩ pull-down to GND) → ULN2803A IN8
  - ULN2803A COM → +5V (NOT GND, NOT 3.3V)
  - USB-C CC1 and CC2 → 5.1kΩ each → GND
  - CH340C V3 pin → +3V3
  - Auto-reset: DTR# → R_DTR → T1 base; RTS# → R_RTS → T2 base

- [ ] **Step 4: Run ERC and fix all errors**

  Tools → Electrical Rules Check. Zero errors required before proceeding. Expected warnings (safe to ignore): unconnected NC pins on ESP32. Expected errors to fix: any net with no driver, any power pin unconnected.

- [ ] **Step 5: Convert to PCB and place components**

  Design → Convert to PCB. Place components per guide Section 5.2 zone strategy. Do NOT route any traces until placement is complete and logical.

- [ ] **Step 6: Route traces**

  Follow routing order in guide Section 5.3:
  1. Power traces (0.5–1.0mm)
  2. USB D+/D− matched pair
  3. UART (TX/RX)
  4. Auto-reset circuit
  5. ZONE1–8 signals
  6. Relay coil traces
  7. Relay contact traces (1.0mm, 2mm clearance from low-V)
  8. GND pour last (both layers)

- [ ] **Step 7: Run DRC — zero errors**

  Tools → DRC. Fix all violations. Check ESP32 antenna keep-out has no copper. Check silkscreen labels are readable and not overlapping pads.

- [ ] **Step 8: Export all files**

  - Fabrication → Generate Gerber → save ZIP to `hardware/gerbers/`
  - Fabrication → BOM → save CSV to `hardware/bom/sprinklr8-bom.csv`
  - Fabrication → CPL → save CSV to `hardware/bom/sprinklr8-cpl.csv`
  - Export schematic PDF to `hardware/schematics/sprinklr8-schematic.pdf`
  - Export PCB PDF to `hardware/pcb/sprinklr8-pcb.pdf`

- [ ] **Step 9: Commit hardware files**

  ```bash
  git add hardware/
  git commit -m "Add SprinKlr-8 custom PCB design files (EasyEDA exports)"
  git push origin custom-board
  ```

---

### Task 4: Order and Validate 5-Unit Prototype Batch

**Purpose:** Engineering validation — confirm the custom PCB design works before committing to a sellable batch. Do NOT sell these units.

- [ ] **Step 1: Place JLCPCB order**

  Go to jlcpcb.com → Order Now → upload Gerber ZIP from `hardware/gerbers/`.

  PCB options:
  - Layers: 2
  - Thickness: 1.6mm
  - Surface finish: HASL-LF (lead-free)
  - Soldermask: Green
  - Quantity: 5

  Enable PCB Assembly → Standard PCBA → Top Side → Quantity: 5.
  Upload `hardware/bom/sprinklr8-bom.csv` and `hardware/bom/sprinklr8-cpl.csv`.
  Confirm component placement visually in JLCPCB's online preview. Check for any out-of-stock parts.

  Expected cost: ~$125–175 for 5 assembled boards + shipping.

- [ ] **Step 2: Visual inspection on arrival**

  Inspect each board under magnification:
  - All ICs present and correctly oriented (check pin 1 markers)
  - No solder bridges on CH340C, ULN2803A, ESP32 castellated pads
  - All relay orientations consistent
  - Screw terminals seated correctly

- [ ] **Step 3: Continuity check before power**

  Use multimeter in continuity mode:
  - Check +5V to GND — should NOT beep (no short)
  - Check +3V3 to GND — should NOT beep

- [ ] **Step 4: Power up bare board**

  Connect USB-C. Measure with multimeter:
  - +5V rail: should read 4.9–5.1V
  - +3V3 rail: should read 3.28–3.35V
  - Power LED should illuminate

  If no voltage: check AMS1117 orientation, check polyfuse, check USB-C connection.

- [ ] **Step 5: Flash firmware via Arduino IDE**

  Open `esp32_firmware/sprinkler_controller/sprinkler_controller.ino` in Arduino IDE.
  Select board: ESP32 Dev Module. Select correct COM port (CH340C should appear automatically — install CH340 driver if not: [wch-ic.com/downloads/CH341SER_ZIP.html](http://www.wch-ic.com/downloads/CH341SER_ZIP.html)).

  Click Upload. The auto-reset circuit should trigger flash mode automatically — no button presses needed. Expected output:
  ```
  Connecting........_____....
  Chip is ESP32-D0WDQ6
  Uploading stub...
  Running stub...
  Changing baud rate to 921600
  Writing at 0x00010000... (100 %)
  Leaving...
  Hard resetting via RTS pin...
  ```

  If auto-reset fails: hold BOOT button, tap RESET, release BOOT, then click Upload.

- [ ] **Step 6: Verify serial output**

  Open Serial Monitor at 115200 baud. Expected output on boot:
  ```
  Sprinkler Controller starting...
  Mounting LittleFS...
  WiFiManager: Starting AP SprinklerSetup
  ```

- [ ] **Step 7: Upload web UI via LittleFS**

  In `data/index.html`, confirm `PREVIEW = false`. Close Serial Monitor. Press Ctrl+Shift+P → "Upload LittleFS image". Use IO0/EN boot sequence if needed (hold IO0, tap EN, release IO0).

- [ ] **Step 8: Test all 8 relay zones**

  Connect to SprinklerSetup WiFi, configure network, note IP address. Open browser → `http://[board IP]/`. Test each zone via Manual tab. Verify:
  - Audible relay click on activation
  - Multimeter continuity on NO contact when zone active
  - No click = zone fails (check ZONE signal trace and ULN2803A pin)

- [ ] **Step 9: 24VAC valve test**

  Connect a single 24VAC irrigation valve solenoid to Zone 1 terminals. Connect a 24VAC transformer common wire to COM terminal. Activate Zone 1 — valve should open (hiss/flow). Deactivate — valve closes.

- [ ] **Step 10: 48-hour soak test**

  Program a repeating schedule across all 8 zones (5 minutes each). Run for 48 hours. Check:
  - WiFi stays connected throughout
  - No unexpected reboots (check serial log)
  - No relay failures
  - Board temperature normal (touch test — should be warm, not hot)

- [ ] **Step 11: Gate check**

  All 5 prototypes pass all steps → proceed to Task 5 (FCC SDoC) and Task 6 (30-unit batch).
  Any failures → identify root cause, update EasyEDA design, respin before ordering batch.

---

### Task 5: Prepare FCC Supplier's Declaration of Conformity (SDoC)

**Purpose:** Legal requirement before selling in the US. Self-declaration — no lab fees.

**Files:**
- Create: `docs/business/fcc-sdoc.md` (working draft)
- Create: `docs/business/fcc-sdoc.pdf` (final signed version)

- [ ] **Step 1: Draft the SDoC**

  Create `docs/business/fcc-sdoc.md` with the following content (fill in bracketed fields):

  ```markdown
  # Supplier's Declaration of Conformity

  **Product Name:** SprinKlr-8 8-Zone WiFi Sprinkler Controller
  **Model Number:** SPRINKLR8-V1
  **Responsible Party:** [LLC Name]
  **Address:** [Business address]
  **Phone:** [Phone number]
  **Email:** [Business email]

  This device complies with Part 15 of the FCC Rules. Operation is subject to
  the following two conditions:
  1. This device may not cause harmful interference, and
  2. This device must accept any interference received, including interference
     that may cause undesired operation.

  **Module Used:** Espressif ESP32-WROOM-32E
  **FCC ID of Module:** 2AC7Z-ESPWROOM32

  The SprinKlr-8 incorporates the above FCC-certified module and has been
  evaluated to comply with FCC Part 15 Subpart B unintentional radiator
  requirements. No modifications have been made to the certified module.

  **Signed:** [Your name]
  **Title:** Owner, [LLC Name]
  **Date:** [Date]
  ```

- [ ] **Step 2: Add FCC logo and ID to product**

  The PCB silkscreen or product label must display:
  ```
  FCC ID: 2AC7Z-ESPWROOM32
  Contains FCC certified module
  ```

  Add this text to the PCB silkscreen in EasyEDA before the production batch order.

- [ ] **Step 3: Commit SDoC draft**

  ```bash
  git add docs/business/fcc-sdoc.md
  git commit -m "Add FCC SDoC draft for SprinKlr-8"
  ```

---

## PHASE 2 — Soft Launch (Month 3)

---

### Task 6: Order 30-Unit Production Batch

**Prerequisites:** Task 4 complete — all 5 prototypes pass testing.

- [ ] **Step 1: Update PCB with any prototype fixes**

  Apply any design corrections from prototype testing to the EasyEDA project. Re-export Gerber, BOM, CPL. Commit updated files:
  ```bash
  git add hardware/
  git commit -m "Apply prototype v1 corrections to PCB design"
  ```

- [ ] **Step 2: Place 30-unit JLCPCB order**

  Same process as Task 4, Step 1 — quantity 30. Expected cost: ~$18–25/board = ~$540–750 total + shipping.

  Check all LCSC parts are in stock with >100 units available before ordering. Substitute any out-of-stock parts with approved equivalents from the design guide (e.g. HF46F is pin-compatible with Omron G5NB-1A-E).

- [ ] **Step 3: Source enclosures**

  Order 35 off-shelf ABS project boxes (5 spares) sized to fit the PCB (~80×90mm footprint). Search AliExpress or LCSC for "ABS project enclosure 100x80x35mm" or similar. Expected cost: ~$3–5 each. Order now — 2–3 week lead time.

- [ ] **Step 4: Order packaging materials**

  - 30× anti-static poly bags (sized for PCB + enclosure)
  - 30× cardboard mailer boxes (sized for product)
  - Print 30× instruction cards (one double-sided card with: QR code to GitHub, WiFi setup steps, support email)

  Print instruction cards at home or at a local print shop. Content:
  ```
  SprinKlr-8 Quick Start
  1. Power via USB-C (5V 1A minimum)
  2. Connect to WiFi network: SprinklerSetup
  3. Open browser → 192.168.4.1 → configure your WiFi
  4. Access web interface at http://sprinkler/ or your router's IP list
  Full docs: github.com/John-High-Sierra/sprinkler.board
  Support: [your email]
  ```

- [ ] **Step 5: Assemble and test spot-check**

  When boards arrive, spot-check 3 of 30 through the full test sequence (Task 4 Steps 2–9). If all 3 pass, the batch is good. If any fail, test all 30 and set aside failures for root cause analysis.

---

### Task 7: Set Up Tindie Seller Account and Listing

- [ ] **Step 1: Create Tindie seller account**

  Go to tindie.com → Sign Up → complete seller profile. Business name: your LLC name. PayPal or Stripe for payouts. Tindie fee: 5% per sale.

- [ ] **Step 2: Take product photos**

  Minimum 4 photos:
  1. Top-down board shot on white background (clean, well-lit)
  2. Angled shot showing USB-C port and relay block
  3. Web UI screenshot (Dashboard view on mobile)
  4. Board installed in enclosure

  Use natural light or a lightbox. No phone HDR — flat, accurate colour.

- [ ] **Step 3: Write the Tindie listing**

  Title: `SprinKlr-8 — 8-Zone WiFi Sprinkler Controller (No Cloud, No Subscription)`

  Description:
  ```
  A smart 8-zone sprinkler controller built on ESP32. Runs entirely on your
  local network — no cloud account, no app, no subscription, no data tracking.
  Control it from any browser on any device.

  WORKS WITH HOME ASSISTANT — local API, no cloud relay required.

  What's included:
  - Assembled SprinKlr-8 board in ABS enclosure
  - Quick start card
  - Open source firmware (MIT license)

  Features:
  - 8-zone SPST relay outputs for 24VAC irrigation valves
  - WiFi setup via captive portal (no config file editing)
  - Mobile-first web interface — Dashboard, Schedule, Manual, Settings
  - Per-zone scheduling with 7-day calendar
  - OTA firmware updates via web UI (from GitHub)
  - USB-C programming port
  - Fully local — works without internet once configured

  Open source firmware: github.com/John-High-Sierra/sprinkler.board
  FCC SDoC on file. For indoor installation only.
  ```

  Price: $54.00. Shipping: calculate via Tindie's built-in rate calculator (USPS First Class ~$4–6 for this weight).

- [ ] **Step 4: Publish listing**

  Set quantity to 28 (keep 2 units as warranty stock). Publish. Copy listing URL for use in Reddit post and YouTube description.

---

### Task 8: Publish Reddit Launch Post

- [ ] **Step 1: Write the post**

  Subreddits: post to r/homeassistant first, then r/DIY 48 hours later (don't cross-post simultaneously — it looks spammy).

  Title for r/homeassistant:
  `I built a local-only ESP32 sprinkler controller — no cloud, no subscription, open source firmware`

  Post body (adapt freely — write in your own voice):
  ```
  After getting burned by my Rachio losing connection during a heat wave and
  finding out their "local control" requires their cloud anyway, I built my own.

  SprinKlr-8: 8-zone WiFi controller, runs entirely on your local network.
  Control it from any browser, no app needed.

  Features:
  - Local API (no cloud relay)
  - 7-day zone scheduling
  - OTA updates from GitHub
  - Mobile-first web UI
  - Open source firmware (MIT)

  I've got a small batch available on Tindie for $54:
  [Tindie link]

  Firmware on GitHub if you want to build your own or contribute:
  [GitHub link]

  Happy to answer questions about the build — EasyEDA schematic, ESP32 relay
  driver design, or the WebServer.h migration (ESPAsyncWebServer crashes on
  core 3.x, for anyone who's hit that).
  ```

- [ ] **Step 2: Post and engage**

  Post during peak hours (Tuesday–Thursday, 9am–12pm US Eastern). Check back every few hours for the first 24 hours and respond to every comment. Upvotes and comments in the first hour determine Reddit visibility.

---

### Task 9: Publish YouTube Launch Video

- [ ] **Step 1: Plan the video (target: 8–10 minutes)**

  ```
  0:00 Hook — show the web UI, show it working offline (pull ethernet from router, zones still run)
  0:45 Problem — Rachio/B-hyve cloud dependency, subscription nags
  2:00 The build — show the custom PCB, explain ESP32 + relay driver + USB-C
  4:00 Web UI walkthrough — Dashboard, Schedule, Manual, Settings
  6:00 Home Assistant integration — show local API call activating a zone
  7:30 Where to get it — Tindie link, GitHub link
  9:00 Call to action — subscribe, GitHub star, leave questions below
  ```

- [ ] **Step 2: Record and edit**

  Record screen captures of the web UI. Record physical board shots (relays clicking is satisfying on camera). Keep editing tight — cut dead air.

- [ ] **Step 3: Upload with optimised metadata**

  Title: `I Built a $54 Sprinkler Controller That Works Without Internet (ESP32, No Cloud)`

  Description (first 3 lines show before "more"):
  ```
  SprinKlr-8: an 8-zone WiFi sprinkler controller with local API, no cloud,
  no subscription. Works with Home Assistant. Open source firmware on GitHub.

  Buy on Tindie: [link]
  GitHub: [link]
  ```

  Tags: `esp32, home assistant, smart sprinkler, irrigation controller, diy smart home, local api, no cloud, open source`

  Thumbnail: board + web UI screenshot + text overlay "No Cloud. No Subscription. $54."

- [ ] **Step 4: Post Tindie listing link in YouTube description and pin a comment**

  Pin a comment: "Assembled boards available on Tindie — link in description. Firmware is MIT licensed on GitHub if you want to build your own."

---

## PHASE 3 — Iterate & Scale (Months 4–6)

---

### Task 10: Set Up Shopify Store

**Trigger:** 15+ units sold on Tindie (demand validated).

- [ ] **Step 1: Create Shopify account**

  Go to shopify.com → Start free trial → use Starter plan ($5/month) if you only need a buy button embeddable on a simple site, or Basic plan ($39/month) for a full storefront.

- [ ] **Step 2: Register domain**

  Register `sprinklr8.com` (or your chosen domain) via Namecheap or Cloudflare (~$12/year). Point to Shopify or to a simple landing page with an embedded Shopify buy button.

- [ ] **Step 3: Create product listing**

  Copy Tindie listing content. Add:
  - Warranty policy: 30-day return for defective units
  - Shipping: USPS First Class, 3–5 business days
  - Tax: enable automatic US tax calculation

- [ ] **Step 4: Connect business bank account**

  Shopify Payments or Stripe — connect to business bank account from Task 1. Test with a $1 order to yourself.

---

### Task 11: Home Assistant HACS Integration

**Purpose:** Getting listed in HACS (Home Assistant Community Store) dramatically increases visibility — it puts SprinKlr-8 in front of every Home Assistant user directly in their dashboard.

**Files:**
- Create: `ha-integration/` directory in repo root
- Create: `ha-integration/custom_components/sprinklr8/__init__.py`
- Create: `ha-integration/custom_components/sprinklr8/manifest.json`
- Create: `ha-integration/custom_components/sprinklr8/switch.py`
- Create: `ha-integration/hacs.json`

- [ ] **Step 1: Review existing API endpoints**

  Open `esp32_firmware/sprinkler_controller/sprinkler_controller.ino`. Confirm these endpoints exist:
  - `POST /api/run_zone` body: `{"zone": 0, "duration": 10}` — activates one zone
  - `POST /api/stop` — stops all zones
  - `GET /api/status` — returns zone states

  If `GET /api/status` does not exist, add it to the firmware before writing the HA integration.

- [ ] **Step 2: Create hacs.json**

  Create `ha-integration/hacs.json`:
  ```json
  {
    "name": "SprinKlr-8",
    "content_in_root": false,
    "homeassistant": "2023.1.0",
    "iot_class": "local_polling"
  }
  ```

- [ ] **Step 3: Create manifest.json**

  Create `ha-integration/custom_components/sprinklr8/manifest.json`:
  ```json
  {
    "domain": "sprinklr8",
    "name": "SprinKlr-8 Sprinkler Controller",
    "version": "1.0.0",
    "documentation": "https://github.com/John-High-Sierra/sprinkler.board",
    "requirements": [],
    "dependencies": [],
    "codeowners": ["@John-High-Sierra"],
    "iot_class": "local_polling"
  }
  ```

- [ ] **Step 4: Create __init__.py**

  Create `ha-integration/custom_components/sprinklr8/__init__.py`:
  ```python
  """SprinKlr-8 Home Assistant integration."""
  ```

- [ ] **Step 5: Create switch.py**

  Create `ha-integration/custom_components/sprinklr8/switch.py`:
  ```python
  """SprinKlr-8 zone switches for Home Assistant."""
  import requests
  from homeassistant.components.switch import SwitchEntity

  ZONE_COUNT = 8

  async def async_setup_platform(hass, config, async_add_entities, discovery_info=None):
      host = config.get("host")
      duration = config.get("duration", 10)
      async_add_entities(
          [SprinKlr8Zone(host, zone, duration) for zone in range(ZONE_COUNT)]
      )

  class SprinKlr8Zone(SwitchEntity):
      def __init__(self, host, zone, duration):
          self._host = host
          self._zone = zone
          self._duration = duration
          self._is_on = False

      @property
      def name(self):
          return f"SprinKlr-8 Zone {self._zone + 1}"

      @property
      def is_on(self):
          return self._is_on

      def turn_on(self, **kwargs):
          requests.post(
              f"http://{self._host}/api/run_zone",
              json={"zone": self._zone, "duration": self._duration},
              timeout=5
          )
          self._is_on = True

      def turn_off(self, **kwargs):
          requests.post(f"http://{self._host}/api/stop", timeout=5)
          self._is_on = False
  ```

- [ ] **Step 6: Commit and submit to HACS**

  ```bash
  git add ha-integration/
  git commit -m "Add Home Assistant HACS integration for SprinKlr-8"
  git push origin master
  ```

  Go to [hacs.xyz/docs/publish/integration](https://hacs.xyz/docs/publish/integration) and follow the submission checklist. Add the repo to the HACS default store by opening a PR to [github.com/hacs/default](https://github.com/hacs/default).

---

### Task 12: Order 100-Unit Growth Batch

**Trigger:** Selling 25+ units/month consistently.

- [ ] **Step 1: Review and finalise PCB revision**

  Incorporate any design improvements from customer feedback. Update EasyEDA, export fresh Gerbers, BOM, CPL. Commit:
  ```bash
  git add hardware/
  git commit -m "PCB revision v2 — incorporate batch 1 feedback"
  ```

- [ ] **Step 2: Add branding to PCB silkscreen**

  In EasyEDA PCB editor, add to silkscreen layer:
  - Product name: `SprinKlr-8`
  - Version: `v2.0`
  - FCC text: `FCC ID: 2AC7Z-ESPWROOM32 | Contains FCC certified module`
  - Website: `sprinklr8.com`

- [ ] **Step 3: Place 100-unit JLCPCB order**

  Same process as Task 6. Expected cost: ~$12/board = ~$1,200 total + shipping. Verify all LCSC parts in stock with >500 units available.

- [ ] **Step 4: Update financial tracking**

  In `docs/business/`, create `financials.md` tracking:
  - Batch costs (JLCPCB invoices)
  - Units sold per month
  - Revenue and gross margin per batch
  - Running total profit/loss

---

## PHASE 4 — Steady State (Months 7–12)

---

### Task 13: Reach 300-Unit Annual Milestone

- [ ] **Step 1: Maintain inventory buffer**

  Reorder when stock drops to 6-week supply. At 25 units/month: reorder at ~37 units remaining.

- [ ] **Step 2: Publish second YouTube video**

  Topic: "How I designed a custom PCB for my ESP32 project — EasyEDA to JLCPCB PCBA." This video targets a different audience (makers who want to learn PCB design) and drives GitHub stars + organic product discovery.

- [ ] **Step 3: Evaluate 500-unit batch economics**

  At 500 units: JLCPCB cost ~$8/board. All-in ~$21/unit. Margin at $54 = $33/unit (61%). Evaluate:
  - Custom injection-moulded enclosure (amortises at 500+ units, ~$800–1,500 tooling cost)
  - 3PL fulfilment service (ShipBob, Shipwire) if packing/shipping is taking >4 hours/week

- [ ] **Step 4: Year-end review**

  Review against business plan targets:
  - [ ] 300 units sold
  - [ ] LLC profitable (revenue > all expenses)
  - [ ] GitHub repo has 100+ stars
  - [ ] Active community presence on r/homeassistant
  - [ ] HACS integration live

  Update `docs/business/2026-04-25-sprinklr8-business-plan.md` with actuals vs projections and revise the plan for Year 2.
