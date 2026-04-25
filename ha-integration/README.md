# SprinKlr-8 Home Assistant Integration

Local polling integration for the SprinKlr-8 8-zone WiFi sprinkler controller.
No cloud required.

## Installation via HACS

1. In Home Assistant, open HACS → Integrations → ⋮ → Custom repositories
2. Add `https://github.com/John-High-Sierra/sprinkler.board` as an Integration
3. Search for "SprinKlr-8" and install

## Manual Installation

Copy `custom_components/sprinklr8/` into your HA `config/custom_components/` directory.

## Configuration

Add to `configuration.yaml`:

```yaml
switch:
  - platform: sprinklr8
    host: 192.168.1.xxx   # Replace with your board's IP address
    duration: 10          # Default run duration in minutes (optional, default: 10)
```

## Entities

Creates 8 switch entities: `switch.sprinklr_8_zone_1` through `switch.sprinklr_8_zone_8`

- **Turn on:** Activates the zone for the configured duration
- **Turn off:** Stops all zones (`/api/stop_sequence`)
- **State:** Polled from `/api/status` — on when `is_running=true` and `active_sprinkler` matches zone

## API endpoints used

| Method | Endpoint | Purpose |
|--------|----------|---------|
| GET | `/api/status` | Poll zone state |
| POST | `/api/run_zone` | Activate zone (`{"zone": 0, "duration": 10}`) |
| POST | `/api/stop_sequence` | Stop all zones |
