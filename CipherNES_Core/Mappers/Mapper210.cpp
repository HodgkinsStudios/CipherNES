#include "Mapper210.h"
#include "../state_io.h"

#include <algorithm>
#include <cstring>

Mapper210::Mapper210(uint16_t p, uint16_t c, uint8_t submapper, bool batteryOrPrgRam)
    : Mapper(p, c), variant(submapper == 1u ? Namco175 :
                           submapper == 2u ? Namco340 :
                           batteryOrPrgRam ? Namco175 : Namco340) {
    reset();
}

uint16_t Mapper210::prgCount8() const {
    return std::max<uint16_t>(1u, uint16_t(prgBanks * 2u));
}
uint16_t Mapper210::chrCount1() const {
    return chrBanks ? uint16_t(chrBanks * 8u) : 8u;
}

void Mapper210::reset() {
    std::memset(prg8, 0, sizeof(prg8));
    std::memset(chr1, 0, sizeof(chr1));
    writeProtect = 0;
    // Namco 175 mirroring is board-wired and therefore left to the header.
    // Namco 340 powers up in its first one-screen mode.
    mirrorMode = (variant == Namco340) ? 3u : 0xFFu;
}

bool Mapper210::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr >= 0x6000u && addr <= 0x7FFFu) {
        if (variant != Namco175) return false;
        mappedAddr = PRG_RAM_FLAG | uint32_t(addr & 0x1FFFu);
        return true;
    }
    if (addr < 0x8000u) return false;

    const uint16_t count = prgCount8();
    const uint8_t slot = uint8_t((addr - 0x8000u) >> 13);
    const uint16_t page = (slot < 3u) ? uint16_t(prg8[slot] % count)
                                     : uint16_t(count - 1u);
    mappedAddr = uint32_t(page) * 0x2000u + uint32_t(addr & 0x1FFFu);
    return true;
}

bool Mapper210::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    mappedAddr = 0;
    if (addr >= 0x6000u && addr <= 0x7FFFu) {
        if (variant != Namco175) return false;
        if (writeProtect & 0x01u) mappedAddr = PRG_RAM_FLAG | uint32_t(addr & 0x1FFFu);
        return true; // disabled writes are electrically ignored
    }
    if (addr < 0x8000u) return false;

    switch (addr & 0xF800u) {
        case 0x8000: case 0x8800: case 0x9000: case 0x9800:
        case 0xA000: case 0xA800: case 0xB000: case 0xB800: {
            const uint8_t bank = uint8_t((addr - 0x8000u) >> 11);
            chr1[bank] = data;
            break;
        }
        case 0xC000:
            if (variant == Namco175) writeProtect = data;
            break;
        case 0xE000:
            prg8[0] = uint16_t(data & 0x3Fu);
            if (variant == Namco340) {
                // Namco 340 encoding: 0=A, 1=V, 2=H, 3=B.
                static const uint8_t modes[4] = { 3u, 1u, 0u, 4u };
                mirrorMode = modes[(data >> 6) & 3u];
            }
            break;
        case 0xE800: prg8[1] = uint16_t(data & 0x3Fu); break;
        case 0xF000: prg8[2] = uint16_t(data & 0x3Fu); break;
        default: break; // $C800-$DFFF are absent on both reduced ASICs
    }
    return true;
}

bool Mapper210::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr > 0x1FFFu) return false;
    const uint8_t slot = uint8_t(addr >> 10);
    const uint16_t page = uint16_t(chr1[slot] % chrCount1());
    mappedAddr = uint32_t(page) * 0x0400u + uint32_t(addr & 0x03FFu);
    return true;
}

bool Mapper210::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (chrBanks != 0u || addr > 0x1FFFu) return false;
    const uint8_t slot = uint8_t(addr >> 10);
    mappedAddr = uint32_t(chr1[slot] % chrCount1()) * 0x0400u + uint32_t(addr & 0x03FFu);
    return true;
}

void Mapper210::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    write_u8(out, uint8_t(variant));
    for (uint8_t i = 0; i < 3; ++i) write_u16(out, prg8[i]);
    for (uint8_t i = 0; i < 8; ++i) write_u16(out, chr1[i]);
    write_u8(out, writeProtect);
    write_u8(out, mirrorMode);
}

bool Mapper210::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    uint8_t loadedVariant = 0;
    if (!read_u8(p, end, loadedVariant) || loadedVariant < 1u || loadedVariant > 2u) return false;
    variant = Variant(loadedVariant);
    for (uint8_t i = 0; i < 3; ++i) if (!read_u16(p, end, prg8[i])) return false;
    for (uint8_t i = 0; i < 8; ++i) if (!read_u16(p, end, chr1[i])) return false;
    return read_u8(p, end, writeProtect) && read_u8(p, end, mirrorMode);
}
