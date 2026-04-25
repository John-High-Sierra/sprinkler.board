# SprinKlr-8 Business Plan

**Date:** 2026-04-25
**Framework:** Lean Canvas
**Model:** Small product business — open source firmware, direct manufacturing via JLCPCB, community-driven sales

---

## Lean Canvas Summary

| Block | SprinKlr-8 |
|-------|-----------|
| **Customers** | DIY homeowners, Home Assistant enthusiasts |
| **Problem** | Cloud lock-in, subscriptions, $150–250 price, no local API, clunky UIs |
| **UVP** | Open, local, affordable — no app, no cloud, no fee, fully private |
| **Channels** | Reddit, YouTube, TikTok |
| **Price** | $49–59/unit (target $54) |
| **Cost** | ~$36/unit (30 units), ~$26/unit (100 units), ~$21/unit (500 units) |
| **Unfair Advantage** | Source-available community trust, direct manufacturing, truly-local niche |

---

## 1. Product & Opportunity

**What it is**
SprinKlr-8 is an 8-zone WiFi sprinkler controller built on a custom ESP32 PCB. It runs entirely on your local network — no cloud account, no app, no subscription. You control it from any browser on any device via a clean mobile-first web interface. Firmware updates are delivered over WiFi from GitHub with one tap.

**The gap**
The smart irrigation market is moving in one direction: more cloud, more subscription, more lock-in. Rachio charges $30/year for weather intelligence features. Orbit B-hyve requires their cloud to function at all. RainBird's WiFi module is $80 on top of the controller cost. None of them offer a local API that works without internet. Meanwhile, the Home Assistant community — now over 500,000 active users — is actively looking for local-first alternatives to every cloud-dependent device in their home. Sprinkler controllers are a known gap.

**Why now**
Three things have converged: ESP32 modules are mature and cheap, JLCPCB has made small-batch custom PCB assembly accessible to individuals, and the backlash against cloud-dependent consumer hardware is at a peak. A well-executed local-first sprinkler controller launched into the right communities today has a clear, underserved audience waiting for it.

**The goal**
Not a billion-dollar company. A sustainable small product business: sell 500–1,000 units per year at $49–59, generate meaningful side income, build a reputation in the Home Assistant and DIY communities, and retain full control of the product.

---

## 2. Target Customer

**Primary: The Home Assistant Enthusiast**
Technically capable homeowner, typically 30–50 years old, already running a home automation hub (Home Assistant, HomeSeer, or similar). Has replaced most cloud-dependent devices with local alternatives — smart switches, sensors, cameras. The sprinkler controller is one of the last cloud-dependent devices they haven't solved yet. Values local API, open source firmware, and community support over polished packaging. Discovers products on Reddit (r/homeassistant has 600K+ members), GitHub, and YouTube. Will flash their own firmware if needed, but prefers a product that works out of the box. Willing to pay $50–80 for something that does exactly what they need without compromise.

**Secondary: The DIY Homeowner**
Technically capable but not necessarily a home automation user. Bought a Rachio or B-hyve, got frustrated with the app, the subscription nag, or the cloud dependency when their internet was down during a heat wave. Now looking for something simpler and more reliable. Finds products through YouTube reviews and Reddit. Less likely to dig into the firmware, but will appreciate that it exists. Price-sensitive — the $49–59 price point lands well against $150–250 alternatives.

**Who it is NOT (yet)**
Irrigation contractors, commercial customers, or non-technical homeowners who want a polished app experience and don't care about local control. These are potential future segments but require different packaging, support infrastructure, and pricing.

**Common buying trigger**
Both segments share a specific moment: their current controller fails during a hot week, or they try to integrate it into Home Assistant and hit a wall. That frustration is the purchase trigger — SprinKlr-8 needs to be findable at that moment.

---

## 3. Competition

| Product | Price | Local API | Cloud Required | Subscription | Open Source |
|---------|-------|-----------|----------------|--------------|-------------|
| **SprinKlr-8** | **$49–59** | **Yes** | **No** | **No** | **Yes** |
| Rachio 3 | $229 | No | Yes | $30/yr | No |
| Orbit B-hyve | $80–150 | No | Yes (breaks offline) | No | No |
| RainBird ST8I-WiFi | $150+ | No | Yes | No | No |
| OpenSprinkler | $80–150 | Yes | No | No | Yes |
| Wyze Sprinkler | $50 | No | Yes | No | No |
| DIY ESP32 project | $20 (parts) | Yes | No | No | Yes |

