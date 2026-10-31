#!/usr/bin/env bash
set -euo pipefail

# Created by Jacob Hodgkins
# CipherNES native macOS build. Produces a self-contained CipherNES.app for the
# current Mac architecture (Intel x86_64 or Apple Silicon arm64).

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
CORE_DIR="$ROOT/CipherNES_Core"
MAC_DIR="$ROOT/CipherNES_macOS"
UNIX_DIR="$ROOT/CipherNES_Ubuntu"
OUT_DIR="$ROOT/build/macos"
APP_DIR="$OUT_DIR/CipherNES.app"
CONTENTS="$APP_DIR/Contents"
MACOS_DIR="$CONTENTS/MacOS"
FRAMEWORKS_DIR="$CONTENTS/Frameworks"
RESOURCES_DIR="$CONTENTS/Resources"
OBJ_DIR="$OUT_DIR/obj/core"
CXX="${CXX:-clang++}"
ARCH="$(uname -m)"

COMMON_FLAGS=(
    -std=gnu++11 -O3 -DNDEBUG -fPIC
    -ffunction-sections -fdata-sections
    -fvisibility=hidden -fvisibility-inlines-hidden
    -Wall -Wextra -Wno-unused-parameter -Wno-unused-function
    -Wno-misleading-indentation -Wno-missing-field-initializers
)
LINK_FLAGS=( -Wl,-dead_strip )

CORE_SOURCES=(
    apu2A03.cpp bus.cpp cartridge.cpp cpu6502.cpp game_genie.cpp ciphernes_core.cpp ppu2C02.cpp
    Mappers/Mapper000.cpp Mappers/Mapper001.cpp Mappers/Mapper002.cpp
    Mappers/Mapper003.cpp Mappers/Mapper004.cpp Mappers/Mapper005.cpp
    Mappers/Mapper006.cpp Mappers/Mapper007.cpp Mappers/Mapper009.cpp
    Mappers/Mapper010.cpp Mappers/Mapper011.cpp Mappers/Mapper013.cpp Mappers/Mapper015.cpp
    Mappers/Mapper016.cpp Mappers/Mapper017.cpp Mappers/Mapper018.cpp
    Mappers/Mapper019.cpp Mappers/Mapper020.cpp Mappers/Mapper210.cpp Mappers/Mapper022.cpp Mappers/Mapper023.cpp Mappers/MapperVrc6.cpp
    Mappers/Mapper028.cpp Mappers/Mapper032.cpp Mappers/Mapper033.cpp
    Mappers/Mapper034.cpp Mappers/Mapper053.cpp Mappers/Mapper064.cpp Mappers/Mapper065.cpp
    Mappers/Mapper066.cpp Mappers/Mapper067.cpp Mappers/Mapper068.cpp
    Mappers/Mapper069.cpp Mappers/Mapper070.cpp Mappers/Mapper071.cpp
    Mappers/Mapper075.cpp Mappers/Mapper077.cpp Mappers/Mapper078.cpp Mappers/Mapper079.cpp
    Mappers/Mapper080.cpp Mappers/Mapper082.cpp Mappers/Mapper087.cpp
    Mappers/Mapper091.cpp Mappers/Mapper097.cpp Mappers/Mapper099.cpp Mappers/Mapper105.cpp Mappers/Mapper111.cpp
    Mappers/Mapper113.cpp Mappers/Mapper130.cpp Mappers/Mapper160.cpp Mappers/Mapper180.cpp Mappers/Mapper184.cpp Mappers/Mapper188.cpp
    Mappers/Mapper185.cpp Mappers/Mapper193.cpp Mappers/Mapper222.cpp
    Mappers/Mapper232.cpp Mappers/Mapper234.cpp Mappers/Mapper073.cpp Mappers/Mapper083.cpp Mappers/Mapper085.cpp
    Mappers/MapperCommonBoards.cpp Mappers/MapperAdditionalBoards.cpp
    Mappers/MapperJY.cpp Mappers/MapperKaiser202.cpp Mappers/MapperMmc3Variant.cpp Mappers/MapperJalecoSimple.cpp Mappers/MapperMulticart.cpp Mappers/MapperNamco108.cpp Mappers/Mapper252.cpp
)

rm -rf "$OUT_DIR"
mkdir -p "$MACOS_DIR" "$FRAMEWORKS_DIR" "$RESOURCES_DIR" "$OBJ_DIR"

