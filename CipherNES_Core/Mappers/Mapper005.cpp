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

Mapper005::Mapper005(uint8_t prgBanks, uint8_t chrBanks, uint32_t ramBytes, uint32_t nvBytes)
    : Mapper(prgBanks, chrBanks), prgRamSizeBytes(ramBytes), prgNvRamSizeBytes(nvBytes) {
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

    splitControl = splitScroll = splitBank = 0;
    splitActive = false; splitTileOffset = 0; splitFineColumn = splitFineRow = 0;
    exRamDirty = false;
    pcmReadMode = pcmIrqEnabled = pcmIrqTrip = pcmTouched = false;
    pcmDac = 0;
    for (auto& pulse : pulses) pulse = Pulse();
    audioFrameCounter = 0;
    audioHalfCycle = false;

    multiplierA = 0;
    multiplierB = 0;

    updatePrg();
}

void Mapper005::reset(bool hardReset) {
    (void)hardReset;
    // Resetting the console's CPU does not replace the cartridge's internal
    // SRAM. The cartridge constructor zeros fresh storage, and .SAV restore
    // initializes battery-backed storage independently of emulator resets.
    uint8_t preservedExRam[sizeof(exRam)];
    std::memcpy(preservedExRam, exRam, sizeof(exRam));
    const bool dirty = exRamDirty;
    reset();
    std::memcpy(exRam, preservedExRam, sizeof(exRam));
    exRamDirty = dirty;
}

