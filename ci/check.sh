#!/bin/sh
# Artha CI check: configure + build with xmake, verify the committed
# flatpak manifest is in sync with deps.json, and smoke-test the binary.
#
# Requirements (Debian/Ubuntu 22.04): build-essential pkg-config python3
#   libgtk-4-dev libglib2.0-dev libx11-dev wordnet wordnet-dev
#   xvfb xauth   (only for the smoke test's X server; used automatically
#                 when DISPLAY is unset)
# xmake >= 3.0 on PATH (the CI workflow installs the exact version
# pinned in deps.json; nothing is downloaded by this script itself).
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)

# Prefer the system toolchain (GTK 4.6 on jammy) over any linuxbrew
# shadowing; CI runners have no linuxbrew.
PATH=/usr/bin:$PATH
export PATH

command -v xmake >/dev/null 2>&1 || { echo "ci/check: xmake not found on PATH" >&2; exit 1; }

echo "== xmake configure"
xmake f -P "$ROOT"

echo "== xmake build"
xmake -P "$ROOT"

BIN=$(find "$ROOT/sandbox" -type f -name artha -path '*release*' -perm -u+x 2>/dev/null | head -n 1)
[ -n "$BIN" ] || { echo "ci/check: artha binary not found under $ROOT/sandbox" >&2; exit 1; }
echo "ci/check: built $BIN"

echo "== flatpak manifest drift check (deps.json -> flatpak/org.artha.Artha.json)"
python3 "$ROOT/tools/gen_flatpak_manifest.py" --check

echo "== smoke test"
"$ROOT/tests/test_smoke.sh" "$BIN"

echo "ci/check: OK"
