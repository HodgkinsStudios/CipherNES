#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Mapper;

class Cartridge {
public:
    enum class Mirror : uint8_t {
        HORIZONTAL = 0,
        VERTICAL = 1,
        FOUR_SCREEN = 2,
        ONE_SCREEN_LO = 3, // all nametables map to NT0
        ONE_SCREEN_HI = 4, // all nametables map to NT1
    };

    explicit Cartridge(const std::string& nesFilePath);
    Cartridge(const std::string& nesFilePath, const std::string& saveIdentityPath);
    ~Cartridge();

    bool imageValid() const { return valid; }
    uint16_t mapperId() const { return mapper; }
    uint16_t prgBanks() const { return prgBankCount; }
    uint16_t chrBanks() const { return chrBankCount; }

    Mirror mirroring() const;

    // Mapper IRQ (used by MMC3 and others)
    bool irqState() const;
    void irqClear();

    // Optional: mapper scanline hook (MMC3 IRQ ticking)
    void scanline();

    // Optional: mapper per-CPU-cycle hook (cycle IRQs)
    bool usesCpuClock() const;
    bool usesPpuFetchHooks() const { return mapperUsesPpuFetchHooks; }
    bool canAssertIrq() const { return mapperCanAssertIrq; }
    bool usesScanlineHook() const { return mapperUsesScanline; }
    bool usesPpuAddressBus() const { return mapperUsesPpuAddressBus; }
    bool hasDynamicMirroring() const { return mapperControlsMirroring; }
    bool handlesExternalNametables() const { return mapperHandlesExternalNametables; }
    void cpuClock();
    void cpuClockEx(uint64_t masterClock, uint16_t cpuAddressBus, bool cpuIsWriteCycle, bool dmaActive);
    bool usesPpuClock() const;
    void ppuClock(int16_t scanline, int16_t cycle, uint64_t masterClock);
    void beforePpuFetch(uint16_t addr, uint8_t kind, int16_t scanline, int16_t cycle, uint64_t masterClock);
    void afterPpuFetch(uint16_t addr, uint8_t kind, int16_t scanline, int16_t cycle, uint64_t masterClock, uint8_t data);
    void ppuAddressBus(uint16_t addr);
    bool setMmc3AlternateIrqRevision(bool alternate);

    // CPU bus access
    bool cpuRead(uint16_t addr, uint8_t& data);
    bool cpuWrite(uint16_t addr, uint8_t data);

    // PPU bus access (pattern tables / CHR)
    bool ppuRead(uint16_t addr, uint8_t& data);
    bool ppuWrite(uint16_t addr, uint8_t data);
    bool mapperNametableRead(uint16_t addr, uint8_t& data);
    bool mapperNametableWrite(uint16_t addr, uint8_t data);

    // Save-state support
    void saveState(std::vector<uint8_t>& out) const;
    bool loadState(const uint8_t*& p, const uint8_t* end);

    // Reset/persistence hooks used by the core/frontend.
    // Mapper capability flags are cached for hot paths.
    void resetMapper(bool hardReset);
    void flushSave();

    // Famicom Disk System controls. Ordinary cartridge mappers report false/0.
    bool isDiskSystem() const;
    uint32_t diskSideCount() const;
    uint32_t currentDiskSide() const;
    bool insertDiskSide(uint32_t side);
    void ejectDisk();

    // Memory views (for UI tools)
    const std::vector<uint8_t>& prgRom() const { return prgMemory; }
    const std::vector<uint8_t>& chrMem() const { return chrMemory; }
    const std::vector<uint8_t>& chrRamMem() const { return chrRam; }
    const std::vector<uint8_t>& prgRamMem() const { return prgRam; }

    // Mapper expansion-audio path. No-op when the active mapper has no expansion audio.
    // normal mappers. Mapper 777 uses the stock NES DMC output route.
    void setExpansionAudioSampleRate(double sampleRateHz);
    void processCartridgeAudioPath(float* samples, int count);
    void processCartridgeAudioPathI16(int16_t* samples, int count);
    std::string mapperDebugText() const;

private:
    bool valid = false;

    uint16_t mapper = 0;
    uint16_t prgBankCount = 0;
    uint16_t chrBankCount = 0;

    Mirror mirror = Mirror::HORIZONTAL;

    std::vector<uint8_t> prgMemory;
    std::vector<uint8_t> chrMemory;
    std::vector<uint8_t> chrRam;
    std::vector<uint8_t> prgRam;

    // Battery-backed PRG-RAM (SRAM) persistence
    bool batteryBacked = false;
    bool prgRamDirty = false;
    std::string saveFilePath;

    void initSaveFilePath(const std::string& nesFilePath);
    void loadOrCreateSave();

    std::shared_ptr<Mapper> mapperImpl;
    uint32_t mapperCapabilities = 0;
    bool mapperUsesCpuClock = false;
    bool mapperUsesPpuClock = false;
    bool mapperUsesPpuFetchHooks = false;
    bool mapperCanAssertIrq = false;
    bool mapperUsesScanline = false;
    bool mapperUsesPpuAddressBus = false;
    bool mapperControlsMirroring = false;
    bool mapperHandlesExternalNametables = false;
};
