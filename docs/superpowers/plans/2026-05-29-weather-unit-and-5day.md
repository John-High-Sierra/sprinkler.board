# Weather Unit & 5-Day Forecast Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add °F/°C preference (stored in config.json), a 5-day forecast strip on the Weather page, and live location/weather refresh without requiring a board reset.

**Architecture:** `temp_unit` ("C"/"F") is added to `BoardConfig` and exposed in both `/api/config` and `/api/weather`. The firmware always stores and fetches temperatures in Celsius; conversion is done entirely in the browser. The Open-Meteo request is expanded to 5 days, `WeatherCache` gains per-day arrays (`hiTemps[5]`, `loTemps[5]`, `rainProb[5]`, `codes[5]`), and `/api/weather` returns a `forecast` array. The `weatherTask` switches from `vTaskDelay` to `ulTaskNotifyTake` with a 1-hour timeout; `POST /api/config` sends a task notification whenever lat/lon or `weather_enabled` changes so a fresh fetch happens immediately. The UI gains a `fmtTemp()` helper, a °F/°C pill toggle on the Weather page, and a colorful 5-day strip matching the approved mockup design.

**Tech Stack:** FreeRTOS task notifications, ArduinoJson v6, Open-Meteo REST API, vanilla JS, existing dark UI CSS variables.

---

## File Map

| File | Change |
|------|--------|
| `esp32_firmware/sprinkler_controller/sprinkler_controller.ino` | `BoardConfig` (add `tempUnit`), `WeatherCache` (5-day arrays), `fetchWeather()`, `shouldSkipForWeather()`, `scheduleCheckerTask` log, `weatherTask()`, `POST /api/config`, `GET /api/config`, `GET /api/weather`, `setup()` |
| `esp32_firmware/sprinkler_controller/data/index.html` | CSS for forecast strip + unit toggle, Weather page HTML, `fmtTemp()`, unit toggle JS, `updateWeatherPage()`, `updateWeatherWidget()`, `loadWeatherSettings()`, `saveWeatherSettings()`, Settings freeze-threshold unit label |
| `esp32_firmware/sprinkler_controller/html_content.h` | Regenerated from `data/index.html` |

---

## Task 1: Firmware — add `temp_unit` to BoardConfig + weatherTaskHandle

**Files:**
- Modify: `esp32_firmware/sprinkler_controller/sprinkler_controller.ino`

- [ ] **Step 1: Add `tempUnit` to `BoardConfig` and add `weatherTaskHandle` global**

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
};
BoardConfig boardConfig;
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
};
BoardConfig boardConfig;
```

Find (line ~162):
```cpp
WeatherCache weatherCache = {0, 0, 0, 0, 0, false, 0};
SemaphoreHandle_t weatherMutex;
```

Replace with:
```cpp
WeatherCache weatherCache = {0, 0, 0, 0, 0, false, 0};
SemaphoreHandle_t weatherMutex;
TaskHandle_t      weatherTaskHandle = NULL;
```

- [ ] **Step 2: Update `loadConfig()` — add tempUnit default and JSON read**

Find inside `loadConfig()`:
```cpp
  boardConfig.cycleAndSoakEnabled = false;
  boardConfig.cycleTime           = 4;
```

Replace with:
```cpp
  boardConfig.cycleAndSoakEnabled = false;
  boardConfig.cycleTime           = 4;
  strlcpy(boardConfig.tempUnit, "C", sizeof(boardConfig.tempUnit));
```

Find inside `loadConfig()`:
```cpp
  boardConfig.cycleAndSoakEnabled = doc["cycle_and_soak_enabled"]| false;
  boardConfig.cycleTime           = doc["cycle_time"]            | 4;
```

Replace with:
```cpp
  boardConfig.cycleAndSoakEnabled = doc["cycle_and_soak_enabled"]| false;
  boardConfig.cycleTime           = doc["cycle_time"]            | 4;
  const char* tu = doc["temp_unit"] | "C";
  strlcpy(boardConfig.tempUnit, (tu[0]=='F' ? "F" : "C"), sizeof(boardConfig.tempUnit));
```

- [ ] **Step 3: Update `saveConfig()` — write tempUnit**

Find inside `saveConfig()`:
```cpp
  doc["cycle_and_soak_enabled"] = boardConfig.cycleAndSoakEnabled;
  doc["cycle_time"]             = boardConfig.cycleTime;
```

Replace with:
```cpp
  doc["cycle_and_soak_enabled"] = boardConfig.cycleAndSoakEnabled;
  doc["cycle_time"]             = boardConfig.cycleTime;
  doc["temp_unit"]              = boardConfig.tempUnit;
```

- [ ] **Step 4: Update `GET /api/config` — return tempUnit**

Find inside the `GET /api/config` lambda:
```cpp
    doc["cycle_and_soak_enabled"] = boardConfig.cycleAndSoakEnabled;
    doc["cycle_time"]             = boardConfig.cycleTime;
```

Replace with:
```cpp
    doc["cycle_and_soak_enabled"] = boardConfig.cycleAndSoakEnabled;
    doc["cycle_time"]             = boardConfig.cycleTime;
    doc["temp_unit"]              = boardConfig.tempUnit;
