# Run History — Design Spec

**Date:** 2026-05-31  
**Version:** v1.7.0  
**Status:** Approved

---

## Goal

Add a History tab to the sprinkler controller UI showing the last 60 days of watering activity — which zones ran, for how long, and any weather skips. Data is stored on the ESP32 in LittleFS and served via a new API endpoint.

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

Bar widths are relative: the zone with the longest total runtime across the entire dataset gets 100% width; all others scale proportionally. Duration is displayed in minutes (`Math.round(sec / 60) + ' min'`). Runs of 0 seconds are not rendered.

Zone labels are resolved via the existing `getNames()` helper (reads `localStorage['znames']`), so user-defined zone names ("Front Lawn", "Back Beds", etc.) appear in the cards automatically. The log stores zone indices only.

`loadHistoryPage()` is called once when the History tab is first tapped, and re-called each time the tab is re-opened to pick up new runs.

---

## Data Model

Stored in `/runs.json` on LittleFS. A JSON array of up to 60 entries, newest last in the file (reversed to newest-first in the UI). When entry 61 is added the oldest (index 0) is removed.

### Run entry
```json
{
  "ts": 1748700000,
  "trigger": "schedule",
  "zones": [
    {"z": 0, "sec": 840},
    {"z": 1, "sec": 600},
    {"z": 2, "sec": 1020},
    {"z": 3, "sec": 420}
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
| `zones` | array | `{z: 0-7, sec: int}` per zone that ran; empty on skip |
| `skip` | string\|null | `null`, `"rain forecast"`, `"freeze risk"` |

Days with no scheduled run have no entry — displayed as nothing in the UI.

**Storage:** ~100 bytes/entry × 60 entries = ~6KB max.

---

## Firmware Changes

### New function: `appendRunLog()`
```
void appendRunLog(time_t ts, const char* trigger, int zones[], int durations[], int zoneCount, const char* skipReason)
```
- Opens `runs.json`, parses with `DynamicJsonDocument(8192)`
- Appends new entry to the array
- If array length > 60, removes index 0
- Serializes back to LittleFS
- If file doesn't exist, creates it with a fresh array

**Called from:**
1. `runSequenceTask` — after the final zone completes (both scheduled and manual runs)
2. `scheduleCheckerTask` — when `shouldSkipForWeather()` returns a non-null reason

### New endpoint: `GET /api/history`
- Reads `runs.json` from LittleFS
- Returns contents as `application/json`
- If file doesn't exist, returns `[]`
- No filtering or transformation — raw data, browser handles display

### JSON document budget
`DynamicJsonDocument(8192)` for read/write of the log file. Well within ESP32 heap.

---

## File Map

| File | Change |
|------|--------|
| `esp32_firmware/sprinkler_controller/sprinkler_controller.ino` | `appendRunLog()`, call sites in `runSequenceTask` + `scheduleCheckerTask`, `GET /api/history` |
| `esp32_firmware/sprinkler_controller/data/index.html` | History tab in nav, History page HTML, CSS for day cards + zone bars, `loadHistoryPage()` JS |
| `esp32_firmware/sprinkler_controller/html_content.h` | Regenerated from `data/index.html` |

---

## Out of Scope

- Zone naming in the log (names resolved from localStorage at display time, not stored in the log)
- Editing or deleting log entries
- Exporting history
- History retention longer than 60 days
- ET-based scheduling (separate future feature)
