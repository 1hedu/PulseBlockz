#!/usr/bin/env bash
# Sets a fresh Ubuntu droplet up to run the town at play.safewrap.xyz, and to keep running it.
#
#   scp pblockz-town.tar.gz luau/tools/droplet/setup.sh root@<droplet ip>:/root/
#   ssh root@<droplet ip> bash /root/setup.sh /root/pblockz-town.tar.gz
#
# Run again with a newer bundle to update: it replaces the town and restarts the service.
#
# To run a PUBLISHED game rather than the bundle's own scripts -- a server for somebody else's game,
# fetched from the chain and checked file by file against its hash -- set PLACE:
#
#   PLACE='pblockz://<hash>?chain=943:...' bash /root/setup.sh /root/pblockz-town.tar.gz
#
# The bundle is still what provides the engine and the host; the game's scripts come from the chain.
#
# What it does, and nothing else:
#   - a `pblockz` user with no login shell to run the server as (never root)
#   - headless-capable Godot 4.3 in /opt/godot
#   - the bundle in /opt/pblockz/demo2
#   - a systemd service, pblockz-town, restarted on failure and at boot
#   - the firewall: SSH, and UDP 8800 for players. Nothing else is open.
#
# No key of any kind goes on this machine. The server signs nothing (SERVER.md, SAFETY.md); every
# player pays from their own wallet.
set -euo pipefail

BUNDLE="${1:?usage: setup.sh <pblockz-town.tar.gz>}"
PORT="${PORT:-8800}"
# The name players dial. A sign-in names the server it was signed for, and this server refuses
# one signed for any name but this and its own addresses.
PUBLIC="${PUBLIC:-play.safewrap.xyz}"
PLACE="${PLACE:-}"
PLACE_ARG=""
# systemd reads % as the start of a specifier, and a pblockz:// uri has %2F in it: doubled, it is a %.
[ -z "$PLACE" ] || PLACE_ARG=" --place '${PLACE//%/%%}'"
GODOT_URL="https://github.com/godotengine/godot/releases/download/4.3-stable/Godot_v4.3-stable_linux.x86_64.zip"

[ "$(id -u)" = "0" ] || { echo "run as root" >&2; exit 1; }

# The running town stops first. On a 1 GB box with no swap, apt beside a live server was enough to
# push the machine into memory pressure so deep it stopped answering SSH and players alike, and it
# needed a power cycle (2026-09-15). Players are off for the minute this takes either way.
echo "== stopping the town"
# What the service was called before the rename, spelled in two pieces so a sweep of the old name
# cannot take the line that retires it. Stopped and removed here, or the old one keeps running on
# the same port and the new one never gets the socket.
WAS="pb""lox-town"
systemctl disable --now "$WAS" 2>/dev/null || true
rm -f "/etc/systemd/system/$WAS.service"
systemctl stop pblockz-town 2>/dev/null || true

# And a swap file, once, so a spike slows the box down rather than locking it up.
if ! swapon --show | grep -q /swapfile; then
  echo "== swap"
  fallocate -l 1G /swapfile && chmod 600 /swapfile && mkswap /swapfile >/dev/null && swapon /swapfile
  grep -q '^/swapfile ' /etc/fstab || echo '/swapfile none swap sw 0 0' >> /etc/fstab
fi

echo "== packages"
apt-get update -qq
DEBIAN_FRONTEND=noninteractive apt-get install -y -qq unzip ufw ca-certificates curl \
  libfontconfig1 libgl1 >/dev/null

echo "== user"
id pblockz >/dev/null 2>&1 || useradd --system --create-home --home-dir /home/pblockz --shell /usr/sbin/nologin pblockz

echo "== godot"
if [ ! -x /opt/godot/godot ]; then
  mkdir -p /opt/godot
  curl -sSL -o /tmp/godot.zip "$GODOT_URL"
  unzip -o -q /tmp/godot.zip -d /opt/godot
  mv /opt/godot/Godot_v4.3-stable_linux.x86_64 /opt/godot/godot
  chmod +x /opt/godot/godot
  rm /tmp/godot.zip
fi
/opt/godot/godot --headless --version

echo "== the town"
mkdir -p /opt/pblockz
rm -rf /opt/pblockz/demo2
tar -xzf "$BUNDLE" -C /opt/pblockz
chown -R pblockz:pblockz /opt/pblockz

echo "== service"
cat > /etc/systemd/system/pblockz-town.service <<UNIT
[Unit]
Description=PulseBlockz town
After=network-online.target
Wants=network-online.target

[Service]
User=pblockz
WorkingDirectory=/opt/pblockz/demo2
ExecStart=/opt/godot/godot --headless --path /opt/pblockz/demo2 -s res://Serve.gd -- --port=$PORT --public=$PUBLIC$PLACE_ARG
Restart=always
RestartSec=3
# Deliberately no PBLOCKZ_PLAYER_KEY, and no .env.testnet anywhere on this box. The server does not sign.

[Install]
WantedBy=multi-user.target
UNIT
systemctl daemon-reload
systemctl enable --now pblockz-town

echo "== firewall"
ufw allow OpenSSH >/dev/null
ufw allow "$PORT/udp" >/dev/null
ufw --force enable >/dev/null
ufw status | sed 's/^/  /'

sleep 5
systemctl --no-pager --lines=15 status pblockz-town || true
echo
echo "The town is on UDP $PORT. Point an A record for play.safewrap.xyz at this machine's IPv4."
echo "Logs: journalctl -u pblockz-town -f"
