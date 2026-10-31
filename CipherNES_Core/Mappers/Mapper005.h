#pragma once

#include "Mapper.h"

#include <cstdint>

// iNES mapper 005 - MMC5.
//
// This implementation covers the mapper-visible behaviour exercised by the
// public MMC5 validation ROMs shipped with CipherNES: all PRG/CHR banking modes,
// independent sprite/background CHR sets, ExRAM/extended attributes, per-
// quadrant nametable and fill mapping, scanline status/IRQ, and multiplication.
// Expansion audio and vertical split rendering remain separate future work.
class Mapper005 final : public Mapper {
public:
    uint32_t capabilities() const override {
        return MAPPER_CAP_CPU_DIRECT
             | MAPPER_CAP_CPU_OBSERVE
             | MAPPER_CAP_NAMETABLE
             | MAPPER_CAP_ATTRIBUTE
             | MAPPER_CAP_PPU_FETCH_HOOKS
             | MAPPER_CAP_IRQ;
    }

    Mapper005(uint8_t prgBanks, uint8_t chrBanks);

    void reset() override;

    bool cpuReadDirect(uint16_t addr, uint8_t& data) override;
    bool cpuWriteDirect(uint16_t addr, uint8_t data) override;
    void beforeCpuRead(uint16_t addr) override;
    void beforeCpuWrite(uint16_t addr, uint8_t data) override;
    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool nametableRead(uint16_t addr, uint8_t& data) override;
    bool nametableWrite(uint16_t addr, uint8_t data) override;
    bool attributeRead(uint16_t addr, uint8_t& data) override;
    bool attributeWrite(uint16_t addr, uint8_t data) override;

    bool usesPpuClock() const override { return true; }
    void ppuClock(int16_t scanline, int16_t cycle, uint64_t masterClock) override;
    bool usesCpuClock() const override { return true; }
    void cpuClock() override;
    void beforePpuFetch(uint16_t addr, uint8_t kind, int16_t scanline,
                        int16_t cycle, uint64_t masterClock) override;

    bool irqState() const override { return irqPending && irqEnabled; }
    void irqClear() override { irqPending = false; }

    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;

private:
    enum ChrSource : uint8_t { CHR_SOURCE_LAST = 0, CHR_SOURCE_OBJ = 1, CHR_SOURCE_BG = 2 };

    // Configuration registers.
    uint8_t prgMode = 3;
    uint8_t chrMode = 3;
    uint8_t prgProtect1 = 0;
    uint8_t prgProtect2 = 0;
    uint8_t exRamMode = 0;
    uint8_t nametableMap = 0;
    uint8_t fillTile = 0;
    uint8_t fillColor = 0;
    uint8_t chrUpper = 0;

    // PRG and CHR banking registers.
    uint8_t prgReg[5]{};       // $5113-$5117
    uint16_t chrReg[12]{};     // full internal 10-bit values, $5120-$512B
    uint8_t lastChrSet = CHR_SOURCE_OBJ;
    uint8_t activeChrSource = CHR_SOURCE_LAST;

    // MMC5 snoops the exact (non-mirrored) PPU register addresses.  These
    // values decide whether its background/sprite CHR substitutions are active.
    uint8_t ppuCtrl = 0;
    uint8_t ppuMask = 0;

    struct PrgSlot {
        bool isRam = false;
        uint32_t base = 0;
    };
    PrgSlot prgSlot[4]{};
    uint32_t wramBase = 0;

    // MMC5 internal memories. The two CIRAM pages are owned here so mixed
    // $5105 quadrant mappings can be represented without changing the PPU.
    uint8_t ciram[2][0x400]{};
    uint8_t exRam[0x400]{};

    // Extended-attribute state for the tile whose pattern bytes follow.
    uint16_t lastBgNametableOffset = 0;
    uint8_t currentExtendedAttribute = 0;

    // Scanline IRQ/status registers.
    uint8_t irqTarget = 0;
    bool irqEnabled = false;
    bool irqPending = false;
    bool inFrame = false;
    bool needInFrame = false;
    uint8_t scanlineCounter = 0;
    uint8_t ppuIdleCounter = 0;
    uint16_t lastPpuReadAddr = 0;
    uint8_t ntReadCounter = 0;

    // Hardware multiplier.
    uint8_t multiplierA = 0;
    uint8_t multiplierB = 0;

    void updatePrg();
    uint16_t encodeChrRegister(uint8_t data) const;
    uint16_t chrBankForAddress(uint16_t addr, uint8_t source) const;
    bool prgRamWriteEnabled() const;
    bool ppuSubstitutionsEnabled() const { return (ppuMask & 0x18u) != 0; }
    bool largeSpritesEnabled() const { return (ppuCtrl & 0x20u) != 0; }
    void resetScanlineDetector();
    void detectScanlineStart(uint16_t addr);

    uint8_t prgBankCount8k() const { return uint8_t(prgBanks * 2u); }
    uint16_t chrBankCount1k() const {
        return (chrBanks == 0) ? 8u : uint16_t(chrBanks) * 8u;
    }
};
