#!/usr/bin/env bash
# External ROM is provided by the owner; no copyrighted test ROM is stored here.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ROM="${1:?Usage: tests/run_accuracycoin.sh /absolute/path/to/AccuracyCoin.nes [/tmp/accuracycoin_final.ppm]}"
OUT="${2:-/tmp/ciphernes_accuracycoin_final.ppm}"
[[ -f "$ROM" ]] || { echo "ROM missing: $ROM" >&2; exit 2; }
if [[ ! -f "$ROOT/build/ubuntu/libCipherNES_CORE.a" ]]; then
  CIPHERNES_LTO=0 CIPHERNES_JOBS=4 "$ROOT/build_ubuntu.sh"
fi
mkdir -p "$ROOT/build/tests"
"${CXX:-g++}" -std=c++11 -O2 -I"$ROOT/CipherNES_Core" \
  "$ROOT/tests/verdict_probe_core.cpp" "$ROOT/tests/accuracycoin_full_run.cpp" \
  "$ROOT/build/ubuntu/libCipherNES_CORE.a" -ldl -pthread -o "$ROOT/build/tests/accuracycoin_full_run"
"$ROOT/build/tests/accuracycoin_full_run" "$ROM" "$OUT"
echo "A completed run is not a verdict. Inspect the final PPM screen for the displayed PASS tally."
