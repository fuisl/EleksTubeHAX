"""Install / uninstall: Claude Code hooks + status line in ~/.claude/settings.json, and the launchd agent.

Existing settings are kept: our entries are recognized by their command (containing "tubehax-bridge"),
a backup of settings.json is written before every change, and a status line you already had keeps
running behind ours (its command is saved in ~/.config/tubehax/statusline-original).
"""

from __future__ import annotations

import json
import os
import plistlib
import shutil
import subprocess
import sys
import time

from .paths import CLAUDE_SETTINGS, CONFIG_DIR, LAUNCHD_LABEL, LAUNCHD_PLIST, LOG_FILE, ORIGINAL_STATUSLINE

HOOK_EVENTS = [
    "SessionStart", "UserPromptSubmit", "PreToolUse", "PostToolUse", "PostToolUseFailure",
    "PermissionRequest", "Notification", "Stop", "StopFailure", "SessionEnd",
]
MARK = "tubehax-bridge"
STATUSLINE_REFRESH_S = 10  # Keeps idle sessions' timers and limits fresh


def executable() -> str:
    found = shutil.which("tubehax-bridge")
    return found or os.path.abspath(sys.argv[0])


def _load_settings() -> dict:
    if not CLAUDE_SETTINGS.exists():
        return {}
    return json.loads(CLAUDE_SETTINGS.read_text())


def _save_settings(settings: dict) -> None:
    if CLAUDE_SETTINGS.exists():
        backup = CLAUDE_SETTINGS.with_name(f"settings.json.bak-tubehax-{time.strftime('%Y%m%d-%H%M%S')}")
        shutil.copy2(CLAUDE_SETTINGS, backup)
        print(f"backup: {backup}")
    CLAUDE_SETTINGS.parent.mkdir(parents=True, exist_ok=True)
    CLAUDE_SETTINGS.write_text(json.dumps(settings, indent=2) + "\n")


def _remove_our_hooks(settings: dict) -> None:
    hooks = settings.get("hooks") or {}
    for event in list(hooks):
        groups = []
        for group in hooks[event]:
            kept = [h for h in group.get("hooks", []) if MARK not in h.get("command", "")]
            if kept:
                groups.append({**group, "hooks": kept})
        if groups:
            hooks[event] = groups
        else:
            del hooks[event]
    if hooks:
        settings["hooks"] = hooks
    else:
        settings.pop("hooks", None)


def install_claude(exe: str) -> None:
    settings = _load_settings()
    _remove_our_hooks(settings)
    hooks = settings.setdefault("hooks", {})
    for event in HOOK_EVENTS:
        hooks.setdefault(event, []).append({"hooks": [{"type": "command", "command": f"{exe} hook", "async": True}]})

    current = settings.get("statusLine") or {}
    command = current.get("command", "")
    if command and MARK not in command:
        CONFIG_DIR.mkdir(parents=True, exist_ok=True)
        ORIGINAL_STATUSLINE.write_text(command + "\n")
        print(f"your status line keeps running behind ours (saved in {ORIGINAL_STATUSLINE})")
    settings["statusLine"] = {**current, "type": "command", "command": f"{exe} statusline",
                              "refreshInterval": STATUSLINE_REFRESH_S}
    _save_settings(settings)
    print(f"hooks ({len(HOOK_EVENTS)} events, async) and status line installed in {CLAUDE_SETTINGS}")


def uninstall_claude() -> None:
    settings = _load_settings()
    _remove_our_hooks(settings)
    command = (settings.get("statusLine") or {}).get("command", "")
    if MARK in command:
        if ORIGINAL_STATUSLINE.exists():
            settings["statusLine"] = {"type": "command", "command": ORIGINAL_STATUSLINE.read_text().strip()}
            ORIGINAL_STATUSLINE.unlink()
        else:
            settings.pop("statusLine", None)
    _save_settings(settings)
    print(f"hooks and status line removed from {CLAUDE_SETTINGS}")


def _launchctl(*args: str) -> subprocess.CompletedProcess:
    return subprocess.run(["launchctl", *args], capture_output=True, text=True)


def install_agent(exe: str) -> None:
    LAUNCHD_PLIST.parent.mkdir(parents=True, exist_ok=True)
    LOG_FILE.parent.mkdir(parents=True, exist_ok=True)
    plist = {
        "Label": LAUNCHD_LABEL,
        "ProgramArguments": [exe, "run"],
        "RunAtLoad": True,
        "KeepAlive": True,
        "ProcessType": "Background",
        "StandardOutPath": str(LOG_FILE),
        "StandardErrorPath": str(LOG_FILE),
    }
    domain = f"gui/{os.getuid()}"
    _launchctl("bootout", f"{domain}/{LAUNCHD_LABEL}")
    LAUNCHD_PLIST.write_bytes(plistlib.dumps(plist))
    result = _launchctl("bootstrap", domain, str(LAUNCHD_PLIST))
    if result.returncode != 0:
        raise SystemExit(f"launchctl bootstrap failed: {result.stderr.strip()}")
    print(f"launchd agent {LAUNCHD_LABEL} running (log: {LOG_FILE})")


def uninstall_agent() -> None:
    _launchctl("bootout", f"gui/{os.getuid()}/{LAUNCHD_LABEL}")
    LAUNCHD_PLIST.unlink(missing_ok=True)
    print(f"launchd agent {LAUNCHD_LABEL} removed")


def install() -> int:
    exe = executable()
    install_agent(exe)
    install_claude(exe)
    print("New Claude Code sessions report to the clock; running sessions pick up hooks after a restart.")
    return 0


def uninstall() -> int:
    uninstall_claude()
    uninstall_agent()
    return 0
