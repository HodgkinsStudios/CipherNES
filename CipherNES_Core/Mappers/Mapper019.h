#pragma once

#include "Mapper.h"
#include <array>

// Mapper 19: Namco 129/163 family. Implements 12x1 KiB CHR/NT banks,
// internal 128-byte data port/RAM, 15-bit cycle IRQ and N163 wavetable
// synthesis. NES 2.0 submappers 1/2 lack expansion audio.
class Mapper019 final : public Mapper {
public:
    Mapper019(uint16_t prgBanks, uint16_t chrBanks, uint8_t submapper = 0, bool externalRamPresent = true);
    uint32_t capabilities() const override {
        return MAPPER_CAP_IRQ | MAPPER_CAP_CPU_DIRECT | MAPPER_CAP_PPU_DIRECT |
               MAPPER_CAP_NAMETABLE | MAPPER_CAP_EXPANSION_AUDIO;
    }
    void reset() override;
    bool cpuReadDirect(uint16_t addr, uint8_t& data) override;
    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuReadDirect(uint16_t addr, uint8_t& data) override;
    bool ppuWriteDirect(uint16_t addr, uint8_t data) override;
    bool nametableRead(uint16_t addr, uint8_t& data) override;
    bool nametableWrite(uint16_t addr, uint8_t data) override;
    bool nametableMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool nametableMapWrite(uint16_t addr, uint32_t& mappedAddr) override;
    bool irqState() const override { return irqActive; }
    void irqClear() override { irqActive = false; }
    bool usesCpuClock() const override { return true; }
    void cpuClock() override;
    bool usesExpansionAudio() const override { return submapper != 1 && submapper != 2; }
    float audioSample() override;
    size_t persistentRamSize() const override { return chipRam.size(); }
    void copyPersistentRam(uint8_t* dst, size_t size) const override;
    void restorePersistentRam(const uint8_t* src, size_t size) override;
    bool persistentRamDirty() const override { return chipRamDirty; }
    void clearPersistentRamDirty() override { chipRamDirty = false; }
    uint32_t saveStateVersion() const override { return 3u; }
    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;

private:
    const uint8_t submapper;
    const bool externalRamPresent;
    uint8_t prgBank8k[3]{};
    uint8_t chrBank1k[12]{};
    uint8_t wramProtect = 0;
    uint8_t chrRamDisable = 0;
    uint16_t irqCounter = 0;
    bool irqEnabled = false;
    bool irqActive = false;
    std::array<uint8_t,128> chipRam{};
    std::array<uint8_t,2048> ciram{};
    uint8_t chipAddr = 0;
    bool chipIncrement = false;
    bool chipRamDirty = false;
    bool soundDisabled = false;
    uint8_t soundDivider = 0;
    uint8_t currentChannel = 7;
    float heldOutput = 0;

    void stepAudio();
    void stepChipAddr();
    bool ciramSelected(uint8_t slot) const;
    uint16_t chrCount() const { return chrBanks ? uint16_t(chrBanks * 8u) : 8u; }
    uint16_t prgCount() const { return prgBanks ? uint16_t(prgBanks * 2u) : 1u; }
    bool wramWriteEnabled(uint16_t addr) const;
};
