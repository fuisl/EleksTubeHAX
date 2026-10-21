from pathlib import Path

CONFIG_DIR = Path.home() / ".config" / "tubehax"
SOCKET_PATH = CONFIG_DIR / "bridge.sock"
BROKER_ENV = CONFIG_DIR / "broker.env"
ORIGINAL_STATUSLINE = CONFIG_DIR / "statusline-original"  # The user's own status line command, if any
CLAUDE_SETTINGS = Path.home() / ".claude" / "settings.json"
LAUNCHD_LABEL = "dev.tubehax.bridge"
LAUNCHD_PLIST = Path.home() / "Library" / "LaunchAgents" / f"{LAUNCHD_LABEL}.plist"
LOG_FILE = Path.home() / "Library" / "Logs" / "tubehax-bridge.log"
