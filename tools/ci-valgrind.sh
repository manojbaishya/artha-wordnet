#!/usr/bin/env bash
# ci-valgrind.sh — Run the Valgrind lane inside ubuntu:24.04 container.
# libc6-dbg (glibc debuginfo) is installed in-container so valgrind can
# redirect ld.so — no host mutation (ground rule 8).
# Invoked by task valgrind-docker.
set -euo pipefail

# xmake refuses to run as root without this (container runs as root)
export XMAKE_ROOT=y

# Use a fresh xmake home inside the container so it doesn't read the host's
# cached project directory (which points to /home/manoj/... and doesn't exist
# inside the container).
export XMAKE_HOME=/tmp/xmake-home

cd /artha

# Clear any host-cached xmake project config (sandbox/ was configured on the
# host with absolute paths that don't exist inside the container).
rm -rf sandbox

XMAKE=/root/.artha-cache/xmake-3.1.2/bin/xmake

# Ensure xmake 3.1.2 is available (mounted cache or bootstrap inside container)
if [ ! -x "$XMAKE" ]; then
    echo "ci-valgrind: xmake not cached in container; bootstrapping..."
    task bootstrap-xmake
    XMAKE=/root/.artha-cache/xmake-3.1.2/bin/xmake
fi

echo "ci-valgrind: running Valgrind lane (valgrind + libc6-dbg for ld.so redirection)"

# Configure release build
"$XMAKE" config --prefix=/artha/sandbox/stage

# Build unit + integration tests + the artha binary (needed for install)
"$XMAKE" build artha-unit-test artha-integration-test artha

# Build suppression args from available files
build_supp_args() {
    SUPP_ARGS=""
    for s in /usr/share/glib-2.0/valgrind/glib.supp \
             /usr/share/gtk-4.0/valgrind/gtk.supp \
             /artha/tools/valgrind/artha.supp; do
        if [ -f "$s" ]; then
            SUPP_ARGS="$SUPP_ARGS --suppressions=$s"
        else
            echo "valgrind: skipping missing suppression $s"
        fi
    done
    echo "$SUPP_ARGS"
}

SUPP_ARGS=$(build_supp_args)

# Run unit test under Valgrind
G_SLICE=always-malloc G_DEBUG=resident-modules \
    valgrind --leak-check=full --error-exitcode=1 --num-callers=20 $SUPP_ARGS \
    "$XMAKE" run artha-unit-test

# Run integration test under Valgrind
G_SLICE=always-malloc G_DEBUG=resident-modules \
    valgrind --leak-check=full --error-exitcode=1 --num-callers=20 $SUPP_ARGS \
    "$XMAKE" run artha-integration-test

# Stage install
"$XMAKE" install -o /artha/sandbox/stage

# Run staged binary under Valgrind (smoke test).
# GTK one-shot allocations at exit are expected GUI behaviour, not Artha leaks.
# The container has no display server, so GTK may fail to initialise — the
# smoke test is informational only and must not fail the lane.
G_SLICE=always-malloc G_DEBUG=resident-modules \
    valgrind --leak-check=full --error-exitcode=0 --num-callers=20 $SUPP_ARGS \
    /artha/sandbox/stage/bin/artha || true

# Clean sandbox so host-side tasks aren't affected by container-configured
# artifacts (container paths like /artha don't exist on the host).
rm -rf sandbox