void Mapper005::updatePrg() {
    const uint8_t count = std::max<uint8_t>(1, prgBankCount8k());

    auto set8 = [&](int slot, uint8_t value, bool romOnly) {
        const bool rom = romOnly || ((value & 0x80u) != 0);
        // RAM address decoding ignores D6..D3 and is independent of ROM size.
        const uint8_t bank = rom ? uint8_t((value & 0x7Fu) % count)
                                 : uint8_t(value & 0x07u);
        prgSlot[slot].isRam = !rom;
        prgSlot[slot].base = uint32_t(bank) * 0x2000u;
    };

    auto set16 = [&](int slot, uint8_t value, bool romOnly) {
        const bool rom = romOnly || ((value & 0x80u) != 0);
        const uint8_t base = rom ? uint8_t((value & 0x7Eu) % count)
                                 : uint8_t(value & 0x06u);
        for (int i = 0; i < 2; ++i) {
            prgSlot[slot + i].isRam = !rom;
            prgSlot[slot + i].base = uint32_t(rom ? (base + i) % count
                                                 : (base + i) & 7u) * 0x2000u;
        }
    };

    // $5113 drives RAM chip select (A2) and per-chip bank bits (A1..A0).
    // A8K RAM chip mirrors A1..A0; a 32K chip decodes them independently.
    // The actual chip population is resolved by mapWorkRam().
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
    if (splitActive && source == CHR_SOURCE_BG)
        return uint16_t((uint16_t(splitBank) * 4u + (slot & 3u)) %
                        std::max<uint16_t>(1, chrBankCount1k()));
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

void Mapper005::writePcmDac(uint8_t value) {
    pcmTouched = true;
    if (value == 0) {
        pcmIrqTrip = true;
    } else {
        pcmDac = value;
        pcmIrqTrip = false;
    }
}

void Mapper005::afterCpuRead(uint16_t addr, uint8_t data) {
    // In read mode the PCM DAC snoops the ROM data bus for $8000-$BFFF.
    // When RAM is banked into that window, ROM /OE is inactive.
    if (pcmReadMode && addr >= 0x8000 && addr <= 0xBFFF &&
        !prgSlot[(addr - 0x8000u) >> 13].isRam) writePcmDac(data);
}

uint8_t Mapper005::Pulse::output() const {
    static const uint8_t duty[4][8] = {
        {0,1,0,0,0,0,0,0}, {0,1,1,0,0,0,0,0},
        {0,1,1,1,1,0,0,0}, {1,0,0,1,1,1,1,1}
    };
    if (!enabled || !length || !duty[(control >> 6) & 3u][step]) return 0;
    return (control & 0x10u) ? (control & 15u) : envelopeDecay;
}

void Mapper005::clockPulseQuarter() {
    for (auto& p : pulses) {
        if (p.envelopeStart) {
            p.envelopeStart = false;
            p.envelopeDecay = 15;
            p.envelopeDivider = p.control & 15u;
        } else if (p.envelopeDivider) {
            --p.envelopeDivider;
        } else {
            p.envelopeDivider = p.control & 15u;
            if (p.envelopeDecay) --p.envelopeDecay;
            else if (p.control & 0x20u) p.envelopeDecay = 15;
        }
    }
}

void Mapper005::clockPulseHalf() {
    for (auto& p : pulses)
        if (!(p.control & 0x20u) && p.length) --p.length;
}

float Mapper005::audioSample() {
    // A modest conservative mix avoids DC when the PCM DAC has not been used.
    const float pulse = float(pulses[0].output() + pulses[1].output()) / 30.0f;
    const float pcm = pcmTouched ? (float(pcmDac) / 255.0f) : 0.0f;
    return -0.16f * pulse - 0.22f * pcm;
}

void Mapper005::copyPersistentRam(uint8_t* dst, size_t count) const {
    if (dst && count >= sizeof(exRam)) std::memcpy(dst, exRam, sizeof(exRam));
}
void Mapper005::restorePersistentRam(const uint8_t* src, size_t count) {
    if (src && count >= sizeof(exRam)) std::memcpy(exRam, src, sizeof(exRam));
    exRamDirty = false;
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
        case 0x5010:
            data = uint8_t((pcmIrqTrip && pcmIrqEnabled ? 0x80u : 0) | 0x01u); // MMC5A revision flag
            pcmIrqTrip = false;
            return true;
        case 0x5015:
            data = uint8_t((pulses[0].length ? 1u : 0u) |
                           (pulses[1].length ? 2u : 0u));
            return true;
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
            exRamDirty = true;
        } else if (exRamMode == 2) {
            exRam[index] = data;
            exRamDirty = true;
        }
        // Mode 3 is read-only.
        return true;
    }

    // Two APU-like pulse channels (no sweep unit) and an 8-bit PCM DAC.
    if ((addr >= 0x5000u && addr <= 0x5007u)) {
        const uint8_t reg = uint8_t(addr & 3u);
        Pulse& p = pulses[(addr >> 2) & 1u];
        static const uint8_t lengthTable[32] = {
            10,254,20,2,40,4,80,6,160,8,60,10,14,12,26,14,
            12,16,24,18,48,20,96,22,192,24,72,26,16,28,32,30
        };
        if (reg == 0) p.control = data;
        if (reg == 2) p.period = uint16_t((p.period & 0x0700u) | data);
        if (reg == 3) {
            p.period = uint16_t((p.period & 0x00FFu) | ((data & 7u) << 8));
            p.step = 0;
            p.envelopeStart = true;
            if (p.enabled) p.length = lengthTable[data >> 3];
        }
        return true; // $5001/$5005 sweep registers are unimplemented in hardware.
    }
    switch (addr) {
        case 0x5010:
            pcmReadMode = (data & 1u) != 0;
            pcmIrqEnabled = (data & 0x80u) != 0;
            return true;
        case 0x5011:
            if (!pcmReadMode) writePcmDac(data);
            return true;
        case 0x5015:
            for (unsigned i = 0; i != 2; ++i) {
                pulses[i].enabled = (data & (1u << i)) != 0;
                if (!pulses[i].enabled) pulses[i].length = 0;
            }
            return true;
        case 0x5100: prgMode = data & 3u; updatePrg(); return true;
        case 0x5101: chrMode = data & 3u; return true;
        case 0x5102: prgProtect1 = data; return true;
        case 0x5103: prgProtect2 = data; return true;
        case 0x5104: exRamMode = data & 3u; return true;
        case 0x5105: nametableMap = data; return true;
        case 0x5106: fillTile = data; return true;
        case 0x5107: fillColor = data & 3u; return true;
        case 0x5130: chrUpper = data & 3u; return true;
        case 0x5200: splitControl = data; return true;
        case 0x5201: splitScroll = data; return true;
        case 0x5202: splitBank = data; return true;
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

// Decode the two PRG-RAM /CE pins and the physically populated SRAM banks.
// MMC5's $5113-$5116 RAM bank bits are not modulo PRG-ROM bank count.
// Known layouts: 8K (one 8K chip), 16K (two 8K chips), 32K (one
// 32K chip), or 64K (two 32K chips). Unpopulated chip selects open bus.
bool Mapper005::mapWorkRam(uint32_t rawOffset, uint32_t& mappedAddr) const {
    const uint32_t logicalBank = (rawOffset >> 13) & 7u;
    uint32_t physicalBank = 0;
    // NES 2.0 mixed-chip extensions: physical chip 0 is battery backed,
    // and can be either 8K or 32K. Chip 1 is volatile (8K or 32K). The
    // 8K chip mirrors bank address bits A1/A0 but *not* chip select A2.
    if (prgRamSizeBytes == 40960u) {
        const uint32_t chip0Bytes = prgNvRamSizeBytes == 8192u ? 8192u : 32768u;
        const uint32_t chip1Bytes = 40960u - chip0Bytes;
        physicalBank = (logicalBank < 4u)
            ? ((logicalBank * 8192u) % chip0Bytes) / 8192u
            : (chip0Bytes / 8192u) +
              (((logicalBank & 3u) * 8192u) % chip1Bytes) / 8192u;
    } else if (prgRamSizeBytes >= 65536u) {
        physicalBank = logicalBank;
    } else if (prgRamSizeBytes >= 32768u) {
        if (logicalBank & 4u) return false;
        physicalBank = logicalBank & 3u;
    } else if (prgRamSizeBytes >= 16384u) {
        physicalBank = logicalBank >> 2; // Two individual 8 KiB SRAM chips.
    } else if (prgRamSizeBytes >= 8192u) {
        if (logicalBank & 4u) return false;
        physicalBank = 0; // A1/A0 are unconnected on an 8 KiB SRAM.
    } else {
        return false;
    }
    mappedAddr = PRG_RAM_FLAG | (physicalBank * 0x2000u)
                               | (rawOffset & 0x1FFFu);
    return true;
}

bool Mapper005::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr >= 0x6000 && addr <= 0x7FFF)
        return mapWorkRam(wramBase + uint32_t(addr & 0x1FFFu), mappedAddr);

    if (addr >= 0x8000) {
        const int slot = int((addr - 0x8000u) >> 13);
        const PrgSlot& mapping = prgSlot[slot];
        const uint32_t offset = mapping.base + uint32_t(addr & 0x1FFFu);
        if (mapping.isRam) return mapWorkRam(offset, mappedAddr);
        mappedAddr = offset;
        return true;
    }
    return false;
}

