#pragma once

#include <cstdint>
#include "Mapper.h"

// Mapper 64 (Tengen RAMBO-1)
// MMC3-like with extra CHR registers and an optional CPU-cycle IRQ mode.
class Mapper064 final : public Mapper {
public:
    uint32_t capabilities() const override { return MAPPER_CAP_MIRRORING | MAPPER_CAP_IRQ | MAPPER_CAP_SCANLINE; }
    Mapper064(uint8_t prgBanks, uint8_t chrBanks);

    void reset() override;

    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    uint8_t mirroringOverride() const override { return mirrorMode; }

    bool irqState() const override { return irqActive; }
    void irqClear() override { irqActive = false; }

    void scanline() override;
    bool usesCpuClock() const override { return true; }
    void cpuClock() override;

private:
    // Bank select ($8000 even)
    uint8_t bankSelect = 0;
    uint8_t regSelect = 0;
    bool chrInvert = false;   // C
    bool prgMode = false;     // P
    bool chr1kMode = false;   // K

    // Bank registers (R0..R9, R6,R7,RF)
    uint8_t reg[16]{};

    // Cached banks
    uint8_t prgBank[4]{};    // 8KB
    uint16_t chrBank[8]{};   // 1KB

    // Mirroring
    uint8_t mirrorMode = 0xFF; // 0=H,1=V

    // IRQ
    uint8_t irqLatch = 0;
    uint8_t irqCounter = 0;
    bool irqReload = false;
    bool irqEnabled = false;
    bool irqActive = false;

    // Mode select: 0=scanline, 1=CPU cycle mode
    bool irqCycleMode = false;
    uint8_t cpuPrescaler = 0; // clocks every 4 CPU cycles

    uint8_t prgBankCount8k() const;
    uint16_t chrBankCount1k() const;

    void updateBanks();
};
