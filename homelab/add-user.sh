#!/usr/bin/env bash
# Create or update a broker user on the homelab.
# Usage: ./add-user.sh <user>   (password is read from stdin, e.g. `printf %s "$PW" | ./add-user.sh clock`)
set -euo pipefail
HOST="${TUBEHAX_HOST:-homelab-local}"
DEST="${TUBEHAX_DEST:-tubehax}"
USER_NAME="$1"
[[ "$USER_NAME" =~ ^[a-z0-9_-]+$ ]] || { echo "invalid user name" >&2; exit 1; }
# The password travels over ssh stdin, never on a command line. Mosquitto runs as uid 1883 and wants a private file.
ssh "$HOST" "cd $DEST && docker run --rm -i -v \"\$PWD/mosquitto:/m\" eclipse-mosquitto:2.0.20 \
  sh -c 'touch /m/passwd && mosquitto_passwd -b /m/passwd $USER_NAME \"\$(cat)\" && chown 1883:1883 /m/passwd && chmod 0600 /m/passwd' \
  && docker compose restart mosquitto >/dev/null"
