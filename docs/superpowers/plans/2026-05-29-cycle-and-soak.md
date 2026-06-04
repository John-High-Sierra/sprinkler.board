# Cycle & Soak Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add Cycle & Soak watering mode — splits each zone's total runtime into short cycles separated by soak periods, preventing runoff on clay or compacted soils.

**Architecture:** A `cycleAndSoakEnabled` flag and `cycleTime` (minutes) are added to `BoardConfig` and persisted in `config.json`. Both `runSequenceTask` and a new named `runSingleZoneTask` function (extracted from the current lambda) implement a cycle path: multi-zone sequences rotate through all active zones each pass; single-zone runs (manual) alternate water/soak periods. When soaking, `runStatus.activeZone = -1` with `isRunning = true` — the UI detects this and shows "Soaking…" in the running banner.

**Tech Stack:** FreeRTOS (ESP32 Arduino), ArduinoJson v6, vanilla JS.

---

## File Map

| File | Change |
|------|--------|
| `esp32_firmware/sprinkler_controller/sprinkler_controller.ino` | BoardConfig expansion, config API, extract `runSingleZoneTask`, modify `runSequenceTask` |
| `esp32_firmware/sprinkler_controller/data/index.html` | Cycle & Soak settings section, soaking banner state |
| `esp32_firmware/sprinkler_controller/html_content.h` | Regenerated from `data/index.html` |

---

## Task 1: Add cycleAndSoakEnabled + cycleTime to BoardConfig and config API

**Files:**
- Modify: `esp32_firmware/sprinkler_controller/sprinkler_controller.ino`

- [ ] **Step 1: Expand the BoardConfig struct**

Find:
```cpp
struct BoardConfig {
  char timezone[64];
  float latitude;
  float longitude;
  bool weatherEnabled;
  int  rainThreshold;
  float freezeThreshold;
};
```
Replace with:
```cpp
struct BoardConfig {
  char timezone[64];
  float latitude;
  float longitude;
  bool weatherEnabled;
  int  rainThreshold;
  float freezeThreshold;
  bool cycleAndSoakEnabled;
  int  cycleTime;            // minutes per cycle, default 4
};
```

- [ ] **Step 2: Update loadConfig() defaults and JSON reads**

Find the defaults block at the top of `loadConfig()` and add two lines:
```cpp
  boardConfig.cycleAndSoakEnabled = false;
  boardConfig.cycleTime           = 4;
```

Find the JSON read block and add:
```cpp
  boardConfig.cycleAndSoakEnabled = doc["cycle_and_soak_enabled"] | false;
  boardConfig.cycleTime           = doc["cycle_time"]             | 4;
```

- [ ] **Step 3: Update saveConfig() to write new fields**

In `saveConfig()`, find the block where fields are serialised and add:
```cpp
  doc["cycle_and_soak_enabled"] = boardConfig.cycleAndSoakEnabled;
  doc["cycle_time"]             = boardConfig.cycleTime;
```

- [ ] **Step 4: Update GET /api/config to return new fields**

In the `GET /api/config` lambda, add:
```cpp
  doc["cycle_and_soak_enabled"] = boardConfig.cycleAndSoakEnabled;
  doc["cycle_time"]             = boardConfig.cycleTime;
```
Also change the `DynamicJsonDocument` size from `512` to `768` to accommodate the extra fields.

- [ ] **Step 5: Update POST /api/config to accept new fields**

In the `POST /api/config` lambda, add inside the containsKey block:
```cpp
  if (doc.containsKey("cycle_and_soak_enabled")) boardConfig.cycleAndSoakEnabled = doc["cycle_and_soak_enabled"].as<bool>();
  if (doc.containsKey("cycle_time"))             boardConfig.cycleTime           = doc["cycle_time"].as<int>();
```

- [ ] **Step 6: Compile check — read the changed sections and verify C++ syntax is valid**

- [ ] **Step 7: Commit**
```
git add esp32_firmware/sprinkler_controller/sprinkler_controller.ino
git commit -m "Add cycleAndSoakEnabled and cycleTime to BoardConfig and config API"
```

---

## Task 2: Extract single-zone lambda to named function + add Cycle & Soak

