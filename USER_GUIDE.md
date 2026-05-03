# SprinKlr-8 User Guide

---

## 1. First Time Setup

### What you need
- The SprinKlr-8 board powered on
- A phone or laptop with WiFi

### Steps

1. Power on the board
2. On your phone or laptop, open WiFi settings and connect to **SprinklerSetup**
   - Password: `sprinkler123`
3. A setup page will open automatically (if it doesn't, open a browser and go to `192.168.4.1`)
4. Tap **Configure WiFi**
5. Select your home WiFi network and enter the password
6. Tap **Save** — the board will reboot and connect to your network

### Finding the board on your network

After setup, open your router's device list and look for **sprinkler**. Note the IP address (e.g. `10.110.201.40`). You can also try opening `http://sprinkler/` directly in a browser.

---

## 2. Using the Web Interface

Open a browser and go to `http://[board IP address]/`

The interface has four pages accessible from the bottom navigation bar:

### Dashboard
Shows the current status — which zone is running, time remaining, and a Run/Stop button for each zone.

### Schedule
Set up your automatic watering schedule. Enable or disable individual days, set the start time, and enter how many minutes each zone should run. Use the master toggle at the top to turn automatic scheduling on or off without losing your settings.

### Manual
Run a single zone for a set number of minutes, or trigger a full day's schedule to run immediately.

### Settings
- View system info (IP address, WiFi network)
- Edit zone names
- Download and apply updates (see Section 4)

---

## 3. Switching to a Different WiFi Network

If you move the board to a new location or change your WiFi:

1. Press the **EN** button on the board and release it
2. Within 2 seconds, press and hold the **IO0** button
3. Hold IO0 for 3 seconds — the board wipes the saved WiFi credentials
4. Release — the board opens the **SprinklerSetup** hotspot again
5. Follow the First Time Setup steps above to connect to the new network

> **Tip:** If you release IO0 before 3 seconds the reset is cancelled and the board boots normally.

---

## 4. Updating Over the Air

No laptop or cables needed for updates. Everything is done from the web interface.

### Update the Web UI

1. Open `http://[board IP]/`
2. Go to **Settings**
3. Under **Updates**, tap **Download & Apply from GitHub**
4. The board downloads the latest interface and reloads automatically

### Update the Firmware

1. Open `http://[board IP]/`
2. Go to **Settings**
3. Under **Updates**, tap **Download & Flash from GitHub**
4. Confirm the prompt
5. The board downloads and installs the new firmware then reboots
6. Wait about 15 seconds, then refresh the page

> The current firmware version is shown next to the Firmware button in Settings.

---

## 5. Troubleshooting

**Can't find the board on the network**
- Check your router's device list for a device named `sprinkler`
- Make sure the board is powered on and the status LED is blinking

**Web page won't load**
- Confirm you're on the same WiFi network as the board
- Try the IP address directly instead of `http://sprinkler/`

**Automatic schedule not running**
- Check the master schedule toggle in the Schedule page is turned ON
- Confirm the correct days are enabled and the time is set correctly
- Check that NTP is synced (Settings → system info)

**Need to reset everything**
- To reset WiFi only: see Section 3
- To reset the schedule to defaults: contact your installer

---

*SprinKlr-8 — ESP32 8-Zone Sprinkler Controller*
