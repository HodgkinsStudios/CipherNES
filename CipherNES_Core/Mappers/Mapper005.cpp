#include "Mapper005.h"

#include "../state_io.h"

#include <algorithm>
#include <cstring>

namespace {
constexpr uint8_t FETCH_UNKNOWN    = 0;
constexpr uint8_t FETCH_NAMETABLE  = 1;
constexpr uint8_t FETCH_ATTRIBUTE  = 2;
constexpr uint8_t FETCH_PATTERN_LO = 3;
constexpr uint8_t FETCH_PATTERN_HI = 4;
constexpr uint8_t FETCH_SPRITE_LO  = 5;
constexpr uint8_t FETCH_SPRITE_HI  = 6;
}

Mapper005::Mapper005(uint8_t prgBanks, uint8_t chrBanks)
    : Mapper(prgBanks, chrBanks) {
    reset();
}

bool Mapper005::prgRamWriteEnabled() const {
    return ((prgProtect1 & 0x03u) == 0x02u)
        && ((prgProtect2 & 0x03u) == 0x01u);
}

void Mapper005::reset() {
    prgMode = 3;
    chrMode = 3;
    prgProtect1 = 0;
    prgProtect2 = 0;
    exRamMode = 0;
    nametableMap = 0;
    fillTile = 0;
    fillColor = 0;
    chrUpper = 0;

    std::memset(prgReg, 0, sizeof(prgReg));
    std::memset(chrReg, 0, sizeof(chrReg));
    std::memset(ciram, 0, sizeof(ciram));
    std::memset(exRam, 0, sizeof(exRam));

    // $5117 is ROM-only and powers up selecting the final 8 KiB page.
    prgReg[4] = 0xFF;
    lastChrSet = CHR_SOURCE_OBJ;
    activeChrSource = CHR_SOURCE_LAST;
    ppuCtrl = 0;
    ppuMask = 0;
    lastBgNametableOffset = 0;
    currentExtendedAttribute = 0;

    irqTarget = 0;
    irqEnabled = false;
    irqPending = false;
    inFrame = false;
    needInFrame = false;
    scanlineCounter = 0;
    ppuIdleCounter = 0;
    lastPpuReadAddr = 0;
    ntReadCounter = 0;

    multiplierA = 0;
    multiplierB = 0;

    updatePrg();
}

void Mapper005::updatePrg() {
    const uint8_t count = std::max<uint8_t>(1, prgBankCount8k());

    auto set8 = [&](int slot, uint8_t value, bool romOnly) {
        const bool rom = romOnly || ((value & 0x80u) != 0);
        const uint8_t bank = uint8_t((value & 0x7Fu) % count);
        prgSlot[slot].isRam = !rom;
        prgSlot[slot].base = uint32_t(bank) * 0x2000u;
    };

    auto set16 = [&](int slot, uint8_t value, bool romOnly) {
        const bool rom = romOnly || ((value & 0x80u) != 0);
        const uint8_t base = uint8_t((value & 0x7Eu) % count);
        for (int i = 0; i < 2; ++i) {
            prgSlot[slot + i].isRam = !rom;
            prgSlot[slot + i].base = uint32_t((base + i) % count) * 0x2000u;
        }
    };

    // CipherNES currently allocates the iNES-declared PRG-RAM; clamp the MMC5
    // bank window to the common first 8 KiB when no larger board metadata exists.
    wramBase = uint32_t(prgReg[0] & 0x07u) * 0x2000u;

    switch (prgMode & 3u) {
        case 0: {
            const uint8_t base = uint8_t((prgReg[4] & 0x7Cu) % count);
            for (int i = 0; i < 4; ++i) {
                prgSlot[i].isRam = false;
                prgSlot[i].base = uint32_t((base + i) % count) * 0x2000u;
            }
            break;
        }
        case 1:
            set16(0, prgReg[2], false);
            set16(2, prgReg[4], true);
            break;
        case 2:
            set16(0, prgReg[2], false);
            set8(2, prgReg[3], false);
            set8(3, prgReg[4], true);
            break;
        default:
            set8(0, prgReg[1], false);
            set8(1, prgReg[2], false);
            set8(2, prgReg[3], false);
            set8(3, prgReg[4], true);
            break;
    }
}

