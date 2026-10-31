#pragma once

#include <cstdint>
#include <vector>
#include "Mapper.h"

// iNES/NES 2.0 mapper 210: Namco 175 (submapper 1) and Namco 340
// (submapper 2).  These are cost-reduced Namco 163-family ASICs: neither has
// the 15-bit IRQ counter nor expansion audio.  Legacy submapper 0 is resolved
// according to the NES 2.0 recommendation: battery/PRG-RAM => 175, otherwise
// 340.
class Mapper210 final : public Mapper {
public:
    Mapper210(uint16_t prgBanks16k, uint16_t chrBanks8k,
              uint8_t submapper, bool batteryOrPrgRam);

    uint32_t capabilities() const override { return MAPPER_CAP_MIRRORING; }
    void reset() override;
    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    uint8_t mirroringOverride() const override { return mirrorMode; }
    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;

private:
    enum Variant : uint8_t { Namco175 = 1, Namco340 = 2 };
    Variant variant;
    uint16_t prg8[3]{};
    uint16_t chr1[8]{};
    uint8_t writeProtect = 0;
    uint8_t mirrorMode = 0xFF;

    uint16_t prgCount8() const;
    uint16_t chrCount1() const;
};
