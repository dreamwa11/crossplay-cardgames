#!/bin/sh
# Checks the Severance port against the phone version's own engine: both play
# the same seeded quarters with the same policy and must print the same state
# and the same log after every action. Needs node for the phone side.
#
#   host-tests/severance/run.sh [games]
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-severance-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/severance
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC trace.cpp $SRC/SeveranceCore.cpp -o "$BUILD_DIR/trace"
N="${1:-200}"
for L in 0 1; do
  "$BUILD_DIR/trace" "$N" "$L" > "$BUILD_DIR/port.txt"
  node phone_trace.js "$N" "$L" > "$BUILD_DIR/phone.txt"
  if ! cmp -s "$BUILD_DIR/port.txt" "$BUILD_DIR/phone.txt"; then
    echo "FAIL: the port and the phone version differ (easier Milchick: $L)"
    diff "$BUILD_DIR/phone.txt" "$BUILD_DIR/port.txt" | head -20
    exit 1
  fi
  echo "ok: $N quarters identical (easier Milchick: $L), $(grep -c 'END 1' "$BUILD_DIR/port.txt") won"
done
