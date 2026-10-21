"""Fire-and-forget clients used by Claude Code: hooks and the status line.

They must never slow Claude down or fail it: one short socket write, errors ignored.
Kept free of third-party imports so Python starts fast.
"""

import json
import socket
import sys
import time

from .paths import SOCKET_PATH, ORIGINAL_STATUSLINE


def send(kind: str, data: dict) -> None:
    message = json.dumps({"kind": kind, "t": time.time(), "data": data}).encode()
    try:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
            s.settimeout(0.3)
            s.connect(str(SOCKET_PATH))
            s.sendall(message)
    except OSError:
        pass  # Bridge not running: the clock just misses this update


def read_stdin() -> dict:
    try:
        data = json.loads(sys.stdin.read() or "{}")
        return data if isinstance(data, dict) else {}
    except ValueError:
        return {}


def hook() -> int:
    send("hook", read_stdin())
    return 0


def statusline() -> int:
    raw = sys.stdin.read()
    try:
        data = json.loads(raw or "{}")
    except ValueError:
        data = {}
    send("status", data)

    original = ORIGINAL_STATUSLINE.read_text().strip() if ORIGINAL_STATUSLINE.exists() else ""
    if original:  # The user's own status line keeps working: run it with the same input
        import subprocess

        result = subprocess.run(original, shell=True, input=raw, text=True, capture_output=True, timeout=5)
        sys.stdout.write(result.stdout)
        return 0
    print(format_statusline(data))
    return 0


def format_statusline(data: dict) -> str:
    orange, muted, reset = "\033[38;2;217;119;87m", "\033[38;2;143;138;131m", "\033[0m"
    parts = [f"{orange}✻ {(data.get('model') or {}).get('display_name', 'Claude')}{reset}"]
    ctx = (data.get("context_window") or {}).get("used_percentage")
    if ctx is not None:
        parts.append(f"ctx {ctx:.0f}%")
    limits = data.get("rate_limits") or {}
    for key, label in (("five_hour", "5h"), ("seven_day", "7d")):
        pct = (limits.get(key) or {}).get("used_percentage")
        if pct is not None:
            parts.append(f"{label} {pct:.0f}%")
    cost = (data.get("cost") or {}).get("total_cost_usd")
    if cost is not None:
        parts.append(f"${cost:.2f}")
    return f"{muted} · {reset}".join(parts)
