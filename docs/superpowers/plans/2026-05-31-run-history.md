# Run History Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a History tab showing the last 60 days of watering runs as daily cards with per-zone duration bars, stored in LittleFS on the ESP32; also migrate zone names from browser localStorage to `config.json` on the board so names are snapshotted into each log entry.

**Architecture:** Zone names are added to `BoardConfig` and wired through the config API. A new `appendRunLog()` function appends run/skip entries to `runs.json` in LittleFS; it is called at teardown of `runSequenceTask`, `runSingleZoneTask`, and in `scheduleCheckerTask` on weather-skip. `GET /api/history` serves the raw JSON array. The UI renders daily cards using zone names stored in each log entry (no localStorage lookup), and the Settings zone-name inputs POST to the board instead of writing localStorage.

**Tech Stack:** ArduinoJson v6, LittleFS, FreeRTOS, vanilla JS, existing dark UI CSS variables.

---

## File Map

| File | Change |
|------|--------|
| `esp32_firmware/sprinkler_controller/sprinkler_controller.ino` | `BoardConfig` (add `zoneNames`), `loadConfig`/`saveConfig`/GET+POST `/api/config`, `appendRunLog()`, call sites in `runSequenceTask`/`runSingleZoneTask`/`scheduleCheckerTask`, `GET /api/history`, FW_VERSION bump |
| `esp32_firmware/sprinkler_controller/data/index.html` | Zone names migration (localStorage → board), History tab + page + CSS + JS |
| `esp32_firmware/sprinkler_controller/html_content.h` | Regenerated from `data/index.html` |

---

## Task 1: Firmware — zone names in BoardConfig + config API

**Files:**
- Modify: `esp32_firmware/sprinkler_controller/sprinkler_controller.ino`

- [ ] **Step 1: Add `zoneNames` to `BoardConfig` struct**

Find (line ~140):
```cpp
struct BoardConfig {
  char timezone[64];
  float latitude;
  float longitude;
  bool weatherEnabled;
  int  rainThreshold;    // percent 0-100, skip if forecast >= this
  float freezeThreshold; // degrees C, skip if min temp <= this
  bool cycleAndSoakEnabled;
  int  cycleTime;            // minutes per cycle, default 4
  char tempUnit[2];          // "C" or "F", display preference only
};
```

Replace with:
```cpp
struct BoardConfig {
  char timezone[64];
  float latitude;
  float longitude;
  bool weatherEnabled;
  int  rainThreshold;    // percent 0-100, skip if forecast >= this
  float freezeThreshold; // degrees C, skip if min temp <= this
  bool cycleAndSoakEnabled;
  int  cycleTime;            // minutes per cycle, default 4
  char tempUnit[2];          // "C" or "F", display preference only
  char zoneNames[8][32];     // user-defined zone names
};
```

- [ ] **Step 2: Update `loadConfig()` — defaults + JSON read**

Find inside `loadConfig()`:
```cpp
  strlcpy(boardConfig.tempUnit, "C", sizeof(boardConfig.tempUnit));

  if (!LittleFS.exists(CONFIG_FILE)) return;
```

Replace with:
```cpp
  strlcpy(boardConfig.tempUnit, "C", sizeof(boardConfig.tempUnit));
  for (int i = 0; i < 8; i++) {
    snprintf(boardConfig.zoneNames[i], sizeof(boardConfig.zoneNames[i]), "Zone %d", i + 1);
  }

  if (!LittleFS.exists(CONFIG_FILE)) return;
```

Find inside `loadConfig()`, after the `temp_unit` parse lines:
```cpp
  Serial.printf("[CFG] Timezone: %s  Lat: %.4f  Lon: %.4f  WeatherSkip: %s\n",
```

Add immediately before that line:
```cpp
  if (doc.containsKey("zone_names")) {
    JsonArray zn = doc["zone_names"].as<JsonArray>();
    for (int i = 0; i < 8 && i < (int)zn.size(); i++) {
      const char* n = zn[i];
      if (n) strlcpy(boardConfig.zoneNames[i], n, sizeof(boardConfig.zoneNames[i]));
    }
  }
```

