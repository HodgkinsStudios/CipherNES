#pragma once

#include <cstdint>
#include "Mapper.h"

// Mapper 4 (MMC3)
class Mapper004 final : public Mapper {
public:
    uint32_t capabilities() const override {
        return MAPPER_CAP_MIRRORING | MAPPER_CAP_IRQ | MAPPER_CAP_PPU_ADDRESS_BUS;
    }
    Mapper004(uint8_t prgBanks, uint8_t chrBanks);

    void reset() override;

    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    uint8_t mirroringOverride() const override { return mirrorMode; }

    bool irqState() const override { return irqActive; }
    void irqClear() override {
        irqActive = false;
        lastA12 = uint16_t(lastA12 & ~uint16_t(0x0007));
    }

    void ppuAddressLatch(uint16_t addr) override;
    void ppuAddressBus(uint16_t addr) override;
    void setMmc3AlternateIrqRevision(bool alternate) override {
        alternateIrqRevision = alternate;
    }

    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;

private:
    // Bank registers
    uint8_t bankSelect = 0;
    uint8_t bankData[8]{};
    uint8_t prgMode = 0;
    uint8_t chrMode = 0;

    // Mirroring (0 = horizontal, 1 = vertical). 0xFF means none.
    uint8_t mirrorMode = 0xFF;

    // PRG-RAM
    bool prgRamEnabled = false;
    bool prgRamWriteProtected = false;

    // IRQ
    uint8_t irqLatch = 0;
    uint8_t irqCounter = 0;
    bool irqReload = false;
    bool irqEnabled = false;
    bool irqActive = false;

    // Board/profile configuration, not mutable emulated state. It deliberately
    // survives reset and therefore does not need to be part of save states.
    bool alternateIrqRevision = false;

    // A12 edge detector. Bit 12 stores the previous A12 sample; low bits are
    // reserved for a tiny scheduler-phase IRQ delay without changing the save
    // state layout.
    uint16_t lastA12 = 0;
    uint8_t a12LowCounter = 0;

    // Cached bank indices
    uint8_t prgBank[4]{};   // 8KB banks
    uint16_t chrBank[8]{};  // 1KB banks

    void clockIrqCounter();
    void updateBanks();
    uint8_t prgBankCount8k() const;
    uint16_t chrBankCount1k() const;
};
