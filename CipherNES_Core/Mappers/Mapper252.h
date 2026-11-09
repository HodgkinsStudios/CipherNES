#pragma once

#include "Mapper.h"

// Waixing mapper 252: two switchable 8 KiB PRG pages, eight nibble-programmed
// 1 KiB CHR pages, and a Konami-style VRC IRQ counter.
class Mapper252 final : public Mapper {
public:
    Mapper252(uint16_t prgBanks16k, uint16_t chrBanks8k);

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
    uint32_t prgPages8k = 0;
    uint32_t chrPages1k = 0;
    uint8_t prg[2]{0, 1};
    uint8_t chr[8]{};

    uint8_t irqLatch = 0;
    uint8_t irqCounter = 0;
    bool irqEnable = false;
    bool irqEnableAfterAck = false;
    bool irqCycleMode = false;
    int32_t irqPrescaler = 341;
    bool irqActive = false;
};
