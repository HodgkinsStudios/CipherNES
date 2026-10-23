#pragma once

#include <cstdint>
#include "Mapper.h"

// Mapper 1 (MMC1)
class Mapper001 final : public Mapper {
public:
    uint32_t capabilities() const override {
        return MAPPER_CAP_MIRRORING
            | ((chrBanks == 0 && prgBanks == 8) ? MAPPER_CAP_PPU_ADDRESS_BUS : 0u);
    }
    Mapper001(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) { reset(); }

    void reset() override;

    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    uint8_t mirroringOverride() const override { return mirrorMode; }
    void ppuAddressBus(uint16_t addr) override { currentPpuA12 = (addr & 0x1000u) != 0; }

    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;

private:
    // MMC1 serial load register
    uint8_t shiftReg = 0x10; // 0b10000 indicates empty (per common implementations)
    uint8_t shiftCount = 0;

    // Internal registers
    uint8_t control = 0x1C;   // default: PRG mode 3, CHR mode 0, mirroring 0
    uint8_t chrBank0 = 0;
    uint8_t chrBank1 = 0;
    uint8_t prgBank = 0;

    // Derived state
    uint8_t mirrorMode = 0xFF; // 0=H,1=V,2=FOUR not used; MMC1 provides one-screen too
    bool prgRamEnabled = true;
    bool currentPpuA12 = false;

    void commitRegister(uint16_t addr, uint8_t value);
    void updateMirroring();
    bool prgRamAccessible() const;
};
