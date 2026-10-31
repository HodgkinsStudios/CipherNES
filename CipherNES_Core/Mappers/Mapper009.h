#pragma once

#include "Mapper.h"

// -----------------------------------------------------------------------------
// Mapper009 (MMC2 / PxROM)
// Used by Mike Tyson's Punch-Out!!
//
// PRG:
//   $8000-$9FFF : 8 KB switchable
//   $A000-$FFFF : last 24 KB fixed (three 8 KB banks)
//   $6000-$7FFF : 8 KB PRG-RAM (PlayChoice variants)
//
// CHR:
//   Two 4 KB regions ($0000-$0FFF and $1000-$1FFF), each controlled by a latch.
//   Latches switch between $FD and $FE banks based on PPU reads.
// -----------------------------------------------------------------------------
class Mapper009 : public Mapper {
public:
    uint32_t capabilities() const override { return MAPPER_CAP_MIRRORING | MAPPER_CAP_PPU_ADDR_LATCH; }
    Mapper009(uint8_t prgBanks, uint8_t chrBanks);

    void reset() override;

    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    void ppuAddressLatch(uint16_t addr) override;
    uint8_t mirroringOverride() const override { return mirrorMode; }

private:
    // PRG: 8 KB bank index for $8000-$9FFF
    uint8_t prgBank8k = 0;

    // CHR: 4 KB bank indices
    uint8_t chrFD_0000 = 0;
    uint8_t chrFE_0000 = 0;
    uint8_t chrFD_1000 = 0;
    uint8_t chrFE_1000 = 0;

    // Latch states: 0xFD or 0xFE
    uint8_t latch0 = 0xFE;
    uint8_t latch1 = 0xFE;

    // Mapper-controlled mirroring override:
    // Cartridge expects 0 = HORIZONTAL, 1 = VERTICAL.
    uint8_t mirrorMode = 1; // default vertical

    uint8_t prg8kBankCount() const { return uint8_t(prgBanks * 2); }
    uint8_t chr4kBankCount() const { return uint8_t(chrBanks * 2); }

    uint32_t mapChr4k(uint8_t bank4k, uint16_t addr) const;
};
