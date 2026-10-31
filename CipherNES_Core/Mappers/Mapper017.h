#pragma once

#include "Mapper.h"

// iNES Mapper 17: Front Fareast Super Magic Card (subset: play mode banking + IRQ)
class Mapper017 : public Mapper {
public:
    uint32_t capabilities() const override { return MAPPER_CAP_MIRRORING | MAPPER_CAP_IRQ | MAPPER_CAP_PPU_ADDR_LATCH; }
    Mapper017(uint8_t prgBanks16k, uint8_t chrBanks8k);

    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    uint8_t mirroringOverride() const override { return mirrorMode; }

    bool irqState() const override { return irqActive; }
    void irqClear() override { irqActive = false; }
    bool usesCpuClock() const override { return true; }
    void cpuClock() override;
    void ppuAddressLatch(uint16_t addr) override;
    void reset() override;

private:
    uint8_t mirrorMode = 0xFF;
    uint32_t prgBanks8k = 0;

    // 4M banking ($4504-$4507)
    uint8_t prg4m[4] = {0, 1, 2, 3};
    // 2M banking ($8000/$A000/$C000/$E000)
    uint8_t prg2m[4] = {0, 1, 2, 3};

    bool bankingEnabled = true;
    bool use2m = false;

    // CHR
    uint8_t chr8kBank = 0;
    bool chr1kMode = true;
    uint8_t chr1k[8] = {0, 1, 2, 3, 4, 5, 6, 7};

    void setMirroringFrom42Fx(uint16_t addr);

    // IRQ
    bool irqEnabled = false;
    bool irqSourcePA12 = false;
    uint16_t irqCounter = 0;
    bool lastA12 = false;
    bool irqActive = false;

    void clockIrq();
};