**Files:**
- Modify: `esp32_firmware/sprinkler_controller/sprinkler_controller.ino`

The current single-zone run is an inline lambda passed to `xTaskCreate` inside the `POST /api/run_zone` handler. Extract it to a named `runSingleZoneTask` function so Cycle & Soak logic can be added cleanly.

- [ ] **Step 1: Add the SingleZoneArgs struct before runSequenceTask**

Find the existing `struct RunArgs` near `runSequenceTask`. Add immediately after it:
```cpp
struct SingleZoneArgs { int zone; int dur; };
```

(The struct may already exist as a local — if so, move it to file scope here and remove the local definition in the next step.)

- [ ] **Step 2: Add the named runSingleZoneTask function**

Add the following complete function immediately before `startSequence()`:

```cpp
void runSingleZoneTask(void* p) {
  SingleZoneArgs* a = (SingleZoneArgs*)p;
  int z   = a->zone;
  int dur = a->dur;
  delete a;

  stopRequested = false;

  if (boardConfig.cycleAndSoakEnabled) {
    int cycleTimeSec   = boardConfig.cycleTime * 60;
    int totalRemaining = dur * 60;

    while (totalRemaining > 0 && !stopRequested) {
      int thisRun = min(cycleTimeSec, totalRemaining);
      Serial.printf("[RUN] C&S zone %d: water %d sec\n", z + 1, thisRun);
      RELAY_ON(RELAY_PINS[z]);

      int elapsed = 0;
      while (elapsed < thisRun && !stopRequested) {
        xSemaphoreTake(statusMutex, portMAX_DELAY);
        runStatus.isRunning     = true;
        runStatus.dayIndex      = -1;
        runStatus.activeZone    = z;
        runStatus.remainingTime = totalRemaining - elapsed;
        runStatus.manualRun     = true;
        xSemaphoreGive(statusMutex);
        vTaskDelay(pdMS_TO_TICKS(1000));
        elapsed++;
      }

      RELAY_OFF(RELAY_PINS[z]);
      totalRemaining -= elapsed;

      if (totalRemaining > 0 && !stopRequested) {
        Serial.printf("[RUN] C&S zone %d: soak %d sec, %d sec remaining\n", z + 1, cycleTimeSec, totalRemaining);
        int soakElapsed = 0;
        while (soakElapsed < cycleTimeSec && !stopRequested) {
          xSemaphoreTake(statusMutex, portMAX_DELAY);
          runStatus.isRunning     = true;
          runStatus.dayIndex      = -1;
          runStatus.activeZone    = -1; // soaking indicator
          runStatus.remainingTime = totalRemaining;
          runStatus.manualRun     = true;
          xSemaphoreGive(statusMutex);
          vTaskDelay(pdMS_TO_TICKS(1000));
          soakElapsed++;
        }
      }
    }
  } else {
    RELAY_ON(RELAY_PINS[z]);
    int remaining = dur * 60;
    while (remaining > 0 && !stopRequested) {
      xSemaphoreTake(statusMutex, portMAX_DELAY);
      runStatus.isRunning     = true;
      runStatus.dayIndex      = -1;
      runStatus.activeZone    = z;
      runStatus.remainingTime = remaining;
      runStatus.manualRun     = true;
      xSemaphoreGive(statusMutex);
      vTaskDelay(pdMS_TO_TICKS(1000));
      remaining--;
    }
    RELAY_OFF(RELAY_PINS[z]);
  }

  allRelaysOff();
  xSemaphoreTake(statusMutex, portMAX_DELAY);
  runStatus.isRunning     = false;
  runStatus.dayIndex      = -1;
  runStatus.activeZone    = -1;
  runStatus.remainingTime = 0;
  runStatus.manualRun     = false;
  xSemaphoreGive(statusMutex);
  vTaskDelete(NULL);
}
```

- [ ] **Step 3: Replace the inline lambda in POST /api/run_zone with a call to runSingleZoneTask**

Find the `xTaskCreate([](void* p) { ... }, "single_zone", 2048, args, 1, NULL);` block inside the `/api/run_zone` handler.

Replace the entire `xTaskCreate(...)` call with:
```cpp
    xTaskCreate(runSingleZoneTask, "single_zone", 4096, args, 1, NULL);
```

