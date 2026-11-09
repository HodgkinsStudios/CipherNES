#!/usr/bin/env bash
# Created by Jacob Hodgkins
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$ROOT/build/tests"
g++ -std=c++11 -O2 -Wall -Wextra "$ROOT/.github/ci/host_audio_regression.cpp" -o "$ROOT/build/tests/host_audio_regression"
"$ROOT/build/tests/host_audio_regression"