uint16_t Mapper005::encodeChrRegister(uint8_t data) const {
    // The internal registers are always ten bits. Larger CHR modes source fewer
    // bank bits from the CPU data bus and pass low PPU address bits through.
    switch (chrMode & 3u) {
        case 0: // 8 KiB: D6-D0 select an 8 KiB bank
            return uint16_t((uint16_t(data & 0x7Fu) << 3) | (data & 1u));
        case 1: // 4 KiB: D7-D0 select a 4 KiB bank
            return uint16_t((uint16_t(data) << 2) | (data & 1u));
        case 2: // 2 KiB: $5130.0 plus D7-D0 select a 2 KiB bank
            return uint16_t((uint16_t(chrUpper & 1u) << 9)
                          | (uint16_t(data) << 1) | (data & 1u));
        default: // 1 KiB: $5130.1-0 plus D7-D0
            return uint16_t((uint16_t(chrUpper & 3u) << 8) | data);
    }
}

uint16_t Mapper005::chrBankForAddress(uint16_t addr, uint8_t source) const {
    const uint16_t slot = uint16_t((addr >> 10) & 7u);
    const bool extendedBg = source == CHR_SOURCE_BG && exRamMode == 1 &&
                            ppuSubstitutionsEnabled();
    uint8_t selected = (source == CHR_SOURCE_LAST) ? lastChrSet : source;

    // With 8x8 sprites the MMC5 cannot distinguish ordinary sprite and
    // background pattern fetches, so both use CHR set A ($5120-$5127).
    // Extended-attribute background fetches are the exception: their per-tile
    // 4 KiB bank substitution still works in 8x8 mode. PPUDATA (LAST) never
    // receives this substitution.
    if (!extendedBg && source != CHR_SOURCE_LAST &&
        (!ppuSubstitutionsEnabled() || !largeSpritesEnabled())) {
        selected = CHR_SOURCE_OBJ;
    }

    uint16_t regValue = 0;
    uint16_t bank = 0;

    if (extendedBg) {
        // Extended attributes force 4 KiB background banking per tile.
        const uint16_t bank4k = uint16_t((uint16_t(chrUpper & 3u) << 6)
                                      | (currentExtendedAttribute & 0x3Fu));
        bank = uint16_t(bank4k * 4u + ((addr >> 10) & 3u));
    } else if (selected == CHR_SOURCE_BG) {
        switch (chrMode & 3u) {
            case 0:
                regValue = chrReg[11];
                bank = uint16_t((regValue & ~uint16_t(7)) + slot);
                break;
            case 1:
                regValue = chrReg[11];
                bank = uint16_t((regValue & ~uint16_t(3)) + (slot & 3u));
                break;
            case 2: {
                const uint8_t regIndex = uint8_t((slot & 3u) < 2u ? 9 : 11);
                regValue = chrReg[regIndex];
                bank = uint16_t((regValue & ~uint16_t(1)) + (slot & 1u));
                break;
            }
            default:
                regValue = chrReg[8 + (slot & 3u)];
                bank = regValue;
                break;
        }
    } else {
        switch (chrMode & 3u) {
            case 0:
                regValue = chrReg[7];
                bank = uint16_t((regValue & ~uint16_t(7)) + slot);
                break;
            case 1: {
                const uint8_t regIndex = (slot < 4u) ? 3u : 7u;
                regValue = chrReg[regIndex];
                bank = uint16_t((regValue & ~uint16_t(3)) + (slot & 3u));
                break;
            }
            case 2: {
                const uint8_t regIndex = uint8_t((slot & ~uint16_t(1)) + 1u);
                regValue = chrReg[regIndex];
                bank = uint16_t((regValue & ~uint16_t(1)) + (slot & 1u));
                break;
            }
            default:
                bank = chrReg[slot];
                break;
        }
    }

    return uint16_t(bank % std::max<uint16_t>(1, chrBankCount1k()));
}

void Mapper005::resetScanlineDetector() {
    inFrame = false;
    needInFrame = false;
    ppuIdleCounter = 0;
    lastPpuReadAddr = 0xFFFFu;
    ntReadCounter = 0;
    activeChrSource = CHR_SOURCE_LAST;
}

void Mapper005::beforeCpuRead(uint16_t addr) {
    // MMC5A resets frame detection when the CPU fetches either NMI vector byte.
    if (addr == 0xFFFAu || addr == 0xFFFBu) resetScanlineDetector();
}

void Mapper005::beforeCpuWrite(uint16_t addr, uint8_t data) {
    // These are fully decoded by MMC5: mirrors such as $2008/$2009 do not count.
    if (addr == 0x2000u) {
        ppuCtrl = data;
    } else if (addr == 0x2001u) {
        ppuMask = data;
        if (!ppuSubstitutionsEnabled()) resetScanlineDetector();
    } else if (addr == 0x4014u) {
        // OAM DMA writes reset the scanline counter detector on MMC5 hardware.
        resetScanlineDetector();
    }
}

