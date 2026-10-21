"""Turns hook events and status line samples into the clock's MQTT topics.

Several Claude Code sessions can run at once; the clock shows the one with the most recent activity.
Topics and fields: docs/DESIGN.md#mqtt-contract (tubehax/claude/state|metrics|limits).
"""

from __future__ import annotations

import json
import os
import re
import time
from dataclasses import dataclass, field
from datetime import datetime

STALE_AFTER_S = 2 * 3600  # Forget sessions without any event or sample for this long (crashed, closed)

WAITING_NOTIFICATIONS = {"permission_prompt", "elicitation_dialog", "agent_needs_input"}
STATUSLINE_FRESH_S = 120  # Status line samples newer than this win over the transcript fallback


def pretty_model(model_id: str) -> str:
    """claude-opus-5-5 -> Opus 5.5 (the status line gives display names; transcripts only ids)."""
    m = re.match(r"claude-([a-z]+)-(\d+)(?:-(\d{1,2}))?(?!\d)", model_id or "")
    if not m:
        return model_id or ""
    return f"{m.group(1).capitalize()} {m.group(2)}" + (f".{m.group(3)}" if m.group(3) else "")


@dataclass
class Transcript:
    """Token usage read incrementally from the session transcript (JSONL). Used when no status line
    runs, e.g. in the VS Code panel: context, output tokens, model and session time; no cost or limits."""

    path: str
    offset: int = 0
    first_ts: float = 0.0
    model_id: str = ""
    ctx_tokens: int = 0
    out_by_message: dict = field(default_factory=dict)  # Streaming writes one entry per block: dedupe by id

    def update(self) -> None:
        try:
            with open(self.path, "rb") as f:
                f.seek(self.offset)
                chunk = f.read()
        except OSError:
            return
        end = chunk.rfind(b"\n") + 1  # Only complete lines; the rest is read next time
        self.offset += end
        for line in chunk[:end].splitlines():
            try:
                entry = json.loads(line)
            except ValueError:
                continue
            if not self.first_ts and entry.get("timestamp"):
                self.first_ts = datetime.fromisoformat(entry["timestamp"].replace("Z", "+00:00")).timestamp()
            message = entry.get("message") if entry.get("type") == "assistant" else None
            usage = message.get("usage") if isinstance(message, dict) else None
            if not usage:
                continue
            self.model_id = message.get("model") or self.model_id
            self.ctx_tokens = sum(usage.get(k) or 0 for k in
                                  ("input_tokens", "cache_creation_input_tokens", "cache_read_input_tokens"))
            self.out_by_message[message.get("id") or len(self.out_by_message)] = usage.get("output_tokens") or 0

    def metrics(self, t: float) -> dict:
        size = 1_000_000 if self.ctx_tokens > 200_000 or self.model_id.endswith("[1m]") else 200_000
        return {
            "ctx_pct": round(100 * self.ctx_tokens / size, 1),
            "ctx_size": size,
            "tokens_in": self.ctx_tokens,
            "tokens_out": sum(self.out_by_message.values()),
            "cost_usd": None,
            "duration_ms": int((t - self.first_ts) * 1000) if self.first_ts else 0,
            "lines_added": None,
            "lines_removed": None,
            "sampled_at": round(t, 1),
        }


@dataclass
class Session:
    session_id: str
    state: str = "idle"  # idle | working | waiting | done
    tool: str = ""
    since: float = 0.0
    work_started: float = 0.0
    work_ended: float = 0.0
    model: str = ""
    project: str = ""
    last_activity: float = 0.0  # Last hook event (or first status sample): picks the active session
    last_seen: float = 0.0  # Last hook or status sample: expiry
    metrics: dict = field(default_factory=dict)
    status_at: float = 0.0  # Last status line sample
    transcript: Transcript | None = None

    def set_state(self, state: str, t: float) -> None:
        if state != self.state:
            self.state = state
            self.since = t


