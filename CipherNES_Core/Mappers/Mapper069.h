#pragma once

#include <cstdint>
#include <array>
#include "Mapper.h"

class Mapper069 final : public Mapper {
public:
    uint32_t capabilities() const override { return MAPPER_CAP_MIRRORING | MAPPER_CAP_IRQ | MAPPER_CAP_EXPANSION_AUDIO; }
    Mapper069(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) { reset(); }
    void reset() override;
    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    uint8_t mirroringOverride() const override { return mirrorMode; }
    bool irqState() const override { return irqActive; }
    void irqClear() override { irqActive = false; }
    bool usesCpuClock() const override { return true; }
    void cpuClock() override;
    // Mapper 69 is shared by silent FME-7/5A and audio-capable 5B.
    // Only titles that program the $C000/$E000 sound ports produce output.
    bool usesExpansionAudio() const override { return true; }
    float audioSample() override;
    uint32_t saveStateVersion() const override { return 2u; }
    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;
private:
    uint8_t command = 0;
    uint8_t chr[8] = {0,1,2,3,4,5,6,7};
    uint8_t prg[4] = {0,0,0,0};
    bool wramEnabled = false;
    bool wramSelected = false;
    uint8_t mirrorMode = 0xFF;
    bool irqEnabled = false;
    bool irqCounterEnabled = false;
    bool irqActive = false;
    uint16_t irqCounter = 0;

    // Sunsoft 5B (YM2149F-type) programmable sound generator.
    uint8_t audioAddress = 0;
    std::array<uint8_t,16> audioRegs{};
    uint8_t audioClockDivider = 0;
    uint16_t toneCounter[3] = {0,0,0};
    bool toneHigh[3] = {true,true,true};
    uint16_t noiseCounter = 0;
    uint32_t noiseShift = 0x1FFFFu;
    bool noiseHigh = true;
    uint16_t envelopeCounter = 0;
    int8_t envelopeStep = 31;
    bool envelopeAttack = false;
    bool envelopeHolding = false;

    void clockSound();
    void resetEnvelope(uint8_t shape);
    uint8_t envelopeLevel() const;

    uint8_t prg8kCount() const { return uint8_t(prgBanks * 2); }
    uint16_t chr1kCount() const { return uint16_t(chrBanks * 8); }
};