CORE_OBJECTS=()
for source in "${CORE_SOURCES[@]}"; do
    object_name="${source//\//_}"
    object="$OBJ_DIR/${object_name%.cpp}.o"
    CORE_OBJECTS+=("$object")
    "$CXX" "${COMMON_FLAGS[@]}" -DCIPHERNES_CORE_BUILD -I"$CORE_DIR"         -c "$CORE_DIR/$source" -o "$object"
done

find_sdl2() {
    if [[ -n "${SDL2_DYLIB:-}" && -f "$SDL2_DYLIB" ]]; then
        printf '%s\n' "$SDL2_DYLIB"
        return 0
    fi
    if command -v brew >/dev/null 2>&1; then
        local prefix
        prefix="$(brew --prefix sdl2 2>/dev/null || true)"
        for candidate in             "$prefix/lib/libSDL2-2.0.0.dylib"             "$prefix/lib/libSDL2.dylib"; do
            if [[ -f "$candidate" ]]; then printf '%s\n' "$candidate"; return 0; fi
        done
    fi
    for candidate in         /opt/homebrew/opt/sdl2/lib/libSDL2-2.0.0.dylib         /opt/homebrew/opt/sdl2/lib/libSDL2.dylib         /usr/local/opt/sdl2/lib/libSDL2-2.0.0.dylib         /usr/local/opt/sdl2/lib/libSDL2.dylib; do
        if [[ -f "$candidate" ]]; then printf '%s\n' "$candidate"; return 0; fi
    done
    return 1
}

SDL2_SOURCE="$(find_sdl2 || true)"
if [[ -z "$SDL2_SOURCE" ]]; then
    echo "SDL2 was not found. Install it with: brew install sdl2" >&2
    exit 1
fi
cp -L "$SDL2_SOURCE" "$FRAMEWORKS_DIR/libSDL2-2.0.0.dylib"