```

- [ ] **Step 5: Update `POST /api/config` — accept tempUnit and notify weatherTask on location change**

Find at the end of the POST /api/config lambda, just before `saveConfig()`:
```cpp
    if (doc.containsKey("cycle_and_soak_enabled")) boardConfig.cycleAndSoakEnabled = doc["cycle_and_soak_enabled"].as<bool>();
    if (doc.containsKey("cycle_time"))            boardConfig.cycleTime           = doc["cycle_time"].as<int>();
    saveConfig();
    server.send(200, "application/json", "{\"message\":\"Config saved\"}");
```

Replace with:
```cpp
    if (doc.containsKey("cycle_and_soak_enabled")) boardConfig.cycleAndSoakEnabled = doc["cycle_and_soak_enabled"].as<bool>();
    if (doc.containsKey("cycle_time"))            boardConfig.cycleTime           = doc["cycle_time"].as<int>();
    if (doc.containsKey("temp_unit")) {
      const char* tu = doc["temp_unit"].as<const char*>();
      if (tu) strlcpy(boardConfig.tempUnit, (tu[0]=='F' ? "F" : "C"), sizeof(boardConfig.tempUnit));
    }
    // Notify weatherTask to refetch immediately when location or skip toggle changes
    bool locationChanged = doc.containsKey("latitude") || doc.containsKey("longitude") || doc.containsKey("weather_enabled");
    saveConfig();
    if (locationChanged && weatherTaskHandle) {
      xTaskNotify(weatherTaskHandle, 0, eNoAction);
    }
    server.send(200, "application/json", "{\"message\":\"Config saved\"}");
```

- [ ] **Step 6: Update `setup()` to capture `weatherTaskHandle`**

Find:
```cpp
  xTaskCreate(weatherTask,         "weather",     8192, NULL, 1, NULL);
```

Replace with:
```cpp
  xTaskCreate(weatherTask,         "weather",     8192, NULL, 1, &weatherTaskHandle);
```

- [ ] **Step 7: Compile check in Arduino IDE — verify no errors**

- [ ] **Step 8: Commit**
```
git add esp32_firmware/sprinkler_controller/sprinkler_controller.ino
git commit -m "Add temp_unit to BoardConfig and weatherTaskHandle for notify-on-location-change"
```

---

## Task 2: Firmware — 5-day WeatherCache, fetchWeather, weatherTask notify

**Files:**
- Modify: `esp32_firmware/sprinkler_controller/sprinkler_controller.ino`

- [ ] **Step 1: Replace `WeatherCache` struct with 5-day version**

Find (line ~152):
```cpp
struct WeatherCache {
  float currentTemp;      // degrees C
  float minTempToday;     // degrees C
  int   rainProbToday;    // percent
  int   rainProbTomorrow; // percent
  int   weatherCode;      // WMO code
  bool  valid;
  unsigned long fetchedAt; // millis() when last fetched
};
WeatherCache weatherCache = {0, 0, 0, 0, 0, false, 0};
```

Replace with:
```cpp
struct WeatherCache {
  float currentTemp;   // degrees C, current conditions
  int   currentCode;   // WMO weather code, current conditions
  bool  valid;
  unsigned long fetchedAt;
  float hiTemps[5];    // degrees C, daily max, index 0 = today
  float loTemps[5];    // degrees C, daily min, index 0 = today
  int   rainProb[5];   // percent, index 0 = today
  int   codes[5];      // WMO weather code, daily, index 0 = today
};
WeatherCache weatherCache = {};
```

- [ ] **Step 2: Update `fetchWeather()` — 5-day URL and parse**

Replace the entire `fetchWeather()` function body:

```cpp
void fetchWeather() {
  if (boardConfig.latitude == 0.0f && boardConfig.longitude == 0.0f) return;

  char url[384];
  snprintf(url, sizeof(url),
    "https://api.open-meteo.com/v1/forecast"
    "?latitude=%.4f&longitude=%.4f"
    "&current=temperature_2m,weathercode,precipitation"
    "&daily=precipitation_probability_max,temperature_2m_min,temperature_2m_max,weathercode"
    "&timezone=auto&forecast_days=5",
    boardConfig.latitude, boardConfig.longitude);

  Serial.printf("[WEATHER] Fetching: %s\n", url);

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  if (!https.begin(client, url)) {
    Serial.println("[WEATHER] Failed to begin HTTPS");
    return;
  }
  int code = https.GET();
  if (code != 200) {
    Serial.printf("[WEATHER] HTTP error: %d\n", code);
    https.end();
    return;
  }

  String body = https.getString();
  https.end();

  DynamicJsonDocument doc(3072);
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.printf("[WEATHER] JSON parse error: %s\n", err.c_str());
    return;
  }

  xSemaphoreTake(weatherMutex, portMAX_DELAY);
  weatherCache.currentTemp = doc["current"]["temperature_2m"] | 0.0f;
  weatherCache.currentCode = doc["current"]["weathercode"]    | 0;
  for (int i = 0; i < 5; i++) {
    weatherCache.rainProb[i] = doc["daily"]["precipitation_probability_max"][i] | 0;
    weatherCache.loTemps[i]  = doc["daily"]["temperature_2m_min"][i]            | 0.0f;
    weatherCache.hiTemps[i]  = doc["daily"]["temperature_2m_max"][i]            | 0.0f;
    weatherCache.codes[i]    = doc["daily"]["weathercode"][i]                   | 0;
  }
  weatherCache.valid     = true;
  weatherCache.fetchedAt = millis();
  xSemaphoreGive(weatherMutex);

  Serial.printf("[WEATHER] Temp: %.1f°C  Rain today: %d%%  Rain tomorrow: %d%%  Lo: %.1f°C  Hi: %.1f°C\n",
    weatherCache.currentTemp, weatherCache.rainProb[0],
    weatherCache.rainProb[1], weatherCache.loTemps[0], weatherCache.hiTemps[0]);
}
```

- [ ] **Step 3: Update `shouldSkipForWeather()` to use new array fields**

Replace the entire `shouldSkipForWeather()` function:

```cpp
const char* shouldSkipForWeather() {
  if (!boardConfig.weatherEnabled) return nullptr;

  xSemaphoreTake(weatherMutex, portMAX_DELAY);
  bool  valid      = weatherCache.valid;
  int   rainToday  = weatherCache.rainProb[0];
  int   rainTomrw  = weatherCache.rainProb[1];
  float minTemp    = weatherCache.loTemps[0];
  xSemaphoreGive(weatherMutex);

  if (!valid) return nullptr;
  if (rainToday >= boardConfig.rainThreshold || rainTomrw >= boardConfig.rainThreshold)
    return "rain forecast";
  if (minTemp <= boardConfig.freezeThreshold)
    return "freeze risk";
  return nullptr;
}
```

- [ ] **Step 4: Fix `scheduleCheckerTask` skip-log to use new array fields**

Find (line ~700):
```cpp
        xSemaphoreTake(weatherMutex, portMAX_DELAY);
        int   rainToday    = weatherCache.rainProbToday;
        int   rainTomorrow = weatherCache.rainProbTomorrow;
        float minTemp      = weatherCache.minTempToday;
        xSemaphoreGive(weatherMutex);
        Serial.printf("[SCHED] Run SKIPPED — %s (rain today: %d%%, rain tomorrow: %d%%, min: %.1f°C)\n",
          skipReason, rainToday, rainTomorrow, minTemp);
