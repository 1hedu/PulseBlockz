#!/usr/bin/env bash
# Packs the town for a Linux server: the demo2 project with the Linux extension and none of the tests,
# Windows binaries or logs.
#
#   bash luau/tools/linux-build.sh           (first: builds the .so into demo2/bin)
#   bash luau/tools/droplet/bundle.sh [out]  (writes pblockz-town.tar.gz, default ~/pblockz-town.tar.gz)
#
# Then copy it and setup.sh to the droplet: see setup.sh.
set -euo pipefail

HERE="$(cd "$(dirname "$0")/../.." && pwd)"        # the checkout's luau/
DEMO="$HERE/gdextension/demo2"
OUT="${1:-$HOME/pblockz-town.tar.gz}"
SO="$DEMO/bin/libpulseblockz_luau.linux.template_debug.x86_64.so"

[ -f "$SO" ] || { echo "no Linux build at $SO -- run luau/tools/linux-build.sh first" >&2; exit 1; }
# And a CURRENT one. The bundle ships whatever .so is lying in demo2/bin, and one built before the
# last engine change shipped a server that did not have the fix everybody was waiting on (twice).
# Newer than every engine source, or refused; STALE_OK=1 to ship it anyway, knowingly.
NEWEST_SRC="$(find "$HERE/src" "$HERE/gdextension/src" -type f \( -name '*.cpp' -o -name '*.h' \) -newer "$SO" | head -1)"
if [ -n "$NEWEST_SRC" ] && [ "${STALE_OK:-}" != "1" ]; then
  echo "refusing: $SO is older than $NEWEST_SRC -- run luau/tools/linux-build.sh (or STALE_OK=1)" >&2
  exit 1
fi
# A dev chain's override would point every player at contracts that exist nowhere but a laptop.
[ ! -f "$DEMO/scripts/src/shared/ContractsDev.luau" ] || { echo "refusing: ContractsDev.luau exists (node scripts/dev-chain.js stop)" >&2; exit 1; }

STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
mkdir -p "$STAGE/demo2"
rsync -a \
  --exclude 'tests/' --exclude '.godot/' --exclude '*.dll' --exclude '*.pdb' --exclude '*.exp' --exclude '*.lib' \
  --exclude 'logs/' --exclude '*.log' \
  "$DEMO/" "$STAGE/demo2/"
# The one file under .godot/ a project cannot run without: it is how Godot knows to load the engine
# extension at all. Without it every class the extension adds (PulseBlockzWorld...) is missing.
mkdir -p "$STAGE/demo2/.godot"
echo 'res://pulseblockz_luau.gdextension' > "$STAGE/demo2/.godot/extension_list.cfg"
# What this is: the commit it was built from, whether the tree was clean, and when. Serve.gd prints
# it as the town starts, so a droplet's journal says which build is answering -- a stale process
# looks exactly like a bad fix otherwise.
COMMIT="$(git -C "$HERE" rev-parse --short HEAD 2>/dev/null || echo unknown)"
DIRTY=""
[ -z "$(git -C "$HERE" status --porcelain -- . 2>/dev/null)" ] || DIRTY="-dirty"
printf 'commit=%s%s
built=%s
so=%s
' "$COMMIT" "$DIRTY" "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$(stat -c %y "$SO" | cut -d. -f1)" > "$STAGE/demo2/BUILD_STAMP"
cat "$STAGE/demo2/BUILD_STAMP"
tar -C "$STAGE" -czf "$OUT" demo2
echo "wrote $OUT ($(du -h "$OUT" | cut -f1))"
