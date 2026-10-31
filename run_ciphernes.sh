#!/usr/bin/env bash
set -euo pipefail
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
BIN="$ROOT/build/ubuntu/bin/CipherNES"
if [[ ! -x "$BIN" ]]; then
    echo "CipherNES is not built. Run ./build_ubuntu.sh first." >&2
    exit 2
fi
exec "$BIN" "$@"
