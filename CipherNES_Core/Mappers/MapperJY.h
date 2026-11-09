#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include "Mapper.h"

// J.Y. Company ASIC used by iNES mappers 90 and 211. This implements the
// ASIC's PRG/CHR modes,
// advanced nametable mapping, multiplier/register RAM and four IRQ sources.
class MapperJY final : public Mapper {
public:
    MapperJY(uint16_t prgBanks16k, uint16_t chrBanks8k, uint16_t mapperNumber);

    uint32_t capabilities() const override;
    void reset() override;

    bool cpuReadDirect(uint16_t addr, uint8_t& data) override;
    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool nametableRead(uint16_t addr, uint8_t& data) override;
    bool nametableWrite(uint16_t addr, uint8_t data) override;

    uint8_t mirroringOverride() const override { return mirrorMode; }
    bool usesCpuClock() const override { return true; }
    void cpuClockEx(uint64_t masterClock, uint16_t cpuAddressBus,
                    bool cpuIsWriteCycle, bool dmaActive) override;
    void ppuAddressBus(uint16_t addr) override;
    void beforePpuFetch(uint16_t addr, uint8_t kind, int16_t scanline,
                        int16_t cycle, uint64_t masterClock) override;
    bool irqState() const override { return irqActive; }
    void irqClear() override { irqActive = false; }

    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;

private:
    enum IrqSource : uint8_t { CpuClock=0, PpuA12Rise=1, PpuRead=2, CpuWrite=3 };
    const uint16_t mapperNumber;

    uint8_t prgRegs[4]{};
    uint8_t chrLow[8]{};
    uint8_t chrHigh[8]{};
    uint16_t prgPage[4]{};
    uint16_t chrPage[8]{};

    uint8_t prgMode = 0;
    bool enablePrg6000 = false;
    uint8_t chrMode = 0;
    bool chrBlockMode = false;
    uint8_t chrBlock = 0;
    bool mirrorChr = false;

    uint8_t mirroringReg = 0;
    bool advancedNtControl = false;
    bool disableNtRam = false;
    uint8_t ntRamSelectBit = 0;
    uint8_t ntLow[4]{};
    uint8_t ntHigh[4]{};
    uint8_t mirrorMode = 1;
    std::array<uint8_t,0x800> ntRam{};

    bool irqEnabled = false;
    IrqSource irqSource = CpuClock;
    uint8_t irqDirection = 0;
    bool irqFunkyMode = false;
    uint8_t irqFunkyReg = 0;
    bool irqSmallPrescaler = false;
    uint8_t irqPrescaler = 0;
    uint8_t irqCounter = 0;
    uint8_t irqXor = 0;
    bool irqActive = false;
    uint16_t lastPpuAddr = 0;

    uint8_t multiply1 = 0;
    uint8_t multiply2 = 0;
    uint8_t regRam = 0;

    bool advancedNametables() const;
    uint8_t invertPrg(uint8_t value) const;
    uint16_t getChrReg(uint8_t index) const;
    void updatePrg();
    void updateChr();
    void updateMirroring();
    void updateState();
    void tickIrq();
    uint32_t prgCount8() const;
    uint32_t chrCount1() const;
};
