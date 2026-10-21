#!/usr/bin/env bash
# Build and upload to the clock over WiFi (HTTP POST /update, see src/OTA.cpp).
# Usage: scripts/ota.sh [firmware|filesystem]   (default: firmware)
# Reads TUBEHAX_OTA_PASSWORD (and optional TUBEHAX_CLOCK_HOST) from ~/.config/tubehax/broker.env.
set -euo pipefail
cd "$(dirname "$0")/.."
set -a; source "$HOME/.config/tubehax/broker.env"; set +a

ENV=EleksTube_Gen2
HOST="${TUBEHAX_CLOCK_HOST:-elekstubeg2-aa4860.local}"
TARGET="${1:-firmware}"
BUILD=".pio/build/$ENV"

case "$TARGET" in
  firmware)   uv run pio run -e "$ENV"; FILE=$(ls "$BUILD/${ENV}"_v*.bin) ;;
  filesystem) uv run pio run -e "$ENV" -t buildfs; FILE="$BUILD/littlefs.bin" ;;
  *) echo "usage: $0 [firmware|filesystem]" >&2; exit 2 ;;
esac

# curl's own .local (mDNS) lookups can stall for a long time on macOS; resolve once via the system resolver.
IP=$(dscacheutil -q host -a name "$HOST" 2>/dev/null | awk '/^ip_address/ {print $2; exit}')
HOST="${IP:-$HOST}"

echo "Uploading $FILE to http://$HOST/update ..."
# Credentials go to curl on stdin, not on the command line.
curl --fail-with-body --show-error --progress-bar --max-time 300 -o - \
  -K - -F "$TARGET=@$FILE" "http://$HOST/update?size=$(stat -f%z "$FILE")" <<<"user = \"ota:$TUBEHAX_OTA_PASSWORD\""