Also bump the `DynamicJsonDocument` in `loadConfig()` from 512 to 1024:

Find:
```cpp
  DynamicJsonDocument doc(512);
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.printf("[CFG] JSON parse error: %s, using defaults\n", err.c_str());
```

Replace:
```cpp
  DynamicJsonDocument doc(1024);
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.printf("[CFG] JSON parse error: %s, using defaults\n", err.c_str());
```

- [ ] **Step 3: Update `saveConfig()` — write zone_names + bump doc size**

Find:
```cpp
void saveConfig() {
  DynamicJsonDocument doc(512);
```

Replace:
```cpp
void saveConfig() {
  DynamicJsonDocument doc(1024);
```

Find inside `saveConfig()`:
```cpp
  doc["temp_unit"]              = boardConfig.tempUnit;
  File f = LittleFS.open(CONFIG_FILE, "w");
```

Replace:
```cpp
  doc["temp_unit"]              = boardConfig.tempUnit;
  JsonArray zn = doc.createNestedArray("zone_names");
  for (int i = 0; i < 8; i++) zn.add(boardConfig.zoneNames[i]);
  File f = LittleFS.open(CONFIG_FILE, "w");
```

- [ ] **Step 4: Update `GET /api/config` — return zone_names + bump doc size**

Find the GET /api/config handler's document allocation:
```cpp
    doc["cycle_and_soak_enabled"] = boardConfig.cycleAndSoakEnabled;
    doc["cycle_time"]             = boardConfig.cycleTime;
    doc["temp_unit"]              = boardConfig.tempUnit;
    String out; serializeJson(doc, out);
    server.send(200, "application/json", out);
  });

  // ── POST /api/config
```

Replace:
```cpp
    doc["cycle_and_soak_enabled"] = boardConfig.cycleAndSoakEnabled;
    doc["cycle_time"]             = boardConfig.cycleTime;
    doc["temp_unit"]              = boardConfig.tempUnit;
    JsonArray zn = doc.createNestedArray("zone_names");
    for (int i = 0; i < 8; i++) zn.add(boardConfig.zoneNames[i]);
    String out; serializeJson(doc, out);
    server.send(200, "application/json", out);
  });

  // ── POST /api/config
```

Find the GET /api/config DynamicJsonDocument allocation. It will look like:
```cpp
  server.on("/api/config", HTTP_GET, []() {
    DynamicJsonDocument doc(512);
```

Replace with:
```cpp
  server.on("/api/config", HTTP_GET, []() {
    DynamicJsonDocument doc(1024);
```

- [ ] **Step 5: Update `POST /api/config` — accept zone_names + bump doc size**

Find the POST /api/config DynamicJsonDocument allocation:
```cpp
    DynamicJsonDocument doc(512);
```
(This is the one inside the POST /api/config lambda — confirm by context.)

Replace with:
```cpp
    DynamicJsonDocument doc(1024);
```

Find inside the POST /api/config lambda, just before `saveConfig()`:
```cpp
    bool locationChanged = doc.containsKey("latitude") || doc.containsKey("longitude") || doc.containsKey("weather_enabled");
    saveConfig();
```

Add zone_names handling immediately before that line:
```cpp
    if (doc.containsKey("zone_names")) {
      JsonArray zn = doc["zone_names"].as<JsonArray>();
      for (int i = 0; i < 8 && i < (int)zn.size(); i++) {
        const char* n = zn[i].as<const char*>();
        if (n) strlcpy(boardConfig.zoneNames[i], n, sizeof(boardConfig.zoneNames[i]));
      }
    }
    bool locationChanged = doc.containsKey("latitude") || doc.containsKey("longitude") || doc.containsKey("weather_enabled");
    saveConfig();
```

- [ ] **Step 6: Compile in Arduino IDE — verify no errors**

- [ ] **Step 7: Commit**
```
git add esp32_firmware/sprinkler_controller/sprinkler_controller.ino
git commit -m "Add zoneNames to BoardConfig and wire through config API"
```

---

## Task 2: Firmware — appendRunLog() + GET /api/history

