#!/usr/bin/env python3
"""Hand the feeds container only the short-lived access token of the homelab's Claude Code login.

The login (~/.claude/.credentials.json) also holds the long-lived refresh token; the container never sees
it. Run from cron every few minutes (deploy.sh installs the entry):

  ~/.claude/.credentials.json  ->  ~/tubehax/secrets/claude-usage.json  (accessToken + expiresAt, mode 600)

When the access token is about to expire, Claude Code itself is asked to renew the login (RENEW below), so
renewal stays inside Claude Code and nothing here talks to Anthropic.
"""
import json
import os
import subprocess
import time
from pathlib import Path

HOME = Path.home()
SOURCE = HOME / ".claude" / ".credentials.json"
TARGET = HOME / "tubehax" / "secrets" / "claude-usage.json"
CLAUDE = HOME / ".local" / "bin" / "claude"
RENEW = [str(CLAUDE), "-p", "/usage"]  # Renews an expiring login; no model call ("auth status" does not renew)
RENEW_BEFORE_S = 15 * 60


def read_login() -> dict:
    return json.loads(SOURCE.read_text())["claudeAiOauth"]


def main() -> None:
    oauth = read_login()
    if oauth["expiresAt"] / 1000 < time.time() + RENEW_BEFORE_S:
        subprocess.run(RENEW, capture_output=True, timeout=120, cwd="/tmp")
        oauth = read_login()
    TARGET.parent.mkdir(mode=0o700, exist_ok=True)
    tmp = TARGET.with_suffix(".tmp")
    fd = os.open(tmp, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(fd, "w") as f:
        json.dump({"claudeAiOauth": {"accessToken": oauth["accessToken"], "expiresAt": oauth["expiresAt"]}}, f)
    tmp.replace(TARGET)  # Atomic; the container mounts the folder, so it sees the new file


if __name__ == "__main__":
    main()
