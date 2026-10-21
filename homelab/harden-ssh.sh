#!/usr/bin/env bash
# Harden SSH on the homelab (run as root on the homelab):
#   - reachable only from loopback, NetBird (wt0) and the home LAN; anything else gets a TCP reset
#     (public IPv6, other networks). Only port 22 is touched; Docker, k3s and Slurm rules are not.
#   - keys only (no passwords), no X11 forwarding.
# A 10-minute rollback is armed on every apply; confirm once a NEW ssh session works.
#
#   sudo bash harden-ssh.sh            apply (+ rollback in 10 min)
#   sudo bash harden-ssh.sh confirm    keep it (cancel the rollback)
#   sudo bash harden-ssh.sh undo       remove everything
set -euo pipefail

LAN4="192.168.1.0/24"
SSHD_DROPIN=/etc/ssh/sshd_config.d/00-tubehax-hardening.conf # 00-: sshd uses the first value it reads
NFT_FILE=/etc/tubehax/ssh-access.nft
UNIT=tubehax-ssh-access
ROLLBACK=tubehax-ssh-rollback

[[ $EUID -eq 0 ]] || { echo "run with sudo" >&2; exit 1; }

undo() {
  systemctl disable --now "$UNIT.service" 2>/dev/null || true
  nft delete table inet tubehax_ssh 2>/dev/null || true
  rm -f "/etc/systemd/system/$UNIT.service" "$NFT_FILE" "$SSHD_DROPIN"
  systemctl daemon-reload
  systemctl reload ssh
  echo "SSH hardening removed."
}

case "${1:-apply}" in
confirm)
  systemctl stop "$ROLLBACK.timer" 2>/dev/null && echo "Rollback cancelled: hardening stays." || echo "No rollback was pending."
  exit 0
  ;;
undo)
  systemctl stop "$ROLLBACK.timer" 2>/dev/null || true
  undo
  exit 0
  ;;
apply) ;;
*)
  echo "usage: $0 [apply|confirm|undo]" >&2
  exit 2
  ;;
esac

# 1. sshd: keys only, no X11. Validate before it can take effect.
cat >"$SSHD_DROPIN" <<'EOF'
# TubeHAX hardening (homelab/harden-ssh.sh in the EleksTubeHAX repo)
PasswordAuthentication no
KbdInteractiveAuthentication no
X11Forwarding no
EOF
if ! sshd -t; then
  rm -f "$SSHD_DROPIN"
  echo "sshd config check failed: nothing changed." >&2
  exit 1
fi

# 2. Firewall for port 22 only, in its own nftables table, loaded at boot by its own unit.
mkdir -p /etc/tubehax
cat >"$NFT_FILE" <<EOF
#!/usr/sbin/nft -f
# SSH only from loopback, NetBird and the home LAN (homelab/harden-ssh.sh). Other traffic is untouched.
table inet tubehax_ssh
delete table inet tubehax_ssh
table inet tubehax_ssh {
  chain input {
    type filter hook input priority -5; policy accept;
    tcp dport 22 iifname { "lo", "wt0" } accept
    tcp dport 22 ip saddr $LAN4 accept
    tcp dport 22 ip6 saddr fe80::/10 accept
    tcp dport 22 reject with tcp reset
  }
}
EOF
nft -c -f "$NFT_FILE" # Syntax check

cat >"/etc/systemd/system/$UNIT.service" <<EOF
[Unit]
Description=TubeHAX: SSH only via NetBird, LAN and loopback
Before=ssh.service
After=network-pre.target

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/usr/sbin/nft -f $NFT_FILE
ExecStop=/usr/sbin/nft delete table inet tubehax_ssh

[Install]
WantedBy=multi-user.target
EOF

# 3. Safety net, then apply. Existing sessions stay open (reload doesn't drop them).
systemctl stop "$ROLLBACK.timer" 2>/dev/null || true
systemd-run --quiet --unit "$ROLLBACK" --on-active=10min /bin/bash "$(readlink -f "$0")" undo
systemctl daemon-reload
systemctl enable --now "$UNIT.service"
systemctl reload ssh

echo
echo "Applied. Rollback in 10 minutes unless confirmed."
echo "From a NEW terminal, check:  ssh homelab        (NetBird)   and   ssh homelab-local   (LAN)"
echo "If both work:                ssh -t homelab 'sudo bash ~/tubehax/harden-ssh.sh confirm'"
