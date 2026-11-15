#pragma once

#include <cstdint>
#include "Mapper.h"

// Front Fareast FFE cartridge family.
//
// Mapper 6: F4xxx board, switchable 16 KiB PRG, fixed final 16 KiB,
//           switchable 8 KiB CHR, mirroring ports and 16-bit cycle IRQ.
// Mapper 8: F3xxx board, switchable lower 16 KiB PRG, fixed bank 1 in the
//           upper 16 KiB, switchable 8 KiB CHR, same service/IRQ ports.
class Mapper006 final : public Mapper {
public:
    Mapper006(uint16_t prgBanks, uint16_t chrBanks, uint16_t mapperNumber = 6);

    uint32_t capabilities() const override {
        return MAPPER_CAP_MIRRORING | MAPPER_CAP_IRQ;
    }

    void reset() override;
    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    uint8_t mirroringOverride() const override { return mirrorMode; }
    bool usesCpuClock() const override { return true; }
    void cpuClock() override;
    bool irqState() const override { return irqActive; }
    void irqClear() override { irqActive = false; }

    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;

private:
    const uint16_t mapperNumber;
    uint16_t prg16[2]{};
    uint16_t chr8 = 0;
    uint16_t irqCounter = 0;
    uint8_t mirrorMode = 0xFF;
    bool irqEnabled = false;
    bool irqActive = false;
    bool ffeAltMode = true;
};