bool Mapper005::cpuReadDirect(uint16_t addr, uint8_t& data) {
    if (addr >= 0x5C00 && addr <= 0x5FFF) {
        // Modes 0/1 are write-only from the CPU side and therefore leave the
        // CPU data bus open. Modes 2/3 expose the stored byte for reading.
        if (exRamMode <= 1) return false;
        data = exRam[addr & 0x03FFu];
        return true;
    }

    switch (addr) {
        case 0x5204:
            data = uint8_t((irqPending ? 0x80u : 0u) | (inFrame ? 0x40u : 0u));
            irqPending = false;
            return true;
        case 0x5205: {
            const uint16_t product = uint16_t(multiplierA) * uint16_t(multiplierB);
            data = uint8_t(product);
            return true;
        }
        case 0x5206: {
            const uint16_t product = uint16_t(multiplierA) * uint16_t(multiplierB);
            data = uint8_t(product >> 8);
            return true;
        }
        default:
            return false;
    }
}

bool Mapper005::cpuWriteDirect(uint16_t addr, uint8_t data) {
    if (addr >= 0x5C00 && addr <= 0x5FFF) {
        const uint16_t index = addr & 0x03FFu;
        if (exRamMode <= 1) {
            // During rendering the byte reaches ExRAM. Outside the detected
            // frame the MMC5 writes zero instead, which is observable when the
            // mode is later changed to CPU-readable mode.
            exRam[index] = inFrame ? data : 0;
        } else if (exRamMode == 2) {
            exRam[index] = data;
        }
        // Mode 3 is read-only.
        return true;
    }

    switch (addr) {
        case 0x5100: prgMode = data & 3u; updatePrg(); return true;
        case 0x5101: chrMode = data & 3u; return true;
        case 0x5102: prgProtect1 = data; return true;
        case 0x5103: prgProtect2 = data; return true;
        case 0x5104: exRamMode = data & 3u; return true;
        case 0x5105: nametableMap = data; return true;
        case 0x5106: fillTile = data; return true;
        case 0x5107: fillColor = data & 3u; return true;
        case 0x5130: chrUpper = data & 3u; return true;
        case 0x5200: // vertical split control (not rendered yet)
        case 0x5201:
        case 0x5202:
            return true;
        case 0x5203:
            irqTarget = data;
            return true;
        case 0x5204:
            irqEnabled = (data & 0x80u) != 0;
            return true;
        case 0x5205:
            multiplierA = data;
            return true;
        case 0x5206:
            multiplierB = data;
            return true;
        default:
            break;
    }

    if (addr >= 0x5113 && addr <= 0x5117) {
        prgReg[addr - 0x5113] = data;
        updatePrg();
        return true;
    }

    if (addr >= 0x5120 && addr <= 0x512B) {
        const uint8_t index = uint8_t(addr - 0x5120);
        chrReg[index] = encodeChrRegister(data);
        lastChrSet = (index < 8) ? CHR_SOURCE_OBJ : CHR_SOURCE_BG;
        return true;
    }

    return false;
}

bool Mapper005::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        mappedAddr = PRG_RAM_FLAG | (wramBase + uint32_t(addr & 0x1FFFu));
        return true;
    }

    if (addr >= 0x8000) {
        const int slot = int((addr - 0x8000u) >> 13);
        const PrgSlot& mapping = prgSlot[slot];
        mappedAddr = mapping.base + uint32_t(addr & 0x1FFFu);
        if (mapping.isRam) mappedAddr |= PRG_RAM_FLAG;
        return true;
    }
    return false;
}

bool Mapper005::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    mappedAddr = 0;
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        if (prgRamWriteEnabled()) {
            mappedAddr = PRG_RAM_FLAG | (wramBase + uint32_t(addr & 0x1FFFu));
        }
        return true;
    }

    if (addr >= 0x8000) {
        const int slot = int((addr - 0x8000u) >> 13);
        const PrgSlot& mapping = prgSlot[slot];
        if (mapping.isRam && prgRamWriteEnabled()) {
            mappedAddr = PRG_RAM_FLAG | (mapping.base + uint32_t(addr & 0x1FFFu));
        }
        return true;
    }
    return false;
}

bool Mapper005::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr > 0x1FFF) return false;
    const uint16_t bank = chrBankForAddress(addr, activeChrSource);
    mappedAddr = uint32_t(bank) * 0x0400u + uint32_t(addr & 0x03FFu);
    return true;
}

bool Mapper005::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (addr > 0x1FFF || chrBanks != 0) return false;
    const uint16_t bank = chrBankForAddress(addr, activeChrSource);
    mappedAddr = uint32_t(bank) * 0x0400u + uint32_t(addr & 0x03FFu);
    return true;
}

