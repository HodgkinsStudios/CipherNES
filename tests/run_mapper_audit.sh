#!/usr/bin/env bash
# Cartridge mapper smoke and focused hardware-register regressions.
# Uses original synthetic ROM bytes only; no commercial game ROMs required.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
if [[ ! -f "$ROOT/build/ubuntu/libCipherNES_CORE.a" ]]; then
    CIPHERNES_JOBS="${CIPHERNES_JOBS:-4}" "$ROOT/build_ubuntu.sh"
fi
mkdir -p "$ROOT/build/tests"
g++ -std=c++11 -O2 -Wall -Wextra -I"$ROOT/CipherNES_Core" \
    "$ROOT/tests/mapper_audit_regression.cpp" "$ROOT/build/ubuntu/libCipherNES_CORE.a" \
    -ldl -pthread -o "$ROOT/build/tests/mapper_audit_regression"
ROM="$(mktemp "$ROOT/build/tests/mapper_audit_XXXXXX.nes")"
# Isolate shared Datach accessory NVRAM: never read or create a player's save.
export CIPHERNES_DATACH_SAVE_PATH="$(mktemp "$ROOT/build/tests/mapper_datach_XXXXXX.sav")"
SAVE_STEM="$(basename "${ROM%.nes}")"
trap 'rm -f "$ROM" "$CIPHERNES_DATACH_SAVE_PATH" "Saves/${SAVE_STEM}.SAV"' EXIT
"$ROOT/build/tests/mapper_audit_regression" "$ROM"
