#pragma once

#include <cstdint>
#include <vector>
#include <string>

enum MapperCapability : uint32_t {
    MAPPER_CAP_CPU_DIRECT       = 1u << 0,
    MAPPER_CAP_PPU_DIRECT       = 1u << 1,
    MAPPER_CAP_NAMETABLE        = 1u << 2,
    MAPPER_CAP_ATTRIBUTE        = 1u << 3,
    MAPPER_CAP_PPU_ADDR_LATCH   = 1u << 4,
    MAPPER_CAP_CPU_OBSERVE      = 1u << 5,
    MAPPER_CAP_PPU_OBSERVE      = 1u << 6,
    MAPPER_CAP_PPU_FETCH_HOOKS  = 1u << 7,
    MAPPER_CAP_IRQ              = 1u << 8,
    MAPPER_CAP_MIRRORING        = 1u << 9,
    MAPPER_CAP_EXPANSION_AUDIO  = 1u << 10,
    MAPPER_CAP_SCANLINE         = 1u << 11,
    // Observe the actual external PPU address bus once per PPU dot. This is
    // separate from MMC2/MMC4 read-triggered latches and is required for MMC3
    // A12 filtering, including CPU-driven $2006/$2007 activity.
    MAPPER_CAP_PPU_ADDRESS_BUS  = 1u << 12,
    // Mapper register writes are electrically ANDed with the currently visible PRG-ROM byte.
    MAPPER_CAP_BUS_CONFLICT      = 1u << 13,
    // Post-process a byte after normal PRG-ROM/PRG-RAM mapping. Used by
    // protection hardware such as mapper 254 without putting RAM ownership
    // inside the mapper object.
    MAPPER_CAP_CPU_READ_FILTER    = 1u << 14
};

// Base class for mappers. Responsible for translating CPU/PPU addresses
// into indices into PRG/CHR memory.
class Mapper {
public:
    // Convention used by this project: if bit 31 of mappedAddr is set, the target is PRG-RAM
    // and the low 31 bits are the index into the PRG-RAM array.
    static constexpr uint32_t PRG_RAM_FLAG = 0x80000000u;
    // Mixed CHR-ROM/CHR-RAM boards use bit 30 to select the cartridge's
    // dedicated CHR-RAM array; low 30 bits are the byte index.
    static constexpr uint32_t CHR_RAM_FLAG = 0x40000000u;

    Mapper(uint16_t prgBanks, uint16_t chrBanks) : prgBanks(prgBanks), chrBanks(chrBanks) {}
    virtual ~Mapper() = default;

    // Immutable mapper feature flags. Cartridge and PPU cache these once so
    // ordinary mappers avoid millions of calls through default no-op hooks.
    virtual uint32_t capabilities() const { return 0; }

    // Reset internal mapper state. The boolean overload distinguishes a
    // power/hard reset from the console RESET line for multicarts that use
    // reset sequencing. Existing mappers retain their original behavior.
    virtual void reset() {}
    virtual void reset(bool hardReset) { (void)hardReset; reset(); }

    // Optional direct CPU register hooks. Return true when the mapper itself
    // handled the read/write without mapping to PRG/PRG-RAM. Existing mappers
    // keep the default false behavior unless the mapper owns direct CPU registers.
    virtual bool cpuReadDirect(uint16_t /*addr*/, uint8_t& /*data*/) { return false; }
    virtual bool cpuWriteDirect(uint16_t /*addr*/, uint8_t /*data*/) { return false; }
    virtual void cpuFilterRead(uint16_t /*addr*/, uint8_t& /*data*/) {}