bool Mapper005::nametableRead(uint16_t addr, uint8_t& data) {
    const uint16_t normalized = uint16_t((addr - 0x2000u) & 0x0FFFu);
    const uint8_t quadrant = uint8_t(normalized >> 10);
    const uint16_t offset = normalized & 0x03FFu;
    const uint8_t source = uint8_t((nametableMap >> (quadrant * 2u)) & 3u);

    switch (source) {
        case 0: data = ciram[0][offset]; return true;
        case 1: data = ciram[1][offset]; return true;
        case 2:
            data = (exRamMode <= 1) ? exRam[offset] : 0x00;
            return true;
        default:
            data = (offset >= 0x03C0u) ? uint8_t((fillColor & 3u) * 0x55u) : fillTile;
            return true;
    }
}

bool Mapper005::nametableWrite(uint16_t addr, uint8_t data) {
    const uint16_t normalized = uint16_t((addr - 0x2000u) & 0x0FFFu);
    const uint8_t quadrant = uint8_t(normalized >> 10);
    const uint16_t offset = normalized & 0x03FFu;
    const uint8_t target = uint8_t((nametableMap >> (quadrant * 2u)) & 3u);

    switch (target) {
        case 0: ciram[0][offset] = data; break;
        case 1: ciram[1][offset] = data; break;
        case 2:
            if (exRamMode == 0) exRam[offset] = data;
            break;
        default:
            break; // fill mode is read-only
    }
    return true;
}

bool Mapper005::attributeRead(uint16_t addr, uint8_t& data) {
    if (exRamMode == 1 && ppuSubstitutionsEnabled() &&
        activeChrSource == CHR_SOURCE_BG) {
        const uint8_t palette = uint8_t(currentExtendedAttribute >> 6);
        data = uint8_t(palette * 0x55u);
        return true;
    }
    return nametableRead(addr, data);
}

bool Mapper005::attributeWrite(uint16_t addr, uint8_t data) {
    return nametableWrite(addr, data);
}

void Mapper005::detectScanlineStart(uint16_t addr) {
    // MMC5 does not use the emulator's nominal PPU scanline number.  It watches
    // the actual PPU read stream.  Three consecutive reads of the same
    // nametable address arm the detector; the following read (normally the
    // attribute fetch) is the scanline boundary.  Keeping this bus-derived is
    // important for forced blanking and deliberately unusual test sequences.
    if (ntReadCounter >= 2) {
        if (!inFrame && !needInFrame) {
            needInFrame = true;
            scanlineCounter = 0;
        } else {
            ++scanlineCounter;
            if (irqTarget != 0 && scanlineCounter == irqTarget) irqPending = true;
        }
    } else if (addr >= 0x2000u && addr <= 0x2FFFu) {
        if (lastPpuReadAddr == addr) ++ntReadCounter;
    }

    if (lastPpuReadAddr != addr) ntReadCounter = 0;
}

void Mapper005::beforePpuFetch(uint16_t addr, uint8_t kind, int16_t /*scanline*/,
                               int16_t cycle, uint64_t /*masterClock*/) {
    const bool isNametableByte = addr >= 0x2000u && addr <= 0x2FFFu
                              && (addr & 0x03FFu) < 0x03C0u;

    // The first ordinary nametable fetch after the detector is armed changes
    // status bit 6 to "in frame".  This ordering matches the MMC5 bus state:
    // the attribute read detects the boundary, then the next tile fetch enters
    // the active frame.
    if (isNametableByte && needInFrame) {
        needInFrame = false;
        inFrame = true;
    }

    detectScanlineStart(addr);
    ppuIdleCounter = 3;
    lastPpuReadAddr = addr;

    if (kind == FETCH_NAMETABLE) {
        // Background NT fetches occur in 1-256 and 321-340. Sprite dummy NT
        // reads at 257-320 must not replace the extended-attribute tile latch.
        if ((cycle >= 1 && cycle <= 256) || cycle >= 321) {
            const uint16_t offset = uint16_t((addr - 0x2000u) & 0x03FFu);
            if (offset < 0x03C0u) {
                lastBgNametableOffset = offset;
                currentExtendedAttribute = exRam[offset];
            }
            activeChrSource = CHR_SOURCE_BG;
        }
    } else if (kind == FETCH_ATTRIBUTE || kind == FETCH_PATTERN_LO || kind == FETCH_PATTERN_HI) {
        activeChrSource = CHR_SOURCE_BG;
    } else if (kind == FETCH_SPRITE_LO || kind == FETCH_SPRITE_HI) {
        activeChrSource = CHR_SOURCE_OBJ;
    } else if (kind == FETCH_UNKNOWN) {
        activeChrSource = CHR_SOURCE_LAST;
    }
}

