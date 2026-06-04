# Weather Features Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add weather display and weather-based skip logic to the SprinKlr-8 sprinkler controller using the Open-Meteo free API (no key required).

**Architecture:** The ESP32 fetches weather from Open-Meteo hourly, caches it in RAM, and exposes it via `/api/weather`. Before each scheduled run, the firmware checks the cache and skips if rain or freeze thresholds are met. Location (lat/lon) and skip thresholds are stored in `config.json`. The browser UI handles zip-code and auto-location geocoding client-side — the ESP32 only ever stores lat/lon.

**Tech Stack:** Open-Meteo REST API (HTTPS, no key), Nominatim geocoding (browser-side only), ArduinoJson (already in project), WiFiClientSecure + HTTPClient (already in project), vanilla JS in `data/index.html`.

---

## File Map

| File | Change |
|------|--------|
| `esp32_firmware/sprinkler_controller/sprinkler_controller.ino` | Expand `BoardConfig`, add `WeatherCache`, weather fetch task, `/api/weather` route, skip logic |
| `esp32_firmware/sprinkler_controller/data/index.html` | Add location settings, weather widget, skip settings |
| `esp32_firmware/sprinkler_controller/html_content.h` | Regenerate from updated `data/index.html` |

---

## Task 1: Expand BoardConfig in firmware

**Files:**
- Modify: `esp32_firmware/sprinkler_controller/sprinkler_controller.ino`

- [ ] **Step 1: Replace the BoardConfig struct**

Find (around line 141):
```cpp
struct BoardConfig {
  char timezone[64];
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
};
BoardConfig boardConfig;
```

- [ ] **Step 2: Update loadConfig() to read new fields**

Find the existing `loadConfig()` function and replace its body with:
```cpp
void loadConfig() {
  strlcpy(boardConfig.timezone, DEFAULT_TZ, sizeof(boardConfig.timezone));
  boardConfig.latitude       = 0.0f;
  boardConfig.longitude      = 0.0f;
  boardConfig.weatherEnabled = false;
  boardConfig.rainThreshold  = 50;
  boardConfig.freezeThreshold = 2.0f;

  if (!LittleFS.exists(CONFIG_FILE)) return;
  File f = LittleFS.open(CONFIG_FILE, "r");
  if (!f) return;
  DynamicJsonDocument doc(512);
  if (deserializeJson(doc, f) == DeserializationError::Ok) {
    const char* tz = doc["timezone"];
    if (tz) strlcpy(boardConfig.timezone, tz, sizeof(boardConfig.timezone));
    boardConfig.latitude        = doc["latitude"]        | 0.0f;
    boardConfig.longitude       = doc["longitude"]       | 0.0f;
    boardConfig.weatherEnabled  = doc["weather_enabled"] | false;
    boardConfig.rainThreshold   = doc["rain_threshold"]  | 50;
    boardConfig.freezeThreshold = doc["freeze_threshold"]| 2.0f;
  }
  f.close();
  Serial.printf("[CFG] Timezone: %s  Lat: %.4f  Lon: %.4f  WeatherSkip: %s\n",
    boardConfig.timezone, boardConfig.latitude, boardConfig.longitude,
    boardConfig.weatherEnabled ? "ON" : "OFF");
}
```

- [ ] **Step 3: Update saveConfig() to write new fields**

Replace the existing `saveConfig()` body with:
```cpp
void saveConfig() {
  DynamicJsonDocument doc(512);
  doc["timezone"]         = boardConfig.timezone;
  doc["latitude"]         = boardConfig.latitude;
  doc["longitude"]        = boardConfig.longitude;
  doc["weather_enabled"]  = boardConfig.weatherEnabled;
  doc["rain_threshold"]   = boardConfig.rainThreshold;
  doc["freeze_threshold"] = boardConfig.freezeThreshold;
  File f = LittleFS.open(CONFIG_FILE, "w");
  if (!f) return;
  serializeJson(doc, f);
  f.close();
  Serial.printf("[CFG] Config saved\n");
}
```

- [ ] **Step 4: Update /api/config GET to return new fields**

Find the `GET /api/config` handler and replace its lambda:
```cpp
server.on("/api/config", HTTP_GET, []() {
  DynamicJsonDocument doc(512);
  doc["timezone"]         = boardConfig.timezone;
  doc["latitude"]         = boardConfig.latitude;
  doc["longitude"]        = boardConfig.longitude;
  doc["weather_enabled"]  = boardConfig.weatherEnabled;
  doc["rain_threshold"]   = boardConfig.rainThreshold;
  doc["freeze_threshold"] = boardConfig.freezeThreshold;
  String out; serializeJson(doc, out);
  server.send(200, "application/json", out);
});
```