```

Replace with:
```cpp
        xSemaphoreTake(weatherMutex, portMAX_DELAY);
        int   rainToday  = weatherCache.rainProb[0];
        int   rainTomrw  = weatherCache.rainProb[1];
        float minTemp    = weatherCache.loTemps[0];
        xSemaphoreGive(weatherMutex);
        Serial.printf("[SCHED] Run SKIPPED — %s (rain today: %d%%, rain tomorrow: %d%%, min: %.1f°C)\n",
          skipReason, rainToday, rainTomrw, minTemp);
```

- [ ] **Step 5: Update `weatherTask()` to use task notification instead of vTaskDelay**

Find:
```cpp
void weatherTask(void* param) {
  vTaskDelay(pdMS_TO_TICKS(5000)); // wait 5s for WiFi/NTP to settle
  fetchWeather();
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(3600000)); // fetch every hour
    fetchWeather();
  }
}
```

Replace with:
```cpp
void weatherTask(void* param) {
  vTaskDelay(pdMS_TO_TICKS(5000)); // wait 5s for WiFi/NTP to settle
  fetchWeather();
  while (true) {
    // Block up to 1 hour; POST /api/config wakes us early via xTaskNotify
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(3600000));
    fetchWeather();
  }
}
```

- [ ] **Step 6: Update `GET /api/weather` to return 5-day forecast array and temp_unit**

Find the entire `GET /api/weather` handler:
```cpp
  server.on("/api/weather", HTTP_GET, []() {
    DynamicJsonDocument doc(512);
    xSemaphoreTake(weatherMutex, portMAX_DELAY);
    doc["valid"]              = weatherCache.valid;
    doc["current_temp"]       = weatherCache.currentTemp;
    doc["min_temp_today"]     = weatherCache.minTempToday;
    doc["rain_prob_today"]    = weatherCache.rainProbToday;
    doc["rain_prob_tomorrow"] = weatherCache.rainProbTomorrow;
    doc["weather_code"]       = weatherCache.weatherCode;
    doc["fetched_ago_sec"]    = weatherCache.valid ? (long)((millis() - weatherCache.fetchedAt) / 1000) : -1;
    xSemaphoreGive(weatherMutex);
    doc["weather_enabled"]    = boardConfig.weatherEnabled;
    doc["rain_threshold"]     = boardConfig.rainThreshold;
    doc["freeze_threshold"]   = boardConfig.freezeThreshold;
    String out; serializeJson(doc, out);
    server.send(200, "application/json", out);
  });