void Mapper005::cpuClock() {
    if (ppuIdleCounter != 0 && --ppuIdleCounter == 0) {
        // Three M2 rises without PPU /RD end the frame and invalidate the
        // repeated-nametable-address detector.
        inFrame = false;
        needInFrame = false;
        lastPpuReadAddr = 0xFFFFu;
        ntReadCounter = 0;
        activeChrSource = CHR_SOURCE_LAST;
    }
}

void Mapper005::ppuClock(int16_t scanline, int16_t cycle, uint64_t /*masterClock*/) {
    // Safety reset at pre-render; the normal hardware path is the three-CPU-
    // cycle idle detector above.
    if (scanline == 261 && cycle <= 1) {
        resetScanlineDetector();
        scanlineCounter = 0;
    }
}

void Mapper005::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    write_u8(out, prgMode);
    write_u8(out, chrMode);
    write_u8(out, prgProtect1);
    write_u8(out, prgProtect2);
    write_u8(out, exRamMode);
    write_u8(out, nametableMap);
    write_u8(out, fillTile);
    write_u8(out, fillColor);
    write_u8(out, chrUpper);
    write_u8(out, lastChrSet);
    write_u8(out, activeChrSource);
    write_u8(out, ppuCtrl);
    write_u8(out, ppuMask);
    write_bytes(out, prgReg, sizeof(prgReg));
    write_bytes(out, chrReg, sizeof(chrReg));
    write_bytes(out, prgSlot, sizeof(prgSlot));
    write_u32(out, wramBase);
    write_bytes(out, ciram, sizeof(ciram));
    write_bytes(out, exRam, sizeof(exRam));
    write_u16(out, lastBgNametableOffset);
    write_u8(out, currentExtendedAttribute);
    write_u8(out, irqTarget);
    write_bool(out, irqEnabled);
    write_bool(out, irqPending);
    write_bool(out, inFrame);
    write_bool(out, needInFrame);
    write_u8(out, scanlineCounter);
    write_u8(out, ppuIdleCounter);
    write_u16(out, lastPpuReadAddr);
    write_u8(out, ntReadCounter);
    write_u8(out, multiplierA);
    write_u8(out, multiplierB);
}

bool Mapper005::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    if (!read_u8(p,end,prgMode)) return false;
    if (!read_u8(p,end,chrMode)) return false;
    if (!read_u8(p,end,prgProtect1)) return false;
    if (!read_u8(p,end,prgProtect2)) return false;
    if (!read_u8(p,end,exRamMode)) return false;
    if (!read_u8(p,end,nametableMap)) return false;
    if (!read_u8(p,end,fillTile)) return false;
    if (!read_u8(p,end,fillColor)) return false;
    if (!read_u8(p,end,chrUpper)) return false;
    if (!read_u8(p,end,lastChrSet)) return false;
    if (!read_u8(p,end,activeChrSource)) return false;
    if (!read_u8(p,end,ppuCtrl)) return false;
    if (!read_u8(p,end,ppuMask)) return false;
    if (!read_bytes(p,end,prgReg,sizeof(prgReg))) return false;
    if (!read_bytes(p,end,chrReg,sizeof(chrReg))) return false;
    if (!read_bytes(p,end,prgSlot,sizeof(prgSlot))) return false;
    if (!read_u32(p,end,wramBase)) return false;
    if (!read_bytes(p,end,ciram,sizeof(ciram))) return false;
    if (!read_bytes(p,end,exRam,sizeof(exRam))) return false;
    if (!read_u16(p,end,lastBgNametableOffset)) return false;
    if (!read_u8(p,end,currentExtendedAttribute)) return false;
    if (!read_u8(p,end,irqTarget)) return false;
    if (!read_bool(p,end,irqEnabled)) return false;
    if (!read_bool(p,end,irqPending)) return false;
    if (!read_bool(p,end,inFrame)) return false;
    if (!read_bool(p,end,needInFrame)) return false;
    if (!read_u8(p,end,scanlineCounter)) return false;
    if (!read_u8(p,end,ppuIdleCounter)) return false;
    if (!read_u16(p,end,lastPpuReadAddr)) return false;
    if (!read_u8(p,end,ntReadCounter)) return false;
    if (!read_u8(p,end,multiplierA)) return false;
    if (!read_u8(p,end,multiplierB)) return false;
    return true;
}
