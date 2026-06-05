# Tailscale Remote Access Setup

Access your SprinKlr-8 from anywhere — same web UI, no firmware changes.

## How it works

The ESP32 cannot run a Tailscale client. Your Windows PC acts as a **single-host router**:
it runs Tailscale and advertises only the board's IP address, so your phone can reach the
board through the encrypted WireGuard tunnel — nothing else on your home network is exposed.

## Setup (about 10 minutes)

### 1. Install Tailscale on your Windows PC

Download and install from https://tailscale.com/download

Set it to start automatically (the installer does this by default).

### 2. Advertise only the board's IP on the PC

Open a command prompt and run:
```
tailscale up --advertise-routes=10.110.201.40/32 --accept-routes
```

`/32` means a single host only — not the whole subnet. Only the board is reachable
through Tailscale; all other devices on your home network remain private.

### 3. Approve the route in the Tailscale admin console

- Open https://login.tailscale.com/admin/machines
- Find your Windows PC in the list
- Click the `…` menu → **Edit route settings**
- Enable `10.110.201.40/32`
- Click Save

### 4. Install Tailscale on your phone

- iOS: App Store → Tailscale
- Android: Play Store → Tailscale

Log into the same Tailscale account as your PC.

### 5. Done

With Tailscale running on your phone, open:

```
http://10.110.201.40
```

The SprinKlr-8 dashboard loads exactly as it does at home.

## Switching from /24 to /32

If you previously ran Tailscale with `--advertise-routes=10.110.201.0/24`, run the
single-host command above to replace it. Then in the admin console, remove the old `/24`
route approval and approve the new `/32` one.

## Tips

- **Local access unchanged** — `http://sprinkler.local` and `http://10.110.201.40` still
  work on your home network without Tailscale.
- **Tailscale must be connected on both devices** — toggle it on before opening the app.
- **PC must be on** — if the PC is off, the route is unreachable. Keep it in sleep mode
  rather than fully powered off, or use a NAS/Raspberry Pi as a permanent router.
- **PWA** — once the board is accessible remotely, install the web UI as an app:
  - Android: Chrome → ⋮ → Add to Home screen
  - iOS: Safari → Share → Add to Home Screen