```

Replace with:
```cpp
  server.on("/api/weather", HTTP_GET, []() {
    DynamicJsonDocument doc(1024);
    xSemaphoreTake(weatherMutex, portMAX_DELAY);
    doc["valid"]              = weatherCache.valid;
    doc["current_temp"]       = weatherCache.currentTemp;
    doc["weather_code"]       = weatherCache.currentCode;
    doc["min_temp_today"]     = weatherCache.loTemps[0];
    doc["rain_prob_today"]    = weatherCache.rainProb[0];
    doc["rain_prob_tomorrow"] = weatherCache.rainProb[1];
    doc["fetched_ago_sec"]    = weatherCache.valid ? (long)((millis() - weatherCache.fetchedAt) / 1000) : -1;
    JsonArray forecast = doc.createNestedArray("forecast");
    for (int i = 0; i < 5; i++) {
      JsonObject day = forecast.createNestedObject();
      day["hi"]   = weatherCache.hiTemps[i];
      day["lo"]   = weatherCache.loTemps[i];
      day["rain"] = weatherCache.rainProb[i];
      day["code"] = weatherCache.codes[i];
    }
    xSemaphoreGive(weatherMutex);
    doc["weather_enabled"]  = boardConfig.weatherEnabled;
    doc["rain_threshold"]   = boardConfig.rainThreshold;
    doc["freeze_threshold"] = boardConfig.freezeThreshold;
    doc["temp_unit"]        = boardConfig.tempUnit;
    String out; serializeJson(doc, out);
    server.send(200, "application/json", out);
  });