- [ ] **Step 5: Update /api/config POST to accept new fields**

Find the `POST /api/config` handler. Replace the validation/save block with:
```cpp
server.on("/api/config", HTTP_POST, []() {
  if (!server.hasArg("plain")) { server.send(400, "application/json", "{\"error\":\"No body\"}"); return; }
  DynamicJsonDocument doc(512);
  if (deserializeJson(doc, server.arg("plain")) != DeserializationError::Ok) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return;
  }
  if (doc.containsKey("timezone")) {
    const char* tz = doc["timezone"];
    if (!tz || strlen(tz) == 0 || strlen(tz) >= sizeof(boardConfig.timezone)) {
      server.send(400, "application/json", "{\"error\":\"Invalid timezone\"}"); return;
    }
    strlcpy(boardConfig.timezone, tz, sizeof(boardConfig.timezone));
    applyTimezone();
  }
  if (doc.containsKey("latitude"))         boardConfig.latitude        = doc["latitude"].as<float>();
  if (doc.containsKey("longitude"))        boardConfig.longitude       = doc["longitude"].as<float>();
  if (doc.containsKey("weather_enabled"))  boardConfig.weatherEnabled  = doc["weather_enabled"].as<bool>();
  if (doc.containsKey("rain_threshold"))   boardConfig.rainThreshold   = doc["rain_threshold"].as<int>();
  if (doc.containsKey("freeze_threshold")) boardConfig.freezeThreshold = doc["freeze_threshold"].as<float>();
  saveConfig();
  server.send(200, "application/json", "{\"message\":\"Config saved\"}");
});
```

- [ ] **Step 6: Verify — compile in Arduino IDE, no errors**

- [ ] **Step 7: Commit**
```
git add esp32_firmware/sprinkler_controller/sprinkler_controller.ino
git commit -m "Add lat/lon and weather skip settings to BoardConfig and config API"
```

---

## Task 2: Add WeatherCache struct and fetch function

**Files:**
- Modify: `esp32_firmware/sprinkler_controller/sprinkler_controller.ino`

- [ ] **Step 1: Add WeatherCache struct and global instance after the BoardConfig globals**

Insert after `BoardConfig boardConfig;`:
```cpp
struct WeatherCache {
  float currentTemp;     // degrees C
  float minTempToday;    // degrees C
  int   rainProbToday;   // percent
  int   rainProbTomorrow;// percent
  int   weatherCode;     // WMO code
  bool  valid;
  unsigned long fetchedAt; // millis() when last fetched
};
WeatherCache weatherCache = {0, 0, 0, 0, 0, false, 0};
SemaphoreHandle_t weatherMutex;
```

- [ ] **Step 2: Create weatherMutex in setup()**

In `setup()`, alongside the other mutex creations add:
```cpp
weatherMutex = xSemaphoreCreateMutex();
```

- [ ] **Step 3: Add fetchWeather() function — place after saveConfig()**

```cpp
// ═══════════════════════════════════════════════════════════════
//  WEATHER FETCH (Open-Meteo, no API key required)
// ═══════════════════════════════════════════════════════════════
void fetchWeather() {
  if (boardConfig.latitude == 0.0f && boardConfig.longitude == 0.0f) return;

  char url[256];
  snprintf(url, sizeof(url),
    "https://api.open-meteo.com/v1/forecast"
    "?latitude=%.4f&longitude=%.4f"
    "&current=temperature_2m,weathercode,precipitation"
    "&daily=precipitation_probability_max,temperature_2m_min"
    "&timezone=auto&forecast_days=2",
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

  DynamicJsonDocument doc(1024);
  DeserializationError err = deserializeJson(doc, https.getStream());
  https.end();

  if (err) {
    Serial.printf("[WEATHER] JSON parse error: %s\n", err.c_str());
    return;
  }

  xSemaphoreTake(weatherMutex, portMAX_DELAY);
  weatherCache.currentTemp      = doc["current"]["temperature_2m"]              | 0.0f;
  weatherCache.weatherCode      = doc["current"]["weathercode"]                 | 0;
  weatherCache.rainProbToday    = doc["daily"]["precipitation_probability_max"][0] | 0;
  weatherCache.rainProbTomorrow = doc["daily"]["precipitation_probability_max"][1] | 0;
  weatherCache.minTempToday     = doc["daily"]["temperature_2m_min"][0]         | 0.0f;
  weatherCache.valid            = true;
  weatherCache.fetchedAt        = millis();
  xSemaphoreGive(weatherMutex);

  Serial.printf("[WEATHER] Temp: %.1f°C  Rain today: %d%%  Rain tomorrow: %d%%  Min: %.1f°C\n",
    weatherCache.currentTemp, weatherCache.rainProbToday,
    weatherCache.rainProbTomorrow, weatherCache.minTempToday);
}
```

