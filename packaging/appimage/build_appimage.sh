#!/usr/bin/env bash
# Created by Jacob Hodgkins
# Build the CipherNES AppImage V2 package.
set -euo pipefail

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
VERSION="$(tr -d '[:space:]' < "$ROOT/VERSION")"
ARCH="${CIPHERNES_APPIMAGE_ARCH:-x86_64}"
BUILD_ROOT="$ROOT/build/appimage"
APPDIR="$BUILD_ROOT/CipherNES.AppDir"
BIN_DIR="${CIPHERNES_BIN_DIR:-$ROOT/build/ubuntu/bin}"
OUT_DIR="${CIPHERNES_APPIMAGE_OUT_DIR:-$ROOT/dist}"
OUTPUT="$OUT_DIR/CipherNES-${VERSION}-${ARCH}.AppImage"
TOOL="$BUILD_ROOT/appimagetool-${ARCH}.AppImage"

if [[ "$ARCH" != "x86_64" ]]; then
    echo "CipherNES AppImage V2 currently supports x86_64 only." >&2
    exit 1
fi

for cmd in curl desktop-file-validate appstreamcli ldconfig ldd; do
    command -v "$cmd" >/dev/null 2>&1 || {
        echo "Required command not found: $cmd" >&2
        exit 1
    }
done

if [[ "${CIPHERNES_SKIP_BUILD:-0}" != "1" ]]; then
    "$ROOT/build_ubuntu.sh"
fi

[[ -x "$BIN_DIR/CipherNES" ]] || { echo "Missing $BIN_DIR/CipherNES" >&2; exit 1; }

rm -rf "$BUILD_ROOT"
mkdir -p     "$APPDIR/usr/bin"     "$APPDIR/usr/lib"     "$APPDIR/usr/share/applications"     "$APPDIR/usr/share/icons/hicolor/scalable/apps"     "$APPDIR/usr/share/metainfo"     "$APPDIR/usr/share/licenses/ciphernes"     "$APPDIR/usr/share/doc/ciphernes/bios"     "$OUT_DIR"

install -m 0755 "$BIN_DIR/CipherNES" "$APPDIR/usr/bin/CipherNES"
install -m 0755 "$ROOT/packaging/appimage/AppRun" "$APPDIR/AppRun"
install -m 0644 "$ROOT/packaging/appimage/ciphernes.desktop" "$APPDIR/usr/share/applications/ciphernes.desktop"
install -m 0644 "$ROOT/packaging/appimage/ciphernes.svg" "$APPDIR/usr/share/icons/hicolor/scalable/apps/ciphernes.svg"
install -m 0644 "$ROOT/packaging/appimage/io.github.hodgkinsstudios.CipherNES.metainfo.xml" "$APPDIR/usr/share/metainfo/io.github.hodgkinsstudios.CipherNES.metainfo.xml"
install -m 0644 "$ROOT/LICENSE" "$APPDIR/usr/share/licenses/ciphernes/LICENSE"
install -m 0644 "$ROOT/THIRD_PARTY_NOTICES.md" "$APPDIR/usr/share/doc/ciphernes/THIRD_PARTY_NOTICES.md"
install -m 0644 "$ROOT/README.md" "$APPDIR/usr/share/doc/ciphernes/README.md"
install -m 0644 "$ROOT/bios/README.md" "$APPDIR/usr/share/doc/ciphernes/bios/README.md"

ln -s usr/share/applications/ciphernes.desktop "$APPDIR/ciphernes.desktop"
ln -s usr/share/icons/hicolor/scalable/apps/ciphernes.svg "$APPDIR/ciphernes.svg"
ln -s ciphernes.svg "$APPDIR/.DirIcon"

resolve_lib() {
    local name="$1"
    ldconfig -p | awk -v want="$name" '
        $1 == want && $0 ~ /x86-64/ { print $NF; exit }
    '
}

copy_runtime_lib() {
    local name="$1"
    local src
    src="$(resolve_lib "$name")"
    if [[ -z "$src" || ! -f "$src" ]]; then
        echo "Could not resolve required runtime library: $name" >&2
        exit 1
    fi
    install -m 0644 "$src" "$APPDIR/usr/lib/$name"
}

# CipherNES loads SDL2 dynamically, so it must be included explicitly.
# zlib and the GCC C++ runtimes are also bundled for portability.
copy_runtime_lib "libSDL2-2.0.so.0"
copy_runtime_lib "libz.so.1"
copy_runtime_lib "libstdc++.so.6"
copy_runtime_lib "libgcc_s.so.1"

desktop-file-validate "$APPDIR/usr/share/applications/ciphernes.desktop"
appstreamcli validate --no-net "$APPDIR/usr/share/metainfo/io.github.hodgkinsstudios.CipherNES.metainfo.xml"

if LD_LIBRARY_PATH="$APPDIR/usr/lib" ldd "$APPDIR/usr/bin/CipherNES" | grep -q "not found"; then
    LD_LIBRARY_PATH="$APPDIR/usr/lib" ldd "$APPDIR/usr/bin/CipherNES" >&2
    echo "CipherNES still has unresolved linked libraries." >&2
    exit 1
fi

curl -L --fail --retry 3     -o "$TOOL"     "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"
chmod +x "$TOOL"

rm -f "$OUTPUT" "$OUTPUT.sha256"
ARCH="$ARCH" VERSION="$VERSION" APPIMAGE_EXTRACT_AND_RUN=1     "$TOOL" "$APPDIR" "$OUTPUT"
chmod +x "$OUTPUT"
sha256sum "$OUTPUT" > "$OUTPUT.sha256"

echo "AppImage created: $OUTPUT"
echo "Checksum: $OUTPUT.sha256"