```

- [ ] **Step 7: Compile check in Arduino IDE — verify no errors**

- [ ] **Step 8: Commit**
```
git add esp32_firmware/sprinkler_controller/sprinkler_controller.ino
git commit -m "5-day WeatherCache, fetchWeather expansion, weatherTask notify-on-demand"
```

---

## Task 3: UI — °F/°C toggle + fmtTemp + update existing temperature displays

**Files:**
- Modify: `esp32_firmware/sprinkler_controller/data/index.html`

- [ ] **Step 1: Add CSS for the unit toggle and forecast strip to the `<style>` block**

Find the end of the existing weather CSS (after `.wx-no-data p { ... }`):
```css
.wx-no-data p { font-size: 1rem; line-height: 1.6; }
```

Add immediately after:
```css
/* ── Unit toggle ──────────────────────────────────────── */
.wx-unit-row { display:flex; justify-content:center; margin-bottom:14px; }
.unit-toggle { display:flex; align-items:center; background:#131f2e; border:1.5px solid #1a3a5c; border-radius:24px; padding:3px; position:relative; }
.unit-btn { background:none; border:none; color:#607080; font-family:inherit; font-size:0.85rem; font-weight:700; letter-spacing:1px; padding:5px 18px; cursor:pointer; border-radius:20px; transition:color 0.2s; position:relative; z-index:1; }
.unit-btn.active { color:#0d1b2a; }
.unit-slider { position:absolute; top:3px; left:3px; height:calc(100% - 6px); background:#4fc3f7; border-radius:20px; transition:transform 0.25s cubic-bezier(.4,0,.2,1), width 0.25s; pointer-events:none; }

/* ── 5-day forecast strip ────────────────────────────── */
.wx-forecast-hdr { font-size:0.75rem; color:#607080; text-transform:uppercase; letter-spacing:2px; font-weight:600; margin:14px 0 8px; display:flex; align-items:center; gap:8px; }
.wx-forecast-hdr::after { content:''; flex:1; height:1px; background:linear-gradient(90deg,#1a3a5c,transparent); }
.forecast-strip { display:flex; gap:8px; margin-bottom:14px; }
.fc-card { flex:1; background:#131f2e; border:1.5px solid #1a3a5c; border-radius:14px; padding:10px 6px 8px; display:flex; flex-direction:column; align-items:center; gap:4px; transition:border-color 0.2s, transform 0.2s; cursor:default; }
.fc-card:hover { border-color:#4fc3f7; transform:translateY(-2px); }
.fc-card.fc-today { border-color:#4fc3f7; background:#0e2030; }
.fc-day { font-size:0.62rem; font-weight:700; text-transform:uppercase; letter-spacing:1.5px; color:#607080; }
.fc-card.fc-today .fc-day { color:#4fc3f7; }
.fc-icon { font-size:1.5rem; line-height:1; }
.fc-hi { font-size:1rem; font-weight:700; line-height:1; }
.fc-lo { font-size:0.78rem; color:#4fc3f7; line-height:1; }
.fc-bar-track { width:calc(100% - 8px); height:3px; background:rgba(79,195,247,0.12); border-radius:2px; overflow:hidden; }
.fc-bar-fill { height:100%; border-radius:2px; }
.fc-rain { display:flex; align-items:center; gap:3px; width:calc(100% - 8px); }
.fc-rain-pct { font-size:0.65rem; font-weight:700; min-width:22px; }
.fc-rain-track { flex:1; height:2px; background:rgba(79,195,247,0.12); border-radius:1px; overflow:hidden; }
.fc-rain-fill { height:100%; border-radius:1px; }
```

- [ ] **Step 2: Add the `wxTempUnit` global and helper functions to the `<script>` block**

Find near the top of the `<script>` block (after `const GET = ...` and `const POST = ...` helpers):

```javascript
// ── Temperature unit preference ────────────────────────
let wxTempUnit = 'C';

function fmtTemp(c) {
  if (wxTempUnit === 'F') return (c * 9/5 + 32).toFixed(1) + '°F';
  return c.toFixed(1) + '°C';
}

function fmtTempRound(c) {
  if (wxTempUnit === 'F') return Math.round(c * 9/5 + 32) + '°';
  return Math.round(c) + '°';
}
```

Add this block immediately after the `GET`/`POST` helper definitions.

- [ ] **Step 3: Update `updateWeatherWidget()` to use `fmtTemp` and read `temp_unit` from response**

Find:
```javascript
async function updateWeatherWidget() {
  try {
    const w = await GET('/api/weather');
    const card = document.getElementById('weather-card');
    if (!w.valid) { card.style.display = 'none'; return; }
    card.style.display = 'block';
    document.getElementById('wx-icon').textContent          = WX_ICONS[w.weather_code] || '🌡';
    document.getElementById('wx-temp').textContent          = w.current_temp.toFixed(1) + '°C';
    document.getElementById('wx-rain-today').textContent    = w.rain_prob_today + '%';
    document.getElementById('wx-rain-tomorrow').textContent = w.rain_prob_tomorrow + '%';
    document.getElementById('wx-min-temp').textContent      = w.min_temp_today.toFixed(1) + '°C';
    const ageMin = Math.round(w.fetched_ago_sec / 60);
    document.getElementById('wx-age').textContent           = ageMin < 2 ? 'just updated' : `${ageMin}m ago`;
    const warn = document.getElementById('wx-skip-warn');
    if (w.weather_enabled) {
      if (w.rain_prob_today >= w.rain_threshold || w.rain_prob_tomorrow >= w.rain_threshold) {
        warn.style.display = 'block';
        warn.textContent = `⚠ Watering will be skipped — rain forecast (${Math.max(w.rain_prob_today, w.rain_prob_tomorrow)}% ≥ ${w.rain_threshold}% threshold)`;
      } else if (w.min_temp_today <= w.freeze_threshold) {
        warn.style.display = 'block';
        warn.textContent = `⚠ Watering will be skipped — freeze risk (min ${w.min_temp_today.toFixed(1)}°C ≤ ${w.freeze_threshold}°C threshold)`;
      } else {
        warn.style.display = 'none';
      }
    } else {
      warn.style.display = 'none';
    }
  } catch(e) {}
}
```

Replace with:
```javascript
async function updateWeatherWidget() {
  try {
    const w = await GET('/api/weather');
    const card = document.getElementById('weather-card');
    if (!w.valid) { card.style.display = 'none'; return; }
    if (w.temp_unit) wxTempUnit = w.temp_unit;
    card.style.display = 'block';
    document.getElementById('wx-icon').textContent          = WX_ICONS[w.weather_code] || '🌡';
    document.getElementById('wx-temp').textContent          = fmtTemp(w.current_temp);
    document.getElementById('wx-rain-today').textContent    = w.rain_prob_today + '%';
    document.getElementById('wx-rain-tomorrow').textContent = w.rain_prob_tomorrow + '%';
    document.getElementById('wx-min-temp').textContent      = fmtTemp(w.min_temp_today);
    const ageMin = Math.round(w.fetched_ago_sec / 60);
    document.getElementById('wx-age').textContent           = ageMin < 2 ? 'just updated' : `${ageMin}m ago`;
    const warn = document.getElementById('wx-skip-warn');
    if (w.weather_enabled) {
      if (w.rain_prob_today >= w.rain_threshold || w.rain_prob_tomorrow >= w.rain_threshold) {
        warn.style.display = 'block';
        warn.textContent = `⚠ Watering will be skipped — rain forecast (${Math.max(w.rain_prob_today, w.rain_prob_tomorrow)}% ≥ ${w.rain_threshold}% threshold)`;
      } else if (w.min_temp_today <= w.freeze_threshold) {
        warn.style.display = 'block';
        warn.textContent = `⚠ Watering will be skipped — freeze risk (min ${fmtTemp(w.min_temp_today)} ≤ ${fmtTemp(w.freeze_threshold)} threshold)`;
      } else {
        warn.style.display = 'none';
      }
    } else {
      warn.style.display = 'none';
    }
  } catch(e) {}
}
```

- [ ] **Step 4: Update `updateWeatherPage()` to use `fmtTemp` and read `temp_unit`**

Find these lines inside `updateWeatherPage()`:
```javascript
    document.getElementById('wxp-icon').textContent = WX_ICONS[w.weather_code] || '🌡';
    document.getElementById('wxp-temp').textContent = w.current_temp.toFixed(1) + '°C';
    document.getElementById('wxp-desc').textContent = WX_DESCS[w.weather_code] || 'Unknown';
```

Replace with:
```javascript
    if (w.temp_unit) { wxTempUnit = w.temp_unit; syncUnitToggle(); }
    document.getElementById('wxp-icon').textContent = WX_ICONS[w.weather_code] || '🌡';
    document.getElementById('wxp-temp').textContent = fmtTemp(w.current_temp);
    document.getElementById('wxp-desc').textContent = WX_DESCS[w.weather_code] || 'Unknown';
```

Find inside `updateWeatherPage()`:
```javascript
    document.getElementById('wxp-min-temp').textContent = w.min_temp_today.toFixed(1) + '°C';
```
Replace with:
```javascript
    document.getElementById('wxp-min-temp').textContent = fmtTemp(w.min_temp_today);
```

Find inside `updateWeatherPage()` (skip status reason strings that show °C):
```javascript
      reason.textContent = `Freeze risk: min ${w.min_temp_today.toFixed(1)}°C ≤ ${w.freeze_threshold}°C threshold`;
```
Replace with:
```javascript
      reason.textContent = `Freeze risk: min ${fmtTemp(w.min_temp_today)} ≤ ${fmtTemp(w.freeze_threshold)} threshold`;
```

- [ ] **Step 5: Update `loadWeatherSettings()` to convert freeze threshold display + read temp_unit**

Find:
```javascript
async function loadWeatherSettings() {
  try {
    const r = await GET('/api/config');
    if (r.latitude)  document.getElementById('lat-inp').value = r.latitude;
    if (r.longitude) document.getElementById('lon-inp').value = r.longitude;
    document.getElementById('weather-skip-chk').checked    = r.weather_enabled  || false;
    document.getElementById('rain-thresh-inp').value        = r.rain_threshold   ?? 50;
    document.getElementById('freeze-thresh-inp').value      = r.freeze_threshold ?? 2;
  } catch(e) {}
}
```

Replace with:
```javascript
async function loadWeatherSettings() {
  try {
    const r = await GET('/api/config');
    if (r.temp_unit) wxTempUnit = r.temp_unit;
    if (r.latitude)  document.getElementById('lat-inp').value = r.latitude;
    if (r.longitude) document.getElementById('lon-inp').value = r.longitude;
    document.getElementById('weather-skip-chk').checked = r.weather_enabled  || false;
    document.getElementById('rain-thresh-inp').value     = r.rain_threshold   ?? 50;
    const ft = r.freeze_threshold ?? 2;
    document.getElementById('freeze-thresh-inp').value   = wxTempUnit === 'F' ? (ft * 9/5 + 32).toFixed(1) : ft;
    document.getElementById('freeze-unit-lbl').textContent = '°' + wxTempUnit;
  } catch(e) {}
}
```

- [ ] **Step 6: Update `saveWeatherSettings()` to convert freeze threshold back to °C**

Find:
```javascript
async function saveWeatherSettings() {
  const r = await POST('/api/config', {
    weather_enabled:  document.getElementById('weather-skip-chk').checked,
    rain_threshold:   parseInt(document.getElementById('rain-thresh-inp').value),
    freeze_threshold: parseFloat(document.getElementById('freeze-thresh-inp').value)
  });
  r.message ? toast('Weather settings saved ✓') : toast(r.error || 'Save failed','err');
}
```

Replace with:
```javascript
async function saveWeatherSettings() {
  let ft = parseFloat(document.getElementById('freeze-thresh-inp').value);
  if (wxTempUnit === 'F') ft = (ft - 32) * 5/9;  // always store in °C
  const r = await POST('/api/config', {
    weather_enabled:  document.getElementById('weather-skip-chk').checked,
    rain_threshold:   parseInt(document.getElementById('rain-thresh-inp').value),
    freeze_threshold: parseFloat(ft.toFixed(2))
  });
  r.message ? toast('Weather settings saved ✓') : toast(r.error || 'Save failed','err');
}
```

- [ ] **Step 7: Add `id="freeze-unit-lbl"` to the freeze threshold unit span in Settings HTML**

Find:
```html
        <span class="settings-val">°C</span>
      </div>
    </div>
    <div class="settings-row">
      <button class="save-btn" onclick="saveWeatherSettings()">Save Weather Settings</button>
```

Replace:
```html
        <span class="settings-val" id="freeze-unit-lbl">°C</span>
      </div>
    </div>
    <div class="settings-row">
      <button class="save-btn" onclick="saveWeatherSettings()">Save Weather Settings</button>
```

- [ ] **Step 8: Commit**
```
git add esp32_firmware/sprinkler_controller/data/index.html
git commit -m "UI: fmtTemp helper, °F/°C-aware temperature displays and freeze threshold"
```

---

## Task 4: UI — °F/°C toggle control + 5-day forecast strip

**Files:**
- Modify: `esp32_firmware/sprinkler_controller/data/index.html`

- [ ] **Step 1: Add unit toggle HTML and 5-day strip HTML to the Weather page**

Find in the Weather page section:
```html
  <div id="wx-page-content" style="display:none">
    <div class="wx-hero">
      <div class="wx-hero-icon" id="wxp-icon">🌤</div>
      <div class="wx-hero-temp" id="wxp-temp">--°C</div>
      <div class="wx-hero-desc" id="wxp-desc">--</div>
    </div>
    <div class="wx-grid">
```

Replace with:
```html
  <div id="wx-page-content" style="display:none">
    <div class="wx-hero">
      <div class="wx-hero-icon" id="wxp-icon">🌤</div>
      <div class="wx-hero-temp" id="wxp-temp">--°</div>
      <div class="wx-hero-desc" id="wxp-desc">--</div>
    </div>
    <div class="wx-unit-row">
      <div class="unit-toggle">
        <div class="unit-slider" id="wx-unit-slider"></div>
        <button class="unit-btn active" id="wx-btn-c" onclick="setTempUnit('C')">°C</button>
        <button class="unit-btn"        id="wx-btn-f" onclick="setTempUnit('F')">°F</button>
      </div>
    </div>
    <div class="wx-grid">
```

Find inside `wx-page-content`, after the `wx-skip-box` closing tag:
```html
    <div class="wx-skip-box" id="wxp-skip-box">
      <div class="wx-skip-label" id="wxp-skip-label">Weather Skip</div>
      <div class="wx-skip-reason" id="wxp-skip-reason">--</div>
    </div>
  </div>
```

Replace with:
```html
    <div class="wx-skip-box" id="wxp-skip-box">
      <div class="wx-skip-label" id="wxp-skip-label">Weather Skip</div>
      <div class="wx-skip-reason" id="wxp-skip-reason">--</div>
    </div>
    <div class="wx-forecast-hdr">5-Day Forecast</div>
    <div class="forecast-strip" id="wxp-forecast-strip"></div>
  </div>
```

- [ ] **Step 2: Add `setTempUnit()`, `syncUnitToggle()`, and `renderForecastStrip()` JS functions**

Find `function refreshWeatherPage() { ... }` and add the following block immediately before it:

```javascript
// ── Unit toggle ────────────────────────────────────────
function syncUnitToggle() {
  const btnC   = document.getElementById('wx-btn-c');
  const btnF   = document.getElementById('wx-btn-f');
  const slider = document.getElementById('wx-unit-slider');
  if (!btnC || !btnF || !slider) return;
  btnC.classList.toggle('active', wxTempUnit === 'C');
  btnF.classList.toggle('active', wxTempUnit === 'F');
  slider.style.width     = (wxTempUnit === 'C' ? btnC : btnF).offsetWidth + 'px';
  slider.style.transform = wxTempUnit === 'C' ? 'translateX(0)' : `translateX(${btnC.offsetWidth}px)`;
}

async function setTempUnit(u) {
  wxTempUnit = u;
  syncUnitToggle();
  // update freeze threshold label in settings if visible
  const lbl = document.getElementById('freeze-unit-lbl');
  if (lbl) lbl.textContent = '°' + u;
  await POST('/api/config', { temp_unit: u });
  updateWeatherPage();
  updateWeatherWidget();
}

// ── 5-Day forecast strip ───────────────────────────────
function tempColor(c) {
  // 5°C (ice blue) → 35°C (amber)
  const t = Math.max(0, Math.min(1, (c - 5) / 30));
  const lerpHex = (a, b, t) => {
    const ah = parseInt(a.slice(1),16), bh = parseInt(b.slice(1),16);
    const r = Math.round(((ah>>16)&0xff) + (((bh>>16)&0xff)-((ah>>16)&0xff))*t);
    const g = Math.round(((ah>>8)&0xff)  + (((bh>>8)&0xff) -((ah>>8)&0xff))*t);
    const bl= Math.round((ah&0xff)       + ((bh&0xff)       -(ah&0xff))*t);
    return `#${((r<<16)|(g<<8)|bl).toString(16).padStart(6,'0')}`;
  };
  return t < 0.5 ? lerpHex('#4fc3f7','#ffca28',t*2) : lerpHex('#ffca28','#ef6c00',(t-0.5)*2);
}

function rainColor(pct) {
  if (pct < 20)  return '#4fc3f7';
  if (pct < 50)  return '#1e88e5';
  if (pct < 70)  return '#1565c0';
  return '#0d47a1';
}

const DAY_NAMES = ['Sun','Mon','Tue','Wed','Thu','Fri','Sat'];

function renderForecastStrip(forecast) {
  const strip = document.getElementById('wxp-forecast-strip');
  if (!strip || !forecast || !forecast.length) return;

  // Get absolute temp range across all days for bar sizing
  const allHi = forecast.map(d => d.hi);
  const allLo = forecast.map(d => d.lo);
  const absMin = Math.min(...allLo) - 2;
  const absMax = Math.max(...allHi) + 2;
  const span   = absMax - absMin || 1;

  const now    = new Date();
  strip.innerHTML = '';

  forecast.forEach((d, i) => {
    const dayDate  = new Date(now); dayDate.setDate(now.getDate() + i);
    const dayLabel = i === 0 ? 'Today' : DAY_NAMES[dayDate.getDay()];
    const hiC      = wxTempUnit === 'F' ? Math.round(d.hi * 9/5 + 32) : Math.round(d.hi);
    const loC      = wxTempUnit === 'F' ? Math.round(d.lo * 9/5 + 32) : Math.round(d.lo);
    const hiColor  = tempColor(d.hi);
    const loColor  = tempColor(d.lo);
    const rColor   = rainColor(d.rain);
    const barLeft  = ((d.lo - absMin) / span * 100).toFixed(1);
    const barWidth = ((d.hi - d.lo)   / span * 100).toFixed(1);
    const icon     = WX_ICONS[d.code] || '🌡';

    const card = document.createElement('div');
    card.className = 'fc-card' + (i === 0 ? ' fc-today' : '');
    card.innerHTML = `
      <div class="fc-day">${dayLabel}</div>
      <div class="fc-icon">${icon}</div>
      <div class="fc-hi" style="color:${hiColor}">${hiC}°</div>
      <div class="fc-lo" style="color:${loColor}">${loC}°</div>
      <div class="fc-bar-track">
        <div class="fc-bar-fill" style="margin-left:${barLeft}%;width:${barWidth}%;background:linear-gradient(90deg,${loColor},${hiColor})"></div>
      </div>
      <div class="fc-rain">
        <span class="fc-rain-pct" style="color:${rColor}">${d.rain}%</span>
        <div class="fc-rain-track"><div class="fc-rain-fill" style="width:${d.rain}%;background:${rColor}"></div></div>
      </div>`;
    strip.appendChild(card);
  });
}
```

- [ ] **Step 3: Call `renderForecastStrip` and `syncUnitToggle` inside `updateWeatherPage()`**

Find near the end of `updateWeatherPage()`, just before the skip-box logic:
```javascript
    // Age
    const ageMin = Math.round(w.fetched_ago_sec / 60);
    document.getElementById('wxp-age').textContent = ageMin < 2 ? 'Just now' : `${ageMin}m ago`;
```

Add `renderForecastStrip(w.forecast);` immediately after the age line:
```javascript
    // Age
    const ageMin = Math.round(w.fetched_ago_sec / 60);
    document.getElementById('wxp-age').textContent = ageMin < 2 ? 'Just now' : `${ageMin}m ago`;

    renderForecastStrip(w.forecast);
```

- [ ] **Step 4: Init unit toggle slider size after page load**

Find the boot sequence at the bottom of the script (after `setInterval(updateWeatherWidget, 300000);`):
```javascript
updateWeatherWidget();
setInterval(updateWeatherWidget, 300000); // refresh every 5 minutes
```

Add `syncUnitToggle()` on the next line:
```javascript
updateWeatherWidget();
setInterval(updateWeatherWidget, 300000); // refresh every 5 minutes
syncUnitToggle();
```

- [ ] **Step 5: Commit**
```
git add esp32_firmware/sprinkler_controller/data/index.html
git commit -m "UI: °F/°C toggle on Weather page and 5-day colorful forecast strip"
```

---

## Task 5: Regenerate html_content.h, bump version, compile, push

**Files:**
- Modify: `esp32_firmware/sprinkler_controller/sprinkler_controller.ino`
- Modify: `esp32_firmware/sprinkler_controller/html_content.h`

- [ ] **Step 1: Bump FW_VERSION to 1.6.0**

Find:
```cpp
#define FW_VERSION     "1.5.1"
```

Replace with:
```cpp
#define FW_VERSION     "1.6.0"
```

- [ ] **Step 2: Regenerate `html_content.h` from `data/index.html`**

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

- [ ] **Step 3: Final compile in Arduino IDE — verify no errors, check sketch size is under 1.9MB**

- [ ] **Step 4: Commit and push**
```
git add esp32_firmware/sprinkler_controller/sprinkler_controller.ino
git add esp32_firmware/sprinkler_controller/html_content.h
git commit -m "v1.6.0: °F/°C toggle, 5-day forecast strip, location refresh without reset"
git push origin custom-board
```

---

## Self-Review

**Spec coverage:**
- ✅ °F/°C stored in `config.json` — `boardConfig.tempUnit`, Task 1 Steps 1–5
- ✅ All displayed temperatures convert via `fmtTemp()` — Task 3 Steps 3–4
- ✅ Freeze threshold in settings shows/saves in active unit — Task 3 Steps 5–7
- ✅ 5-day forecast strip, colorful (temp gradient + rain bar) — Task 4 Steps 1–3
- ✅ Location update triggers immediate weather refetch without reset — Task 1 Step 5 (notify), Task 2 Step 5 (`ulTaskNotifyTake`)
- ✅ `weatherTaskHandle` captured at `xTaskCreate` — Task 1 Step 6
- ✅ Open-Meteo fetches 5 days with `temperature_2m_max` and daily `weathercode` — Task 2 Step 2
- ✅ `shouldSkipForWeather()` uses new array fields — Task 2 Step 3
- ✅ `scheduleCheckerTask` skip log uses new array fields — Task 2 Step 4
- ✅ `/api/weather` returns `forecast` array + `temp_unit` — Task 2 Step 6
- ✅ Unit toggle pill on Weather page, saves to board — Task 4 Steps 1–2
- ✅ Version bumped to 1.6.0 — Task 5 Step 1

**Type consistency check:**
- `weatherCache.rainProb[i]` — defined in Task 2 Step 1, used in Steps 2, 3, 4, 6 ✓
- `weatherCache.loTemps[i]` / `weatherCache.hiTemps[i]` — defined Task 2 Step 1, used Steps 2, 3, 4, 6 ✓
- `weatherCache.currentCode` — defined Task 2 Step 1, used Step 6 (as `weather_code` in JSON) ✓
- `weatherCache.codes[i]` — defined Task 2 Step 1, used Step 2 (parse) and Step 6 (output) ✓
- `weatherTaskHandle` — declared Task 1 Step 1, captured Task 1 Step 6, used Task 1 Step 5 ✓
- `wxTempUnit` — declared Task 3 Step 2, read by `fmtTemp()`, `syncUnitToggle()`, `renderForecastStrip()`, `loadWeatherSettings()`, `saveWeatherSettings()` ✓
- `fmtTemp(c)` — declared Task 3 Step 2, called in Steps 3, 4 and Task 4 Step 2 ✓
- `syncUnitToggle()` — declared Task 4 Step 2, called from `setTempUnit()` and Task 4 Step 4 ✓
- `renderForecastStrip(forecast)` — declared Task 4 Step 2, called Task 4 Step 3 ✓
- `WX_ICONS` — existing global, used in `renderForecastStrip()` in Task 4 Step 2 ✓
