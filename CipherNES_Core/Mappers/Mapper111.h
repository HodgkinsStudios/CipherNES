#pragma once

#include <array>
#include <cstdint>
#include "Mapper.h"

// iNES mapper 111 has two assignments in circulation:
//  - Legacy Chinese MMC1-derived board with direct (non-serial) registers.
//  - NES 2.0 GTROM/Cheapocabra homebrew board with 32 KiB PRG banking,
//    16 KiB CHR-RAM and mapper-controlled 16 KiB nametable RAM.
// CHR-ROM presence distinguishes the legacy board from GTROM dumps.
class Mapper111 final : public Mapper {
public:
    Mapper111(uint16_t prgBanks16k, uint16_t chrBanks8k);

    uint32_t capabilities() const override;
    void reset() override;
    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool nametableRead(uint16_t addr, uint8_t& data) override;
    bool nametableWrite(uint16_t addr, uint8_t data) override;
    uint8_t mirroringOverride() const override { return legacyMode ? mirrorMode : 0xFFu; }
    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;

private:
    const bool legacyMode;

    // Legacy direct-register MMC1 state.
    uint8_t control = 0x1Cu;
    uint8_t chr0 = 0;
    uint8_t chr1 = 0;
    uint8_t prg = 0;
    uint8_t mirrorMode = 3;

    // GTROM state.
    uint8_t gtReg = 0;
    std::array<uint8_t,0x4000> gtNametable{};

    void updateLegacyMirror();
    uint32_t legacyPrgOffset(uint16_t addr) const;
    uint32_t legacyChrOffset(uint16_t addr) const;
};