Note: stack bumped from 2048 → 4096 to accommodate Cycle & Soak's deeper call path.

Also remove the local `struct SingleZoneArgs { int zone; int dur; };` definition that was inside the lambda scope (it's now at file scope from Step 1).

- [ ] **Step 4: Compile check**

- [ ] **Step 5: Commit**
```
git add esp32_firmware/sprinkler_controller/sprinkler_controller.ino
git commit -m "Extract runSingleZoneTask + add Cycle and Soak for manual single-zone runs"
```

---

## Task 3: Add Cycle & Soak to runSequenceTask

**Files:**
- Modify: `esp32_firmware/sprinkler_controller/sprinkler_controller.ino`

- [ ] **Step 1: Replace the body of runSequenceTask with the cycle-aware version**

Find `void runSequenceTask(void* param)` and replace its entire body (from the opening `{` to the closing `}`) with:

```cpp
void runSequenceTask(void* param) {
  RunArgs* args = (RunArgs*)param;
  int dayIndex  = args->dayIndex;
  bool manual   = args->manual;
  delete args;

  const char* dayNames[] = {"Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Sunday"};
  Serial.printf("[RUN] Starting sequence for %s\n", dayNames[dayIndex]);

  xSemaphoreTake(scheduleMutex, portMAX_DELAY);
  DaySchedule daySched = schedule.days[dayIndex];
  xSemaphoreGive(scheduleMutex);

  stopRequested = false;

  if (boardConfig.cycleAndSoakEnabled) {
    // ── Cycle & Soak mode ────────────────────────────────
    int cycleTimeSec = boardConfig.cycleTime * 60;
    int remaining[NUM_ZONES];
    int activeCount = 0;
    for (int z = 0; z < NUM_ZONES; z++) {
      remaining[z] = daySched.durations[z] * 60;
      if (remaining[z] > 0) activeCount++;
    }

    int passNum = 0;
    while (!stopRequested) {
      bool anyLeft = false;
      for (int z = 0; z < NUM_ZONES; z++) {
        if (remaining[z] > 0) { anyLeft = true; break; }
      }
      if (!anyLeft) break;

      passNum++;
      Serial.printf("[RUN] C&S pass %d\n", passNum);

      for (int z = 0; z < NUM_ZONES; z++) {
        if (stopRequested) break;
        if (remaining[z] <= 0) continue;

        int thisRun = min(cycleTimeSec, remaining[z]);
        Serial.printf("[RUN] C&S pass %d zone %d: %d sec\n", passNum, z + 1, thisRun);
        RELAY_ON(RELAY_PINS[z]);

        int elapsed = 0;
        while (elapsed < thisRun && !stopRequested) {
          xSemaphoreTake(statusMutex, portMAX_DELAY);
          runStatus.isRunning     = true;
          runStatus.dayIndex      = dayIndex;
          runStatus.activeZone    = z;
          runStatus.remainingTime = remaining[z] - elapsed;
          runStatus.manualRun     = manual;
          xSemaphoreGive(statusMutex);
          vTaskDelay(pdMS_TO_TICKS(1000));
          elapsed++;
        }

        remaining[z] -= elapsed;
        RELAY_OFF(RELAY_PINS[z]);
        Serial.printf("[RUN] C&S zone %d OFF, %d sec remaining\n", z + 1, remaining[z]);

        // Single active zone: explicit soak between cycles
        if (activeCount == 1 && remaining[z] > 0 && !stopRequested) {
          Serial.printf("[RUN] C&S soaking %d sec\n", cycleTimeSec);
          int soakElapsed = 0;
          while (soakElapsed < cycleTimeSec && !stopRequested) {
            xSemaphoreTake(statusMutex, portMAX_DELAY);
            runStatus.isRunning     = true;
            runStatus.dayIndex      = dayIndex;
            runStatus.activeZone    = -1; // soaking
            runStatus.remainingTime = remaining[z];
            runStatus.manualRun     = manual;
            xSemaphoreGive(statusMutex);
            vTaskDelay(pdMS_TO_TICKS(1000));
            soakElapsed++;
          }
        }
      }
    }

  } else {
    // ── Standard sequential mode (unchanged) ─────────────
    for (int z = 0; z < NUM_ZONES; z++) {
      if (stopRequested) {
        Serial.println("[RUN] Stop requested, halting sequence");
        break;
      }

      int durMin = daySched.durations[z];
      if (durMin <= 0) {
        Serial.printf("[RUN] Zone %d skipped (duration=0)\n", z + 1);
        continue;
      }

      Serial.printf("[RUN] Zone %d ON for %d min\n", z + 1, durMin);
      RELAY_ON(RELAY_PINS[z]);
      int rem = durMin * 60;

      while (rem > 0) {
        if (stopRequested) {
          Serial.printf("[RUN] Stop during zone %d\n", z + 1);
          break;
        }
        xSemaphoreTake(statusMutex, portMAX_DELAY);
        runStatus.isRunning     = true;
        runStatus.dayIndex      = dayIndex;
        runStatus.activeZone    = z;
        runStatus.remainingTime = rem;
        runStatus.manualRun     = manual;
        xSemaphoreGive(statusMutex);
        vTaskDelay(pdMS_TO_TICKS(1000));
        rem--;
      }

      RELAY_OFF(RELAY_PINS[z]);
      Serial.printf("[RUN] Zone %d OFF\n", z + 1);
      if (stopRequested) break;
    }
  }

  allRelaysOff();

  xSemaphoreTake(statusMutex, portMAX_DELAY);
  runStatus.isRunning     = false;
  runStatus.dayIndex      = -1;
  runStatus.activeZone    = -1;
  runStatus.remainingTime = 0;
  xSemaphoreGive(statusMutex);

  Serial.println("[RUN] Sequence complete");
  vTaskDelete(NULL);
}
```

- [ ] **Step 2: Compile check**

- [ ] **Step 3: Commit**
```
git add esp32_firmware/sprinkler_controller/sprinkler_controller.ino
git commit -m "Add Cycle and Soak mode to runSequenceTask"
```

---

## Task 4: UI — Cycle & Soak settings, soaking banner state, regenerate, version bump

**Files:**
- Modify: `esp32_firmware/sprinkler_controller/data/index.html`
- Modify: `esp32_firmware/sprinkler_controller/html_content.h` (regenerated)
- Modify: `esp32_firmware/sprinkler_controller/sprinkler_controller.ino` (version bump)

- [ ] **Step 1: Add Cycle & Soak section to Settings page in data/index.html**

Find the Weather Skip section in the Settings page. Add the following block immediately AFTER it (before the Updates section):

```html
<div class="sec-hdr"><span class="sec-title">Cycle &amp; Soak</span></div>
<div class="settings-section">
  <div class="settings-row">
    <span class="settings-label">Enable Cycle &amp; Soak</span>
    <label class="toggle"><input type="checkbox" id="cs-enabled-chk"><span class="slider"></span></label>
  </div>
  <div class="settings-row">
    <span class="settings-label">Cycle time</span>
    <div style="display:flex;align-items:center;gap:6px">
      <input id="cs-cycle-inp" type="number" min="1" max="30" class="tz-inp" style="width:60px" value="4">
      <span class="settings-val">min</span>
    </div>
  </div>
  <div class="settings-row" style="font-size:0.85rem;color:#607080;padding-top:0">
    Splits each zone into short cycles. Multiple zones soak while others water. Single zone alternates water/soak.
  </div>
  <div class="settings-row">
    <button class="save-btn" onclick="saveCycleAndSoak()">Save Cycle &amp; Soak</button>
  </div>
</div>
```

- [ ] **Step 2: Add JS functions for Cycle & Soak settings**

Add the following functions near the other settings functions (e.g. after `loadWeatherSettings()`):

```javascript
// ── Cycle & Soak settings ──────────────────────────────
async function saveCycleAndSoak() {
  const r = await POST('/api/config', {
    cycle_and_soak_enabled: document.getElementById('cs-enabled-chk').checked,
    cycle_time: parseInt(document.getElementById('cs-cycle-inp').value)
  });
  r.message ? toast('Cycle & Soak saved ✓') : toast(r.error || 'Save failed','err');
}

async function loadCycleAndSoak() {
  try {
    const r = await GET('/api/config');
    document.getElementById('cs-enabled-chk').checked = r.cycle_and_soak_enabled || false;
    document.getElementById('cs-cycle-inp').value      = r.cycle_time ?? 4;
  } catch(e) {}
}
```

- [ ] **Step 3: Call loadCycleAndSoak() when the settings page opens**

Find the `showPage` function. The `if (pg==='settings')` line currently calls:
```javascript
  if (pg==='settings') { renderZoneNameSettings(); loadTz(); loadWeatherSettings(); pollInfo(); }
```

Add `loadCycleAndSoak();` to that call:
```javascript
  if (pg==='settings') { renderZoneNameSettings(); loadTz(); loadWeatherSettings(); loadCycleAndSoak(); pollInfo(); }
```

- [ ] **Step 4: Show "Soaking…" in the running banner when activeZone is -1 during a run**

Find the `updateBanner(s)` function. Inside the `if (s.is_running)` branch, find where it sets the zone name/number display. Add handling for `s.active_sprinkler === -1`:

Look for code that reads `s.active_sprinkler` to display the zone. The exact form will vary but it will be something like:
```javascript
zone.innerHTML = `Zone <span>${s.active_sprinkler + 1}</span>`;
```

Change it so that when `s.active_sprinkler === -1` (soaking), it shows a soaking label instead:
```javascript
if (s.active_sprinkler === -1) {
  zone.innerHTML = '<span style="color:#81d4fa">Soaking…</span>';
} else {
  const name = zoneNames[s.active_sprinkler] || `Zone ${s.active_sprinkler + 1}`;
  zone.innerHTML = `Zone <span>${name}</span>`;
}
```

Read the actual `updateBanner` function carefully before editing — match the existing variable names exactly.

- [ ] **Step 5: Bump FW_VERSION in sprinkler_controller.ino**

```cpp
#define FW_VERSION  "1.5.1"
```

- [ ] **Step 6: Regenerate html_content.h**

Run from the project root:
```powershell
python -c "
with open('esp32_firmware/sprinkler_controller/data/index.html', 'r', encoding='utf-8') as f:
    content = f.read()
out = '// Auto-generated — do not edit. Rebuild from data/index.html\n'
out += 'static const char INDEX_HTML[] PROGMEM = R\"====(\n'
out += content
out += '\n)====\";\n'
with open('esp32_firmware/sprinkler_controller/html_content.h', 'w', encoding='utf-8') as f:
    f.write(out)
print('Done')
"
```

- [ ] **Step 7: Compile check in Arduino IDE — verify sketch size still under 1.9MB**

- [ ] **Step 8: Commit and push**
```
git add esp32_firmware/sprinkler_controller/sprinkler_controller.ino
git add esp32_firmware/sprinkler_controller/data/index.html
git add esp32_firmware/sprinkler_controller/html_content.h
git commit -m "v1.5.1: Cycle and Soak — configurable cycle time, soaking banner state"
git push origin custom-board
```

---

## Self-Review

**Spec coverage:**
- ✅ Total watering time per zone unchanged — `remaining[z]` counts down only during watering, not soaking
- ✅ Multi-zone: rotate through zones each pass, each zone runs `min(cycleTime, remaining)` per pass
- ✅ Single active zone in sequence: explicit soak period between cycles (`activeCount == 1` check)
- ✅ Manual single-zone run: cycle + soak via `runSingleZoneTask`
- ✅ Cycle time configurable, default 4 min — in `BoardConfig.cycleTime`
- ✅ Applies to both scheduled and manual runs — both `runSequenceTask` and `runSingleZoneTask` check `boardConfig.cycleAndSoakEnabled`
- ✅ UI settings (toggle + cycle time input) — Task 4
- ✅ Soaking state visible in running banner (`activeZone = -1` with `isRunning = true`) — Task 4 Step 4

**Potential edge case noted:** If all zone durations are 0 except one, `activeCount = 1` and the single-zone soak path fires even though it's technically part of a day sequence. This is correct behaviour — the soaking behaviour is appropriate whenever only one zone is actually running.

**Stack sizes:** `runSingleZoneTask` uses 4096 stack bytes (bumped from 2048 — Cycle & Soak needs the extra depth). `runSequenceTask` uses its existing 4096 (unchanged, already adequate).
