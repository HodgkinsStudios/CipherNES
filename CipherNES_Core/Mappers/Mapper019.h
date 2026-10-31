#pragma once

#include "Mapper.h"

// iNES Mapper 019 - Namco 129/163 (banking + cycle IRQ)
// This implementation covers the common mapper-facing behaviors needed by most games:
//  - 8KB PRG banks at $8000/$A000/$C000, fixed last at $E000
//  - 8KB PRG-RAM window at $6000-$7FFF
//  - 8 x 1KB CHR banks for $0000-$1FFF (nametable CHR mapping and expansion audio not emulated)
//  - 15-bit CPU-cycle IRQ up-counter
//  - PRG-RAM write-protect windows via $F800-$FFFF

class Mapper019 : public Mapper {
public:
    uint32_t capabilities() const override { return MAPPER_CAP_IRQ; }
    Mapper019(uint8_t prgBanks, uint8_t chrBanks);

    void reset() override;

    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    bool irqState() const override { return irqActive; }
    void irqClear() override { irqActive = false; }
    bool usesCpuClock() const override { return true; }
    void cpuClock() override;

private:
    uint8_t prgBank8k[3]{}; // $8000, $A000, $C000
    uint16_t chrBank1k[8]{}; // $0000-$1FFF

    // PRG-RAM write protect
    uint8_t wramProtect = 0x00;

    // IRQ
    uint16_t irqCounter = 0; // 15-bit
    bool irqEnabled = false;
    bool irqActive = false;

    bool wramWriteEnabled(uint16_t addr) const;
    uint8_t prgBankCount8k() const { return uint8_t(prgBanks * 2); }
    uint16_t chrBankCount1k() const { return (chrBanks == 0) ? 8u : uint16_t(chrBanks) * 8u; }
};
