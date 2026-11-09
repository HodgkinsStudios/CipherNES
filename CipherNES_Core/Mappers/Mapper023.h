#pragma once

#include "Mapper.h"

// Konami VRC2/VRC4 family used by iNES mappers 21, 23 and 25.
// NES 2.0 submappers select exact address-line wiring; legacy submapper 0
// accepts the non-overlapping address aliases used by both known variants.
class Mapper023 final : public Mapper {
public:
    Mapper023(uint16_t prgBanks16k, uint16_t chrBanks8k, uint16_t mapperNumber = 23, uint8_t submapperNumber = 0);

    uint32_t capabilities() const override {
        return MAPPER_CAP_MIRRORING | (vrc2Only ? 0u : MAPPER_CAP_IRQ);
    }
    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    uint8_t mirroringOverride() const override { return mirrorMode; }
    bool irqState() const override { return irqActive; }
    void irqClear() override { irqActive = false; }
    bool usesCpuClock() const override { return !vrc2Only; }
    void cpuClock() override;
    void reset() override;
    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;

private:
    const uint16_t mapperNumber;
    const uint8_t submapperNumber;
    const bool vrc2Only;

    uint8_t mirrorMode = 0xFF;
    uint32_t prgBanks8k = 0;
    uint8_t prg0 = 0;
    uint8_t prg1 = 1;
    bool swapMode = false;

    uint16_t chr[8] = {0, 1, 2, 3, 4, 5, 6, 7};

    bool irqEnable = false;
    bool irqEnableAfterAck = false;
    bool irqModeCycle = false;
    uint8_t irqLatch = 0;
    uint8_t irqCounter = 0;
    int32_t prescaler = 341;
    bool irqActive = false;

    uint16_t translateAddress(uint16_t addr) const;
    void setMirroring(uint8_t v);
    void writeChrLow(int idx, uint8_t v);
    void writeChrHigh(int idx, uint8_t v);
};