class Tracker:
    def __init__(self) -> None:
        self.sessions: dict[str, Session] = {}
        self.limits: dict | None = None
        self.last_model = ""
        self.last_project = ""

    def _session(self, data: dict, t: float) -> Session | None:
        sid = data.get("session_id")
        if not sid:
            return None
        session = self.sessions.get(sid)
        if session is None:
            session = self.sessions[sid] = Session(sid, since=t, last_activity=t)
        session.last_seen = t
        cwd = data.get("cwd") or (data.get("workspace") or {}).get("current_dir")
        if cwd:
            session.project = os.path.basename(cwd.rstrip("/"))
        return session

    def hook(self, data: dict, t: float) -> None:
        event = data.get("hook_event_name", "")
        if event == "SessionEnd":
            self.sessions.pop(data.get("session_id", ""), None)
            return
        session = self._session(data, t)
        if session is None:
            return
        session.last_activity = t
        path = data.get("transcript_path")
        if path and (session.transcript is None or session.transcript.path != path):
            session.transcript = Transcript(path)
        if session.transcript and event in ("SessionStart", "UserPromptSubmit", "PostToolUse", "Stop") \
                and t - session.status_at > STATUSLINE_FRESH_S:
            session.transcript.update()
            session.metrics = session.transcript.metrics(t)
            session.model = session.model or pretty_model(session.transcript.model_id)
        if event == "SessionStart":
            model = data.get("model")
            if isinstance(model, dict):
                model = model.get("display_name") or model.get("id")
            if isinstance(model, str) and model and not session.model:
                session.model = model
        elif event == "UserPromptSubmit":
            session.set_state("working", t)
            session.work_started, session.work_ended, session.tool = t, 0.0, ""
        elif event == "PreToolUse":
            session.set_state("working", t)
            session.tool = data.get("tool_name", "")
            if not session.work_started:
                session.work_started = t
        elif event in ("PostToolUse", "PostToolUseFailure"):
            if session.state == "waiting":
                session.set_state("working", t)
            session.tool = ""
        elif event == "PermissionRequest":
            session.set_state("waiting", t)
        elif event == "Notification" and data.get("notification_type") in WAITING_NOTIFICATIONS:
            session.set_state("waiting", t)
        elif event in ("Stop", "StopFailure"):
            session.set_state("done", t)
            session.tool = ""
            if session.work_started:
                session.work_ended = t

    def status(self, data: dict, t: float) -> None:
        session = self._session(data, t)
        if session is None:
            return
        session.status_at = t
        model = (data.get("model") or {}).get("display_name")
        if model:
            session.model = model
        ctx = data.get("context_window") or {}
        cost = data.get("cost") or {}
        session.metrics = {
            "ctx_pct": ctx.get("used_percentage") or 0,
            "ctx_size": ctx.get("context_window_size") or 0,
            "tokens_in": ctx.get("total_input_tokens") or 0,
            "tokens_out": ctx.get("total_output_tokens") or 0,
            "cost_usd": round(cost.get("total_cost_usd") or 0, 4),
            "duration_ms": cost.get("total_duration_ms") or 0,
            "lines_added": cost.get("total_lines_added") or 0,
            "lines_removed": cost.get("total_lines_removed") or 0,
            "sampled_at": round(t, 1),
        }
        limits = data.get("rate_limits")
        if isinstance(limits, dict) and limits:
            self.limits = {
                key: {"pct": w.get("used_percentage"), "resets_at": w.get("resets_at")}
                for key in ("five_hour", "seven_day")
                if isinstance(w := limits.get(key), dict)
            }
            self.limits["sampled_at"] = round(t, 1)

    def expire(self, t: float) -> None:
        for sid in [s for s, session in self.sessions.items() if t - session.last_seen > STALE_AFTER_S]:
            del self.sessions[sid]

    def active(self) -> Session | None:
        return max(self.sessions.values(), key=lambda s: s.last_activity, default=None)

    def topics(self) -> dict[str, dict]:
        """Current payloads for the retained topics."""
        session = self.active()
        if session:
            self.last_model = session.model or self.last_model
            self.last_project = session.project or self.last_project
        state = {
            "v": 1,
            "state": session.state if session else "idle",
            "tool": session.tool if session else "",
            "model": self.last_model,
            "project": self.last_project,
            "since": round(session.since, 1) if session else 0,
            "work_started": round(session.work_started, 1) if session else 0,
            "work_ended": round(session.work_ended, 1) if session else 0,
            "sessions": len(self.sessions),
            "session_id": session.session_id if session else "",
        }
        topics = {"tubehax/claude/state": state}
        if session and session.metrics:
            topics["tubehax/claude/metrics"] = {
                "v": 1, "session_id": session.session_id, "model": session.model, "project": session.project,
                **session.metrics,
            }
        if self.limits:
            topics["tubehax/claude/limits"] = {"v": 1, **self.limits}
        return topics


def now() -> float:
    return time.time()
