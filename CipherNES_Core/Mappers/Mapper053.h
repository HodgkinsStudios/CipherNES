#pragma once

#include <cstdint>
#include "Mapper.h"

// Supervision 16-in-1 board (iNES mapper 53).
// The board exposes an 8 KiB PRG-ROM window at $6000 and two 16 KiB
// windows at $8000-$FFFF.  Some historical dumps place the menu EPROM
// first, while older concatenations place it after the game ROMs.
class Mapper053 final : public Mapper {
public:
    Mapper053(uint16_t prgBanks16k, uint16_t chrBanks8k, bool menuEpromFirst);

    uint32_t capabilities() const override { return MAPPER_CAP_MIRRORING; }
    void reset() override;

    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    uint8_t mirroringOverride() const override { return mirrorMode; }

    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;

private:
    uint32_t prgPages8k = 1;
    bool epromFirst = false;
    uint8_t regs[2]{0, 0};
    uint16_t prgPage[5]{0, 0, 1, 2, 3}; // $6000, then four 8 KiB CPU slots
    uint8_t mirrorMode = 1;             // vertical after power-on

    void updateState();
};
