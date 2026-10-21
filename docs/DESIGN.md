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
 Mac (your machine)                                Homelab               Clock
┌───────────────────────────────────────┐        ┌──────────┐        ┌────────────────┐
│ Claude Code                           │        │          │        │ ESP32 firmware │
│  ├─ statusLine script ──┐             │        │          │        │  AppManager    │
│  └─ async hooks ────────┤ unix socket │  MQTT  │ Mosquitto│  MQTT  │   ├ Clock      │
│                         ▼             ├───────▶│  broker  ├───────▶│   ├ Countdown  │
│ macOS Now Playing ─▶ tubehax-bridge   │◀───────┤ (retain, │◀───────┤   ├ Claude     │
│ frontmost app    ─▶ (Python, uv,      │ cmds   │  LWT)    │ status │   └ Music      │
│                      launchd agent)   │        │          │        │                │
└───────────────────────────────────────┘        └──────────┘        └────────────────┘
```

A browser admin page (any device on the LAN or NetBird) talks to the same broker over MQTT-over-WebSockets — see [Homelab](#homelab) and [Admin web](#admin-web).

Principles:

1. **The clock never talks to the Mac directly.** Everything goes through the broker, so the clock keeps working when the Mac sleeps, and the bridge keeps working when the clock reboots.
2. **Publish facts, not frames.** The bridge sends timestamps (`started_at`, `resets_at`, song `position` + `sampled_at`); the clock interpolates locally with its own RTC/NTP time. Displays tick smoothly with very little MQTT traffic.
3. **State topics are retained** so a freshly (re)connected clock immediately shows the right thing.
4. **Hooks and the status line must never slow Claude down.** They hand data to the bridge over a local socket and exit; hooks run with `"async": true`.

### Claude Code data sources

| Need | Source | Fields |
|---|---|---|
| Working / waiting / done state | async hooks | `UserPromptSubmit` → working, `PreToolUse`/`PostToolUse` → tool activity, `Notification` (`permission_prompt`, `idle_prompt`) → waiting for you, `Stop` → done, `SessionStart`/`SessionEnd` |
| Active work timer | hooks | timestamp of `UserPromptSubmit` until `Stop` |
| Plan usage window | statusLine JSON | `rate_limits.five_hour.{used_percentage,resets_at}`, `rate_limits.seven_day.*` (Pro/Max only — we are on Max; present after the first API response) |
| Context / tokens | statusLine JSON | `context_window.{used_percentage,total_input_tokens,total_output_tokens,context_window_size}` |
| Session elapsed / cost | statusLine JSON | `cost.{total_duration_ms,total_cost_usd,total_lines_added,total_lines_removed}`, `model.display_name` |
| "I switched to the terminal" | bridge (macOS) | Claude Code has **no** focus event; the bridge watches the frontmost app instead |

The status line is re-run on each assistant message (debounced 300 ms; set `refreshInterval` for idle ticking). Multiple concurrent sessions are keyed by `session_id`; the clock shows the most recently active one.

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
| Claude | previous page | next page | — | pin page (disable auto-switch) |
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
Pages (LEFT/RIGHT):
1. **Live** — tube 1: animated spinner glyph in orange (static `✻` when idle, amber `?` when waiting for permission); tube 2: state word / model in pixel font; tubes 3–6: active work timer `MM SS` in nixie.
2. **Plan window** — time until the 5-hour window resets as `H MM SS` nixie; backlights show usage % (green → orange → red). A second press shows the 7-day window.
3. **Context** — context used %, input/output tokens, rendered in the orange pixel theme with a gauge.
4. **Session** — elapsed session time `HH MM SS` nixie, cost in USD, lines +/−.

Implemented (`src/ClaudeApp.cpp`, `src/Pixel.cpp`): the pixel theme draws the 5x7 font as blocks with a 1 px gap (pixel-matrix look), the CLI spinner `· ✢ ✳ ✶ ✻ ✽` on an 11x11 block grid (only changed blocks are redrawn, ~120 ms per frame), and segmented gauges, all dimmed with the night setting. Page 2 shows a 7-day window as `D HH MM`. Plan usage is a backlight bar in the clock's own color rather than green → orange → red (calmer); waiting is steady amber. Without a status line (VS Code panel) the session page shows output tokens instead of cost.

Auto-switch (setting `auto_claude`, on by default): when the clock shows the time and Claude asks for permission or finishes a turn, it switches to Claude/Live, and back after 60 s without news (any button keeps it there).

### Music
- Song position `MM SS` in nixie (tubes 3–6, interpolated locally), track/artist scrolling in the orange pixel theme (tubes 1–2), progress shown on the backlights.
- Source: any macOS Now Playing client. Needs a spike: since macOS 15.4 the private MediaRemote framework is restricted for third-party processes (the `mediaremote-adapter` / `media-control` project works around this).

### Idle / screensaver (idea)
Claude CLI-style pixel animation across the six tubes when nothing is happening.

## MQTT contract

v1 for the clock and Claude topics; music and focus are still drafts.

Prefix: `tubehax/`. JSON payloads, UTF-8. Timestamps are Unix epoch seconds (float allowed).

| Topic | Dir | Retained | Payload |
|---|---|---|---|
| `tubehax/claude/state` | bridge → clock | yes | `{"v":1,"session_id","state":"idle\|working\|waiting\|done","tool","model","project","since","work_started","work_ended","sessions"}` |
| `tubehax/claude/metrics` | bridge → clock | yes | `{"v":1,"session_id","model","project","ctx_pct","ctx_size","tokens_in","tokens_out","cost_usd","duration_ms","lines_added","lines_removed","sampled_at"}` (cost and lines `null` without a status line) |
| `tubehax/claude/limits` | bridge → clock | yes | `{"v":1,"five_hour":{"pct","resets_at"},"seven_day":{"pct","resets_at"},"sampled_at"}` |
| `tubehax/bridge/status` | bridge → broker | yes (LWT) | `online` / `offline` |
| `tubehax/music/now` | bridge → clock | yes | `{"playing","title","artist","position","duration","sampled_at","rate"}` |
| `tubehax/mac/focus` | bridge → clock | no | `{"app","is_claude_terminal"}` |
| `tubehax/music/cmd` | clock → bridge | no | `"next" \| "prev" \| "toggle"` |
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
              "backlight_intensity": 6, "backlight_hue": 26, "auto_claude": true}}
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
- LWT + retained `status` so the broker and the bridge know when the clock is offline.
- Re-subscribe on every reconnect; retained topics restore the state.
- Broker host/port/credentials changeable at runtime (stored in NVS) without reflashing; optional mDNS lookup of the homelab broker.
- Small unobtrusive "link" indicator (pixel icon) when the broker is unreachable; time display never depends on the network.

## Homelab

The homelab (`fuisl`, Ubuntu 24.04, LAN `192.168.1.13`, NetBird `100.82.196.126`) hosts everything server-side as one Docker Compose stack in [`homelab/`](../homelab/), deployed with `homelab/deploy.sh`.

| Service | Port | Notes |
|---|---|---|
| Mosquitto 2.0 | 1883 (MQTT), 9001 (WebSockets) | Password auth, no anonymous access; users `clock`, `bridge`, `admin`. Persistence on, so retained state survives restarts. 64 MB memory cap. |
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
4. Live mirror of what the bridge publishes (Claude state, limits, now playing) for debugging.
5. Settings that today require a reflash: timezone, 12/24 h, broker host, auto-switch rules.
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
bridge/               tubehax-bridge (Python, uv project), launchd plist, Claude Code hook/statusLine scripts
docs/DESIGN.md        this file
```
