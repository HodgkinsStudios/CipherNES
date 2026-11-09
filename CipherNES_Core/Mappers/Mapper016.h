#pragma once
#include "Mapper.h"
#include "BandaiSerialEeprom.h"
#include <array>
#include <string>

// Bandai FCG-1/FCG-2, LZ93D50, Famicom Jump II (153), Datach (157),
// and X24C01 LZ93D50 (159) share an IRQ/PRG ASIC but NOT identical wiring.
class Mapper016 : public Mapper {
public:
    // Old constructor kept compatible with test and source callers.
    Mapper016(uint16_t prgBanks,uint16_t chrBanks,uint8_t submapper=0,
              bool eepromPresent=false,uint16_t board=16,bool externalEeprom=false);
    uint32_t capabilities() const override;
    void reset() override;
    bool cpuReadDirect(uint16_t addr,uint8_t& data) override;
    bool cpuMapRead(uint16_t addr,uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr,uint32_t& mappedAddr,uint8_t data) override;
    bool ppuMapRead(uint16_t addr,uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr,uint32_t& mappedAddr,uint8_t data) override;
    uint8_t mirroringOverride() const override {return mirrorMode;}
    bool irqState() const override{return irqActive;}
    void irqClear() override{irqActive=false;}
    bool usesCpuClock() const override {return true;}
    void cpuClock() override;
    void ppuAddressBus(uint16_t addr) override;
    size_t persistentRamSize() const override;
    void copyPersistentRam(uint8_t* dst,size_t n) const override;
    void restorePersistentRam(const uint8_t* src,size_t n) override;
    bool persistentRamDirty() const override;
    void clearPersistentRamDirty() override;
    size_t sharedPersistentRamSize() const override;
    void copySharedPersistentRam(uint8_t* dst,size_t n) const override;
    void restoreSharedPersistentRam(const uint8_t* src,size_t n) override;
    bool sharedPersistentRamDirty() const override;
    void clearSharedPersistentRamDirty() override;
    bool queueDatachBarcode(const std::string& digits) override;
    uint32_t saveStateVersion() const override {return 4u;}
    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p,const uint8_t* end) override;
private:
    uint8_t submapper;
    uint16_t board;
    bool hasEeprom,hasExternal;
    BandaiSerialEeprom internal,external;
    uint8_t prgBank=0,outerBank=0,chrBank[8]{},mirrorMode=0xFF;
    bool wramEnabled=false;
    bool internalScl=true,externalScl=true,masterSda=true;
    bool datachClockRegister[4]{true,true,true,true};
    uint16_t datachPpuAddr=0;
    uint16_t irqLatch=0,irqCounter=0;
    bool irqEnabled=false,irqActive=false;
    // Datach scanner sensor waveform; sent at selectable fixed digital speed.
    std::vector<uint8_t> barcodeWave;
    uint32_t barcodeStep=0,barcodeCycle=0;
    void clockDatach();
};
