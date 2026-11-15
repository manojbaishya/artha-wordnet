#!/usr/bin/env bash
# ci-asn.sh — Run the ASan lane inside ubuntu:24.04 container (CI-matching glib 2.74).
# Invoked by task test-asan-docker. The container mounts the artha source tree
# and the cached xmake so nothing is installed on the host (ground rule 8).
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
    echo "ci-asn: xmake not cached in container; bootstrapping..."
    task bootstrap-xmake
    XMAKE=/root/.artha-cache/xmake-3.1.2/bin/xmake
fi

echo "ci-asn: running ASan lane (clang + ASan+UBSan, glib 2.74 / GTK 4.14)"

# Configure: debug build with clang ASan+UBSan into sandbox/asan
"$XMAKE" config -m debug -o sandbox/asan --cc=clang \
  --cxflags="-fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -g" \
  --ldflags="-fsanitize=address -fsanitize=undefined"

# Build unit + integration tests
"$XMAKE" build artha-unit-test artha-integration-test

# Run unit test under ASan
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
LSAN_OPTIONS=suppressions=/artha/tools/sanitizers/lsan.supp \
G_SLICE=always-malloc \
"$XMAKE" run artha-unit-test

# Run integration test under ASan
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
LSAN_OPTIONS=suppressions=/artha/tools/sanitizers/lsan.supp \
G_SLICE=always-malloc \
"$XMAKE" run artha-integration-test

# Reconfigure back to release build into sandbox/
"$XMAKE" config -o sandbox --prefix=/artha/sandbox/stage

# Clean sandbox so host-side tasks aren't affected by container-configured
# artifacts (container paths like /artha don't exist on the host).
rm -rf sandbox
