#!/bin/sh
# Smoke test for a built artha binary.
#
# Launches artha on an X display (an already-running $DISPLAY, or a
# private Xvfb started automatically) with a scratch HOME and XDG_*
# directories, and checks that the process survives startup and writes
# its preferences file into the scratch dir. The app is killed after
# the check; nothing outside the scratch dir (and the X server) is
# touched.
#
# Why scratch HOME/XDG_*: artha unconditionally rewrites
# $XDG_CONFIG_HOME/artha.conf at startup, and a bare HOME=... does not
# help when the caller exports XDG_CONFIG_HOME (g_get_user_config_dir()
# prefers it). All four are pinned to the scratch dir.
#
# Usage: tests/test_smoke.sh [path-to-artha]
set -eu

if [ $# -ge 1 ]; then
	BIN=$1
else
	ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
	BIN=$(find "$ROOT/sandbox" -type f -name artha -path '*release*' -perm -u+x 2>/dev/null | head -n 1)
fi
if [ -z "$BIN" ] || [ ! -x "$BIN" ]; then
	echo "test_smoke: artha binary not found (pass it as \$1 or build first)" >&2
	exit 1
fi

# Re-exec under xvfb-run when there is no X display.
if [ -z "${DISPLAY:-}" ] && [ -z "${ARTHA_SMOKE_XVFB:-}" ]; then
	if command -v xvfb-run >/dev/null 2>&1; then
		ARTHA_SMOKE_XVFB=1 exec xvfb-run -a --server-args='-screen 0 1024x768x24' "$0" "$BIN"
	fi
	echo "test_smoke: no DISPLAY and xvfb-run not found; install xvfb or run on an X display" >&2
	exit 1
fi

SCRATCH=$(mktemp -d /tmp/artha-smoke.XXXXXX)
mkdir -p "$SCRATCH/home" "$SCRATCH/config" "$SCRATCH/data" "$SCRATCH/cache"

HOME=$SCRATCH/home
XDG_CONFIG_HOME=$SCRATCH/config
XDG_DATA_HOME=$SCRATCH/data
XDG_CACHE_HOME=$SCRATCH/cache
GDK_DEBUG=
export HOME XDG_CONFIG_HOME XDG_DATA_HOME XDG_CACHE_HOME GDK_DEBUG

CONF=$SCRATCH/config/artha.conf

"$BIN" >"$SCRATCH/artha.log" 2>&1 &
PID=$!
# Always kill and clean up, however we exit.
trap 'kill "$PID" 2>/dev/null; wait "$PID" 2>/dev/null; rm -rf "$SCRATCH"' EXIT INT TERM

i=0
while [ "$i" -lt 150 ]; do
	if ! kill -0 "$PID" 2>/dev/null; then
		echo "test_smoke: artha exited during startup; log follows:" >&2
		cat "$SCRATCH/artha.log" >&2
		exit 1
	fi
	[ -s "$CONF" ] && break
	i=$((i + 1))
	sleep 0.1
done

if [ ! -s "$CONF" ]; then
	echo "test_smoke: artha did not write $CONF within 15s; log follows:" >&2
	cat "$SCRATCH/artha.log" >&2
	exit 1
fi

kill "$PID" 2>/dev/null || true
wait "$PID" 2>/dev/null || true
trap - EXIT INT TERM
rm -rf "$SCRATCH"
echo "test_smoke: OK (artha survived startup and wrote its preferences file)"
