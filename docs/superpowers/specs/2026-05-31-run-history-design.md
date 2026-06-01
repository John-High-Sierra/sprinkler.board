# Run History — Design Spec

**Date:** 2026-05-31  
**Version:** v1.7.0  
**Status:** Approved

---

## Goal

Add a History tab to the sprinkler controller UI showing the last 60 days of watering activity — which zones ran, for how long, and any weather skips. Data is stored on the ESP32 in LittleFS and served via a new API endpoint.

Zone names are migrated from browser localStorage to `config.json` on the board so the firmware can snapshot the name at run time into each log entry, and names persist across browser clears and multiple devices.

---

## UI Design

### Navigation
The bottom nav gains a fifth tab: **History**, inserted between Weather and Settings. The existing 4-tab pattern extends naturally.

### History Page
A scrollable list of daily run cards, most recent first. Each card shows:

- **Day name** (bold) + **date** (teal, e.g. "May 31, 2026") on separate lines
- **Zone bars** — one horizontal bar per zone that ran, width proportional to duration relative to the longest zone run in the dataset, minutes labeled inside the bar
  - Scheduled runs: `#4fc3f7` (teal/blue)
  - Manual runs: `#81c784` (green), card date shows "· manual run"
- **Skipped day** — amber header "⚠ Skipped — {reason}", no zone bars, "No watering" in dimmed text
- **No schedule** — dimmed "No schedule" text, no zone bars

Bar widths are relative: the zone with the longest zone duration across the entire dataset gets 100% width; all others scale proportionally. Duration is displayed in minutes (`Math.round(sec / 60) + ' min'`). Runs of 0 seconds are not rendered.

Zone labels come from the `name` field stored in each log entry — the name as it was at run time, not the current name. This means renaming a zone doesn't retroactively change old history.

`loadHistoryPage()` is called once when the History tab is first tapped, and re-called each time the tab is re-opened to pick up new runs.

### Settings — Zone Names
The existing zone name inputs in Settings currently save to `localStorage`. They are updated to `POST /api/config` instead, saving to the board. On page load, `loadConfig()` reads zone names from `/api/config` and populates both the settings inputs and a JS `zoneNames` array used everywhere names appear in the UI. The `localStorage['znames']` key and `getNames()` helper are removed; all callers use `zoneNames[]` directly.

---

## Data Model

### Log file: `runs.json`
Stored in LittleFS. A JSON array of up to 60 entries, newest last in the file (reversed to newest-first in the UI). When entry 61 is added the oldest (index 0) is removed.

### Run entry
```json
{
  "ts": 1748700000,
  "trigger": "schedule",
  "zones": [
    {"z": 0, "name": "Front Lawn", "sec": 840},
    {"z": 1, "name": "Back Beds",  "sec": 600},
    {"z": 2, "name": "Side Gate",  "sec": 1020},
    {"z": 3, "name": "Pots",       "sec": 420}
  ],
  "skip": null
}
```

### Skip entry
```json
{
  "ts": 1748786400,
  "trigger": "schedule",
  "zones": [],
  "skip": "rain forecast"
}
```

**Fields:**
| Field | Type | Values |
|-------|------|--------|
| `ts` | int | Unix epoch at run start (local time from NTP) |
| `trigger` | string | `"schedule"` or `"manual"` |
| `zones` | array | `{z: 0-7, name: string, sec: int}` per zone that ran; empty on skip |
| `skip` | string\|null | `null`, `"rain forecast"`, `"freeze risk"` |

Days with no scheduled run have no entry — the UI shows nothing for those days.

**Storage:** ~130 bytes/entry × 60 entries = ~8KB max.

---

## Firmware Changes

### BoardConfig — add zone names
```cpp
struct BoardConfig {
  // ... existing fields ...
  char zoneNames[8][32];  // user-defined names, default "Zone 1"–"Zone 8"
};
```

`loadConfig()`: defaults to `"Zone 1"` through `"Zone 8"`, reads `zone_names` array from JSON.  
`saveConfig()`: writes `zone_names` array.  
`GET /api/config`: includes `"zone_names": ["Front Lawn", ...]` in response.  
`POST /api/config`: accepts `"zone_names"` array and updates `boardConfig.zoneNames`.

### New function: `appendRunLog()`
```
void appendRunLog(time_t ts, const char* trigger, int zones[], int durations[], int zoneCount, const char* skipReason)
```
- Opens `runs.json`, parses with `DynamicJsonDocument(8192)`
- Appends new entry; zone name read from `boardConfig.zoneNames[zones[i]]`
- If array length > 60, removes index 0
- Serializes back to LittleFS
- If file doesn't exist, creates it with a fresh array

**Called from:**
1. `runSequenceTask` — after the final zone completes (scheduled and manual runs)
2. `scheduleCheckerTask` — when `shouldSkipForWeather()` returns a non-null reason

### New endpoint: `GET /api/history`
- Reads `runs.json` from LittleFS and returns it as `application/json`
- If file doesn't exist, returns `[]`
- No transformation — raw data, browser handles display

### JSON document budget
`DynamicJsonDocument(8192)` for the log file. `DynamicJsonDocument` for config already sized adequately — verify it covers 8 × 32-char zone name strings (add ~300 bytes; bump to 768 if needed).

---

## UI Changes

### Zone names migration
- `loadConfig()` reads `zone_names` from `/api/config`, populates module-level `let zoneNames = []`
- Zone name Settings inputs POST `{ zone_names: [...] }` to `/api/config` on save
- Remove `getNames()` / `setNames()` / `localStorage['znames']`; replace all callers with `zoneNames[i]`

### History page
- New `<div id="hist-page">` hidden by default
- `loadHistoryPage()`: calls `GET /api/history`, renders cards newest-first
- Zone bar label uses `entry.zones[i].name` directly from the log

---

## File Map

| File | Change |
|------|--------|
| `esp32_firmware/sprinkler_controller/sprinkler_controller.ino` | `BoardConfig` (add `zoneNames`), `loadConfig`/`saveConfig`/`GET api/config`/`POST api/config`, `appendRunLog()`, call sites in `runSequenceTask` + `scheduleCheckerTask`, `GET /api/history` |
| `esp32_firmware/sprinkler_controller/data/index.html` | Zone name migration (localStorage → board API), History tab + page HTML + CSS, `loadHistoryPage()` JS |
| `esp32_firmware/sprinkler_controller/html_content.h` | Regenerated from `data/index.html` |

---

## Out of Scope

- Editing or deleting log entries
- Exporting history
- History retention longer than 60 days
- ET-based scheduling (separate future feature)
