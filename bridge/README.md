# tubehax-bridge

Mac daemon that turns Claude Code activity into retained MQTT topics for the clock's Claude mode.

```
Claude Code ── hooks (async) ──┐
            └─ status line ────┤ ~/.config/tubehax/bridge.sock ─▶ tubehax-bridge ─▶ broker ─▶ clock
                               └ (transcript, as a fallback for tokens)
```

## Install

```sh
uv tool install --editable ./bridge                 # puts tubehax-bridge in ~/.local/bin
~/.local/bin/tubehax-bridge install                 # launchd agent + hooks + status line
~/.local/bin/tubehax-bridge status                  # agent state, socket, last log lines
~/.local/bin/tubehax-bridge uninstall               # removes all of it again
```

`install` backs up `~/.claude/settings.json` before changing it, only adds its own entries (recognized by their
command) and keeps an existing status line running behind its own. Hooks are `"async": true`, so they never slow
Claude down; if the daemon isn't running they do nothing. Hook edits apply to running sessions right away.

Broker credentials come from `~/.config/tubehax/broker.env` (`TUBEHAX_BROKER_HOST`, `TUBEHAX_BRIDGE_PASSWORD`,
optional `TUBEHAX_BROKER_HOST_ALT`). Log: `~/Library/Logs/tubehax-bridge.log`.

## What it publishes

| Topic | From | Content |
|---|---|---|
| `tubehax/claude/state` | hooks | `state` idle / working / waiting / done, `tool`, `model`, `project`, `work_started`, `work_ended`, `sessions` |
| `tubehax/claude/metrics` | status line, else transcript | context %, window size, tokens in/out, session time; cost and lines only from the status line |
| `tubehax/claude/limits` | status line only | 5-hour and 7-day usage % and reset times (Pro/Max) |
| `tubehax/bridge/status` | daemon (LWT) | `online` / `offline` |

With several sessions open, the one with the most recent hook event is shown.

## Things to know

- **Plan limits need the terminal CLI.** `rate_limits` exist only in the status line JSON, and the VS Code panel
  doesn't run the status line. Any CLI session (`claude` in a terminal) refreshes them; they stay retained until
  they reset.
- **Transcript fallback.** Without a status line, context and token counts are read from the session transcript.
  Its format is internal to Claude Code and may change; unreadable lines are skipped.
- **macOS Local Network privacy** blocks LAN addresses for launchd agents ("No route to host"). The bridge tries
  `TUBEHAX_BROKER_HOST` first and falls back to `TUBEHAX_BROKER_HOST_ALT` (the homelab's NetBird address).