- [ ] **Step 4: Compile — verify no errors**

- [ ] **Step 5: Commit**
```
git add esp32_firmware/sprinkler_controller/sprinkler_controller.ino
git commit -m "Add WeatherCache struct and fetchWeather() using Open-Meteo"
```

---

## Task 3: Hourly weather task and /api/weather route

**Files:**
- Modify: `esp32_firmware/sprinkler_controller/sprinkler_controller.ino`

- [ ] **Step 1: Add weather background task — place after fetchWeather()**

```cpp
void weatherTask(void* param) {
  // Initial fetch after 5s (let WiFi/NTP settle)
  vTaskDelay(pdMS_TO_TICKS(5000));
  fetchWeather();

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(3600000)); // every hour
    fetchWeather();
  }
}
```

- [ ] **Step 2: Start the task in setup() alongside other background tasks**

In `setup()`, after `xTaskCreate(ledTask, ...)`:
```cpp
xTaskCreate(weatherTask, "weather", 8192, NULL, 1, NULL);
```

Note: weather task uses 8192 stack bytes — HTTPS + JSON parsing needs more stack than simpler tasks.

- [ ] **Step 3: Add /api/weather GET route inside setupRoutes()**

Add after the `/api/system_info` handler:
```cpp
// ── GET /api/weather ──────────────────────────────────────────
server.on("/api/weather", HTTP_GET, []() {
  DynamicJsonDocument doc(512);
  xSemaphoreTake(weatherMutex, portMAX_DELAY);
  doc["valid"]              = weatherCache.valid;
  doc["current_temp"]       = weatherCache.currentTemp;
  doc["min_temp_today"]     = weatherCache.minTempToday;
  doc["rain_prob_today"]    = weatherCache.rainProbToday;
  doc["rain_prob_tomorrow"] = weatherCache.rainProbTomorrow;
  doc["weather_code"]       = weatherCache.weatherCode;
  doc["fetched_ago_sec"]    = weatherCache.valid ? (millis() - weatherCache.fetchedAt) / 1000 : -1;
  xSemaphoreGive(weatherMutex);
  doc["weather_enabled"]    = boardConfig.weatherEnabled;
  doc["rain_threshold"]     = boardConfig.rainThreshold;
  doc["freeze_threshold"]   = boardConfig.freezeThreshold;
  String out; serializeJson(doc, out);
  server.send(200, "application/json", out);
});
```

- [ ] **Step 4: Compile and flash to board**

- [ ] **Step 5: Verify via Serial Monitor — confirm [WEATHER] log line appears ~5s after boot (only if lat/lon configured)**

- [ ] **Step 6: Verify via browser**

Open `http://sprinkler.local/api/weather` — should return JSON. With no lat/lon set yet, `valid` will be `false`. After setting lat/lon in config, it should fetch and return real data.

- [ ] **Step 7: Commit**
```
git add esp32_firmware/sprinkler_controller/sprinkler_controller.ino
git commit -m "Add hourly weather fetch task and /api/weather endpoint"
```

---

## Task 4: Weather skip logic in scheduleCheckerTask

**Files:**
- Modify: `esp32_firmware/sprinkler_controller/sprinkler_controller.ino`

- [ ] **Step 1: Add shouldSkipForWeather() helper — place after fetchWeather()**

```cpp
// Returns a skip reason string, or nullptr if no skip needed
const char* shouldSkipForWeather() {
  if (!boardConfig.weatherEnabled) return nullptr;

  xSemaphoreTake(weatherMutex, portMAX_DELAY);
  bool valid          = weatherCache.valid;
  int  rainToday      = weatherCache.rainProbToday;
  int  rainTomorrow   = weatherCache.rainProbTomorrow;
  float minTemp       = weatherCache.minTempToday;
  xSemaphoreGive(weatherMutex);

  if (!valid) return nullptr; // no data — don't skip

  if (rainToday >= boardConfig.rainThreshold || rainTomorrow >= boardConfig.rainThreshold)
    return "rain forecast";
  if (minTemp <= boardConfig.freezeThreshold)
    return "freeze risk";
  return nullptr;
}
```

