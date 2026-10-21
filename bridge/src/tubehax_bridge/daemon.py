"""The bridge daemon: unix socket in, MQTT out.

Hooks and the status line connect to the socket, write one JSON message and close. The tracker
turns them into retained topics, published only when they change (and again after every reconnect,
so the broker and the clock always hold the latest state).
"""

from __future__ import annotations

import asyncio
import json
import logging
import os
import signal
import socket
import time
import uuid

import paho.mqtt.client as mqtt

from .paths import BROKER_ENV, SOCKET_PATH
from .tracker import Tracker, now

log = logging.getLogger("tubehax-bridge")

STATUS_TOPIC = "tubehax/bridge/status"


def read_env() -> dict[str, str]:
    env = {}
    for line in BROKER_ENV.read_text().splitlines():
        if "=" in line and not line.lstrip().startswith("#"):
            key, value = line.split("=", 1)
            env[key.strip()] = value.strip().strip('"')
    return env


class Bridge:
    def __init__(self) -> None:
        self.tracker = Tracker()
        self.published: dict[str, str] = {}
        env = read_env()
        self.mqtt = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=f"tubehax-bridge-{uuid.uuid4().hex[:6]}")
        self.mqtt.username_pw_set("bridge", env["TUBEHAX_BRIDGE_PASSWORD"])
        self.mqtt.will_set(STATUS_TOPIC, "offline", qos=1, retain=True)
        self.mqtt.reconnect_delay_set(1, 60)
        self.mqtt.on_connect = self._on_connect
        self.mqtt.on_disconnect = lambda *a: log.warning("MQTT disconnected, reconnecting")
        # LAN address first, then the NetBird one: macOS "Local Network" privacy can block LAN addresses
        # for launchd agents ("No route to host") while the VPN address stays reachable.
        self.hosts = [h for h in (env["TUBEHAX_BROKER_HOST"], env.get("TUBEHAX_BROKER_HOST_ALT")) if h]
        self.port = int(env.get("TUBEHAX_BROKER_PORT", 1883))
        self.host = ""
        self.disconnected_since = time.monotonic()
        self._choose_host()

    def _choose_host(self) -> None:
        for host in self.hosts:
            try:
                socket.create_connection((host, self.port), timeout=2).close()
            except OSError as e:
                log.warning("broker %s:%d unreachable: %s", host, self.port, e)
                continue
            if host != self.host:
                log.info("using broker %s:%d", host, self.port)
                self.host = host
                self.mqtt.connect_async(host, self.port, keepalive=30)
            return

    def _on_connect(self, client, userdata, flags, reason_code, properties) -> None:
        if reason_code.is_failure:
            log.error("MQTT connect failed: %s", reason_code)
            return
        log.info("MQTT connected to %s", self.host)
        client.publish(STATUS_TOPIC, "online", qos=1, retain=True)
        self.published.clear()  # Republish everything we know
        self.publish()

    def publish(self) -> None:
        if not self.mqtt.is_connected():
            return  # Latest state is kept in the tracker and sent on reconnect
        for topic, payload in self.tracker.topics().items():
            text = json.dumps(payload, separators=(",", ":"))
            if self.published.get(topic) != text:
                self.mqtt.publish(topic, text, qos=1, retain=True)
                self.published[topic] = text
                log.debug("%s %s", topic, text)

    async def handle_client(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        try:
            raw = await asyncio.wait_for(reader.read(), timeout=2)  # Until the client closes
            message = json.loads(raw)
            kind, data, t = message.get("kind"), message.get("data") or {}, message.get("t") or now()
            if kind == "hook":
                log.info("hook %s %s", data.get("hook_event_name"), data.get("tool_name", ""))
                self.tracker.hook(data, t)
            elif kind == "status":
                self.tracker.status(data, t)
            self.publish()
        except (ValueError, asyncio.TimeoutError, OSError) as e:
            log.warning("bad message: %s", e)
        finally:
            writer.close()

    async def run(self) -> None:
        SOCKET_PATH.parent.mkdir(parents=True, exist_ok=True)
        if SOCKET_PATH.exists():
            SOCKET_PATH.unlink()
        server = await asyncio.start_unix_server(self.handle_client, path=str(SOCKET_PATH))
        os.chmod(SOCKET_PATH, 0o600)
        self.mqtt.loop_start()
        log.info("listening on %s", SOCKET_PATH)

        stop = asyncio.Event()
        loop = asyncio.get_running_loop()
        for sig in (signal.SIGINT, signal.SIGTERM):
            loop.add_signal_handler(sig, stop.set)
        async with server:
            while not stop.is_set():
                if self.mqtt.is_connected():
                    self.disconnected_since = time.monotonic()
                elif time.monotonic() - self.disconnected_since > 15:
                    await loop.run_in_executor(None, self._choose_host)  # The network may have changed
                    self.disconnected_since = time.monotonic()
                self.tracker.expire(now())
                self.publish()
                try:
                    await asyncio.wait_for(stop.wait(), timeout=5)
                except asyncio.TimeoutError:
                    pass
        if self.mqtt.is_connected():
            self.mqtt.publish(STATUS_TOPIC, "offline", qos=1, retain=True).wait_for_publish(2)
        self.mqtt.loop_stop()
        SOCKET_PATH.unlink(missing_ok=True)


def run(verbose: bool = False) -> int:
    logging.basicConfig(level=logging.DEBUG if verbose else logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    asyncio.run(Bridge().run())
    return 0