**Files:**
- Modify: `esp32_firmware/sprinkler_controller/sprinkler_controller.ino`

- [ ] **Step 1: Add `RUN_LOG_FILE` constant near the top defines**

Find:
```cpp
#define CONFIG_FILE    "/config.json"
```

Add immediately after:
```cpp
#define RUN_LOG_FILE   "/runs.json"
#define RUN_LOG_MAX    60
```

- [ ] **Step 2: Add `appendRunLog()` function**

Add immediately before the `fetchWeather()` function (look for `// ═══ WEATHER` section header):

```cpp
void appendRunLog(time_t ts, const char* trigger, int zoneIdxs[], int zoneSecs[], int count, const char* skipReason) {
  // Don't log empty non-skip entries (e.g. stopped before any zone ran)
  if (count == 0 && skipReason == nullptr) return;

  DynamicJsonDocument doc(8192);

  if (LittleFS.exists(RUN_LOG_FILE)) {
    File f = LittleFS.open(RUN_LOG_FILE, "r");
    if (f) {
      DeserializationError err = deserializeJson(doc, f);
      f.close();
      if (err || !doc.is<JsonArray>()) doc.to<JsonArray>();
    }
  } else {
    doc.to<JsonArray>();
  }

  JsonArray arr = doc.as<JsonArray>();
  JsonObject entry = arr.createNestedObject();
  entry["ts"]      = (long)ts;
  entry["trigger"] = trigger;
  JsonArray zones  = entry.createNestedArray("zones");
  for (int i = 0; i < count; i++) {
    JsonObject z = zones.createNestedObject();
    z["z"]    = zoneIdxs[i];
    z["name"] = boardConfig.zoneNames[zoneIdxs[i]];
    z["sec"]  = zoneSecs[i];
  }
  if (skipReason) entry["skip"] = skipReason;
  else            entry["skip"] = nullptr;

  while ((int)arr.size() > RUN_LOG_MAX) arr.remove(0);

  File f = LittleFS.open(RUN_LOG_FILE, "w");
  if (f) { serializeJson(doc, f); f.close(); }
  Serial.printf("[LOG] Run logged: trigger=%s zones=%d skip=%s\n",
    trigger, count, skipReason ? skipReason : "none");
}
```

- [ ] **Step 3: Add `GET /api/history` endpoint**

Find a good insertion point — after the existing `GET /api/weather` handler, before `POST /api/relay_test`. Look for:
```cpp
  // ── POST /api/relay_test
```

Add immediately before that line:
```cpp
  // ── GET /api/history ─────────────────────────────────────────
  server.on("/api/history", HTTP_GET, []() {
    if (!LittleFS.exists(RUN_LOG_FILE)) {
      server.send(200, "application/json", "[]");
      return;
    }
    File f = LittleFS.open(RUN_LOG_FILE, "r");
    if (!f) { server.send(200, "application/json", "[]"); return; }
    server.streamFile(f, "application/json");
    f.close();
  });

```

- [ ] **Step 4: Compile in Arduino IDE — verify no errors**

- [ ] **Step 5: Commit**
```
git add esp32_firmware/sprinkler_controller/sprinkler_controller.ino
git commit -m "Add appendRunLog() and GET /api/history"
```

---

## Task 3: Firmware — wire appendRunLog into run tasks + skip

**Files:**
- Modify: `esp32_firmware/sprinkler_controller/sprinkler_controller.ino`

- [ ] **Step 1: Track actual zone seconds in `runSequenceTask` and call `appendRunLog` at teardown**

Find the start of `runSequenceTask`, just after `stopRequested = false;`:
```cpp
  stopRequested = false;

  if (boardConfig.cycleAndSoakEnabled) {
```

Add `actualSecs` declaration immediately after `stopRequested = false;`:
```cpp
  stopRequested = false;
  int actualSecs[NUM_ZONES] = {0};

  if (boardConfig.cycleAndSoakEnabled) {
```

In the C&S block, find the line that closes the outer `while (!stopRequested)` loop:
```cpp
    }
  } else {
    // ── Standard sequential mode
```

