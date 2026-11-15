#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${MAPPER033_OUTPUT_DIR:-$ROOT/build/mapper033_gate}"
mkdir -p "$OUT"
g++ -std=c++11 -O2 -Wall -Wextra -I"$ROOT/CipherNES_Core" \
    "$ROOT/tests/mapper033_hardware_regression.cpp" \
    "$ROOT/CipherNES_Core/Mappers/Mapper033.cpp" -o "$OUT/mapper033_hardware_regression"
"$OUT/mapper033_hardware_regression" 2>&1 | tee "$OUT/hardware_gate.log"
# Independently retain the complete 19-row Mapper33 bank-mapping test.
g++ -std=c++11 -O2 -Wall -Wextra -I"$ROOT/CipherNES_Core" \
    "$ROOT/tests/mapper033_bank_probe.cpp" "$ROOT/CipherNES_Core/Mappers/Mapper033.cpp" \
    -o "$OUT/mapper033_bank_probe"
"$OUT/mapper033_bank_probe" > "$OUT/mapper033_bank_mapping.csv" 2> "$OUT/mapper033_probe_stderr.txt"
if grep -q MISMATCH "$OUT/mapper033_bank_mapping.csv"; then
    cat "$OUT/mapper033_probe_stderr.txt" >&2
    exit 1
fi
cat "$OUT/mapper033_probe_stderr.txt"
echo "Mapper 33 all synthetic mapping expectations passed"
