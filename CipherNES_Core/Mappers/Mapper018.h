#pragma once

#include <cstdint>
#include "Mapper.h"

// iNES mapper 18: Jaleco SS88006.
class Mapper018 final : public Mapper {
public:
    Mapper018(uint16_t prgBanks, uint16_t chrBanks) : Mapper(prgBanks, chrBanks) { reset(); }
    uint32_t capabilities() const override { return MAPPER_CAP_MIRRORING | MAPPER_CAP_IRQ; }
    void reset() override;
    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    uint8_t mirroringOverride() const override { return mirrorMode; }
    bool irqState() const override { return irqActive; }
    void irqClear() override { irqActive = false; }
    bool usesCpuClock() const override { return true; }
    void cpuClock() override;
    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;

private:
    uint8_t prg[3]{};
    uint8_t chr[8]{};
    uint8_t irqReloadNibble[4]{};
    uint16_t irqCounter = 0;
    uint8_t irqSize = 0;
    bool irqEnabled = false;
    bool irqActive = false;
    uint8_t mirrorMode = 0xFF;

    static const uint16_t irqMasks[4];
    void writeNibble(uint8_t& target, bool high, uint8_t data);
    void reloadIrq();
};
