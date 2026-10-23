#pragma once

#include <cstdint>
#include <vector>
#include "Mapper.h"

// Exact board equations for a group of discrete and small-ASIC mappers.
// The mapper number selects a documented circuit profile; there is no
// generic fallback or guessed compatibility mode in this class.
class MapperAdditionalBoards final : public Mapper {
public:
    MapperAdditionalBoards(uint16_t prgBanks16k, uint16_t chrBanks8k, uint16_t mapperNumber);

    uint32_t capabilities() const override;
    void reset() override;
    void reset(bool hardReset) override;
    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    uint8_t mirroringOverride() const override { return mirrorMode; }
    bool usesCpuClock() const override;
    void cpuClock() override;
    void ppuAddressBus(uint16_t addr) override;
    bool irqState() const override { return irqActive; }
    void irqClear() override { irqActive = false; }
    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;

private:
    const uint16_t mapperNumber;
    uint16_t prg8[4]{};
    uint16_t chr1[8]{};
    uint8_t regs[8]{};
    uint8_t mirrorMode = 0xFF;

    uint16_t irqCounter = 0;
    uint8_t irqReload = 0;
    bool irqEnabled = false;
    bool irqEnabledAlt = false;
    bool irqActive = false;

    uint8_t bank = 0;
    uint8_t mode = 0;
    uint8_t resetCounter = 0;
    uint8_t outerChr = 0;
    uint8_t innerChr = 0;
    uint16_t lastPpuAddress = 0;
    bool swap = false;
    bool chrEnabled = true;
    uint8_t a12Low = 0;
    bool lastA12 = false;
    bool suppressInitialReset = false;
    bool resetInitialized = false;

    uint16_t prgPageCount8() const;
    uint16_t chrPageCount1() const;
    void setPrg32(uint16_t bank32);
    void setPrg16(int slot16, uint16_t bank16);
    void setChr8(uint16_t bank8);
    void update51();
    void update96();
    void update162();
    void update166();
    void writeRegister(uint16_t addr, uint8_t data);
};
