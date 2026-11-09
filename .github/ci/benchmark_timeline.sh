#!/usr/bin/env bash
# Created by Jacob Hodgkins
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
if [[ $# -lt 1 ]]; then echo "Usage: .github/ci/benchmark_timeline.sh <owned-rom.nes> [frames]" >&2; exit 2; fi
if [[ ! -e "$ROOT/build/ubuntu/libCipherNES_CORE.a" ]]; then CIPHERNES_JOBS=4 "$ROOT/build_ubuntu.sh"; fi
mkdir -p "$ROOT/build/tests"
g++ -std=c++11 -O3 -DNDEBUG -I"$ROOT/CipherNES_Core" "$ROOT/.github/ci/benchmark_timeline.cpp" \
    -L"$ROOT/build/ubuntu" -lCipherNES_CORE -ldl -pthread \
    -o "$ROOT/build/tests/benchmark_timeline"
"$ROOT/build/tests/benchmark_timeline" "$@"
