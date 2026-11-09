#!/usr/bin/env bash
# Created by Jacob Hodgkins
# Build a standalone Flatpak bundle for CipherNES.
set -euo pipefail

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
APP_ID="io.github.hodgkinsstudios.CipherNES"
VERSION="$(tr -d '[:space:]' < "$ROOT/VERSION")"
ARCH="${CIPHERNES_FLATPAK_ARCH:-$(flatpak --default-arch)}"
MANIFEST="$ROOT/packaging/flatpak/$APP_ID.yml"
BUILD_ROOT="$ROOT/build/flatpak"
BUILD_DIR="$BUILD_ROOT/build-dir"
REPO_DIR="$BUILD_ROOT/repo"
OUT_DIR="${CIPHERNES_FLATPAK_OUT_DIR:-$ROOT/dist}"
BUNDLE="$OUT_DIR/CipherNES-${VERSION}-${ARCH}.flatpak"

command -v flatpak >/dev/null 2>&1 || {
    echo "flatpak is required." >&2
    exit 1
}
command -v flatpak-builder >/dev/null 2>&1 || {
    echo "flatpak-builder is required." >&2
    exit 1
}

rm -rf "$BUILD_ROOT"
mkdir -p "$BUILD_ROOT" "$OUT_DIR"

flatpak-builder     --force-clean     --repo="$REPO_DIR"     --default-branch=stable     "$BUILD_DIR"     "$MANIFEST"

rm -f "$BUNDLE"
flatpak build-bundle "$REPO_DIR" "$BUNDLE" "$APP_ID" stable
sha256sum "$BUNDLE" > "$BUNDLE.sha256"

echo "Flatpak bundle created: $BUNDLE"
echo "Checksum: $BUNDLE.sha256"
