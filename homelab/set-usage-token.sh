#!/usr/bin/env bash
# Give the homelab feeder a token for the plan-limit bars (5H / 7D / model weekly).
# Create a long-lived token on the Mac with `claude setup-token`, copy it, then:
#   pbpaste | homelab/set-usage-token.sh
# The token goes over ssh stdin into ~/tubehax/feeds.env (mode 600) and is never echoed.
# Remove it again with:  homelab/set-usage-token.sh --remove
set -euo pipefail
HOST="${TUBEHAX_HOST:-homelab-local}"
DEST="${TUBEHAX_DEST:-tubehax}"
if [[ "${1:-}" == "--remove" ]]; then
  ssh "$HOST" "cd $DEST && sed -i '/^CLAUDE_USAGE_TOKEN=/d' feeds.env && docker compose up -d feeds >/dev/null 2>&1 && echo removed"
  exit 0
fi
TOKEN="$(tr -d '[:space:]')"
[[ -n "$TOKEN" ]] || { echo "no token on stdin" >&2; exit 1; }
printf '%s' "$TOKEN" | ssh "$HOST" "cd $DEST && umask 077 && t=\$(cat) && sed -i '/^CLAUDE_USAGE_TOKEN=/d' feeds.env \
  && printf 'CLAUDE_USAGE_TOKEN=%s\n' \"\$t\" >> feeds.env && docker compose up -d feeds >/dev/null 2>&1"
echo "token stored; checking the feeder..."
sleep 12
ssh "$HOST" "cd $DEST && docker compose logs --since 30s feeds 2>&1 | grep -E 'usage' || echo 'no usage log yet'"
