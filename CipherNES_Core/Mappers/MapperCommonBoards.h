#pragma once

#include <cstdint>
#include <array>
#include "Mapper.h"

// Exact implementations for a group of discrete/register-latch boards that
// share only mapping boilerplate.  Each mapper keeps its own hardware equations
// in the switch below; this is not a heuristic fallback.
class MapperCommonBoards final : public Mapper {
public:
    MapperCommonBoards(uint16_t prgBanks16k, uint16_t chrBanks8k, uint16_t mapperNumber);

    uint32_t capabilities() const override;
    void reset() override;
    bool cpuReadDirect(uint16_t addr, uint8_t& data) override;
    bool cpuWriteDirect(uint16_t addr, uint8_t data) override;
    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool nametableRead(uint16_t addr, uint8_t& data) override;
    bool nametableWrite(uint16_t addr, uint8_t data) override;
    uint8_t mirroringOverride() const override { return mirrorMode; }
    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;

private:
    const uint16_t mapperNumber;
    uint16_t prg8[4]{};
    uint16_t chr1[8]{};
    uint8_t mirrorMode = 0xFF;
    uint8_t regs[8]{};
    uint8_t currentReg = 0;
    uint8_t outerChr = 0;
    uint8_t prgBank41 = 0;
    uint8_t chrBank41 = 0;
    std::array<uint8_t, 0x800> ntRam{};

    void setPrg32(uint16_t bank32);
    void setPrg16(int slot16, uint16_t bank16);
    void setChr8(uint16_t bank8);
    void update112();
    void update243();
    void writeRegister(uint16_t addr, uint8_t data);
};
