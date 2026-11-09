#!/usr/bin/env bash
# Created by Jacob Hodgkins
# Build a native Debian/Ubuntu .deb package for CipherNES.
set -euo pipefail

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
VERSION="$(tr -d '[:space:]' < "$ROOT/VERSION")"
ARCH="${CIPHERNES_DEB_ARCH:-$(dpkg --print-architecture)}"
BIN_DIR="${CIPHERNES_BIN_DIR:-$ROOT/build/ubuntu/bin}"
OUT_DIR="${CIPHERNES_DEB_OUT_DIR:-$ROOT/dist}"
STAGE="$ROOT/build/debian/ciphernes_${VERSION}_${ARCH}"
PACKAGE="$OUT_DIR/CipherNES-${VERSION}-${ARCH}.deb"

command -v dpkg-deb >/dev/null 2>&1 || {
    echo "dpkg-deb is required to build the Debian package." >&2
    exit 1
}

if [[ "${CIPHERNES_SKIP_BUILD:-0}" != "1" ]]; then
    "$ROOT/build_ubuntu.sh"
fi

[[ -x "$BIN_DIR/CipherNES" ]] || { echo "Missing $BIN_DIR/CipherNES" >&2; exit 1; }

rm -rf "$STAGE"
mkdir -p     "$STAGE/DEBIAN"     "$STAGE/usr/bin"     "$STAGE/usr/lib/ciphernes"     "$STAGE/usr/share/applications"     "$STAGE/usr/share/doc/ciphernes/bios"     "$OUT_DIR"

find "$STAGE" -type d -exec chmod g-s {} +

install -m 0755 "$BIN_DIR/CipherNES" "$STAGE/usr/lib/ciphernes/CipherNES"
install -m 0755 "$ROOT/packaging/debian/ciphernes-launcher.sh" "$STAGE/usr/bin/ciphernes"
install -m 0644 "$ROOT/packaging/debian/ciphernes.desktop" "$STAGE/usr/share/applications/ciphernes.desktop"
install -m 0644 "$ROOT/LICENSE" "$STAGE/usr/share/doc/ciphernes/LICENSE"
install -m 0644 "$ROOT/THIRD_PARTY_NOTICES.md" "$STAGE/usr/share/doc/ciphernes/THIRD_PARTY_NOTICES.md"
install -m 0644 "$ROOT/README.md" "$STAGE/usr/share/doc/ciphernes/README.md"
install -m 0644 "$ROOT/bios/README.md" "$STAGE/usr/share/doc/ciphernes/bios/README.md"

cat > "$STAGE/DEBIAN/control" <<EOF2
Package: ciphernes
Version: $VERSION
Section: games
Priority: optional
Architecture: $ARCH
Maintainer: HodgkinsStudios
Homepage: https://github.com/HodgkinsStudios/CipherNES
Depends: libc6 (>= 2.34), libstdc++6, libgcc-s1, libsdl2-2.0-0, zlib1g
Recommends: zenity | kdialog
Description: Native C++ NES/Famicom emulator
 CipherNES is a native C++ NES/Famicom emulator with an NTSC timing target,
 broad built-in mapper support, Famicom Disk System support with a user-supplied
 BIOS, save states, Game Genie support, gamepad configuration, screenshots,
 recording, and TAS input tools.
EOF2

cat > "$STAGE/DEBIAN/postinst" <<'EOF2'
#!/bin/sh
set -e
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database -q /usr/share/applications || true
fi
exit 0
EOF2
chmod 0755 "$STAGE/DEBIAN/postinst"

cat > "$STAGE/DEBIAN/postrm" <<'EOF2'
#!/bin/sh
set -e
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database -q /usr/share/applications || true
fi
exit 0
EOF2
chmod 0755 "$STAGE/DEBIAN/postrm"

rm -f "$PACKAGE"
dpkg-deb --root-owner-group --build "$STAGE" "$PACKAGE"
sha256sum "$PACKAGE" > "$PACKAGE.sha256"

echo "Debian package created: $PACKAGE"
echo "Checksum: $PACKAGE.sha256"
