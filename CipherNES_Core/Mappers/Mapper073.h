#pragma once
#include "Mapper.h"

class Mapper073 final : public Mapper {
public:
    Mapper073(uint16_t prgBanks16k, uint16_t chrBanks8k) : Mapper(prgBanks16k, chrBanks8k) { reset(); }
    uint32_t capabilities() const override { return MAPPER_CAP_IRQ; }
    void reset() override;
    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool usesCpuClock() const override { return true; }
    void cpuClock() override;
    bool irqState() const override { return irqActive; }
    void irqClear() override { irqActive = false; }
    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;
private:
    uint16_t reloadValue = 0;
    uint16_t counter = 0;
    uint16_t prgPage = 0;
    bool enabled = false;
    bool enableOnAck = false;
    bool mode8Bit = false;
    bool irqActive = false;
};
