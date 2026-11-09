#pragma once

#include <cstdint>
#include <vector>
#include "Mapper.h"

// Konami VRC6a/VRC6b (iNES 24/26).  Mapper 26 swaps register A0/A1.
class MapperVrc6 final : public Mapper {
public:
    MapperVrc6(uint16_t prgBanks16k, uint16_t chrBanks8k, uint16_t mapperNumber);

    uint32_t capabilities() const override {
        return MAPPER_CAP_IRQ | MAPPER_CAP_NAMETABLE | MAPPER_CAP_EXPANSION_AUDIO;
    }
    void reset() override;
    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool nametableRead(uint16_t addr, uint8_t& data) override;
    bool nametableWrite(uint16_t addr, uint8_t data) override;

    bool usesCpuClock() const override { return true; }
    void cpuClock() override;
    bool irqState() const override { return irqActive; }
    void irqClear() override { irqActive = false; }

    bool usesExpansionAudio() const override { return true; }
    float audioSample() override;

    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;

private:
    struct Pulse {
        uint8_t volume = 0, duty = 0, step = 0, shift = 0;
        bool ignoreDuty = false, enabled = false;
        uint16_t frequency = 1;
        int32_t timer = 1;
        void reset();
        void write(uint8_t reg, uint8_t data);
        void clock();
        uint8_t output() const;
    } pulse[2];

    struct Saw {
        uint8_t rate = 0, accumulator = 0, step = 0, shift = 0;
        bool enabled = false;
        uint16_t frequency = 1;
        int32_t timer = 1;
        void reset();
        void write(uint8_t reg, uint8_t data);
        void clock();
        uint8_t output() const;
    } saw;

    const uint16_t mapperNumber;
    uint16_t prg8[4]{};
    uint16_t chrReg[8]{};
    uint16_t chrPage[8]{};
    uint8_t bankingMode = 0;
    uint8_t ntPage[4]{};
    bool ntUsesChr = false;
    uint8_t ntRam[0x800]{};

    uint8_t irqLatch = 0, irqCounter = 0;
    int16_t irqPrescaler = 0;
    bool irqEnabled = false, irqEnableAfterAck = false, irqCycleMode = false, irqActive = false;

    bool haltAudio = false;
    uint8_t frequencyShift = 0;

    uint16_t translate(uint16_t addr) const;
    uint16_t prgCount8() const;
    uint16_t chrCount1() const;
    void updatePpuBanking();
    void updateNametables();
    void setNt(uint8_t a, uint8_t b, uint8_t c, uint8_t d);
};
