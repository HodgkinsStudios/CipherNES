#!/usr/bin/env bash
set -euo pipefail
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
VERSION="$(tr -d '[:space:]' < "$ROOT/VERSION")"
DIST="$ROOT/dist"
STAGE="$DIST/CipherNES-$VERSION-Ubuntu-x86_64"

"$ROOT/build_ubuntu.sh"

rm -rf "$DIST"
mkdir -p "$STAGE/docs"
cp "$ROOT/build/ubuntu/bin/CipherNES" "$STAGE/"
cp "$ROOT/LICENSE" "$ROOT/THIRD_PARTY_NOTICES.md" "$ROOT/CHANGELOG.md" "$ROOT/RELEASE_NOTES_${VERSION}.md" "$ROOT/VERSION" "$STAGE/"
cp "$ROOT/docs/VALIDATION.md" "$ROOT/docs/FEATURES.md" "$STAGE/docs/"

cat > "$STAGE/run_ciphernes.sh" <<'RUNEOF'
#!/usr/bin/env bash
set -euo pipefail
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
export LD_LIBRARY_PATH="$ROOT${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$ROOT/CipherNES" "$@"
RUNEOF
chmod +x "$STAGE/run_ciphernes.sh"

cat > "$STAGE/README.md" <<EOF2
# CipherNES $VERSION — Ubuntu x86_64

Created by Jacob Hodgkins.

This is the prebuilt Ubuntu release of CipherNES $VERSION, targeting NTSC timing.

## Accuracy

- AccuracyCoin (updated suite): **146 / 146 (100%)**, measured on the repaired Ubuntu core on October 21, 2026.
- Earlier NTSC validation groups have historical results recorded in the source documentation; they were not all rerun after the RAM repair.
- CI compiles and packages the supported desktop targets; full AccuracyCoin suites on Windows and macOS have not been independently recorded.

See \`docs/VALIDATION.md\` for the full certification record.

## Runtime requirements

Install SDL2 and zlib runtime packages:

\`\`\`bash
sudo apt update
sudo apt install libsdl2-2.0-0 zlib1g zenity
\`\`\`

## Run

\`\`\`bash
chmod +x run_ciphernes.sh CipherNES
./run_ciphernes.sh /path/to/game.nes
\`\`\`

You may also run \`./run_ciphernes.sh\` without a ROM and open one from the frontend. Native desktop file selection requires \`zenity\` or \`kdialog\` on Linux.

## Famicom Disk System

The FDS BIOS is not included. Put disksys.rom in \`~/.local/share/HodgkinsStudios/CipherNES/bios/\`.

## License

See \`LICENSE\` and \`THIRD_PARTY_NOTICES.md\`.
EOF2

(
    cd "$STAGE"
    sha256sum CipherNES run_ciphernes.sh > SHA256SUMS.txt
)
(
    cd "$DIST"
    zip -qr "CipherNES-$VERSION-Ubuntu-x86_64.zip" "$(basename "$STAGE")"
)

SRC_STAGE="$DIST/CipherNES-$VERSION-Source"
mkdir -p "$SRC_STAGE"
(
    cd "$ROOT"
    tar --exclude='./build' --exclude='./dist' --exclude='./.git' \
        --exclude='./tests' --exclude='./Videos'  \
        --exclude='./SaveState' --exclude='./TAS' --exclude='./Saves' \
        --exclude='./Screenshots' --exclude='./Cheats' --exclude='./config' \
        --exclude='./logs' --exclude='./roms' --exclude='./saves' --exclude='./runtime' \
        --exclude='./bios/disksys.rom' \
        --exclude='*.nes' --exclude='*.fds' --exclude='*.qd' \
        --exclude='*.nss' --exclude='*.ci' --exclude='*.avi' \
        --exclude='*.state' --exclude='*.SAV' --exclude='*.sav' \
        --exclude='./LOCAL_REPAIR_HANDOFF_2026-10-21.md' \
        --exclude='./*.user' --exclude='./*.layout' --exclude='./*.depend' \
        --exclude='__pycache__' --exclude='*.pyc' --exclude='.DS_Store' -cf - .
) | (cd "$SRC_STAGE" && tar -xf -)
(
    cd "$DIST"
    zip -qr "CipherNES-$VERSION-Source.zip" "$(basename "$SRC_STAGE")"
)

(
    cd "$DIST"
    sha256sum "CipherNES-$VERSION-Source.zip" "CipherNES-$VERSION-Ubuntu-x86_64.zip" > SHA256SUMS.txt
)

echo "Release archives created in $DIST"
