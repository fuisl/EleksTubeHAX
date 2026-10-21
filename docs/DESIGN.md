# TubeHAX for Claude — design

Personal fork of [EleksTubeHAX](https://github.com/aly-fly/EleksTubeHAX), rebuilt for a single device: the **EleksTube IPS Pro (Gen2)**. It turns the clock into a desk companion that shows the time, a dramatic countdown, live Claude Code session data and the currently playing song.

Ideas and progress are tracked in the [GitHub Project](https://github.com/users/fuisl/projects/27) (vision: #1). This document is the source of truth for architecture, the button map and the MQTT contract.

## Hardware we design for

| Part | Detail | Design consequence |
|---|---|---|
| MCU | ESP32-PICO-D4, 240 MHz dual core, 520 KB SRAM, **no PSRAM** | One 135×240 frame buffer (64.8 KB) at a time; no caching of whole digit sets in RAM |
| Flash | 4 MB | Must be shared between firmware, OTA slot and assets — keep only the clock faces we use |
| Displays | 6 × ST7789 135×240, one SPI bus at 40 MHz, chip select via shift register | Tubes are drawn one after another; a full-screen redraw is ~13 ms per tube, ~80 ms for all six |
| Backlights | 6 × WS2812 RGB, one behind each tube | Free ambient "status channel" independent of the screens |
| Buttons | LEFT, MODE, RIGHT, POWER (active low) | See button map |
| RTC | DS3231 on I2C | Keeps time without WiFi |

## Visual language

- **Anything that is a time** (clock, countdown, elapsed, time-until-reset, song position) is drawn with **nixie digit images** — the clock face we already love.
- **Everything else** (state, tokens, percentages, titles, icons) uses the **Claude CLI theme**: Claude orange on near-black, chunky pixel/ASCII-art glyphs, the CLI's spinner glyphs (`· ✢ ✳ ✶ ✻ ✽`).
- Palette (RGB, converted to RGB565 on device): orange `#D97757`, dim orange `#8A4A36`, cream `#F0EEE6`, background `#141413`, alert amber `#E8A33D`, error red `#C2453A`, ok green `#7FB069`.

## System architecture

```
 Internet                          Homelab (fuisl)                         Clock
┌────────────────────────┐        ┌──────────────────────────┐          ┌────────────────┐
│ status.claude.com      │  HTTPS │ tubehax-feeds ──┐        │          │ ESP32 firmware │
│ GitHub releases        ├───────▶│ (Python, 16 MB) │ MQTT   │   MQTT   │  AppManager    │
│ (anthropics/claude-    │        │                 ▼        ├─────────▶│   ├ Clock      │
│  code)                 │        │ Mosquitto (retain, LWT)  │◀─────────┤   ├ Countdown  │
└────────────────────────┘        │  :9001 admin page + WS   │ state    │   └ Claude     │
                                  └──────────────────────────┘          └────────────────┘
```

The Mac sends nothing: an earlier version fed Claude Code hooks and the status line through a Mac daemon, removed by request. Everything Claude mode shows is public and fetched on the homelab. A browser admin page (LAN or NetBird) talks to the same broker over MQTT-over-WebSockets — see [Homelab](#homelab) and [Admin web](#admin-web).

Principles:

1. **The clock only talks to the broker** (plus NTP and its own small HTTP API). Fetching and condensing web data happens on the homelab, so the ESP32 needs no TLS and parses a few hundred bytes instead of 200 KB.
2. **Publish facts, not frames.** Timestamps (`started_at`, `resets_at`, `published_at`) let the clock compute ages and countdowns locally with its own RTC/NTP time.
3. **State topics are retained** so a freshly (re)connected clock immediately shows the right thing.

### Claude data sources (public)

| Shown | Source | Refresh |
|---|---|---|
| Overall status, components, open incident | `https://status.claude.com/api/v2/summary.json` | 2 min |
| 30-day incident strip, last incident | `https://status.claude.com/api/v2/incidents.json` (last 50) | 10 min |
| Claude Code version, release strip, first change | `https://api.github.com/repos/anthropics/claude-code/releases` | 15 min |
| Plan limits (5 hours, 7 days, Fable weekly) | `https://api.anthropic.com/api/oauth/usage` (undocumented, used by Claude Code's `/usage`) with a long-lived token from `claude setup-token`, set via `homelab/set-usage-token.sh` | 5 min |

Not available without the Mac: what a session is doing (thinking, tools), its tokens and cost.

## Modes and button map

Modes cycle in this order: **Clock → Claude → Countdown → Music → Clock**. A running countdown keeps running in the background.

| Button | Tap | Hold (~0.6 s) | Long hold (~2 s) |
|---|---|---|---|
| MODE | Next mode | Home (Clock) | Settings menu |
| POWER | Screens + backlights on/off | Backlight on/off only (screens stay) | — |
| LEFT / RIGHT | Mode-specific (below) | Mode-specific | — |

| Mode | LEFT tap | RIGHT tap | LEFT hold | RIGHT hold |
|---|---|---|---|---|
| Clock | previous clock face | next clock face | — | — |
| Countdown (stopped) | − time | + time | clear to 00:00 | start |
| Countdown (running / paused) | — | — | reset to the set time | pause / resume |
| Countdown (alarm at zero) | dismiss | dismiss | dismiss | dismiss |
| Claude | poke Clawd | new verb | — | — |
| Music | previous track | next track | — | play / pause |
| Settings menu | value − | value + | — | — |

In settings, MODE tap moves to the next item and POWER tap leaves (instead of their global meaning). Countdown time steps: 1 min up to 10 min, 5 min up to 1 h, 15 min up to 3 h, then 1 h (max 24 h). Taps rather than auto-repeat, because hold is taken by start/reset.

For testing without touching the clock, the same gestures can be sent over the USB serial port: `l m r p` = tap, `L M R P` = hold, `S` = MODE long hold.

## Mode designs

### Clock
Existing behaviour, ported into the mode framework: HH:MM:SS nixie, 12/24 h, night dimming, NTP + RTC.

### Countdown — "Oppenheimer"
- Layout `HH MM SS` (up to 24 h, rounded up like a kitchen timer); in the **last minute** it switches to `00 SS cc` (seconds and hundredths).
- Outside the last minute the tubes change once per second, exactly like the clock. In the last minute the two hundredths tubes use `FastDigits`: the 10 digits of the face cached in RAM at half resolution (8-bit palette indices, ~87 KB, built ~0.35 s at 1:05 so the switch is seamless, freed when the countdown stops or is left), pixel-doubled while pushing. Measured: **~40 fps** on the hundredths ones tube.
- Why: a nixie image from LittleFS costs ~41 ms to decode plus ~14 ms to push (issue #10), far too slow for 100 changes per second; at that rate the lost detail is invisible.
- Last 10 s: the backlights breathe softly in the clock's own backlight color, one cycle per second, brightest on each second change. At zero the tubes blink `00 00 00` for 10 s over a steady backlight in that color (any button dismisses). Red and fast strobing were dropped: too harsh to look at on a desk.
- Remote start/pause/reset over MQTT and HTTP (see below); an iPhone Shortcut starts the phone timer and the clock countdown together.
- Runs in the background of other modes with a draining backlight progress bar (breathing while paused), and pops up when it reaches zero.

### Claude
All pixel, no nixie images, from public data (OnlineData.h). Tubes from the left:

1. **Clawd** — an orange block with eyes, stub arms and legs (`src/Clawd.cpp`, 15 x 16 grid of 9 px cells, only changed cells redrawn). Idle it blinks, looks around, waves, types on a little keyboard and hops. LEFT tap pokes it (hop + wave); it glances at tube 2 whenever the word changes; sweats and glances around while an incident is open; sleeps with drifting z's during night dimming.
2. **Verb** — the CLI spinner `· ✢ ✳ ✶ ✻ ✽` above a Claude Code spinner verb ("Combobulating…", new one every 9 s or on RIGHT tap) with animated dots and an elapsed `(12s)`. During an incident it shows what Anthropic is doing instead ("Investigating", "Fixing", "Monitoring") in amber with the incident's age.
3. **Time** — `HH` over `MM` in big pixel digits, blinking colon, weekday and date, seconds bar.
4. **Status** — check / `!` icon with ALL GOOD / MINOR ISSUE / MAJOR ISSUE / OUTAGE, three plan-limit bars (5H, 7D, FABLE: percent, bar, time to reset; amber from 80 %, red from 95 %), and one square per component.
5. **30 days** — one bar per day, height and color by the worst incident (today on the right); below, the open incident or the last one with its age.
6. **Claude Code** — latest version and age, releases per day for 14 days, the first line of the changelog.

Palette: a true orange `#F5781E` (the brand's salmon looks pink on these IPS panels), dim orange for captions, amber for attention, red only for outages. Backlights are left alone. Desktop preview of the real drawing code: `tools/preview/run.sh` (PNG per scene: normal, typing, incident, night).

### Music
- Song position `MM SS` in nixie (tubes 3–6, interpolated locally), track/artist scrolling in the orange pixel theme (tubes 1–2), progress shown on the backlights.
- Source: any macOS Now Playing client. Needs a spike: since macOS 15.4 the private MediaRemote framework is restricted for third-party processes (the `mediaremote-adapter` / `media-control` project works around this).

### Idle / screensaver (idea)
Claude CLI-style pixel animation across the six tubes when nothing is happening.

## MQTT contract

All v1. The feeder republishes on change and at least hourly; the clock marks status older than 2 h as STALE.

Prefix: `tubehax/`. JSON payloads, UTF-8. Timestamps are Unix epoch seconds (float allowed).

| Topic | Dir | Retained | Payload |
|---|---|---|---|
| `tubehax/online/status` | feeds → clock | yes | `{"v":1,"indicator":"none\|minor\|major\|critical\|maintenance","description","components":[[name,status]…],"incident":{"name","impact","status","started"}\|null,"days":[30 × 0..3],"last_incident":{"name","at"}\|null,"fetched_at"}` (~420 B) |
| `tubehax/online/release` | feeds → clock | yes | `{"v":1,"version","published_at","days":[14 × count],"notes":[…],"fetched_at"}` (~300 B) |
| `tubehax/online/usage` | feeds → clock (with a token) | yes | `{"five_hour":{"pct","resets_at"},"seven_day":{…},"model_week":{"name","pct","resets_at"},"fetched_at"}` |
| `tubehax/clock/<id>/state` | clock → broker | yes | see [clock contract](#mqtt-contract-v1-clock) |
| `tubehax/clock/<id>/cmd` | any → clock | no | see [clock contract](#mqtt-contract-v1-clock) |
| `elekstubehax/<id>/status` | clock → broker | yes (LWT) | `online` / `offline` (Home Assistant availability topic, reused) |

`<id>` is the existing `elekstubeg2-XXYYZZ` (last 3 MAC bytes).

## MQTT contract v1 (clock)

Implemented in `src/Remote.cpp`. The same JSON commands are accepted by the clock's HTTP API (`POST /api/cmd`).

**Command** → `tubehax/clock/<id>/cmd` (all fields optional, combinable):

```json
{"mode": "clock|countdown|claude",
 "countdown": {"start": 1500, "set": 300, "action": "start|pause|resume|toggle|reset"},
 "displays": true, "backlights": false,
 "settings": {"twelve_hour": false, "blank_hours_zero": false, "utc_offset_min": 420,
              "dimming": true, "night_start": 22, "day_start": 7, "night_backlight": 1, "night_tft": 20,
              "backlight_intensity": 6, "backlight_hue": 26}}
```

`start` sets the duration in seconds and starts right away (and shows the countdown); `set` only sets it (stopped); `toggle` starts, pauses or resumes. Unknown modes or actions are rejected (HTTP 400, logged on serial for MQTT). Settings are range-checked (`night_backlight` 0–7, `night_tft` 0–255, hours 0–23, `utc_offset_min` −720…840, `backlight_hue` 0–359), applied at once and saved to NVS 2 s after the last change. The state echoes them under `"settings"` (with `backlight_rgb` instead of the hue). Not yet remote: broker host/credentials.

**State** ← `tubehax/clock/<id>/state`, retained, published on every change and every 30 s:

```json
{"v": 1, "mode": "countdown", "displays": true, "backlights": true,
 "countdown": {"state": "stopped|running|paused|done", "set": 1500, "remaining": 1234.56, "ends_at": 1792582495},
 "ip": "192.168.1.2", "rssi": -48, "ch": 1, "fw": "1.3.13", "uptime": 3600, "heap": 150000}
```

`remaining` is exact when the message is received live; `ends_at` (UTC epoch, only while running) is for retained copies read later. Clients extrapolate between messages.

**HTTP shortcuts** (for iOS Shortcuts and curl): `GET /api/state`, `GET /api/countdown?minutes=|seconds=|action=`, `GET /api/mode?name=`.

## Connectivity requirements

- **Done:** all networking (WiFi reconnect, MQTT, NTP queries, geolocation, OTA server) runs in its own FreeRTOS task on core 0 (`NetTask`); the main loop on core 1 only handles buttons, modes, displays and backlights, so network trouble can't freeze the tubes. Verified by stopping the broker for 60 s during a countdown: no stalls, 40 fps, automatic reconnect. A display mutex lets the OTA progress screen take over the tubes.
- "NO WiFi" / "NO MQTT" warnings appear only after a link has been down for over a minute.
- Done: MQTT reconnects back off exponentially (1 s doubling to 60 s, plus up to 25 % jitter).
- Not split into more tasks: a second network task's stack (~9 KB of heap) made the countdown's hundredths cache land in a heap layout where drawing fell from 40 to 1 fps. Heap is tight (~150 KB free, ~60 KB during the last minute); measure fps after any change that grows RAM use.
- The clock's 2.4 GHz link loses 10–80 % of pings while other hosts on the LAN are clean (RSSI −48 dBm, channel 1, modem sleep off). Not fixed by lower TX power. Everything tolerates it; commands may arrive a few seconds late.
- LWT + retained `status` so the broker and the admin page know when the clock is offline.
- Re-subscribe on every reconnect; retained topics restore the state.
- Broker host/port/credentials changeable at runtime (stored in NVS) without reflashing; optional mDNS lookup of the homelab broker.
- Small unobtrusive "link" indicator (pixel icon) when the broker is unreachable; time display never depends on the network.

## Homelab

The homelab (`fuisl`, Ubuntu 24.04, LAN `192.168.1.13`, NetBird `100.82.196.126`) hosts everything server-side as one Docker Compose stack in [`homelab/`](../homelab/), deployed with `homelab/deploy.sh`.

| Service | Port | Notes |
|---|---|---|
| Mosquitto 2.0 | 1883 (MQTT), 9001 (WebSockets) | Password auth, no anonymous access; users `clock`, `feeds`, `admin`. Persistence on, so retained state survives restarts. 64 MB memory cap. |
| Admin web | served by Mosquitto on 9001 (stage 1) | See below. |

- LAN only for now: the clock connects to `192.168.1.13:1883`. `fuisl` resolves through NetBird on the Mac; other LAN devices use `fuisl.local` or the IP.
- Passwords are generated per client, kept on the homelab in `homelab/mosquitto/passwd` (not in git) and on the Mac in `~/.config/tubehax/broker.env` (mode 600). Add or rotate with `homelab/add-user.sh <user>`.

## Admin web

A small control page at `http://fuisl:9001/` for operating the clock from any browser — the clock's "admin panel".

**Minimal resources first.** Stage 1 has no backend at all: a static single-page app (vanilla JS + MQTT.js, no build step) served by Mosquitto's own WebSocket listener (`http_dir`). It logs in as the `admin` MQTT user and speaks the same MQTT contract as everything else, so it costs zero extra containers or RAM. A backend (a tiny Go service, ~10 MB RAM) is added only when something needs server-side state (schedules, history, firmware hosting).

Features, roughly in order:

1. Live clock status: online/offline (LWT), current mode, WiFi RSSI, IP, firmware version, uptime.
2. Switch mode; set / start / pause / reset the countdown; trigger Claude or Music views.
3. Display + backlight: on/off, brightness, night-dimming window, backlight color/pattern.
4. Claude status, plan limits (once connected), last incident and Claude Code version from the feeds; live MQTT log.
5. Settings that used to require a reflash: timezone, 12/24 h, backlight, night dimming (broker host still open).
6. Firmware: upload a `.bin`, the clock pulls it over HTTP (OTA) — needs the stage-2 backend or an HTTP endpoint on the clock.

## Flash budget

Today: one 1.19 MB app partition (83 % used, no OTA) and 2.69 MB of LittleFS holding seven clock-face sets (1.5 MB). Plan:

| Partition | Size |
|---|---|
| nvs + otadata | 28 KB |
| app0 (ota_0) | 1.5 MB |
| app1 (ota_1) | 1.5 MB |
| littlefs | ~0.9 MB — our nixie face (~210 KB), pixel font, sprites |
| coredump | 64 KB |

This enables over-the-air updates so the clock no longer needs a USB cable for development.

## Repository layout (target)

```
src/, include/        firmware (single target: EleksTube_Gen2)
src/apps/             one file per mode (Clock, Countdown, Claude, Music, Settings)
src/render/           NixieRenderer, PixelRenderer (Claude theme)
data/                 LittleFS assets: our clock face + pixel assets
homelab/              Mosquitto config, admin page (web/), feeds service (feeds/), deploy scripts
tools/preview/        desktop render of Claude mode from the firmware's drawing code
docs/DESIGN.md        this file
```
