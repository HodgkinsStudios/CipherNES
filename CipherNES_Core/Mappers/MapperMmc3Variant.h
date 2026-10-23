#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include "Mapper.h"
#include "Mapper004.h"

// MMC3-compatible boards whose IRQ/protection block is standard but whose
// cartridge wiring transforms PRG/CHR bank lines, adds outer-bank registers,
// mixes CHR ROM with CHR RAM, or repurposes CHR A17 for nametable selection.
class MapperMmc3Variant final : public Mapper {
public:
    MapperMmc3Variant(uint16_t prgBanks16k, uint16_t chrBanks8k, uint16_t mapperNumber);
    uint32_t capabilities() const override;
    void reset() override;
    void reset(bool hardReset) override;
    bool cpuReadDirect(uint16_t addr, uint8_t& data) override;
    void cpuFilterRead(uint16_t addr, uint8_t& data) override;
    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool nametableRead(uint16_t addr, uint8_t& data) override;
    bool nametableWrite(uint16_t addr, uint8_t data) override;
    uint8_t mirroringOverride() const override;
    bool irqState() const override { return core.irqState(); }
    void irqClear() override { core.irqClear(); }
    void ppuAddressBus(uint16_t addr) override;
    void setMmc3AlternateIrqRevision(bool v) override {
        // Mapper 12 is wired around MMC3 revision-A IRQ behaviour.
        core.setMmc3AlternateIrqRevision((mapperNumber == 12 || mapperNumber == 114) ? true : v);
    }
    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;

private:
    const uint16_t mapperNumber;
    Mapper004 core;
    uint8_t bankSelect = 0;
    uint8_t prgMode = 0;
    uint8_t chrMode = 0;
    uint8_t bankRegs[8]{};
    uint8_t exRegs[4]{};
    uint8_t exIndex = 0;
    uint8_t ntPages[4]{};
    bool prgRamEnabled = true;
    bool prgRamWriteProtected = false;
    uint8_t variantMirror = 0xFF;
    uint8_t resetCounter = 0; // mapper 128 Reset-TxROM outer bank
    std::array<uint8_t, 0x800> ntRam{};

    // Mixed CHR-ROM/CHR-RAM boards (74, 119, 195). Values are in 1 KiB pages.
    uint16_t chrRamFirstPage = 0;
    uint16_t chrRamLastPage = 0;
    uint16_t chrRamPageCount = 0;

    uint16_t innerPrg(uint8_t slot) const;
    uint16_t innerChr(uint8_t slot) const;
    uint32_t transformPrgPage(uint32_t page, uint8_t slot) const;
    uint32_t transformChrPage(uint32_t page, uint8_t slot) const;
    bool isMixedChrRamPage(uint32_t page) const;
    uint32_t mapChrPage(uint32_t page, uint32_t offset) const;
    void observeMmc3Write(uint16_t addr, uint8_t data);
};