Add zone time capture immediately before `} else {`:
```cpp
      }
    }
    // Capture actual time per zone after C&S completes
    for (int z = 0; z < NUM_ZONES; z++) {
      actualSecs[z] = daySched.durations[z] * 60 - remaining[z];
    }
  } else {
    // ── Standard sequential mode
```

In the standard mode block, find where each zone's relay turns off:
```cpp
      RELAY_OFF(RELAY_PINS[z]);
      Serial.printf("[RUN] Zone %d OFF\n", z + 1);
      if (stopRequested) break;
```

Add `actualSecs` update immediately after `RELAY_OFF`:
```cpp
      RELAY_OFF(RELAY_PINS[z]);
      actualSecs[z] = durMin * 60 - rem;
      Serial.printf("[RUN] Zone %d OFF\n", z + 1);
      if (stopRequested) break;
```

Find the teardown section just before `vTaskDelete(NULL)`:
```cpp
  Serial.println("[RUN] Sequence complete");
  vTaskDelete(NULL);
```

Add `appendRunLog` call immediately before `vTaskDelete`:
```cpp
  Serial.println("[RUN] Sequence complete");
  {
    time_t ts = time(nullptr);
    int logZ[NUM_ZONES], logS[NUM_ZONES], logCount = 0;
    for (int z = 0; z < NUM_ZONES; z++) {
      if (actualSecs[z] > 0) { logZ[logCount] = z; logS[logCount] = actualSecs[z]; logCount++; }
    }
    appendRunLog(ts, manual ? "manual" : "schedule", logZ, logS, logCount, nullptr);
  }
  vTaskDelete(NULL);
```

- [ ] **Step 2: Track actual zone seconds in `runSingleZoneTask` and call `appendRunLog` at teardown**

Find the start of `runSingleZoneTask`, just after `stopRequested = false;`:
```cpp
  stopRequested = false;

  if (boardConfig.cycleAndSoakEnabled) {
```

Add `actualSec` declaration immediately after `stopRequested = false;`:
```cpp
  stopRequested = false;
  int actualSec = 0;

  if (boardConfig.cycleAndSoakEnabled) {
```

In the C&S block for single zone, find the closing brace of the `while (totalRemaining > 0 && !stopRequested)` loop:
```cpp
    }
  } else {
    RELAY_ON(RELAY_PINS[z]);
    int remaining = dur * 60;
```

Add `actualSec` capture immediately before `} else {`:
```cpp
    }
    actualSec = dur * 60 - totalRemaining;
  } else {
    RELAY_ON(RELAY_PINS[z]);
    int remaining = dur * 60;
```

In the standard mode block, find where the single zone's relay turns off:
```cpp
    RELAY_OFF(RELAY_PINS[z]);
  }

  allRelaysOff();
```

Add `actualSec` capture immediately after `RELAY_OFF`:
```cpp
    RELAY_OFF(RELAY_PINS[z]);
    actualSec = dur * 60 - remaining;
  }

  allRelaysOff();
```

Find the teardown just before `vTaskDelete(NULL)` in `runSingleZoneTask`:
```cpp
  xSemaphoreGive(statusMutex);
  vTaskDelete(NULL);
}

bool startSequence(
```

Add `appendRunLog` call immediately before `vTaskDelete`:
```cpp
  xSemaphoreGive(statusMutex);
  if (actualSec > 0) {
    time_t ts = time(nullptr);
    int logZ[1] = {z}, logS[1] = {actualSec};
    appendRunLog(ts, "manual", logZ, logS, 1, nullptr);
  }
  vTaskDelete(NULL);
}

bool startSequence(
```

- [ ] **Step 3: Log weather skips in `scheduleCheckerTask`**

Find inside `scheduleCheckerTask`, after the skip serial print:
```cpp
        Serial.printf("[SCHED] Run SKIPPED — %s (rain today: %d%%, rain tomorrow: %d%%, min: %.1f°C)\n",
          skipReason, rainToday, rainTomrw, minTemp);
```