**Where SprinKlr-8 wins**
Against Rachio and B-hyve: price, local control, no subscription, privacy. Against OpenSprinkler: lower price, cleaner modern web UI, USB-C programming, more active firmware development. Against DIY projects: it's a finished, assembled product — no soldering, no wiring a relay board, no debugging someone else's GitHub code.

**The real competitor**
OpenSprinkler is the closest comparison. SprinKlr-8's advantages are price ($49–59 vs $80–150), a significantly cleaner mobile-first UI, and direct community engagement where the target customers already live.

**The non-competitor**
Rachio is not actually a competitor for this customer — anyone comparing SprinKlr-8 to Rachio has already decided they don't want Rachio. The real decision is: build it myself from parts, buy OpenSprinkler, or buy SprinKlr-8.

---

## 4. Go-to-Market Plan

**Phase 1: Foundation (Months 1–2)**
- Register LLC
- Open business bank account
- Publish firmware source on GitHub under MIT + Commons Clause (source-available: free for personal/non-commercial use, commercial license required to sell competing products)
- Write clean README with photos, wiring diagram, and buy link
- Set up Tindie seller account
- Order and validate 5-unit prototype batch

**Phase 2: Soft Launch (Month 3)**
Order 20–30 units. List on Tindie at $54.
- Post a genuine write-up on r/homeassistant and r/DIY — not a sales post, a "I built this and it solves X" post
- Publish one YouTube video: "I built a $54 sprinkler controller that doesn't need the internet"
- Respond to every comment and question personally

**Phase 3: Content & Scale (Months 4–6)**
If the first batch sells through, order 50–100 units and add a Shopify store.
- TikTok: short clips of board assembly, relays clicking, web UI in action
- Second YouTube video documenting the custom PCB design process (EasyEDA → JLCPCB)
- Add a product page at your own domain

**Phase 4: Repeat (Month 6+)**
Reinvest margins into the next batch. Each batch is larger and cheaper per unit. Goal is to reach 200–500 unit batches where the economics are strong ($15–20 cost, $54 retail = 60–70% gross margin).

---

## 5. Manufacturing & Operations

**The Manufacturing Stack**
Design in EasyEDA → fabricate and assemble at JLCPCB → ship to you → test → pack → ship to customer. No contract manufacturer, no warehouse, no intermediary.

**Phase 1 — Prototype (5 units)**
- Complete EasyEDA schematic + PCB layout, export Gerber + BOM + CPL
- Order 5 boards from JLCPCB Standard PCBA (~$25–35/board)
- Run full test sequence from PCB Design Guide
- Do not sell these — engineering validation only
- **Gate:** All 5 prototypes pass → proceed

**Phase 2 — Small Batch (20–50 units)**
- Order 20–50 boards (~$18–25/board)
- Source off-shelf ABS enclosures (~$3–5 each)
- Simple packaging: anti-static bag, printed instruction card, cardboard mailer
- Fulfil orders from home
- All-in landed cost: ~$28–35/unit

**Phase 3 — Growth Batch (100–500 units)**
- Order 100–500 boards (~$10–15/board)
- Custom enclosure label or silk-screen logo
- All-in landed cost: ~$18–22/unit (~60–65% gross margin at $54)
- Consider 3PL fulfilment at 500+ units

**Lead Times**

| Step | Lead Time |
|------|-----------|
| JLCPCB PCBA (standard) | 5–7 business days + 3–5 days shipping |
| Enclosures (LCSC/AliExpress) | 2–3 weeks |
| Total reorder to shelf | ~3–4 weeks |

Maintain 4–6 weeks inventory buffer once past Phase 2.

---

## 6. Financial Model

**Cost Per Unit by Phase**

