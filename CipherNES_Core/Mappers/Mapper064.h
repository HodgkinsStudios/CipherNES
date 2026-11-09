#pragma once

#include <cstdint>
#include <array>
#include "Mapper.h"

// Mapper 64 (Tengen RAMBO-1)
// MMC3-like with extra CHR registers and an optional CPU-cycle IRQ mode.
class Mapper064 final : public Mapper {
public:
    uint32_t capabilities() const override { return (variant158 ? MAPPER_CAP_NAMETABLE : MAPPER_CAP_MIRRORING) | MAPPER_CAP_IRQ | MAPPER_CAP_PPU_ADDRESS_BUS; }
    Mapper064(uint8_t prgBanks, uint8_t chrBanks, bool variant158 = false);

    void reset() override;

    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    uint8_t mirroringOverride() const override { return variant158 ? 0xFF : mirrorMode; }
    bool nametableRead(uint16_t addr, uint8_t& data) override;
    bool nametableWrite(uint16_t addr, uint8_t data) override;

    bool irqState() const override { return irqActive; }
    void irqClear() override { irqActive = false; }

    void ppuAddressBus(uint16_t addr) override;
    void scanline() override; // Legacy external call: hardware timing is A12-based.
    bool usesCpuClock() const override { return true; }
    void cpuClock() override;

    uint32_t saveStateVersion() const override { return variant158 ? 1u : 3u; }
    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;

private:
    const bool variant158;
    // Mapper 158 routes CHR bank bit 7 to CIRAM A10, independently for
    // each nametable quadrant; mapper 64 uses ordinary mirroring.
    std::array<uint8_t, 2048> ciram{};
    uint8_t ntPage[4]{};

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
    uint8_t irqDelay = 0; // M2 delay to IRQ assertion after counter reaches zero
    bool a12Previous = false;
    uint8_t a12LowDots = 0;

    uint8_t prgBankCount8k() const;
    uint16_t chrBankCount1k() const;

    void updateBanks();
    void clockIrqCounter();
};
