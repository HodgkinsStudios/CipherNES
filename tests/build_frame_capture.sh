#!/usr/bin/env bash
# Test-only frame capture / per-frame input tracing; produces data outside source.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ ! -f "$ROOT/build/ubuntu/libCipherNES_CORE.a" ]]; then
  CIPHERNES_LTO=0 CIPHERNES_JOBS="${CIPHERNES_JOBS:-4}" "$ROOT/build_ubuntu.sh"
fi
mkdir -p "$ROOT/build/tests"
g++ -std=c++11 -O2 -I"$ROOT/CipherNES_Core" \
  "$ROOT/tests/frame_capture.cpp" "$ROOT/tests/debug_bridge.cpp" \
  "$ROOT/build/ubuntu/libCipherNES_CORE.a" -ldl -pthread \
  -o "$ROOT/build/tests/frame_capture"
echo "$ROOT/build/tests/frame_capture"
