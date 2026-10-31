#!/usr/bin/env bash
set -euo pipefail

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
CORE_DIR="$ROOT/CipherNES_Core"
FRONTEND_DIR="$ROOT/CipherNES_Ubuntu"
OUT_DIR="$ROOT/build/ubuntu"
BIN_DIR="$OUT_DIR/bin"
OBJ_DIR="$OUT_DIR/obj"
CXX="${CXX:-g++}"

COMMON_FLAGS=(
    -std=gnu++11 -O3 -DNDEBUG -fomit-frame-pointer -fPIC
    -flto=4 -fno-semantic-interposition -fno-plt
    -ffunction-sections -fdata-sections
    -Wall -Wextra -Wno-unused-parameter -Wno-unused-function
    -Wno-misleading-indentation -Wno-missing-field-initializers
)
LINK_FLAGS=( -flto=4 -Wl,--gc-sections -Wl,-O1 -Wl,--as-needed )

if [[ "${CIPHERNES_NATIVE:-0}" == "1" ]]; then
    COMMON_FLAGS+=( -march=native -mtune=native )
fi

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
mkdir -p "$BIN_DIR" "$OBJ_DIR/core"

CORE_OBJECTS=()
BUILD_JOBS="${CIPHERNES_JOBS:-$(nproc)}"
if [[ "$BUILD_JOBS" -lt 1 ]]; then BUILD_JOBS=1; fi

compile_core_source() {
    local source="$1" object="$2"
    "$CXX" "${COMMON_FLAGS[@]}" -fvisibility=hidden -fvisibility-inlines-hidden \
        -I"$CORE_DIR" -c "$CORE_DIR/$source" -o "$object"
}

active_jobs=0
for source in "${CORE_SOURCES[@]}"; do
    object_name="${source//\//_}"
    object="$OBJ_DIR/core/${object_name%.cpp}.o"
    CORE_OBJECTS+=("$object")
    compile_core_source "$source" "$object" &
    active_jobs=$((active_jobs + 1))
    if [[ "$active_jobs" -ge "$BUILD_JOBS" ]]; then
        wait -n
        active_jobs=$((active_jobs - 1))
    fi
done
wait

# An internal static archive is retained for the headless timeline regression
# harness, not installed into any release output directory.
ar rcs "$OUT_DIR/libCipherNES_CORE.a" "${CORE_OBJECTS[@]}"

# Link the emulator core directly into the executable.  No separate core .so.
"$CXX" -std=gnu++11 -O3 -DNDEBUG -fomit-frame-pointer -fno-semantic-interposition -fno-plt \
    -ffunction-sections -fdata-sections -Wall -Wextra -Wno-unused-parameter -Wno-unused-function \
    -Wno-misleading-indentation -Wno-missing-field-initializers \
    -I"$CORE_DIR" -I"$FRONTEND_DIR" \
    "$FRONTEND_DIR/main_ubuntu.cpp" "$FRONTEND_DIR/zip_rom_loader.cpp" \
    "${CORE_OBJECTS[@]}" "${LINK_FLAGS[@]}" -ldl -pthread -o "$BIN_DIR/CipherNES"

cp "$ROOT/README.md" "$ROOT/LICENSE" "$ROOT/THIRD_PARTY_NOTICES.md" "$ROOT/VERSION" "$BIN_DIR/"

(
    cd "$BIN_DIR"
    hash_targets=(CipherNES)
    sha256sum "${hash_targets[@]}" > SHA256SUMS.txt
)

echo "CipherNES Ubuntu build complete: $BIN_DIR"
