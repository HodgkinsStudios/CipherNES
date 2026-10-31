#pragma once
#include <cstdint>
#include <vector>
#include "Mapper.h"

// Kaiser KS202 hardware. Mapper 56 exposes the extended address-decoded CHR
// and mirroring registers; mapper 142 uses the base IRQ/PRG register set.
class MapperKaiser202 final : public Mapper {
public:
    MapperKaiser202(uint16_t prgBanks16k, uint16_t chrBanks8k, uint16_t mapperNumber);
    uint32_t capabilities() const override;
    void reset() override;
    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    uint8_t mirroringOverride() const override { return mirrorMode; }
    bool usesCpuClock() const override { return true; }
    void cpuClock() override;
    bool irqState() const override { return irqActive; }
    void irqClear() override { irqActive=false; }
    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p,const uint8_t* end) override;
private:
    const uint16_t mapperNumber;
    uint16_t irqReload=0, irqCounter=0;
    uint8_t irqControl=0, selectedReg=0;
    uint8_t prgRegs[4]{};
    uint16_t chr1[8]{};
    uint8_t mirrorMode=0xFF;
    bool useRom=false, irqActive=false;
    uint16_t prgCount8() const;
};