- [ ] **Step 2: Apply skip check inside scheduleCheckerTask**

Find the block in `scheduleCheckerTask` that calls `startSequence()`:
```cpp
if (day.isActive && day.hour == hour && day.minute == minute) {
  Serial.printf("[SCHED] Scheduled run: ...");
  lastScheduledDay    = dayIndex;
  lastScheduledMinute = minute;
  startSequence(dayIndex, false);
}
```

Replace with:
```cpp
if (day.isActive && day.hour == hour && day.minute == minute) {
  lastScheduledDay    = dayIndex;
  lastScheduledMinute = minute;

  const char* skipReason = shouldSkipForWeather();
  if (skipReason) {
    Serial.printf("[SCHED] Run SKIPPED — %s (rain today: %d%%, rain tomorrow: %d%%, min: %.1f°C)\n",
      skipReason, weatherCache.rainProbToday, weatherCache.rainProbTomorrow, weatherCache.minTempToday);
  } else {
    Serial.printf("[SCHED] Scheduled run: %s %02d:%02d\n",
      (const char*[]){"Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Sunday"}[dayIndex],
      hour, minute);
    startSequence(dayIndex, false);
  }
}
```

- [ ] **Step 3: Compile — verify no errors**

- [ ] **Step 4: Commit**
```
git add esp32_firmware/sprinkler_controller/sprinkler_controller.ino
git commit -m "Add weather skip logic — skip scheduled runs on rain forecast or freeze risk"
```

---

## Task 5: Location and weather settings in the UI

**Files:**
- Modify: `esp32_firmware/sprinkler_controller/data/index.html`

- [ ] **Step 1: Add Location section to Settings page**

In `data/index.html`, find the Settings page section (look for `id="page-settings"` or the timezone settings row). Add this block after the timezone row:

```html
<div class="sec-hdr"><span class="sec-title">Location</span></div>
<div class="settings-section">
  <div class="settings-row" style="gap:8px;flex-wrap:wrap">
    <button class="upd-btn" style="flex:1;min-width:120px" onclick="autoLocate()">Use My Location</button>
    <input id="zip-inp" type="text" class="tz-inp" placeholder="ZIP / Postcode" style="flex:1;min-width:100px">
    <button class="upd-btn" onclick="lookupZip()" style="white-space:nowrap">Look Up</button>
  </div>
  <div class="settings-row" style="gap:8px">
    <span class="settings-label" style="min-width:30px">Lat</span>
    <input id="lat-inp" type="number" step="0.0001" class="tz-inp" style="flex:1" placeholder="0.0000">
    <span class="settings-label" style="min-width:30px">Lon</span>
    <input id="lon-inp" type="number" step="0.0001" class="tz-inp" style="flex:1" placeholder="0.0000">
  </div>
  <div class="settings-row">
    <button class="save-btn" onclick="saveLocation()">Save Location</button>
  </div>
</div>

<div class="sec-hdr"><span class="sec-title">Weather Skip</span></div>
<div class="settings-section">
  <div class="settings-row">
    <span class="settings-label">Enable weather skip</span>
    <label class="toggle"><input type="checkbox" id="weather-skip-chk" onchange="saveWeatherSettings()"><span class="slider"></span></label>
  </div>
  <div class="settings-row">
    <span class="settings-label">Skip if rain forecast &gt;</span>
    <div style="display:flex;align-items:center;gap:6px">
      <input id="rain-thresh-inp" type="number" min="0" max="100" class="tz-inp" style="width:60px" value="50">
      <span class="settings-val">%</span>
    </div>
  </div>
  <div class="settings-row">
    <span class="settings-label">Skip if temp below</span>
    <div style="display:flex;align-items:center;gap:6px">
      <input id="freeze-thresh-inp" type="number" step="0.5" class="tz-inp" style="width:60px" value="2">
      <span class="settings-val">°C</span>
    </div>
  </div>
  <div class="settings-row">
    <button class="save-btn" onclick="saveWeatherSettings()">Save Weather Settings</button>
  </div>
</div>
```

- [ ] **Step 2: Add JavaScript functions for location and weather settings**

Add the following JS block near the other settings functions (e.g. after `loadFwVersion()`):

