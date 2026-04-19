# WebServer.h Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace ESPAsyncWebServer with the built-in WebServer.h in sprinkler_controller.ino to eliminate third-party async library dependency and compile errors on ESP32 core 3.x.

**Architecture:** Swap the async server for the synchronous WebServer.h (already bundled with ESP32 Arduino core). Regex path-parameter routes become fixed-path routes with JSON bodies. FreeRTOS tasks for zone sequencing and schedule checking are unchanged. `server.handleClient()` is called in `loop()`.

**Tech Stack:** ESP32 Arduino core 3.3.8, WebServer.h (built-in), LittleFS, ArduinoJson, NTPClient, WiFiManager (tzapu), FreeRTOS

---

## Files

- Modify: `esp32_firmware/sprinkler_controller/sprinkler_controller.ino`
- Modify: `esp32_firmware/sprinkler_controller/data/index.html` (2 API call changes)
- Delete: `C:/Users/harri/AppData/Local/Arduino15/packages/esp32/hardware/esp32/3.3.8/platform.local.txt`

---

### Task 1: Swap includes, server type, and loop()

**Files:**
- Modify: `esp32_firmware/sprinkler_controller/sprinkler_controller.ino` lines 40-50, 124, 729-751

- [ ] **Step 1: Replace includes**

Change lines 40-49 from:
```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <time.h>
```
To:
```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <time.h>
```

- [ ] **Step 2: Change server declaration**

Change line 124 from:
```cpp
AsyncWebServer    server(80);
```
To:
```cpp
WebServer         server(80);
```

- [ ] **Step 3: Update loop() to call handleClient()**

Change lines 744-751 from:
```cpp
void loop() {
  static unsigned long lastNTP = 0;
  if (millis() - lastNTP > 300000) { // Resync every 5 min
    timeClient.update();
    lastNTP = millis();
  }
  delay(1000);
}
```
To:
```cpp
void loop() {
  server.handleClient();
  static unsigned long lastNTP = 0;
  if (millis() - lastNTP > 300000) { // Resync every 5 min
    timeClient.update();
    lastNTP = millis();
  }
  delay(10);
}
```

- [ ] **Step 4: Update header comment** (lines 28-33)

Change:
```cpp
 * Required Libraries (install via Arduino Library Manager):
 *   - ESPAsyncWebServer  (me-no-dev)  — install as ZIP from GitHub
 *   - AsyncTCP           (me-no-dev)  — install as ZIP from GitHub
 *   - WiFiManager        (tzapu)
 *   - ArduinoJson        (bblanchon) v6+
 *   - NTPClient          (Fabrice Weinberg)
```
To:
```cpp
 * Required Libraries (install via Arduino Library Manager):
 *   - WiFiManager        (tzapu)
 *   - ArduinoJson        (bblanchon) v6+
 *   - NTPClient          (Fabrice Weinberg)
 *   (WebServer.h, LittleFS built into ESP32 core — no extra libs needed)
```

---

### Task 2: Rewrite setupRoutes() — simple routes

**Files:**
- Modify: `esp32_firmware/sprinkler_controller/sprinkler_controller.ino` lines 444-645

- [ ] **Step 1: Replace the entire setupRoutes() function**

Replace everything from `void setupRoutes() {` through the closing `}` (lines 444-645) with:

```cpp
// ═══════════════════════════════════════════════════════════════
//  STATIC FILE HELPER
// ═══════════════════════════════════════════════════════════════
void serveFile(const char* path, const char* contentType) {
  File f = LittleFS.open(path, "r");
  if (!f) { server.send(404, "text/plain", "Not found"); return; }
  server.streamFile(f, contentType);
  f.close();
}

// ═══════════════════════════════════════════════════════════════
//  WEB SERVER ROUTES
// ═══════════════════════════════════════════════════════════════
void setupRoutes() {

  // ── Serve frontend ────────────────────────────────────────────
  server.on("/", HTTP_GET, []() { serveFile("/index.html", "text/html"); });
  server.on("/index.html", HTTP_GET, []() { serveFile("/index.html", "text/html"); });

  // ── GET /api/status ───────────────────────────────────────────
  server.on("/api/status", HTTP_GET, []() {
    server.send(200, "application/json", buildStatusJson());
  });

  // ── GET /api/schedule ─────────────────────────────────────────
  server.on("/api/schedule", HTTP_GET, []() {
    server.send(200, "application/json", buildScheduleJson());
  });

  // ── POST /api/schedule ────────────────────────────────────────
  server.on("/api/schedule", HTTP_POST, []() {
    if (!server.hasArg("plain")) {
      server.send(400, "application/json", "{\"error\":\"No body\"}");
      return;
    }
    DynamicJsonDocument doc(4096);
    DeserializationError err = deserializeJson(doc, server.arg("plain"));
    if (err) {
      server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }
    if (!doc.is<JsonArray>()) {
      server.send(400, "application/json", "{\"error\":\"Expected JSON array of 7 days\"}");
      return;
    }
    JsonArray arr = doc.as<JsonArray>();
    if (arr.size() != 7) {
      server.send(400, "application/json", "{\"error\":\"Schedule must have exactly 7 days\"}");
      return;
    }

    xSemaphoreTake(scheduleMutex, portMAX_DELAY);
    for (int d = 0; d < 7; d++) {
      JsonObject day = arr[d];
      schedule.days[d].isActive = day["is_active"] | false;
      schedule.days[d].hour     = day["hour"]      | 7;
      schedule.days[d].minute   = day["minute"]    | 0;
      JsonArray durs = day["durations"].as<JsonArray>();
      for (int z = 0; z < NUM_ZONES && z < (int)durs.size(); z++) {
        schedule.days[d].durations[z] = durs[z] | 0;
      }
    }
    xSemaphoreGive(scheduleMutex);

    saveSchedule();
    server.send(200, "application/json", "{\"message\":\"Schedule updated\"}");
  });

  // ── POST /api/run_day  body: {"day": 0-6} ────────────────────
  server.on("/api/run_day", HTTP_POST, []() {
    if (!server.hasArg("plain")) {
      server.send(400, "application/json", "{\"error\":\"No body\"}");
      return;
    }
    DynamicJsonDocument doc(64);
    deserializeJson(doc, server.arg("plain"));
    int dayIndex = doc["day"] | -1;
    if (dayIndex < 0 || dayIndex > 6) {
      server.send(400, "application/json", "{\"error\":\"day must be 0-6\"}");
      return;
    }
    if (startSequence(dayIndex, true)) {
      server.send(200, "application/json", "{\"message\":\"Sequence started\"}");
    } else {
      server.send(409, "application/json", "{\"error\":\"A sequence is already running\"}");
    }
  });

  // ── POST /api/run_zone  body: {"zone": 0-7, "duration": 1-120} ─
  server.on("/api/run_zone", HTTP_POST, []() {
    if (!server.hasArg("plain")) {
      server.send(400, "application/json", "{\"error\":\"No body\"}");
      return;
    }
    DynamicJsonDocument doc(64);
    deserializeJson(doc, server.arg("plain"));
    int zoneIndex   = doc["zone"]     | -1;
    int durationMin = doc["duration"] | -1;

    xSemaphoreTake(statusMutex, portMAX_DELAY);
    bool running = runStatus.isRunning;
    xSemaphoreGive(statusMutex);

    if (running) {
      server.send(409, "application/json", "{\"error\":\"A sequence is already running\"}");
      return;
    }
    if (zoneIndex < 0 || zoneIndex >= NUM_ZONES) {
      server.send(400, "application/json", "{\"error\":\"Invalid zone index\"}");
      return;
    }
    if (durationMin < 1 || durationMin > 120) {
      server.send(400, "application/json", "{\"error\":\"Duration must be 1-120 minutes\"}");
      return;
    }

    struct SingleZoneArgs { int zone; int dur; };
    SingleZoneArgs* args = new SingleZoneArgs{zoneIndex, durationMin};

    xTaskCreate([](void* p) {
      SingleZoneArgs* a = (SingleZoneArgs*)p;
      int z = a->zone;
      int dur = a->dur;
      delete a;

      stopRequested = false;
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
      xSemaphoreTake(statusMutex, portMAX_DELAY);
      runStatus.isRunning     = false;
      runStatus.dayIndex      = -1;
      runStatus.activeZone    = -1;
      runStatus.remainingTime = 0;
      runStatus.manualRun     = false;
      xSemaphoreGive(statusMutex);
      vTaskDelete(NULL);
    }, "single_zone", 2048, args, 1, NULL);

    server.send(200, "application/json", "{\"message\":\"Zone started\"}");
  });

  // ── POST /api/stop_sequence ───────────────────────────────────
  server.on("/api/stop_sequence", HTTP_POST, []() {
    xSemaphoreTake(statusMutex, portMAX_DELAY);
    bool running = runStatus.isRunning;
    xSemaphoreGive(statusMutex);

    if (!running) {
      server.send(400, "application/json", "{\"error\":\"No sequence is currently running\"}");
      return;
    }
    stopRequested = true;
    server.send(200, "application/json", "{\"message\":\"Stop signal sent\"}");
  });

  // ── POST /api/toggle_schedule ─────────────────────────────────
  server.on("/api/toggle_schedule", HTTP_POST, []() {
    xSemaphoreTake(scheduleMutex, portMAX_DELAY);
    schedule.enabled = !schedule.enabled;
    bool newState = schedule.enabled;
    xSemaphoreGive(scheduleMutex);

    saveSchedule();
    String resp = "{\"enabled\":" + String(newState ? "true" : "false") + "}";
    server.send(200, "application/json", resp);
  });

  // ── GET /api/get_time ─────────────────────────────────────────
  server.on("/api/get_time", HTTP_GET, []() {
    timeClient.update();
    time_t now = timeClient.getEpochTime();
    struct tm* t = localtime(&now);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);
    String resp = "{\"time\":\"" + String(buf) + "\",\"ntp_synced\":" +
                  String(timeClient.isTimeSet() ? "true" : "false") + "}";
    server.send(200, "application/json", resp);
  });

  // ── GET /api/system_info ──────────────────────────────────────
  server.on("/api/system_info", HTTP_GET, []() {
    server.send(200, "application/json", buildSystemInfoJson());
  });

  // ── POST /api/relay_test  body: {"zone": 0-7, "state": 0|1} ──
  server.on("/api/relay_test", HTTP_POST, []() {
    if (!server.hasArg("plain")) {
      server.send(400, "application/json", "{\"error\":\"No body\"}");
      return;
    }
    DynamicJsonDocument doc(64);
    deserializeJson(doc, server.arg("plain"));
    int zone  = doc["zone"]  | -1;
    int state = doc["state"] | -1;

    xSemaphoreTake(statusMutex, portMAX_DELAY);
    bool running = runStatus.isRunning;
    xSemaphoreGive(statusMutex);

    if (running) {
      server.send(409, "application/json", "{\"error\":\"Cannot test while sequence running\"}");
      return;
    }
    if (zone < 0 || zone >= NUM_ZONES) {
      server.send(400, "application/json", "{\"error\":\"Invalid zone\"}");
      return;
    }
    if (state == 1) {
      RELAY_ON(RELAY_PINS[zone]);
    } else {
      RELAY_OFF(RELAY_PINS[zone]);
    }
    String resp = "{\"zone\":" + String(zone) + ",\"state\":" + String(state) + "}";
    server.send(200, "application/json", resp);
  });

  // ── 404 handler ───────────────────────────────────────────────
  server.onNotFound([]() {
    server.send(404, "application/json", "{\"error\":\"Not found\"}");
  });
}
```