Add `appendRunLog` call immediately after that `Serial.printf`:
```cpp
        Serial.printf("[SCHED] Run SKIPPED — %s (rain today: %d%%, rain tomorrow: %d%%, min: %.1f°C)\n",
          skipReason, rainToday, rainTomrw, minTemp);
        appendRunLog(now, "schedule", nullptr, nullptr, 0, skipReason);
```

- [ ] **Step 4: Compile in Arduino IDE — verify no errors**

- [ ] **Step 5: Commit**
```
git add esp32_firmware/sprinkler_controller/sprinkler_controller.ino
git commit -m "Wire appendRunLog into run tasks and weather skip"
```

---

## Task 4: UI — migrate zone names from localStorage to board

**Files:**
- Modify: `esp32_firmware/sprinkler_controller/data/index.html`

- [ ] **Step 1: Replace `getNames`/`setNames` with a `zoneNames` module variable**

Find:
```javascript
// ── Zone names (localStorage) ──────────────────────────
function getNames() {
  try { return JSON.parse(localStorage.getItem('znames')) || Array.from({length:ZONES},(_,i)=>`Zone ${i+1}`); }
  catch(e) { return Array.from({length:ZONES},(_,i)=>`Zone ${i+1}`); }
}
function setNames(n) { localStorage.setItem('znames', JSON.stringify(n)); }
```

Replace with:
```javascript
// ── Zone names (loaded from board, fallback to numbered) ───────
let zoneNames = Array.from({length: ZONES}, (_, i) => `Zone ${i+1}`);
```

- [ ] **Step 2: Extend `loadTz()` to also populate `zoneNames` from `/api/config`**

Find the end of `loadTz()`:
```javascript
async function loadTz() {
  try {
    const r = await GET('/api/config');
    if (r.timezone) {
```

After the existing timezone block inside `loadTz()`, find the closing `} catch(e) {}` and add zone name population before it. The full updated function:

```javascript
async function loadTz() {
  try {
    const r = await GET('/api/config');
    if (r.timezone) {
      const sel = document.getElementById('tz-select');
      if (sel) {
        let matched = false;
        for (const opt of sel.options) {
          if (opt.value === r.timezone) { sel.value = r.timezone; matched = true; break; }
        }
        if (!matched) {
          const opt = document.createElement('option');
          opt.value = r.timezone; opt.textContent = r.timezone;
          sel.insertBefore(opt, sel.firstChild);
          sel.value = r.timezone;
        }
      }
    }
    if (r.zone_names && r.zone_names.length) {
      zoneNames = r.zone_names.slice(0, ZONES);
      renderZoneNameSettings();
      initManualSelect();
      renderSchedule();
    }
  } catch(e) {}
}
```

- [ ] **Step 3: Update `renderZoneNameSettings()` to use `zoneNames`**

Find:
```javascript
function renderZoneNameSettings() {
  const list = document.getElementById('zoneNamesList');
  if (!list) return;
  const names = getNames();
  list.innerHTML = names.map((n,i) => `
