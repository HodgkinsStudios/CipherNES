#!/usr/bin/env bash
# State-replay regression gate. ROM bytes supplied externally by user; NEVER bundled in source.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out="${STATE_GATE_OUTPUT_DIR:-$root/build/state_gate}"
if (( $# < 1 )); then
    echo "Usage: bash tests/run_state_gate.sh /path/to/your/ROM.nes [more-ROMs...]" >&2
    exit 2
fi
mkdir -p "$out"
if [[ ! -f "$root/build/ubuntu/libCipherNES_CORE.a" ]]; then
    CIPHERNES_LTO=0 CIPHERNES_JOBS="${CIPHERNES_JOBS:-4}" bash "$root/build_ubuntu.sh"
fi
g++ -std=c++11 -O2 -Wall -Wextra -I"$root/CipherNES_Core" \
    "$root/tests/state_replay_regression.cpp" "$root/build/ubuntu/libCipherNES_CORE.a" \
    -ldl -pthread -o "$out/state_replay_regression"
for rom in "$@"; do
    printf '\n=== state v12/v11 gate: %s ===\n' "$rom"
    "$out/state_replay_regression" "$rom"
done