bool Mapper005::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    mappedAddr = 0;
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        if (prgRamWriteEnabled())
            (void)mapWorkRam(wramBase + uint32_t(addr & 0x1FFFu), mappedAddr);
        return true;
    }

    if (addr >= 0x8000) {
        const int slot = int((addr - 0x8000u) >> 13);
        const PrgSlot& mapping = prgSlot[slot];
        if (mapping.isRam && prgRamWriteEnabled())
            (void)mapWorkRam(mapping.base + uint32_t(addr & 0x1FFFu), mappedAddr);
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
    if (splitActive) {
        data = exRam[splitTileOffset];
        return true;
    }
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
            if (exRamMode == 0) { exRam[offset] = data; exRamDirty = true; }
            break;
        default:
            break; // fill mode is read-only
    }
    return true;
}

bool Mapper005::attributeRead(uint16_t addr, uint8_t& data) {
    if (splitActive) {
        const uint16_t attributeOffset = uint16_t(0x3C0u +
            ((splitTileOffset >> 7) * 8u) + ((splitFineColumn >> 2) & 7u));
        const uint8_t packed = exRam[attributeOffset & 0x3FFu];
        const uint8_t shift = uint8_t(((splitFineRow & 2u) << 1) |
                                      (splitFineColumn & 2u));
        const uint8_t palette = uint8_t((packed >> shift) & 3u);
        data = uint8_t(palette * 0x55u);
        return true;
    }
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

    // Visible and prefetch background tiles each have a distinct MMC5
    // horizontal tile count. Split configuration is sampled on NT fetches;
    // subsequent attribute and pattern fetches use the same tile latch.
    if (kind == FETCH_NAMETABLE &&
        ((cycle >= 1 && cycle <= 256) || (cycle >= 321 && cycle <= 336))) {
        const uint8_t column = uint8_t(cycle <= 256 ? (cycle - 1) / 8 : (cycle - 321) / 8);
        const bool right = (splitControl & 0x40u) != 0;
        const uint8_t threshold = splitControl & 31u;
        splitActive = (splitControl & 0x80u) != 0 && exRamMode <= 1 &&
                      ppuSubstitutionsEnabled() && (right ? column >= threshold : column < threshold);
        if (splitActive) {
            const uint8_t row = uint8_t((uint16_t(splitScroll) +
                uint16_t(scanlineCounter) + (cycle >= 321 ? 1u : 0u)) % 240u);
            splitFineRow = uint8_t(row >> 3);
            splitFineColumn = column;
            splitTileOffset = uint16_t((uint16_t(splitFineRow) * 32u + column) & 0x3FFu);
        }
    } else if (kind == FETCH_SPRITE_LO || kind == FETCH_SPRITE_HI || kind == FETCH_UNKNOWN) {
        splitActive = false;
    }

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
    // MMC5 pulse timers count M2/2; envelope/length clocks approximate the
    // NTSC 4-step frame sequence. Expansion-audio exact-phase certification
    // remains an open hardware test item.
    audioHalfCycle = !audioHalfCycle;
    if (audioHalfCycle) for (auto& p : pulses) {
        if (p.timer == 0) { p.timer = p.period; p.step = uint8_t((p.step + 1u) & 7u); }
        else --p.timer;
    }
    ++audioFrameCounter;
    if (audioFrameCounter == 7457u || audioFrameCounter == 14913u ||
        audioFrameCounter == 22371u || audioFrameCounter == 29829u) {
        clockPulseQuarter();
        if (audioFrameCounter == 14913u || audioFrameCounter == 29829u) clockPulseHalf();
    }
    if (audioFrameCounter >= 29830u) audioFrameCounter = 0;
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
    static const uint8_t audioTag[4] = {'M','5','A','2'};
    write_bytes(out, audioTag, sizeof(audioTag));
    write_bool(out, exRamDirty);
    write_bool(out, pcmReadMode);
    write_bool(out, pcmIrqEnabled);
    write_bool(out, pcmIrqTrip);
    write_bool(out, pcmTouched);
    write_u8(out, pcmDac);
    for (const Pulse& pulse : pulses) {
        write_u8(out, pulse.control); write_u16(out, pulse.period);
        write_u16(out, pulse.timer); write_u8(out, pulse.step);
        write_u8(out, pulse.length); write_u8(out, pulse.envelopePeriod);
        write_u8(out, pulse.envelopeDecay); write_u8(out, pulse.envelopeDivider);
        write_bool(out, pulse.envelopeStart); write_bool(out, pulse.enabled);
    }
    write_u32(out, audioFrameCounter);
    write_bool(out, audioHalfCycle);
    write_u8(out, splitControl); write_u8(out, splitScroll);
    write_u8(out, splitBank); write_bool(out, splitActive);
    write_u16(out, splitTileOffset); write_u8(out, splitFineColumn);
    write_u8(out, splitFineRow);
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
    // Cartridge states created before CP017 have no audio section. The
    // current binary reader is non-versioned, so accept only a completely
    // absent audio tail (legacy), never a partial/corrupted tail.
    static const uint8_t audioTag[4] = {'M','5','A','2'};
    if (size_t(end - p) >= sizeof(audioTag) && std::memcmp(p,audioTag,sizeof(audioTag)) == 0) {
        p += sizeof(audioTag);
        if (!read_bool(p,end,exRamDirty)) return false;
        if (!read_bool(p,end,pcmReadMode)) return false;
        if (!read_bool(p,end,pcmIrqEnabled)) return false;
        if (!read_bool(p,end,pcmIrqTrip)) return false;
        if (!read_bool(p,end,pcmTouched)) return false;
        if (!read_u8(p,end,pcmDac)) return false;
        for (Pulse& pulse : pulses) {
            if (!read_u8(p,end,pulse.control)) return false;
            if (!read_u16(p,end,pulse.period)) return false;
            if (!read_u16(p,end,pulse.timer)) return false;
            if (!read_u8(p,end,pulse.step)) return false;
            if (!read_u8(p,end,pulse.length)) return false;
            if (!read_u8(p,end,pulse.envelopePeriod)) return false;
            if (!read_u8(p,end,pulse.envelopeDecay)) return false;
            if (!read_u8(p,end,pulse.envelopeDivider)) return false;
            if (!read_bool(p,end,pulse.envelopeStart)) return false;
            if (!read_bool(p,end,pulse.enabled)) return false;
        }
        if (!read_u32(p,end,audioFrameCounter)) return false;
        if (!read_bool(p,end,audioHalfCycle)) return false;
        if (!read_u8(p,end,splitControl)) return false;
        if (!read_u8(p,end,splitScroll)) return false;
        if (!read_u8(p,end,splitBank)) return false;
        if (!read_bool(p,end,splitActive)) return false;
        if (!read_u16(p,end,splitTileOffset)) return false;
        if (!read_u8(p,end,splitFineColumn)) return false;
        if (!read_u8(p,end,splitFineRow)) return false;
    }
    // Mapping slots and $6000 window are derived from registers. Recalculate
    // them for both CP016 and legacy CP015 save-state layouts; the serialized
    // register/memory layout is unchanged across checkpoints.
    updatePrg();
    return true;
}