```

Replace:
```javascript
function renderZoneNameSettings() {
  const list = document.getElementById('zoneNamesList');
  if (!list) return;
  list.innerHTML = zoneNames.map((n,i) => `
```

- [ ] **Step 4: Update `updateZname()` to POST to the board**

Find:
```javascript
function updateZname(i, v) {
  const n = getNames(); n[i] = v.trim() || `Zone ${i+1}`; setNames(n);
  initManualSelect();
  renderSchedule(); // refresh zone names in schedule
  if (lastStatus) highlightRunningZone(lastStatus);
}
```

Replace:
```javascript
async function updateZname(i, v) {
  zoneNames[i] = v.trim() || `Zone ${i+1}`;
  const inp = document.getElementById(`zn${i}`);
  if (inp) inp.value = zoneNames[i];
  await POST('/api/config', { zone_names: zoneNames });
  initManualSelect();
  renderSchedule();
  if (lastStatus) highlightRunningZone(lastStatus);
}
```

- [ ] **Step 5: Replace remaining `getNames()` callers with `zoneNames`**

Find (in `updateBanner`):
```javascript
  const names    = getNames();
```
Replace:
```javascript
  const names    = zoneNames;
```

Find (in `renderSchedule`):
```javascript
  const names = getNames();
```
Replace:
```javascript
  const names = zoneNames;
```

Find (in manual zone select — `initManualSelect` or similar):
```javascript
  sel.innerHTML = getNames().map((n,i) => `<option value="${i}">${n}</option>`).join('');
```
Replace:
```javascript
  sel.innerHTML = zoneNames.map((n,i) => `<option value="${i}">${n}</option>`).join('');
```

Find (toast on manual run):
```javascript
  r.message ? toast(`${getNames()[z]} · ${dur}min`) : toast(r.error||'Failed','err');
```
Replace:
```javascript
  r.message ? toast(`${zoneNames[z]} · ${dur}min`) : toast(r.error||'Failed','err');
```

- [ ] **Step 6: Call `loadTz()` at boot so zone names load on page open**

Find the boot sequence at the bottom of the script:
```javascript
loadSchedule();
initManualSelect();
pollStatus();
pollInfo();
loadFwVersion();
```

Add `loadTz()` call:
```javascript
loadSchedule();
loadTz();
initManualSelect();
pollStatus();
pollInfo();
loadFwVersion();
```

- [ ] **Step 7: Commit**
```
git add esp32_firmware/sprinkler_controller/data/index.html
git commit -m "Migrate zone names from localStorage to board config"
```

---

## Task 5: UI — History tab + page + CSS + JS

**Files:**
- Modify: `esp32_firmware/sprinkler_controller/data/index.html`

- [ ] **Step 1: Add History page CSS**

Find at end of the existing weather CSS block (just before `/* ── Toast`):
```css
/* ── Toast
```

Add immediately before it:
```css
/* ── History page ───────────────────────────────────── */
#hist-cards { padding: 0 0 80px; }
.hist-card { background:#131f2e; border:1px solid #1a3a5c; border-radius:10px; padding:10px 12px; margin-bottom:8px; }
.hist-day { font-size:0.95rem; font-weight:700; color:#e0e8f0; }
.hist-date { font-size:0.75rem; color:#4fc3f7; margin-top:1px; }
.hist-skip-txt { font-size:0.78rem; color:#ffb74d; margin-top:4px; }
.hist-norun { font-size:0.75rem; color:#2a4060; font-style:italic; margin-top:4px; }
.hist-zone-row { display:flex; align-items:center; gap:8px; margin-top:5px; }
.hist-zone-name { font-size:0.72rem; color:#607080; width:80px; flex-shrink:0; overflow:hidden; text-overflow:ellipsis; white-space:nowrap; }
.hist-bar-track { flex:1; background:#0a1520; border-radius:3px; height:16px; overflow:hidden; }
.hist-bar-fill { height:100%; border-radius:3px; display:flex; align-items:center; padding-left:6px; }
.hist-bar-label { font-size:0.62rem; font-weight:700; color:#0d1b2a; white-space:nowrap; }

```

- [ ] **Step 2: Add History page HTML**

Find the weather page closing div (just before `<nav class="bottom-nav">`):
```html
</div>

<nav class="bottom-nav">
```

Add the History page div immediately before `<nav`:
```html
</div>

<div class="page" id="page-history">
  <div class="sec-hdr"><span class="sec-title">Run History</span></div>
  <div id="hist-cards"></div>
</div>

<nav class="bottom-nav">
```

- [ ] **Step 3: Add History tab to the nav bar**

Find the bottom nav HTML:
```html
<nav class="bottom-nav">
  <button class="nav-btn active" onclick="showPage('schedule',this)">
```

The current nav has tabs for schedule, manual, weather, settings, status. Add a History tab between weather and settings. Find:
```html
  <button class="nav-btn" onclick="showPage('settings',this)">
    <div style="font-size:1.3rem">⚙</div>
    <div style="font-size:0.65rem">Settings</div>
  </button>
```

Add History tab immediately before Settings:
```html
  <button class="nav-btn" onclick="showPage('history',this)">
    <div style="font-size:1.3rem">📋</div>
    <div style="font-size:0.65rem">History</div>
  </button>
  <button class="nav-btn" onclick="showPage('settings',this)">
    <div style="font-size:1.3rem">⚙</div>
    <div style="font-size:0.65rem">Settings</div>
  </button>
```

- [ ] **Step 4: Update `showPage()` to handle history tab**

Find:
```javascript
  if (pg==='settings') { renderZoneNameSettings(); loadTz(); loadWeatherSettings(); loadCycleAndSoak(); pollInfo(); }
  if (pg==='weather')  updateWeatherPage();
```

Add history case:
```javascript
  if (pg==='settings') { renderZoneNameSettings(); loadTz(); loadWeatherSettings(); loadCycleAndSoak(); pollInfo(); }
  if (pg==='weather')  updateWeatherPage();
  if (pg==='history')  loadHistoryPage();
```

- [ ] **Step 5: Add `loadHistoryPage()` JS function**

Add immediately before `function refreshWeatherPage()`:

```javascript
// ── History ────────────────────────────────────────────
async function loadHistoryPage() {
  const container = document.getElementById('hist-cards');
  if (!container) return;
  container.innerHTML = '<div style="text-align:center;padding:40px;color:#607080">Loading…</div>';
  try {
    const entries = await GET('/api/history');
    if (!Array.isArray(entries) || !entries.length) {
      container.innerHTML = '<div style="text-align:center;padding:40px;color:#607080">No runs recorded yet.</div>';
      return;
    }
    const reversed = [...entries].reverse();
    let maxSec = 1;
    reversed.forEach(e => { if (e.zones) e.zones.forEach(z => { if (z.sec > maxSec) maxSec = z.sec; }); });

    const DAY_NAMES = ['Sunday','Monday','Tuesday','Wednesday','Thursday','Friday','Saturday'];
    container.innerHTML = reversed.map(e => {
      const d       = new Date(e.ts * 1000);
      const dayName = DAY_NAMES[d.getDay()];
      const dateStr = d.toLocaleDateString('en-US', {month:'long', day:'numeric', year:'numeric'});
      const manual  = e.trigger === 'manual';

      let inner = '';
      if (e.skip) {
        inner = `<div class="hist-skip-txt">⚠ Skipped — ${e.skip}</div>
                 <div class="hist-norun">No watering</div>`;
      } else if (!e.zones || !e.zones.length) {
        inner = '<div class="hist-norun">No schedule</div>';
      } else {
        inner = e.zones.map(z => {
          const min = Math.round(z.sec / 60);
          if (min === 0) return '';
          const pct   = Math.max(4, Math.round(z.sec / maxSec * 100));
          const color = manual ? '#81c784' : '#4fc3f7';
          return `<div class="hist-zone-row">
            <span class="hist-zone-name">${z.name}</span>
            <div class="hist-bar-track">
              <div class="hist-bar-fill" style="width:${pct}%;background:${color}">
                <span class="hist-bar-label">${min} min</span>
              </div>
            </div>
          </div>`;
        }).join('');
      }
      return `<div class="hist-card">
        <div class="hist-day">${dayName}</div>
        <div class="hist-date">${dateStr}${manual ? ' · manual run' : ''}</div>
        ${inner}
      </div>`;
    }).join('');
  } catch(e) {
    container.innerHTML = '<div style="text-align:center;padding:40px;color:#607080">Could not load history.</div>';
  }
}

```

- [ ] **Step 6: Commit**
```
git add esp32_firmware/sprinkler_controller/data/index.html
git commit -m "Add History tab: daily run cards with zone bars"
```

---

## Task 6: Version bump, regenerate html_content.h, compile, push

**Files:**
- Modify: `esp32_firmware/sprinkler_controller/sprinkler_controller.ino`
- Modify: `esp32_firmware/sprinkler_controller/html_content.h`

- [ ] **Step 1: Bump FW_VERSION to 1.7.0**

Find:
```cpp
#define FW_VERSION     "1.6.0"
```

Replace:
```cpp
#define FW_VERSION     "1.7.0"
```

- [ ] **Step 2: Regenerate `html_content.h`**

Run from project root:
```powershell
python -c "
with open('esp32_firmware/sprinkler_controller/data/index.html', 'r', encoding='utf-8') as f:
    content = f.read()
out = '// Auto-generated — do not edit. Rebuild from data/index.html\n'
out += 'static const char INDEX_HTML[] PROGMEM = R\"====(\n'
out += content
out += '\n)====\" ;\n'.replace(' ', '')
with open('esp32_firmware/sprinkler_controller/html_content.h', 'w', encoding='utf-8') as f:
    f.write(out)
print('Done')
"
```

- [ ] **Step 3: Compile in Arduino IDE — verify no errors, confirm sketch size under 1.9MB**

- [ ] **Step 4: Commit and push**
```
git add esp32_firmware/sprinkler_controller/sprinkler_controller.ino
git add esp32_firmware/sprinkler_controller/html_content.h
git commit -m "v1.7.0: run history, zone names on board"
git push origin custom-board
```

---

## Self-Review

**Spec coverage:**
- ✅ Zone names in `BoardConfig` (`char zoneNames[8][32]`) — Task 1
- ✅ Zone names in `loadConfig` defaults + JSON read — Task 1 Step 2
- ✅ Zone names in `saveConfig` — Task 1 Step 3
- ✅ Zone names in `GET /api/config` — Task 1 Step 4
- ✅ Zone names in `POST /api/config` — Task 1 Step 5
- ✅ `appendRunLog()` with zone name snapshot from `boardConfig.zoneNames` — Task 2 Step 2
- ✅ `GET /api/history` — Task 2 Step 3
- ✅ `runSequenceTask` logs actual per-zone seconds — Task 3 Step 1
- ✅ `runSingleZoneTask` logs actual seconds — Task 3 Step 2
- ✅ `scheduleCheckerTask` logs skip — Task 3 Step 3
- ✅ Log capped at 60 entries (RUN_LOG_MAX) — Task 2 Step 2
- ✅ localStorage `getNames`/`setNames` removed — Task 4 Step 1
- ✅ `loadTz()` populates `zoneNames` from `/api/config` — Task 4 Step 2
- ✅ `updateZname()` POSTs to board — Task 4 Step 4
- ✅ All `getNames()` callers replaced — Task 4 Step 5
- ✅ `loadTz()` called at boot — Task 4 Step 6
- ✅ History page CSS — Task 5 Step 1
- ✅ History page HTML — Task 5 Step 2
- ✅ History nav tab — Task 5 Step 3
- ✅ `showPage('history')` wired — Task 5 Step 4
- ✅ `loadHistoryPage()` JS — Task 5 Step 5
- ✅ Zone name snapshotted in log entry (`z.name`) — Task 2 Step 2
- ✅ History rendered from log `name` field directly — Task 5 Step 5
- ✅ Bars proportional to longest zone run — Task 5 Step 5 (`maxSec`)
- ✅ Minutes labeled inside bars — Task 5 Step 5
- ✅ Manual runs green, scheduled runs teal — Task 5 Step 5
- ✅ Skip cards amber — Task 5 Step 5
- ✅ Version bumped to 1.7.0 — Task 6 Step 1

**Type consistency:**
- `appendRunLog(time_t, const char*, int[], int[], int, const char*)` — defined Task 2 Step 2, called Task 3 Steps 1-3 ✓
- `zoneNames[]` — declared Task 4 Step 1, populated Task 4 Step 2, used Tasks 4 Steps 3-5 ✓
- `RUN_LOG_FILE` / `RUN_LOG_MAX` — defined Task 2 Step 1, used Task 2 Step 2 ✓
- `hist-card` / `hist-zone-row` / `hist-bar-track` / `hist-bar-fill` — defined Task 5 Step 1, used Task 5 Step 5 ✓
- `hist-cards` (element id) — defined Task 5 Step 2, queried Task 5 Step 5 ✓
- `page-history` (page id) — defined Task 5 Step 2, referenced Task 5 Steps 3-4 ✓
