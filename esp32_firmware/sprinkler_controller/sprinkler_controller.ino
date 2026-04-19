/**
 * ESP32 Sprinkler Controller
 * Target: ACEIRMC ESP32-WROOM-32E 8-Channel Relay Board (B0DTK2PB26)
 *
 * ─────────────────────────────────────────────────────────────────────
 *  CONFIRMED PINOUT (verified via ESPHome device registry)
 * ─────────────────────────────────────────────────────────────────────
 *  Relay 1 = GPIO32  |  Relay 5 = GPIO27
 *  Relay 2 = GPIO33  |  Relay 6 = GPIO14
 *  Relay 3 = GPIO25  |  Relay 7 = GPIO12  ← strapping pin, see note
 *  Relay 4 = GPIO26  |  Relay 8 = GPIO13
 *  Status LED = GPIO23
 *  Relay logic: ACTIVE HIGH (relay ON when GPIO = HIGH)
 *
 *  GPIO12 NOTE: This is a strapping pin on ESP32. It must be LOW at
 *  boot or the chip may fail to start. The firmware initialises it
 *  LOW (relay OFF) first thing, which is safe.
 *
 * ─────────────────────────────────────────────────────────────────────
 *  PROGRAMMING (no USB on this board — needs CP2102 adapter)
 * ─────────────────────────────────────────────────────────────────────
 *  Find the 6-pin header near the IO0/EN buttons (bottom-right of board)
 *  Typical pin order: 3V3 | GND | TX | RX | IO0 | EN
 *  Connect CP2102: TX→RX, RX→TX, GND→GND, 3.3V→3V3
 *  Flash mode: hold IO0 button, tap EN button, release IO0, then upload
 * ─────────────────────────────────────────────────────────────────────
 *
 * Required Libraries (install via Arduino Library Manager):
 *   - WiFiManager        (tzapu)
 *   - ArduinoJson        (bblanchon) v6+
 *   - NTPClient          (Fabrice Weinberg)
 *   (WebServer.h, LittleFS built into ESP32 core — no extra libs needed)
 *
 * Board: "ESP32 Dev Module" in Arduino IDE
 * Partition Scheme: "Default 4MB with spiffs"
 * Upload Speed: 921600
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <time.h>

// ═══════════════════════════════════════════════════════════════
//  HARDWARE CONFIGURATION — EDIT THESE FOR YOUR BOARD
// ═══════════════════════════════════════════════════════════════

// CONFIRMED for ACEIRMC B0DTK2PB26 (verified via ESPHome device registry)
// Relay 1=GPIO32, 2=GPIO33, 3=GPIO25, 4=GPIO26, 5=GPIO27, 6=GPIO14, 7=GPIO12, 8=GPIO13
const int RELAY_PINS[8] = {32, 33, 25, 26, 27, 14, 12, 13};

// Relay logic: ACTIVE HIGH — relay energises when GPIO = HIGH
// This board uses optocouplers wired active-high
#define RELAY_ACTIVE_LOW false

// Number of zones (up to 8)
#define NUM_ZONES 8

// Status LED — GPIO23 confirmed for this board
#define STATUS_LED_PIN 23

// WiFi reset button — uses the onboard IO0/BOOT button (no extra hardware needed)
// Hold IO0 for 3 seconds after power-on / EN press to wipe saved WiFi and open portal
#define WIFI_RESET_PIN 0

// ═══════════════════════════════════════════════════════════════
//  SYSTEM CONSTANTS
// ═══════════════════════════════════════════════════════════════
#define SCHEDULE_FILE  "/schedule.json"
#define HOSTNAME       "sprinkler"
#define AP_NAME        "SprinklerSetup"
#define AP_PASSWORD    "sprinkler123"
#define NTP_SERVER     "pool.ntp.org"
#define TZ_OFFSET_SEC  -18000   // UTC-5 (EST). Change for your timezone.
                                // UTC-6 = -21600, UTC-7 = -25200, UTC-8 = -28800

// ═══════════════════════════════════════════════════════════════
//  RELAY HELPER MACROS
// ═══════════════════════════════════════════════════════════════
#if RELAY_ACTIVE_LOW
  #define RELAY_ON(pin)  digitalWrite(pin, LOW)
  #define RELAY_OFF(pin) digitalWrite(pin, HIGH)
#else
  #define RELAY_ON(pin)  digitalWrite(pin, HIGH)
  #define RELAY_OFF(pin) digitalWrite(pin, LOW)
#endif

// ═══════════════════════════════════════════════════════════════
//  SCHEDULE DATA STRUCTURE
// ═══════════════════════════════════════════════════════════════
struct DaySchedule {
  bool isActive;
  int  hour;
  int  minute;
  int  durations[NUM_ZONES]; // minutes per zone, 0 = skip
};

struct SystemSchedule {
  bool enabled;
  DaySchedule days[7];
};

// ═══════════════════════════════════════════════════════════════
//  GLOBAL STATE
// ═══════════════════════════════════════════════════════════════
SystemSchedule schedule;

struct RunStatus {
  bool  isRunning;
  int   dayIndex;       // -1 if not running
  int   activeZone;     // 0-indexed, -1 if not running
  int   remainingTime;  // seconds
  bool  manualRun;
};
volatile RunStatus runStatus = {false, -1, -1, 0, false};

SemaphoreHandle_t statusMutex;
SemaphoreHandle_t scheduleMutex;
volatile bool     stopRequested = false;

WebServer         server(80);
WiFiUDP           ntpUDP;
NTPClient         timeClient(ntpUDP, NTP_SERVER, TZ_OFFSET_SEC, 60000);

int lastScheduledDay = -1;
int lastScheduledMinute = -1;

// ═══════════════════════════════════════════════════════════════
//  RELAY CONTROL
// ═══════════════════════════════════════════════════════════════
void allRelaysOff() {
  for (int i = 0; i < NUM_ZONES; i++) {
    RELAY_OFF(RELAY_PINS[i]);
  }
}

void setupRelayPins() {
  for (int i = 0; i < NUM_ZONES; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    RELAY_OFF(RELAY_PINS[i]);
  }
}

// ═══════════════════════════════════════════════════════════════
//  SCHEDULE PERSISTENCE (LittleFS)
// ═══════════════════════════════════════════════════════════════
void loadDefaultSchedule() {
  schedule.enabled = true;
  for (int d = 0; d < 7; d++) {
    schedule.days[d].isActive = false;
    schedule.days[d].hour     = 7;
    schedule.days[d].minute   = 0;
    for (int z = 0; z < NUM_ZONES; z++) {
      schedule.days[d].durations[z] = 10;
    }
  }
}

bool loadSchedule() {
  if (!LittleFS.exists(SCHEDULE_FILE)) {
    Serial.println("[SCHED] No schedule file found, using defaults");
    loadDefaultSchedule();
    return false;
  }

  File f = LittleFS.open(SCHEDULE_FILE, "r");
  if (!f) {
    Serial.println("[SCHED] Failed to open schedule file");
    loadDefaultSchedule();
    return false;
  }

  DynamicJsonDocument doc(4096);
  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err) {
    Serial.printf("[SCHED] JSON parse error: %s\n", err.c_str());
    loadDefaultSchedule();
    return false;
  }

  xSemaphoreTake(scheduleMutex, portMAX_DELAY);

  schedule.enabled = doc["enabled"] | true;
  JsonArray days = doc["schedule"].as<JsonArray>();

  for (int d = 0; d < 7 && d < (int)days.size(); d++) {
    JsonObject day = days[d];
    schedule.days[d].isActive = day["is_active"] | false;
    schedule.days[d].hour     = day["hour"]      | 7;
    schedule.days[d].minute   = day["minute"]    | 0;

    JsonArray durs = day["durations"].as<JsonArray>();
    for (int z = 0; z < NUM_ZONES && z < (int)durs.size(); z++) {
      schedule.days[d].durations[z] = durs[z] | 10;
    }
    // Fill remaining zones if fewer were saved
    for (int z = durs.size(); z < NUM_ZONES; z++) {
      schedule.days[d].durations[z] = 0;
    }
  }

  xSemaphoreGive(scheduleMutex);
  Serial.println("[SCHED] Schedule loaded OK");
  return true;
}

bool saveSchedule() {
  DynamicJsonDocument doc(4096);

  xSemaphoreTake(scheduleMutex, portMAX_DELAY);
  doc["enabled"] = schedule.enabled;
  JsonArray days = doc.createNestedArray("schedule");

  for (int d = 0; d < 7; d++) {
    JsonObject day = days.createNestedObject();
    day["is_active"] = schedule.days[d].isActive;
    day["hour"]      = schedule.days[d].hour;
    day["minute"]    = schedule.days[d].minute;
    JsonArray durs   = day.createNestedArray("durations");
    for (int z = 0; z < NUM_ZONES; z++) {
      durs.add(schedule.days[d].durations[z]);
    }
  }
  xSemaphoreGive(scheduleMutex);

  File f = LittleFS.open(SCHEDULE_FILE, "w");
  if (!f) {
    Serial.println("[SCHED] Failed to open file for writing");
    return false;
  }
  serializeJson(doc, f);
  f.close();
  Serial.println("[SCHED] Schedule saved OK");
  return true;
}

// ═══════════════════════════════════════════════════════════════
//  SPRINKLER SEQUENCE TASK (runs on separate FreeRTOS task)
// ═══════════════════════════════════════════════════════════════
struct RunArgs {
  int dayIndex;
  bool manual;
};

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
    int remaining = durMin * 60;

    while (remaining > 0) {
      if (stopRequested) {
        Serial.printf("[RUN] Stop during zone %d\n", z + 1);
        break;
      }

      xSemaphoreTake(statusMutex, portMAX_DELAY);
      runStatus.isRunning    = true;
      runStatus.dayIndex     = dayIndex;
      runStatus.activeZone   = z;
      runStatus.remainingTime = remaining;
      runStatus.manualRun    = manual;
      xSemaphoreGive(statusMutex);

      vTaskDelay(pdMS_TO_TICKS(1000));
      remaining--;
    }

    RELAY_OFF(RELAY_PINS[z]);
    Serial.printf("[RUN] Zone %d OFF\n", z + 1);

    if (stopRequested) break;
  }

  // Cleanup
  allRelaysOff();

  xSemaphoreTake(statusMutex, portMAX_DELAY);
  runStatus.isRunning    = false;
  runStatus.dayIndex     = -1;
  runStatus.activeZone   = -1;
  runStatus.remainingTime = 0;
  xSemaphoreGive(statusMutex);

  Serial.println("[RUN] Sequence complete");
  vTaskDelete(NULL);
}

bool startSequence(int dayIndex, bool manual = false) {
  xSemaphoreTake(statusMutex, portMAX_DELAY);
  bool alreadyRunning = runStatus.isRunning;
  xSemaphoreGive(statusMutex);

  if (alreadyRunning) return false;

  RunArgs* args = new RunArgs{dayIndex, manual};
  xTaskCreate(runSequenceTask, "sprinkler_run", 4096, args, 1, NULL);
  return true;
}

// ═══════════════════════════════════════════════════════════════
//  SCHEDULE CHECKER TASK
// ═══════════════════════════════════════════════════════════════
void scheduleCheckerTask(void* param) {
  Serial.println("[SCHED] Checker task started");

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(15000)); // Check every 15 seconds

    if (!timeClient.isTimeSet()) {
      timeClient.update();
      continue;
    }

    xSemaphoreTake(scheduleMutex, portMAX_DELAY);
    bool enabled = schedule.enabled;
    xSemaphoreGive(scheduleMutex);

    if (!enabled) continue;

    time_t now = timeClient.getEpochTime();
    struct tm* t = localtime(&now);

    // tm_wday: 0=Sunday, 1=Monday...6=Saturday
    // We store: 0=Monday...6=Sunday (matching Python weekday())
    int dayIndex = (t->tm_wday == 0) ? 6 : t->tm_wday - 1;
    int hour     = t->tm_hour;
    int minute   = t->tm_min;

    // Prevent firing more than once per minute
    if (dayIndex == lastScheduledDay && minute == lastScheduledMinute) continue;

    xSemaphoreTake(scheduleMutex, portMAX_DELAY);
    DaySchedule day = schedule.days[dayIndex];
    xSemaphoreGive(scheduleMutex);

    if (day.isActive && day.hour == hour && day.minute == minute) {
      Serial.printf("[SCHED] Scheduled run: %s %02d:%02d\n",
        (const char*[]){"Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Sunday"}[dayIndex],
        hour, minute);

      lastScheduledDay    = dayIndex;
      lastScheduledMinute = minute;
      startSequence(dayIndex, false);
    }
  }
}

// ═══════════════════════════════════════════════════════════════
//  JSON RESPONSE HELPERS
// ═══════════════════════════════════════════════════════════════
String buildStatusJson() {
  DynamicJsonDocument doc(256);
  xSemaphoreTake(statusMutex, portMAX_DELAY);
  doc["is_running"]     = runStatus.isRunning;
  doc["day_index"]      = runStatus.dayIndex;
  doc["active_sprinkler"] = runStatus.activeZone;
  doc["remaining_time"] = runStatus.remainingTime;
  doc["manual_run"]     = runStatus.manualRun;
  xSemaphoreGive(statusMutex);
  String out;
  serializeJson(doc, out);
  return out;
}

String buildScheduleJson() {
  DynamicJsonDocument doc(4096);
  xSemaphoreTake(scheduleMutex, portMAX_DELAY);
  doc["enabled"] = schedule.enabled;
  JsonArray days = doc.createNestedArray("schedule");
  for (int d = 0; d < 7; d++) {
    JsonObject day = days.createNestedObject();
    day["is_active"] = schedule.days[d].isActive;
    day["hour"]      = schedule.days[d].hour;
    day["minute"]    = schedule.days[d].minute;
    JsonArray durs   = day.createNestedArray("durations");
    for (int z = 0; z < NUM_ZONES; z++) {
      durs.add(schedule.days[d].durations[z]);
    }
  }
  xSemaphoreGive(scheduleMutex);
  String out;
  serializeJson(doc, out);
  return out;
}

String buildSystemInfoJson() {
  DynamicJsonDocument doc(512);
  time_t now = timeClient.getEpochTime();
  struct tm* t = localtime(&now);
  char timeBuf[32];
  strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", t);

  doc["current_time"]      = timeBuf;
  doc["schedule_enabled"]  = schedule.enabled;
  doc["ip_address"]        = WiFi.localIP().toString();
  doc["hostname"]          = HOSTNAME;
  doc["num_zones"]         = NUM_ZONES;
  doc["ntp_synced"]        = timeClient.isTimeSet();
  doc["uptime_sec"]        = millis() / 1000;
  doc["free_heap"]         = ESP.getFreeHeap();

  JsonArray pins = doc.createNestedArray("relay_pins");
  for (int i = 0; i < NUM_ZONES; i++) pins.add(RELAY_PINS[i]);

  String out;
  serializeJson(doc, out);
  return out;
}

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

// ═══════════════════════════════════════════════════════════════
//  WIFI RESET CHECK — hold IO0 button for 3 s at boot to wipe
//  saved WiFi credentials and force the setup portal to open
// ═══════════════════════════════════════════════════════════════
void checkWiFiReset() {
  pinMode(WIFI_RESET_PIN, INPUT_PULLUP);
  Serial.println("[WIFI-RESET] Press and hold IO0 within 2 seconds to reset WiFi...");

  // 2-second window for user to press IO0
  unsigned long window = millis();
  bool pressed = false;
  while (millis() - window < 2000) {
    if (digitalRead(WIFI_RESET_PIN) == LOW) {
      pressed = true;
      break;
    }
    delay(50);
  }

  if (!pressed) {
    Serial.println("[WIFI-RESET] IO0 not pressed — booting normally");
    return;
  }

  // IO0 pressed — now require 3-second hold to confirm
  Serial.println("[WIFI-RESET] IO0 detected — keep holding for 3 seconds to wipe WiFi...");
  unsigned long start = millis();
  int lastSecond = -1;

  while (millis() - start < 3000) {
    int elapsed = (millis() - start) / 1000;
    if (elapsed != lastSecond) {
      lastSecond = elapsed;
      Serial.printf("[WIFI-RESET] Holding... %d/3 seconds\n", elapsed + 1);
    }
    if (digitalRead(WIFI_RESET_PIN) == HIGH) {
      Serial.println("[WIFI-RESET] IO0 released early — reset CANCELLED, booting normally");
      return;
    }
    digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
    delay(100);
  }

  // Still held after 3 s — wipe credentials
  Serial.println("[WIFI-RESET] 3 seconds confirmed — wiping WiFi credentials...");
  WiFiManager wm;
  wm.resetSettings();
  Serial.println("[WIFI-RESET] Done. WiFi credentials cleared.");
  Serial.println("[WIFI-RESET] Board will now open 'SprinklerSetup' hotspot.");
  for (int i = 0; i < 10; i++) {
    digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
    delay(80);
  }
}

// ═══════════════════════════════════════════════════════════════
//  WIFI SETUP (WiFiManager captive portal)
// ═══════════════════════════════════════════════════════════════
void setupWiFi() {
  WiFi.setHostname(HOSTNAME);
  WiFiManager wm;
  wm.setConfigPortalTimeout(180); // 3 min portal timeout, then continue

  bool connected = wm.autoConnect(AP_NAME, AP_PASSWORD);

  if (connected) {
    Serial.printf("[WIFI] Connected: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("[WIFI] Portal timed out, running in offline mode");
  }
}

// ═══════════════════════════════════════════════════════════════
//  STATUS LED BLINK TASK
// ═══════════════════════════════════════════════════════════════
void ledTask(void* param) {
  while (true) {
    xSemaphoreTake(statusMutex, portMAX_DELAY);
    bool running = runStatus.isRunning;
    xSemaphoreGive(statusMutex);

    if (running) {
      // Fast blink when running
      digitalWrite(STATUS_LED_PIN, HIGH);
      vTaskDelay(pdMS_TO_TICKS(200));
      digitalWrite(STATUS_LED_PIN, LOW);
      vTaskDelay(pdMS_TO_TICKS(200));
    } else {
      // Slow heartbeat when idle
      digitalWrite(STATUS_LED_PIN, HIGH);
      vTaskDelay(pdMS_TO_TICKS(100));
      digitalWrite(STATUS_LED_PIN, LOW);
      vTaskDelay(pdMS_TO_TICKS(2900));
    }
  }
}

// ═══════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n[BOOT] ESP32 Sprinkler Controller starting...");

  // Mutexes
  statusMutex   = xSemaphoreCreateMutex();
  scheduleMutex = xSemaphoreCreateMutex();

  // GPIO — LED first so checkWiFiReset() can blink it
  pinMode(STATUS_LED_PIN, OUTPUT);
  setupRelayPins();
  Serial.println("[BOOT] Relay pins configured, all OFF");

  // WiFi reset check — must run early, before LittleFS/schedule delay
  // Press EN to boot, then immediately hold IO0 for 3 s to wipe WiFi credentials
  checkWiFiReset();

  // LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("[BOOT] LittleFS mount failed — formatting...");
    LittleFS.format();
    LittleFS.begin(true);
  }
  Serial.println("[BOOT] LittleFS OK");

  // Load schedule
  loadSchedule();

  // WiFi
  setupWiFi();

  // NTP
  timeClient.begin();
  timeClient.update();
  Serial.printf("[BOOT] NTP time: %s\n", timeClient.getFormattedTime().c_str());

  // Web server
  setupRoutes();
  server.begin();
  Serial.println("[BOOT] Web server started on port 80");
  Serial.printf("[BOOT] Access UI at: http://%s/\n", WiFi.localIP().toString().c_str());

  // Background tasks
  xTaskCreate(scheduleCheckerTask, "sched_check", 4096, NULL, 1, NULL);
  xTaskCreate(ledTask,             "led_blink",   1024, NULL, 1, NULL);

  Serial.println("[BOOT] All systems GO");
}

// ═══════════════════════════════════════════════════════════════
//  LOOP (just NTP keepalive — real work is in tasks/async server)
// ═══════════════════════════════════════════════════════════════
void loop() {
  server.handleClient();
  static unsigned long lastNTP = 0;
  if (millis() - lastNTP > 300000) { // Resync every 5 min
    timeClient.update();
    lastNTP = millis();
  }
  delay(10);
}