| Cost Item | 30 units | 100 units | 500 units |
|-----------|----------|-----------|-----------|
| PCB + PCBA | $20 | $12 | $8 |
| Enclosure | $4 | $3 | $2.50 |
| Packaging + card | $2 | $1.50 | $1 |
| Outbound shipping | $7 | $7 | $7 |
| Platform fee (5%) | $2.70 | $2.70 | $2.70 |
| **Total landed cost** | **~$36** | **~$26** | **~$21** |
| **Retail price** | **$54** | **$54** | **$54** |
| **Gross margin/unit** | **~$18 (33%)** | **~$28 (52%)** | **~$33 (61%)** |

**Startup Costs (one-time)**

| Item | Cost |
|------|------|
| LLC registration | ~$150 |
| Prototype batch (5 units, not sold) | ~$175 |
| Domain + basic website | ~$50 |
| Shopify (6 months) | ~$234 |
| **Total** | **~$610** |

**Break-Even**
First sellable batch of 30 units generates ~$540 gross profit — covering startup costs almost exactly. Break-even after first batch sells through.

**12-Month Revenue Projection**

| Scenario | Units/Year | Revenue | Gross Profit |
|----------|-----------|---------|-------------|
| Conservative | 150 | $8,100 | ~$3,200 |
| Realistic | 300 | $16,200 | ~$7,500 |
| Optimistic | 600 | $32,400 | ~$16,000 |

Realistic scenario: one Reddit post, one YouTube video, steady Tindie presence → ~25 units/month by month 6. No paid advertising.

---

## 7. Legal & Compliance

**LLC Formation**
Register a single-member LLC before the first sale. Separates personal assets from business liability. Cost: $100–200, done online. Consider a broad name (e.g. "High Sierra Electronics LLC") for flexibility to launch other products later.

**FCC Compliance — SDoC**
The ESP32-WROOM-32E module is FCC certified (FCC ID: 2AC7Z-ESPWROOM32). A Supplier's Declaration of Conformity (SDoC) is required — a self-declaration that the product incorporates a certified module and meets Part 15 rules. No lab fees, no third-party testing. Write it yourself, keep on file, display FCC logo on product.

**CE Marking**
Only required for EU sales. Not needed for US-only launch. Revisit if EU demand emerges.

**Warranty & Returns**
30-day return for defective units, no questions asked. Budget 2–3% of units for warranty replacements (~6–9 boards/year at 300 units).

**Product Liability**
LLC provides liability shield. Include safety notice in packaging: 24VAC wiring should be performed by a qualified person. Do not claim outdoor or weatherproof suitability — rated for indoor installation only.

**Taxes**
Collect sales tax for your state. Tindie and Shopify handle this automatically. Keep all receipts — components, JLCPCB orders, and shipping are deductible business expenses.

---

## 8. Milestones — 12-Month Roadmap

**Month 1–2: Foundation**
- [ ] Register LLC
- [ ] Open business bank account
- [ ] Complete custom PCB design in EasyEDA
- [ ] Order 5-unit prototype batch from JLCPCB
- [ ] Pass full prototype test sequence
- [ ] Open source firmware on GitHub
- **Gate:** All 5 prototypes pass → proceed to Phase 2

**Month 3: Soft Launch**
- [ ] Order 30-unit batch
- [ ] Source enclosures and packaging
- [ ] Set up Tindie listing
- [ ] Post on r/homeassistant and r/DIY
- [ ] Publish first YouTube video
- **Gate:** 15 of 30 units sold within 60 days → validate demand

**Month 4–5: Iterate**
- [ ] Address all customer feedback
- [ ] Apply PCB design improvements if needed
- [ ] Start TikTok content
- [ ] Set up Shopify store

**Month 6: Growth Batch**
- [ ] Order 100-unit batch
- [ ] Add custom enclosure branding
- [ ] Publish second YouTube video (PCB design process)
- **Gate:** 25+ units/month consistently → order 200-unit batch

**Month 7–12: Steady State**
- [ ] Maintain 4–6 week inventory buffer
- [ ] Reinvest margins into next batch
- [ ] Evaluate Home Assistant HACS integration
- [ ] At 500 units: evaluate custom enclosure and 3PL fulfilment
- **Year-end target:** 300 units sold, LLC profitable, community established