---

### Task 3: Update frontend API calls in index.html

**Files:**
- Modify: `esp32_firmware/sprinkler_controller/data/index.html` lines 557-563 and 856, 868

The frontend uses path-parameter URLs for two endpoints. Change them to POST with JSON body.

- [ ] **Step 1: Update mock handler for run_day (line ~557)**

Change:
```javascript
    if (path.startsWith('/api/run_day/')) {
```
To:
```javascript
    if (path === '/api/run_day') {
```

- [ ] **Step 2: Update mock handler for run_zone (line ~562)**

Change:
```javascript
    if (path.startsWith('/api/run_zone/')) {
```
To:
```javascript
    if (path === '/api/run_zone') {
```

- [ ] **Step 3: Update run_day API call (line ~856)**

Change:
```javascript
  const res = await apiPost(`/api/run_day/${dayIndex}`);
```
To:
```javascript
  const res = await apiPost('/api/run_day', {day: dayIndex});
```

- [ ] **Step 4: Update run_zone API call (line ~868)**

Change:
```javascript
  const res = await apiPost(`/api/run_zone/${zone}/${dur}`);
```
To:
```javascript
  const res = await apiPost('/api/run_zone', {zone, duration: dur});
```

---

### Task 4: Clean up

**Files:**
- Delete: `C:/Users/harri/AppData/Local/Arduino15/packages/esp32/hardware/esp32/3.3.8/platform.local.txt`
- No longer needed: `ESP_Async_WebServer` and `Async_TCP` libraries (leave installed — won't cause harm if unused)

- [ ] **Step 1: Delete platform.local.txt**

```bash
rm "C:/Users/harri/AppData/Local/Arduino15/packages/esp32/hardware/esp32/3.3.8/platform.local.txt"
```

- [ ] **Step 2: Restart Arduino IDE and compile**

Expected: clean compile with no ESPAsyncWebServer or mbedtls errors.

- [ ] **Step 3: Flash and test**

1. Flash firmware using boot sequence: hold IO0, tap EN, release IO0 when "Connecting..." appears
2. Open Serial Monitor at 115200 baud — confirm `[BOOT] All systems GO`
3. Navigate to `http://10.110.201.40/` — confirm UI loads
4. Test "Run This Day Now" for any day — confirm relay activates
5. Test manual zone run — confirm relay activates and stops
6. Test Stop button — confirm sequence halts