    // Return true if the mapper handles this CPU address.
    // When true, mappedAddr is an index into the PRG ROM/RAM array.
    virtual bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) = 0;
    // When true, mappedAddr is an index into PRG. Some mappers also expose PRG-RAM.
    // Convention used by this project: if bit 31 of mappedAddr is set, the target is PRG-RAM
    // and the low 31 bits are the index into the PRG-RAM array.
    virtual bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) = 0;

    // Return true if the mapper handles this PPU address.
    // When true, mappedAddr is an index into the CHR ROM/RAM array.
    virtual bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) = 0;
    virtual bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) = 0;

    // Optional: mapper-controlled mirroring and IRQ
    virtual uint8_t mirroringOverride() const { return 0xFF; } // 0xFF = none
    virtual bool irqState() const { return false; }
    virtual void irqClear() {}

    // Optional: per-CPU-cycle clocking (used by mappers with cycle IRQs).
    // Most mappers do not need this, so the bus can skip a virtual call on
    // every CPU cycle for them.
    virtual bool usesCpuClock() const { return false; }
    virtual void cpuClock() {}
    virtual void cpuClockEx(uint64_t /*masterClock*/, uint16_t /*cpuAddressBus*/, bool /*cpuIsWriteCycle*/, bool /*dmaActive*/) { cpuClock(); }

    // Optional: per-PPU-dot clocking for mapper-side video hardware / exact IRQs.
    virtual bool usesPpuClock() const { return false; }
    virtual void ppuClock(int16_t /*scanline*/, int16_t /*cycle*/, uint64_t /*masterClock*/) {}
    virtual void beforePpuFetch(uint16_t /*addr*/, uint8_t /*kind*/, int16_t /*scanline*/, int16_t /*cycle*/, uint64_t /*masterClock*/) {}
    virtual void afterPpuFetch(uint16_t /*addr*/, uint8_t /*kind*/, int16_t /*scanline*/, int16_t /*cycle*/, uint64_t /*masterClock*/, uint8_t /*data*/) {}

    // Optional: scanline-based IRQ clocking (useful for MMC3 in simpler emus)
    virtual void scanline() {}

    // Optional: allow mapper to observe PPU addresses (MMC3 uses A12 edges)
    virtual void ppuAddressLatch(uint16_t /*addr*/) {}

    // Optional: observe the external PPU address bus once per PPU dot.
    virtual void ppuAddressBus(uint16_t /*addr*/) {}

    // Mapper 4 has two incompatible IRQ-counter silicon behaviours. The
    // default is the common revision-B/C profile; callers may explicitly
    // select the alternate/revision-A profile when cartridge metadata or a
    // validation harness identifies it. Other mappers ignore this setting.
    virtual void setMmc3AlternateIrqRevision(bool /*alternate*/) {}

    // Optional direct PPU hooks. These let a mapper provide generated CHR or
    // cartridge-backed nametable data without mapping into the normal CHR array.
    // Existing mappers keep the default false behavior.
    virtual bool ppuReadDirect(uint16_t /*addr*/, uint8_t& /*data*/) { return false; }
    virtual bool ppuWriteDirect(uint16_t /*addr*/, uint8_t /*data*/) { return false; }
    virtual bool nametableRead(uint16_t /*addr*/, uint8_t& /*data*/) { return false; }
    virtual bool nametableWrite(uint16_t /*addr*/, uint8_t /*data*/) { return false; }
    virtual bool attributeRead(uint16_t /*addr*/, uint8_t& /*data*/) { return false; }
    virtual bool attributeWrite(uint16_t /*addr*/, uint8_t /*data*/) { return false; }

    // Optional observation hooks. These do not replace mapping; they let custom
    // hardware observe the real bus before/after normal emulation.
    virtual void beforeCpuRead(uint16_t /*addr*/) {}
    virtual void afterCpuRead(uint16_t /*addr*/, uint8_t /*data*/) {}
    virtual void beforeCpuWrite(uint16_t /*addr*/, uint8_t /*data*/) {}
    virtual void afterCpuWrite(uint16_t /*addr*/, uint8_t /*data*/) {}
    virtual void beforePpuRead(uint16_t /*addr*/) {}
    virtual void afterPpuRead(uint16_t /*addr*/, uint8_t /*data*/) {}
    virtual void beforePpuWrite(uint16_t /*addr*/, uint8_t /*data*/) {}
    virtual void afterPpuWrite(uint16_t /*addr*/, uint8_t /*data*/) {}

    // Optional mapper-side / expansion audio. Implementations add samples to the
    // final APU output in Cartridge::processCartridgeAudioPath*.
    virtual bool usesExpansionAudio() const { return false; }
    virtual void setAudioSampleRate(double /*sampleRateHz*/) {}
    virtual float audioSample() { return 0.0f; }
    virtual float audioSampleEx(double /*sampleRateHz*/, uint64_t /*sampleIndex*/, float /*nesApuSample*/) { return audioSample(); }

    // Optional debug text for frontend/tooling.
    virtual std::string debugText() const { return std::string(); }

    // Optional Famicom Disk System controls/persistence.  These remain no-ops
    // for ordinary cartridge mappers and keep disk mechanics out of the CPU/PPU.
    virtual bool isDiskSystem() const { return false; }
    virtual uint32_t diskSideCount() const { return 0; }
    virtual uint32_t currentDiskSide() const { return 0xFFFFFFFFu; }
    virtual bool insertDiskSide(uint32_t /*side*/) { return false; }
    virtual void ejectDisk() {}
    virtual void flushPersistentData() {}

    virtual uint32_t saveStateVersion() const { return 1u; }

    // ------------------------------
    // Save-state support
    // ------------------------------
    // Default implementation saves nothing. Concrete mappers should override
    // and serialize all internal registers/counters needed for determinism.
    virtual void saveState(std::vector<uint8_t>& /*out*/) const {}
    virtual bool loadState(const uint8_t*& /*p*/, const uint8_t* /*end*/) { return true; }

protected:
    uint16_t prgBanks = 0;
    uint16_t chrBanks = 0;
};
