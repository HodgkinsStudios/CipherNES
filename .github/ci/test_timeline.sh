#!/usr/bin/env bash
# Created by Jacob Hodgkins
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ROM="${1:?Usage: .github/ci/test_timeline.sh <legally-owned-rom.nes>}"
if [[ ! -f "$ROOT/build/ubuntu/libCipherNES_CORE.a" ]]; then
    CIPHERNES_JOBS=4 "$ROOT/build_ubuntu.sh"
fi
mkdir -p "$ROOT/build/tests"
g++ -std=c++11 -O2 -I"$ROOT/CipherNES_Core" "$ROOT/.github/ci/timeline_regression.cpp" -L"$ROOT/build/ubuntu" -lCipherNES_CORE -ldl -pthread -o "$ROOT/build/tests/timeline_regression"
"$ROOT/build/tests/timeline_regression" "$ROM"
