#!/usr/bin/env bash
# Harden the homelab (run as root on the homelab):
#   - admin ports reachable only from the machine itself, NetBird (wt0), the home LAN and local k3s pods /
#     Docker containers; anything else (the internet via port forwards or public IPv6) is refused:
#       22 SSH, 6443 k3s API, 10250 kubelet, 6817/6818 Slurm, 8472/udp flannel
#     Public services are untouched: 80/443 (Traefik, forwarded by the router) and MQTT 1883/9001 (clock).
#     Own nftables table: Docker/k3s rules are not modified.
#   - sshd: keys only, no root login, no X11 forwarding.
# A 10-minute rollback is armed on every apply; confirm once a NEW ssh session works.
#
#   sudo bash harden-homelab.sh            apply (+ rollback in 10 min)
#   sudo bash harden-homelab.sh confirm    keep it (cancel the rollback)
#   sudo bash harden-homelab.sh undo       remove everything
set -euo pipefail

LAN4="192.168.1.0/24"
SSHD_DROPIN=/etc/ssh/sshd_config.d/00-tubehax-hardening.conf # 00-: sshd uses the first value it reads
NFT_FILE=/etc/tubehax/admin-ports.nft
TABLE=tubehax_admin
UNIT=tubehax-admin-ports
ROLLBACK=tubehax-harden-rollback

[[ $EUID -eq 0 ]] || { echo "run with sudo" >&2; exit 1; }

undo() {
  systemctl disable --now "$UNIT.service" 2>/dev/null || true
  nft delete table inet $TABLE 2>/dev/null || true
  rm -f "/etc/systemd/system/$UNIT.service" "$NFT_FILE" "$SSHD_DROPIN"
  systemctl daemon-reload
  systemctl reload ssh
  echo "Homelab hardening removed."
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

# Until everything is applied, a failure removes what this run wrote.
trap 'rm -f "$SSHD_DROPIN" "$NFT_FILE"; echo "Failed: nothing applied." >&2' ERR

# 1. sshd: keys only, no root login, no X11. Validate before it can take effect.
cat >"$SSHD_DROPIN" <<'EOF'
# TubeHAX hardening (homelab/harden-homelab.sh in the EleksTubeHAX repo)
PasswordAuthentication no
KbdInteractiveAuthentication no
PermitRootLogin no
X11Forwarding no
EOF
if ! sshd -t; then
  rm -f "$SSHD_DROPIN"
  echo "sshd config check failed: nothing changed." >&2
  exit 1
fi

# 2. Firewall for the admin ports only, in its own nftables table, loaded at boot by its own unit.
mkdir -p /etc/tubehax
cat >"$NFT_FILE" <<NFT
#!/usr/sbin/nft -f
# Admin ports only from this machine, NetBird, the LAN and local pods/containers (homelab/harden-homelab.sh).
# Other ports and traffic are untouched.
table inet $TABLE
delete table inet $TABLE
table inet $TABLE {
  chain input {
    type filter hook input priority -5; policy accept;
    tcp dport { 22, 6443, 10250, 6817, 6818 } jump admin
    udp dport 8472 jump admin
  }
  chain admin {
    iifname { "lo", "wt0", "cni0", "flannel.1", "docker0" } accept
    iifname "br-*" accept
    ip saddr { $LAN4, 10.42.0.0/16, 10.43.0.0/16, 172.16.0.0/12 } accept
    ip6 saddr fe80::/10 accept
    meta l4proto tcp reject with tcp reset
    drop
  }
}
NFT
nft -c -f "$NFT_FILE" # Syntax check

cat >"/etc/systemd/system/$UNIT.service" <<EOF
[Unit]
Description=TubeHAX: admin ports only via NetBird, LAN and local
Before=ssh.service
After=network-pre.target

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/usr/sbin/nft -f $NFT_FILE
ExecStop=/usr/sbin/nft delete table inet $TABLE

[Install]
WantedBy=multi-user.target
EOF

trap - ERR

# 3. Safety net, then apply. Existing sessions stay open (reload doesn't drop them).
systemctl stop "$ROLLBACK.timer" 2>/dev/null || true
systemd-run --quiet --unit "$ROLLBACK" --on-active=10min /bin/bash "$(readlink -f "$0")" undo
systemctl daemon-reload
systemctl enable --now "$UNIT.service"
systemctl reload ssh

echo
echo "Applied. Rollback in 10 minutes unless confirmed."
echo "From a NEW terminal, check:  ssh homelab        (NetBird)   and   ssh homelab-local   (LAN)"
echo "and that your apps still answer (e.g. https://api.coipe.app)."
echo "If all good:                 ssh -t homelab 'sudo bash ~/tubehax/harden-homelab.sh confirm'"
