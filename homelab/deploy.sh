#!/usr/bin/env bash
# Sync this folder to the homelab and (re)start the stack.
# Users/passwords live only on the homelab in mosquitto/passwd (create with ./add-user.sh).
set -euo pipefail
HOST="${TUBEHAX_HOST:-homelab-local}"
DEST="${TUBEHAX_DEST:-tubehax}"
cd "$(dirname "$0")"
rsync -a --exclude passwd --exclude feeds.env --exclude secrets ./ "$HOST:$DEST/"
# Cron: copy the Claude login's short-lived access token for the feeds container every 5 minutes.
CRON="*/5 * * * * /usr/bin/python3 \$HOME/$DEST/claude-usage-token.py"
ssh "$HOST" "(crontab -l 2>/dev/null | grep -v claude-usage-token.py; echo '$CRON') | crontab - \
  && python3 $DEST/claude-usage-token.py"
ssh "$HOST" "cd $DEST && { [ -e mosquitto/passwd ] || touch mosquitto/passwd; } && docker compose up -d --build && docker compose ps"
