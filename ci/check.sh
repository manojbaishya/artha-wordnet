#!/bin/sh
# Artha CI check: configure + build with xmake, install into a throwaway
# prefix, verify the committed flatpak manifest is in sync with deps.json,
# and smoke-test the installed binary.
#
# Why install into a scratch prefix instead of running the build-tree
# binary directly: the installed binary baked APP_DIR/ICON_DIR at configure
# time. Smoke-testing it against a scratch prefix exercises the exact
# install layout (bin, gui.glade, desktop file, icon) and never reads or
# writes the host's /usr/local (a stale GTK2-era /usr/local/share/artha/
# gui.glade would shadow the fresh build and crash the app at startup).
#
# Requirements (Debian/Ubuntu): build-essential pkg-config python3
#   libgtk-4-dev libglib2.0-dev libx11-dev wordnet wordnet-dev
#   xvfb xauth   (only for the smoke test's X server; used automatically
#                 when DISPLAY is unset)
# xmake >= 3.0 on PATH (the CI workflow installs the exact version
# pinned in deps.json; nothing is downloaded by this script itself).
# The CI workflow pins ubuntu-24.04 because the deps.json xmake bundle
# requires glibc >= 2.38 (ubuntu-22.04 ships glibc 2.35). A host build
# against GTK 4.6 works locally as long as xmake is available.
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)

# Prefer the system toolchain (GTK 4.6 on jammy) over any linuxbrew
# shadowing; CI runners have no linuxbrew.
PATH=/usr/bin:$PATH
export PATH

command -v xmake >/dev/null 2>&1 || { echo "ci/check: xmake not found on PATH" >&2; exit 1; }

# Leave the worktree reconfigured for the default prefix so a later bare
# `xmake` does not bake APP_DIR for the (deleted) scratch prefix.
SCRATCH=$(mktemp -d /tmp/artha-ci-prefix.XXXXXX)
trap 'xmake f -P "$ROOT" --prefix=/usr/local >/dev/null 2>&1 || true; rm -rf "$SCRATCH"' EXIT INT TERM

echo "== xmake configure (prefix $SCRATCH)"
xmake f -P "$ROOT" --prefix="$SCRATCH"

echo "== xmake build"
xmake -P "$ROOT"

echo "== xmake run unit tests"
( cd "$ROOT" && xmake run artha-unit-test )

echo "== xmake run integration tests"
( cd "$ROOT" && xmake run artha-integration-test )

echo "== xmake install"
xmake install -P "$ROOT" -o "$SCRATCH"

BIN="$SCRATCH/bin/artha"
[ -x "$BIN" ] || { echo "ci/check: installed binary not found at $BIN" >&2; exit 1; }
echo "ci/check: installed $BIN"

echo "== flatpak manifest drift check (deps.json -> flatpak/org.artha.Artha.json)"
python3 "$ROOT/tools/gen_flatpak_manifest.py" --check

echo "== smoke test"
"$ROOT/tests/smoke/test_smoke.sh" "$BIN"

echo "ci/check: OK"
