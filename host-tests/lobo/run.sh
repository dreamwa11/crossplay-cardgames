#!/bin/sh
# Builds and runs the Lobo rules tests. No device and no PlatformIO:
# LoboCore is freestanding C++17.
#
#   host-tests/lobo/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-lobo-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/lobo
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC \
  test_lobo.cpp $SRC/LoboCore.cpp -o "$BUILD_DIR/test_lobo"
"$BUILD_DIR/test_lobo" "$@"
