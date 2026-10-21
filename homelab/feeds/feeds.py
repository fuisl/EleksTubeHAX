"""TubeHAX online feeds: public Claude information, summarized for the clock.

Fetches the Anthropic status page and the Claude Code releases on GitHub, condenses them into small
JSON documents the ESP32 can parse, and publishes them retained:

  tubehax/online/status   overall state, components, active incident, 30-day incident strip
  tubehax/online/release  latest Claude Code version, 14-day release strip, first changelog lines

Runs next to Mosquitto on the homelab (docker-compose.yml). Contract: docs/DESIGN.md#mqtt-contract.
"""

import json
import logging
import os
import re
import time
import urllib.request
from datetime import datetime, timedelta, timezone

import paho.mqtt.client as mqtt

STATUS_URL = "https://status.claude.com/api/v2/summary.json"
INCIDENTS_URL = "https://status.claude.com/api/v2/incidents.json"
RELEASES_URL = "https://api.github.com/repos/anthropics/claude-code/releases?per_page=40"

STATUS_EVERY_S = 120
HISTORY_EVERY_S = 600
RELEASES_EVERY_S = 900  # GitHub allows 60 unauthenticated requests per hour

# Short names that fit a 135 px wide tube; anything else keeps its first word.
COMPONENT_NAMES = {
    "claude.ai": "claude.ai",
    "Claude API (api.anthropic.com)": "API",
    "Claude Code": "Code",
    "Claude Console (platform.claude.com)": "Console",
    "Claude Cowork": "Cowork",
    "Claude for Government": "Gov",
}
IMPACT = {"none": 0, "minor": 1, "major": 2, "critical": 3, "maintenance": 0}

log = logging.getLogger("feeds")


def fetch(url: str):
    request = urllib.request.Request(url, headers={"User-Agent": "tubehax-feeds", "Accept": "application/json"})
    with urllib.request.urlopen(request, timeout=20) as response:
        return json.load(response)


def epoch(iso: str | None) -> int:
    if not iso:
        return 0
    return int(datetime.fromisoformat(iso.replace("Z", "+00:00")).timestamp())


def day_strip(timestamps_with_value: list[tuple[int, int]], days: int) -> list[int]:
    """Worst value per UTC day, oldest first, ending today."""
    today = datetime.now(timezone.utc).date()
    strip = [0] * days
    for ts, value in timestamps_with_value:
        age = (today - datetime.fromtimestamp(ts, timezone.utc).date()).days
        if 0 <= age < days:
            strip[days - 1 - age] = max(strip[days - 1 - age], value)
    return strip


def build_status(summary: dict, incidents: list[dict]) -> dict:
    components = [
        [COMPONENT_NAMES.get(c["name"], c["name"].split()[0]), c["status"]]
        for c in summary.get("components", [])
        if not c.get("group")
    ]
    active = summary.get("incidents") or []
    incident = None
    if active:
        worst = max(active, key=lambda i: IMPACT.get(i.get("impact"), 0))
        incident = {
            "name": worst["name"][:90],
            "impact": worst.get("impact"),
            "status": worst.get("status"),
            "started": epoch(worst.get("created_at")),
        }
    real = [i for i in incidents if IMPACT.get(i.get("impact"), 0) > 0]
    last = max(real, key=lambda i: i["created_at"], default=None)
    return {
        "v": 1,
        "indicator": summary["status"]["indicator"],
        "description": summary["status"]["description"],
        "components": components,
        "incident": incident,
        "days": day_strip([(epoch(i["created_at"]), IMPACT.get(i["impact"], 0)) for i in real], 30),
        "last_incident": {"name": last["name"][:90], "at": epoch(last["created_at"])} if last else None,
        "fetched_at": int(time.time()),
    }


def first_notes(body: str, count: int = 3) -> list[str]:
    notes = []
    for line in (body or "").splitlines():
        line = line.strip()
        if line.startswith(("- ", "* ")):
            text = re.sub(r"[`*_]", "", line[2:]).strip()
            notes.append(text[:80])
        if len(notes) == count:
            break
    return notes


def build_release(releases: list[dict]) -> dict:
    published = [r for r in releases if not r.get("draft") and not r.get("prerelease")]
    latest = published[0]
    return {
        "v": 1,
        "version": latest["tag_name"].lstrip("v"),
        "published_at": epoch(latest["published_at"]),
        "days": release_counts(published, 14),
        "notes": first_notes(latest.get("body")),
        "fetched_at": int(time.time()),
    }


def release_counts(releases: list[dict], days: int) -> list[int]:
    today = datetime.now(timezone.utc).date()
    counts = [0] * days
    for r in releases:
        age = (today - datetime.fromtimestamp(epoch(r["published_at"]), timezone.utc).date()).days
        if 0 <= age < days:
            counts[days - 1 - age] += 1
    return counts


def main() -> None:
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="tubehax-feeds")
    client.username_pw_set(os.environ["MQTT_USER"], os.environ["MQTT_PASSWORD"])
    client.reconnect_delay_set(1, 60)
    client.connect_async(os.environ.get("MQTT_HOST", "mosquitto"), int(os.environ.get("MQTT_PORT", 1883)))
    client.loop_start()

    published: dict[str, tuple[str, float]] = {}  # topic -> (payload without fetched_at, publish time)
    incidents: list[dict] = []
    next_status = next_history = next_release = 0.0

    def publish(topic: str, payload: dict) -> None:
        text = json.dumps(payload, separators=(",", ":"))
        # fetched_at alone changing is no news; republish anyway every hour so the clock sees freshness.
        key = re.sub(r'"fetched_at":\d+', "", text)
        last_key, last_time = published.get(topic, ("", 0.0))
        if key != last_key or time.time() - last_time > 3600:
            client.publish(topic, text, qos=1, retain=True)
            published[topic] = (key, time.time())
            log.info("%s %d bytes", topic, len(text))

    while True:
        now = time.time()
        try:
            if now >= next_history:
                incidents = fetch(INCIDENTS_URL).get("incidents", [])
                next_history = now + HISTORY_EVERY_S
            if now >= next_status:
                publish("tubehax/online/status", build_status(fetch(STATUS_URL), incidents))
                next_status = now + STATUS_EVERY_S
        except Exception as e:  # Network or format trouble: keep the last retained copy, retry soon
            log.warning("status: %s", e)
            next_status = now + 60
        try:
            if now >= next_release:
                publish("tubehax/online/release", build_release(fetch(RELEASES_URL)))
                next_release = now + RELEASES_EVERY_S
        except Exception as e:
            log.warning("releases: %s", e)
            next_release = now + 300
        time.sleep(5)


if __name__ == "__main__":
    main()