```javascript
// ── Location ───────────────────────────────────────────
async function autoLocate() {
  if (!navigator.geolocation) { toast('Geolocation not supported','err'); return; }
  navigator.geolocation.getCurrentPosition(
    pos => {
      document.getElementById('lat-inp').value = pos.coords.latitude.toFixed(4);
      document.getElementById('lon-inp').value = pos.coords.longitude.toFixed(4);
      toast('Location found — click Save Location');
    },
    () => toast('Location access denied','err')
  );
}

async function lookupZip() {
  const zip = document.getElementById('zip-inp').value.trim();
  if (!zip) { toast('Enter a ZIP or postcode first','err'); return; }
  try {
    const r = await fetch(`https://nominatim.openstreetmap.org/search?postalcode=${encodeURIComponent(zip)}&format=json&limit=1`,
      { headers: { 'Accept-Language': 'en' } });
    const data = await r.json();
    if (!data.length) { toast('ZIP not found','err'); return; }
    document.getElementById('lat-inp').value = parseFloat(data[0].lat).toFixed(4);
    document.getElementById('lon-inp').value = parseFloat(data[0].lon).toFixed(4);
    toast(`Found: ${data[0].display_name.split(',').slice(-2).join(',').trim()} — click Save Location`);
  } catch(e) { toast('Lookup failed','err'); }
}

async function saveLocation() {
  const lat = parseFloat(document.getElementById('lat-inp').value);
  const lon = parseFloat(document.getElementById('lon-inp').value);
  if (isNaN(lat) || isNaN(lon)) { toast('Invalid coordinates','err'); return; }
  const r = await POST('/api/config', { latitude: lat, longitude: lon });
  r.message ? toast('Location saved ✓') : toast(r.error || 'Save failed','err');
}

async function saveWeatherSettings() {
  const r = await POST('/api/config', {
    weather_enabled:  document.getElementById('weather-skip-chk').checked,
    rain_threshold:   parseInt(document.getElementById('rain-thresh-inp').value),
    freeze_threshold: parseFloat(document.getElementById('freeze-thresh-inp').value)
  });
  r.message ? toast('Weather settings saved ✓') : toast(r.error || 'Save failed','err');
}

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

- [ ] **Step 3: Call loadWeatherSettings() on page load**

Find the settings page load function (likely called when the settings tab is shown, or in a general `loadSettings()` function). Add:
```javascript
loadWeatherSettings();
```

- [ ] **Step 4: Commit**
```
git add esp32_firmware/sprinkler_controller/data/index.html
git commit -m "Add location and weather skip settings UI"
```

---

## Task 6: Weather display widget in the UI

**Files:**
- Modify: `esp32_firmware/sprinkler_controller/data/index.html`

- [ ] **Step 1: Add weather card HTML to the dashboard/home page**

Find the main dashboard page (likely `id="page-home"` or the schedule page). Add a weather card before or after the schedule section:

```html
<div id="weather-card" style="display:none;background:#131f2e;border-radius:12px;padding:14px 16px;margin:12px 16px;border:1px solid #1a3a5c">
  <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:8px">
    <span style="font-size:0.85rem;color:#90caf9;font-weight:600;text-transform:uppercase;letter-spacing:.5px">Weather</span>
    <span id="wx-age" style="font-size:0.75rem;color:#546e7a"></span>
  </div>
  <div style="display:flex;gap:16px;flex-wrap:wrap">
    <div style="display:flex;align-items:center;gap:6px">
      <span style="font-size:1.6rem" id="wx-icon">🌤</span>
      <span id="wx-temp" style="font-size:1.4rem;font-weight:700;color:#e0e0e0">--°C</span>
    </div>
    <div style="display:flex;flex-direction:column;gap:4px;font-size:0.85rem">
      <span>🌧 Today: <strong id="wx-rain-today">--%</strong> &nbsp; Tomorrow: <strong id="wx-rain-tomorrow">--%</strong></span>
      <span>🌡 Min today: <strong id="wx-min-temp">--°C</strong></span>
    </div>
  </div>
  <div id="wx-skip-warn" style="display:none;margin-top:8px;padding:6px 10px;background:#1a3a5c;border-radius:6px;color:#ffb74d;font-size:0.85rem"></div>
</div>
```

- [ ] **Step 2: Add weather update JavaScript**

Add this function near the other data-loading functions:

```javascript
// ── Weather widget ─────────────────────────────────────
const WX_ICONS = {
  0:'☀️', 1:'🌤', 2:'⛅', 3:'☁️',
  45:'🌫', 48:'🌫',
  51:'🌦', 53:'🌦', 55:'🌧', 61:'🌧', 63:'🌧', 65:'🌧',
  71:'🌨', 73:'🌨', 75:'❄️',
  80:'🌦', 81:'🌧', 82:'⛈',
  95:'⛈', 96:'⛈', 99:'⛈'
};

async function updateWeatherWidget() {
  try {
    const w = await GET('/api/weather');
    const card = document.getElementById('weather-card');
    if (!w.valid || (!w.weather_enabled && w.rain_prob_today === 0 && w.current_temp === 0)) {
      card.style.display = 'none';
      return;
    }
    card.style.display = 'block';
    document.getElementById('wx-icon').textContent        = WX_ICONS[w.weather_code] || '🌡';
    document.getElementById('wx-temp').textContent        = w.current_temp.toFixed(1) + '°C';
    document.getElementById('wx-rain-today').textContent  = w.rain_prob_today + '%';
    document.getElementById('wx-rain-tomorrow').textContent = w.rain_prob_tomorrow + '%';
    document.getElementById('wx-min-temp').textContent    = w.min_temp_today.toFixed(1) + '°C';
    const ageMin = Math.round(w.fetched_ago_sec / 60);
    document.getElementById('wx-age').textContent         = ageMin < 2 ? 'just updated' : `${ageMin}m ago`;

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

- [ ] **Step 3: Call updateWeatherWidget() on page load and on a timer**

In the main polling/init code, add:
```javascript
updateWeatherWidget();
setInterval(updateWeatherWidget, 300000); // refresh every 5 minutes
```

- [ ] **Step 4: Commit**
```
git add esp32_firmware/sprinkler_controller/data/index.html
git commit -m "Add weather display widget to dashboard"
```

---

## Task 7: Regenerate html_content.h, bump version, final commit

**Files:**
- Modify: `esp32_firmware/sprinkler_controller/html_content.h`
- Modify: `esp32_firmware/sprinkler_controller/sprinkler_controller.ino`

- [ ] **Step 1: Bump FW_VERSION to 1.5.0**

In `sprinkler_controller.ino`:
```cpp
#define FW_VERSION  "1.5.0"
```

- [ ] **Step 2: Regenerate html_content.h**

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

- [ ] **Step 3: Compile in Arduino IDE — verify no errors and check sketch size is still under 1.9MB**

- [ ] **Step 4: Flash to board via USB**

- [ ] **Step 5: Smoke test**
  - Open `http://sprinkler.local` — Settings tab should show Location and Weather Skip sections
  - Enter your lat/lon (or use zip/auto), save, wait ~10s
  - Open `http://sprinkler.local/api/weather` — should return real weather data
  - Weather card should appear on the dashboard

- [ ] **Step 6: Commit and push**
```
git add esp32_firmware/sprinkler_controller/sprinkler_controller.ino
git add esp32_firmware/sprinkler_controller/data/index.html
git add esp32_firmware/sprinkler_controller/html_content.h
git commit -m "v1.5.0: weather display and weather skip (Open-Meteo, no API key)"
git push origin custom-board
```

---

## Self-Review

**Spec coverage:**
- ✅ Weather info shown in web interface (Task 6 — weather card with temp, rain %, freeze)
- ✅ Weather skip on rain forecast (Task 4 — `shouldSkipForWeather()` checks `rainProbToday` and `rainProbTomorrow`)
- ✅ Weather skip on freeze (Task 4 — checks `minTempToday <= freezeThreshold`)
- ✅ Location via auto-locate (Task 5 — `navigator.geolocation`)
- ✅ Location via zip code (Task 5 — Nominatim lookup in browser)
- ✅ Location via manual lat/lon (Task 5 — direct input fields)
- ✅ Thresholds configurable (Task 5 — rain % and freeze °C in settings)
- ✅ Config persisted (Task 1 — all fields in `config.json`)

**Notes:**
- Weather fetch uses 8192 stack bytes — if memory is tight, reduce to 6144 and test
- Open-Meteo uses UTC for `daily` arrays when `timezone=auto` — the first array element `[0]` is always today in the board's local timezone, which is correct
- Nominatim requires a `User-Agent` header for production use — the browser's default UA is sufficient here
- If lat/lon are both 0.0, `fetchWeather()` returns early — this prevents a fetch for the Gulf of Guinea
