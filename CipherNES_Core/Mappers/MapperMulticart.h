#pragma once

#include <cstdint>
#include "Mapper.h"

// A focused collection of discrete address/data-latch multicart boards whose
// hardware consists only of PRG/CHR banking and optional mirroring. Keeping
// these in one implementation avoids duplicating the mapping boilerplate while
// preserving each mapper's exact latch equations.
class MapperMulticart final : public Mapper {
public:
    MapperMulticart(uint16_t prgBanks, uint16_t chrBanks, uint16_t mapperNumber);
    uint32_t capabilities() const override;
    void reset() override;
    void reset(bool hardReset) override;
    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    uint8_t mirroringOverride() const override { return mirrorMode; }
    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;

private:
    const uint16_t mapperNumber;
    uint16_t prgPage[2]{}; // 16 KiB pages
    uint16_t chrPage = 0;  // 8 KiB page
    uint8_t regs[2]{};
    uint8_t mirrorMode = 0xFF;
    bool resetToggle = false;
    bool resetSeen = false;
    bool suppressInitialReset = false;

    void set32k(uint16_t page32k);
    void update226Style(bool mapper233Mode);
    void writeLatch(uint16_t addr, uint8_t data);
};
