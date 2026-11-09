#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Mapper.h"

// Nintendo Famicom Disk System / iNES mapper 20.
//
// This is not a conventional cartridge mapper.  The RAM Adapter provides:
//   $6000-$DFFF  32 KiB work RAM
//   $E000-$FFFF  8 KiB Disk System BIOS
//   PPU $0000-$1FFF 8 KiB CHR RAM
// and the 2C33 disk/audio registers at $4020-$4092.
class Mapper020 final : public Mapper {
public:
    static const uint32_t NoDiskInserted = 0xFFFFFFFFu;

    Mapper020(uint16_t prgBanks16k,
              uint16_t chrBanks8k,
              const std::vector<uint8_t>& rawDiskImage,
              const std::string& persistentSavePath,
              bool qdFormat = false);
    ~Mapper020() override;

    uint32_t capabilities() const override {
        return MAPPER_CAP_CPU_DIRECT | MAPPER_CAP_IRQ |
               MAPPER_CAP_MIRRORING | MAPPER_CAP_EXPANSION_AUDIO;
    }

    void reset() override;
    void reset(bool hardReset) override;

    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    bool cpuReadDirect(uint16_t addr, uint8_t& data) override;
    bool cpuWriteDirect(uint16_t addr, uint8_t data) override;

    uint8_t mirroringOverride() const override { return mirrorMode; }

    bool usesCpuClock() const override { return true; }
    void cpuClock() override;
    bool irqState() const override { return timerIrqActive || diskIrqActive; }
    void irqClear() override { timerIrqActive = false; diskIrqActive = false; }

    bool usesExpansionAudio() const override { return true; }
    float audioSample() override;

    // Disk controls exposed through the generic Mapper interface.
    bool isDiskSystem() const override { return true; }
    uint32_t diskSideCount() const override { return uint32_t(diskSides.size()); }
    uint32_t currentDiskSide() const override { return diskNumber; }
    bool insertDiskSide(uint32_t side) override;
    void ejectDisk() override;
    void flushPersistentData() override;

    std::string debugText() const override;
    uint32_t saveStateVersion() const override { return 2u; }
    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;

private:
    static const size_t FdsSideCapacity = 65500u;
    static const size_t QdSideCapacity = 65536u;

    struct Envelope {
        uint8_t speed = 0;
        uint8_t gain = 0;
        bool envelopeOff = false;
        bool increase = false;
        uint16_t frequency = 0;
        uint32_t timer = 0;
        uint8_t masterSpeed = 0xE8;

        void reset();
        void writeReg(uint16_t addr, uint8_t value);
        bool tick();
        void resetTimer();
        void save(std::vector<uint8_t>& out) const;
        bool load(const uint8_t*& p, const uint8_t* end);
    } volume;

    struct Modulator : public Envelope {
        int8_t counter = 0;
        bool disabled = false;
        uint8_t table[64]{};
        uint8_t tablePosition = 0;
        uint16_t overflowCounter = 0;
        int32_t output = 0;

        void reset();
        void writeReg(uint16_t addr, uint8_t value);
        void writeTable(uint8_t value);
        bool tickModulator();
        void updateCounter(int value);
        void updateOutput(uint16_t carrierPitch);
        void save(std::vector<uint8_t>& out) const;
        bool load(const uint8_t*& p, const uint8_t* end);
    } mod;

    bool useQdFormat = false;
    bool imageHadHeader = false;
    std::string savePath;
    std::vector<std::vector<uint8_t> > diskSides;
    bool diskDirty = false;

    // Timer IRQ registers/state.
    uint16_t irqReloadValue = 0;
    uint16_t irqCounter = 0;
    bool irqEnabled = false;
    bool irqRepeatEnabled = false;
    bool timerIrqActive = false;

    // I/O enable and drive registers.
    bool diskRegistersEnabled = false;
    bool soundRegistersEnabled = false;
    uint8_t writeDataRegister = 0;
    bool motorOn = false;
    bool resetTransfer = false;
    bool readMode = true;
    bool crcControl = false;
    bool diskReady = false;
    bool diskIrqEnabled = false;
    uint8_t externalConnector = 0;

    // Drive status/internal state.
    uint8_t readDataRegister = 0;
    bool transferComplete = false;
    bool badCrc = false;
    bool endOfHead = true;
    bool scanningDisk = false;
    bool previousCrcControl = true;
    bool gapEnded = false;
    bool diskIrqActive = false;
    uint32_t diskNumber = NoDiskInserted;
    uint32_t diskPosition = 0;
    uint32_t transferDelay = 0;
    uint16_t crcAccumulator = 0;
    uint8_t mirrorMode = 1; // Mapper convention: 0 horizontal, 1 vertical.

    // FDS wavetable/modulation audio.
    uint8_t waveTable[64]{};
    bool waveWriteEnabled = false;
    bool disableEnvelopes = false;
    bool haltWaveform = false;
    uint8_t masterVolume = 0;
    uint16_t waveOverflowCounter = 0;
    uint8_t wavePosition = 0;
    uint8_t currentAudioLevel = 0;

    size_t sideCapacity() const { return useQdFormat ? QdSideCapacity : FdsSideCapacity; }
    bool loadImage(const std::vector<uint8_t>& rawImage);
    void addGaps(std::vector<uint8_t>& expanded, const uint8_t* source, size_t bytes) const;
    std::vector<uint8_t> rebuildImage() const;

    void clockTimerIrq();
    void clockDiskDrive();
    void updateCrc(uint8_t value);
    uint8_t readDiskByte() const;
    void writeDiskByte(uint8_t value);

    void resetAudio();
    void clockAudio();
    uint8_t readAudioRegister(uint16_t addr) const;
    void writeAudioRegister(uint16_t addr, uint8_t value);
};
