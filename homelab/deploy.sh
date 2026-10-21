#!/usr/bin/env bash
# Sync this folder to the homelab and (re)start the stack.
# Users/passwords live only on the homelab in mosquitto/passwd (create with ./add-user.sh).
set -euo pipefail
HOST="${TUBEHAX_HOST:-homelab-local}"
DEST="${TUBEHAX_DEST:-tubehax}"
cd "$(dirname "$0")"
rsync -a --exclude passwd --exclude feeds.env ./ "$HOST:$DEST/"
ssh "$HOST" "cd $DEST && { [ -e mosquitto/passwd ] || touch mosquitto/passwd; } && docker compose up -d --build && docker compose ps"
