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
| Countdown (stopped) | −1 min (auto-repeat, accelerating) | +1 min | reset | start |
| Countdown (running) | — | — | reset | pause / resume |
| Claude | previous page | next page | — | pin page (disable auto-switch) |
| Music | previous track | next track | — | play / pause |
| Settings menu | value − | value + | — | — |

## Mode designs

### Clock
Existing behaviour, ported into the mode framework: HH:MM:SS nixie, 12/24 h, night dimming, NTP + RTC.

### Countdown — "Oppenheimer"
- Layout `MM SS cc` (minutes, seconds, hundredths), max 99:59.99.
- Minutes and seconds are crisp nixie images; the two hundredths tubes redraw as fast as the bus allows (target ≥ 30 fps) using a motion-blurred/streaked digit rendered from code rather than loaded from flash.
- Tension: in the last 10 s the backlights pulse faster and shift orange → red; at zero all tubes flash `00 00 00`, backlights strobe, and an MQTT `countdown/done` event is published.
- Backlights show progress while the countdown runs in the background of another mode.

### Claude
Pages (LEFT/RIGHT):
1. **Live** — tube 1: animated spinner glyph in orange (static `✻` when idle, amber `?` when waiting for permission); tube 2: state word / model in pixel font; tubes 3–6: active work timer `MM SS` in nixie.
2. **Plan window** — time until the 5-hour window resets as `H MM SS` nixie; backlights show usage % (green → orange → red). A second press shows the 7-day window.
3. **Context** — context used %, input/output tokens, rendered in the orange pixel theme with a gauge.
4. **Session** — elapsed session time `HH MM SS` nixie, cost in USD, lines +/−.

Auto-switch (toggleable): the clock jumps to Claude/Live when Claude starts working, needs permission, or finishes — and returns to the previous mode after a timeout.

### Music
- Song position `MM SS` in nixie (tubes 3–6, interpolated locally), track/artist scrolling in the orange pixel theme (tubes 1–2), progress shown on the backlights.
- Source: any macOS Now Playing client. Needs a spike: since macOS 15.4 the private MediaRemote framework is restricted for third-party processes (the `mediaremote-adapter` / `media-control` project works around this).

### Idle / screensaver (idea)
Claude CLI-style pixel animation across the six tubes when nothing is happening.

## MQTT contract (draft v0)

Prefix: `tubehax/`. JSON payloads, UTF-8. Timestamps are Unix epoch seconds (float allowed).

| Topic | Dir | Retained | Payload |
|---|---|---|---|
| `tubehax/claude/state` | bridge → clock | yes | `{"session_id","state":"idle\|working\|tool\|waiting\|done","tool","since","model","project"}` |
| `tubehax/claude/metrics` | bridge → clock | yes | `{"session_id","ctx_pct","tokens_in","tokens_out","cost_usd","session_ms","lines_added","lines_removed","sampled_at"}` |
| `tubehax/claude/limits` | bridge → clock | yes | `{"five_hour":{"pct","resets_at"},"seven_day":{"pct","resets_at"},"sampled_at"}` |
| `tubehax/music/now` | bridge → clock | yes | `{"playing","title","artist","position","duration","sampled_at","rate"}` |
| `tubehax/mac/focus` | bridge → clock | no | `{"app","is_claude_terminal"}` |
| `tubehax/music/cmd` | clock → bridge | no | `"next" \| "prev" \| "toggle"` |
| `tubehax/clock/<id>/status` | clock → broker | yes (LWT `offline`) | `{"online","mode","ip","rssi","fw"}` |
| `tubehax/clock/<id>/countdown` | clock → broker | no | `{"event":"started\|paused\|done","remaining"}` |
| `tubehax/clock/<id>/cmd` | any → clock | no | `{"mode":"clock\|claude\|countdown\|music"}`, `{"countdown":{"set":300,"start":true}}` |

`<id>` is the existing `elekstubeg2-XXYYZZ` (last 3 MAC bytes).

## Connectivity requirements

- WiFi and MQTT reconnect independently, with exponential backoff (1 s → 60 s cap, jitter), never blocking the render loop.
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