# Homebrew may provide SDL2 through sdl2-compat, which can itself depend on
# another non-system dylib (currently SDL3). Bundle every absolute non-system
# dylib dependency and rewrite it to @loader_path so the finished .app does not
# require Homebrew on the user's Mac.
bundle_non_system_dependencies() {
    local pass target dep base dest candidate brew_prefix
    brew_prefix="$(brew --prefix 2>/dev/null || true)"
    for pass in 1 2 3 4; do
        for target in "$FRAMEWORKS_DIR"/*.dylib; do
            [[ -f "$target" ]] || continue
            install_name_tool -id "@rpath/$(basename "$target")" "$target" 2>/dev/null || true
            while IFS= read -r dep; do
                case "$dep" in
                    /usr/lib/*|/System/Library/*) continue ;;
                    @rpath/*|@loader_path/*)
                        base="$(basename "$dep")"
                        dest="$FRAMEWORKS_DIR/$base"
                        if [[ ! -f "$dest" && -n "$brew_prefix" ]]; then
                            candidate=""
                            if [[ -f "$brew_prefix/lib/$base" ]]; then
                                candidate="$brew_prefix/lib/$base"
                            elif [[ -d "$brew_prefix/opt" ]]; then
                                candidate="$(find "$brew_prefix/opt" -maxdepth 4 -name "$base" -print -quit 2>/dev/null || true)"
                            fi
                            if [[ -n "$candidate" && -f "$candidate" ]]; then
                                cp -L "$candidate" "$dest"
                                chmod u+w "$dest"
                                install_name_tool -id "@rpath/$base" "$dest" 2>/dev/null || true
                            fi
                        fi
                        install_name_tool -change "$dep" "@loader_path/$base" "$target" 2>/dev/null || true
                        ;;
                    @executable_path/*)
                        continue
                        ;;
                    /*)
                        [[ -f "$dep" ]] || {
                            echo "Unresolved non-system dylib dependency: $dep (from $target)" >&2
                            exit 1
                        }
                        base="$(basename "$dep")"
                        dest="$FRAMEWORKS_DIR/$base"
                        if [[ ! -f "$dest" ]]; then
                            cp -L "$dep" "$dest"
                            chmod u+w "$dest"
                            install_name_tool -id "@rpath/$base" "$dest" 2>/dev/null || true
                        fi
                        install_name_tool -change "$dep" "@loader_path/$base" "$target"
                        ;;
                esac
            done < <(otool -L "$target" | awk 'NR > 1 { print $1 }')
        done
    done
}

bundle_non_system_dependencies

# install_name_tool changes invalidate a Homebrew dylib's original signature.
# All embedded libraries must be re-signed after relocation, especially for
# arm64 macOS where unsigned Mach-O code can be rejected by the loader.
for dylib in "$FRAMEWORKS_DIR"/*.dylib; do
    [[ -f "$dylib" ]] || continue
    codesign --force --sign - "$dylib"
    codesign --verify "$dylib"
done

# Embed every bundled SDL2 dylib (including transitive Homebrew dependencies)
# into the Mach-O executable. No external library files are needed alongside it.
EMBEDDED_FLAGS=()
MANIFEST_PATH="$OUT_DIR/embedded_runtime_manifest.txt"
: > "$MANIFEST_PATH"
index=0
for dylib in "$FRAMEWORKS_DIR"/*.dylib; do
    [[ -f "$dylib" ]] || continue
    if [[ "$index" -ge 50 ]]; then echo "Too many macOS embedded dylibs" >&2; exit 1; fi
    printf '%s\n' "$(basename "$dylib")" >> "$MANIFEST_PATH"
    printf -v section '__CN%02d' "$index"
    EMBEDDED_FLAGS+=( -Wl,-sectcreate,__DATA,"$section","$dylib" )
    index=$((index + 1))
done
[[ "$index" -ge 1 ]] || { echo "No SDL2 libraries for embedding" >&2; exit 1; }
EMBEDDED_FLAGS+=( -Wl,-sectcreate,__DATA,__CNMAP,"$MANIFEST_PATH" )

"$CXX" -std=gnu++11 -O3 -DNDEBUG -Wall -Wextra \
    -Wno-unused-parameter -Wno-unused-function -Wno-misleading-indentation \
    -Wno-missing-field-initializers -I"$CORE_DIR" -I"$UNIX_DIR" -I"$MAC_DIR" \
    "$MAC_DIR/main_macos.cpp" "$UNIX_DIR/zip_rom_loader.cpp" \
    "${CORE_OBJECTS[@]}" "${EMBEDDED_FLAGS[@]}" -Wl,-dead_strip -pthread -o "$MACOS_DIR/CipherNES"
# The application bundle retains Info.plist/documents, but no additional dylibs.
rm -f "$FRAMEWORKS_DIR"/*.dylib
cp "$MACOS_DIR/CipherNES" "$OUT_DIR/CipherNES-macOS-$ARCH-single-file"
chmod 0755 "$OUT_DIR/CipherNES-macOS-$ARCH-single-file"

VERSION_TEXT="$(tr -d '\r\n' < "$ROOT/VERSION" | sed -E 's/^[^0-9]*//; s/[^0-9.].*$//')"
[[ -n "$VERSION_TEXT" ]] || VERSION_TEXT="1.0.0"

cat > "$CONTENTS/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key><string>en</string>
    <key>CFBundleDisplayName</key><string>CipherNES</string>
    <key>CFBundleExecutable</key><string>CipherNES</string>
    <key>CFBundleIdentifier</key><string>com.hodgkinsstudios.ciphernes</string>
    <key>CFBundleInfoDictionaryVersion</key><string>6.0</string>
    <key>CFBundleName</key><string>CipherNES</string>
    <key>CFBundlePackageType</key><string>APPL</string>
    <key>CFBundleShortVersionString</key><string>$VERSION_TEXT</string>
    <key>CFBundleVersion</key><string>$VERSION_TEXT</string>
    <key>LSApplicationCategoryType</key><string>public.app-category.games</string>
    <key>LSMinimumSystemVersion</key><string>12.0</string>
    <key>NSHighResolutionCapable</key><true/>
</dict>
</plist>
EOF

cp "$ROOT/README.md" "$ROOT/LICENSE" "$ROOT/THIRD_PARTY_NOTICES.md" "$ROOT/VERSION" "$RESOURCES_DIR/"

# Ad-hoc signing keeps the locally built bundle internally consistent. Public
# Gatekeeper distribution can later replace this with Developer ID signing and
# Apple notarization without changing the source port.
codesign --force --deep --sign - "$APP_DIR"
# The separately distributed single-file executable is not the .app's binary
# after signing; it must carry its own valid ad-hoc signature.
codesign --force --sign - "$OUT_DIR/CipherNES-macOS-$ARCH-single-file"
codesign --verify "$OUT_DIR/CipherNES-macOS-$ARCH-single-file"

ZIP_PATH="$OUT_DIR/CipherNES-macOS-$ARCH.zip"
ditto -c -k --sequesterRsrc --keepParent "$APP_DIR" "$ZIP_PATH"
shasum -a 256 "$ZIP_PATH" > "$OUT_DIR/SHA256SUMS.txt"

echo "CipherNES macOS build complete: $APP_DIR"
echo "Architecture: $ARCH"
echo "Package: $ZIP_PATH"
