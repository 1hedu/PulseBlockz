#!/usr/bin/env bash
# Builds the engine extension for Linux x86_64 -- the dedicated server's platform -- from this checkout.
#
#   wsl -d Ubuntu -- bash /mnt/c/.../luau/tools/linux-build.sh        (or on any Ubuntu 22.04+ box)
#
# Works in a copy under ~/pblockz-linux so the Linux objects never mix with the Windows build's, then puts
# libpulseblockz_luau.linux.template_debug.x86_64.so back into demo2/bin, studio/bin and demo/bin.
#
# Needs: g++, cmake, make, python3 with scons (pip install --user scons).
set -euo pipefail

HERE="$(cd "$(dirname "$0")/.." && pwd)"          # the checkout's luau/
WORK="$HOME/pblockz-linux/luau"
JOBS="$(nproc)"

echo "== copying sources from $HERE"
mkdir -p "$WORK/gdextension/demo/bin" "$WORK/gdextension/studio/bin" "$WORK/gdextension/demo2/bin"
rsync -a --delete --exclude '*.obj' "$HERE/src/" "$WORK/src/"
rsync -a --delete --exclude '*.obj' --exclude '*.os' --exclude '*.o' "$HERE/gdextension/src/" "$WORK/gdextension/src/"
rsync -a --delete --exclude '*.obj' --exclude '*.os' --exclude '*.o' "$HERE/gdextension/thirdparty/" "$WORK/gdextension/thirdparty/"
cp "$HERE/gdextension/SConstruct" "$WORK/gdextension/"
rsync -a --exclude '*.obj' --exclude '*.lib' --exclude 'bin/' --exclude '.sconsign*' --exclude '*.os' --exclude '*.o' \
  "$HERE/godot-cpp/" "$WORK/godot-cpp/"
rsync -a --exclude 'build/' --exclude '.git/' "$HERE/luau/" "$WORK/luau/"
rsync -a --exclude 'build/' --exclude '.git/' "$HERE/secp256k1/" "$WORK/secp256k1/"
# SConstruct reads ../../package.json for the version, which from gdextension/ is the
# checkout root -- one level above the luau/ this copies. Without it the build stops at
# "No such file or directory: '../../package.json'" before compiling anything.
cp "$HERE/../package.json" "$WORK/../package.json"

if ! python3 -c 'import SCons' 2>/dev/null; then
  echo "== installing scons"
  python3 -m pip install --user scons
fi
export PATH="$HOME/.local/bin:$PATH"

if [ ! -f "$WORK/luau/build/libLuau.VM.a" ]; then
  echo "== building Luau"
  cmake -S "$WORK/luau" -B "$WORK/luau/build" -DCMAKE_BUILD_TYPE=Release -DLUAU_BUILD_TESTS=OFF \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
  cmake --build "$WORK/luau/build" -j "$JOBS" --target Luau.VM Luau.Compiler Luau.Ast Luau.Common Luau.Bytecode
fi

if [ ! -f "$WORK/secp256k1/build/lib/libsecp256k1.a" ]; then
  echo "== building secp256k1 (with recovery)"
  cmake -S "$WORK/secp256k1" -B "$WORK/secp256k1/build" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF \
    -DSECP256K1_ENABLE_MODULE_RECOVERY=ON -DSECP256K1_BUILD_TESTS=OFF -DSECP256K1_BUILD_EXHAUSTIVE_TESTS=OFF \
    -DSECP256K1_BUILD_BENCHMARK=OFF -DSECP256K1_BUILD_EXAMPLES=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON
  cmake --build "$WORK/secp256k1/build" -j "$JOBS"
fi

echo "== building the extension"
cd "$WORK/gdextension"
scons platform=linux target=template_debug -j "$JOBS"

SO="demo2/bin/libpulseblockz_luau.linux.template_debug.x86_64.so"
ls -la "$SO"
for dest in demo2/bin studio/bin demo/bin; do
  mkdir -p "$HERE/gdextension/$dest"
  cp "$SO" "$HERE/gdextension/$dest/"
done
echo "== copied into the checkout's demo2/bin, studio/bin and demo/bin"
