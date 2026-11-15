#include "Mapper019.h"
#include "../state_io.h"
#include <algorithm>
#include <cstring>

Mapper019::Mapper019(uint16_t p, uint16_t c, uint8_t sub, bool extRam)
    : Mapper(p,c), submapper(sub), externalRamPresent(extRam) { reset(); }

void Mapper019::reset() {
    std::memset(prgBank8k, 0, sizeof(prgBank8k));
    std::memset(chrBank1k, 0, sizeof(chrBank1k));
    wramProtect = chrRamDisable = 0;
    irqCounter = 0;
    irqEnabled = irqActive = false;
    chipAddr = 0;
    chipIncrement = false;
    soundDisabled = false;
    soundDivider = 0;
    currentChannel = 7;
    heldOutput = 0;
    // CIRAM and chip-internal battery-backed RAM are not cleared by RESET.
}

bool Mapper019::wramWriteEnabled(uint16_t addr) const {
    if (wramProtect < 0x40 || wramProtect > 0x4E) return false;
    const unsigned win = unsigned((addr - 0x6000u) / 0x800u);
    return (wramProtect & uint8_t(1u << win)) == 0;
}
void Mapper019::stepChipAddr() {
    if (chipIncrement && chipAddr < 0x7Fu) ++chipAddr;
}

bool Mapper019::cpuReadDirect(uint16_t addr, uint8_t& data) {
    if (addr >= 0x4800u && addr < 0x5000u) {
        data = chipRam[chipAddr];
        stepChipAddr();
        return true;
    }
    if (addr >= 0x5000u && addr < 0x5800u) {
        data = uint8_t(irqCounter);
        return true;
    }
    if (addr >= 0x5800u && addr < 0x6000u) {
        data = uint8_t((irqCounter >> 8u) | (irqEnabled ? 0x80u : 0u));
        return true;
    }
    return false;
}

bool Mapper019::cpuMapRead(uint16_t addr, uint32_t& mapped) {
    if (externalRamPresent && addr >= 0x6000u && addr < 0x8000u) {
        mapped = PRG_RAM_FLAG | (addr & 0x1FFFu);
        return true;
    }
    if (addr >= 0x8000u) {
        const unsigned slot = unsigned((addr - 0x8000u) / 0x2000u);
        const uint16_t bank = (slot == 3u ? prgCount()-1u : prgBank8k[slot] % prgCount());
        mapped = uint32_t(bank) * 0x2000u + (addr & 0x1FFFu);
        return true;
    }
    return false;
}

bool Mapper019::cpuMapWrite(uint16_t addr, uint32_t& mapped, uint8_t data) {
    mapped = 0;
    if (addr >= 0x4800u && addr < 0x5000u) {
        if (chipRam[chipAddr] != data) {
            chipRam[chipAddr] = data;
            chipRamDirty = true;
        }
        stepChipAddr();
        return true;
    }
    if (addr >= 0x5000u && addr < 0x5800u) {
        irqCounter = uint16_t((irqCounter & 0x7F00u) | data);
        irqActive = false;
        return true;
    }
    if (addr >= 0x5800u && addr < 0x6000u) {
        irqCounter = uint16_t((irqCounter & 0x00FFu) | (uint16_t(data & 0x7Fu) << 8u));
        irqEnabled = (data & 0x80u) != 0;
        irqActive = false;
        return true;
    }
    if (addr >= 0x6000u && addr < 0x8000u) {
        if (externalRamPresent && wramWriteEnabled(addr)) mapped = PRG_RAM_FLAG | (addr & 0x1FFFu);
        return true;
    }
    if (addr >= 0x8000u && addr < 0xE000u) {
        chrBank1k[(addr - 0x8000u) >> 11u] = data;
        return true;
    }
    switch (addr & 0xF800u) {
        case 0xE000u:
            prgBank8k[0] = data & 0x3Fu;
            soundDisabled = (data & 0x40u) != 0;
            return true;
        case 0xE800u:
            prgBank8k[1] = data & 0x3Fu;
            chrRamDisable = data & 0xC0u;
            return true;
        case 0xF000u:
            prgBank8k[2] = data & 0x3Fu;
            return true;
        case 0xF800u:
            wramProtect = data;
            chipAddr = data & 0x7Fu;
            chipIncrement = (data & 0x80u) != 0;
            return true;
        default: return false;
    }
}

bool Mapper019::ciramSelected(uint8_t slot) const {
    if (chrBank1k[slot] < 0xE0u) return false;
    if (slot >= 8) return true;
    return (chrRamDisable & (slot < 4 ? 0x40u : 0x80u)) == 0;
}

bool Mapper019::ppuReadDirect(uint16_t addr, uint8_t& data) {
    if (addr >= 0x2000u) return false;
    const uint8_t slot = uint8_t(addr >> 10u);
    if (!ciramSelected(slot)) return false;
    data = ciram[uint16_t((chrBank1k[slot] & 1u) * 1024u + (addr & 0x3FFu))];
    return true;
}

bool Mapper019::ppuWriteDirect(uint16_t addr, uint8_t data) {
    if (addr >= 0x2000u) return false;
    const uint8_t slot = uint8_t(addr >> 10u);
    if (!ciramSelected(slot)) return false;
    ciram[uint16_t((chrBank1k[slot] & 1u) * 1024u + (addr & 0x3FFu))] = data;
    return true;
}

bool Mapper019::ppuMapRead(uint16_t addr, uint32_t& mapped) {
    if (addr >= 0x2000u) return false;
    const uint8_t slot = uint8_t(addr >> 10u);
    mapped = uint32_t(chrBank1k[slot] % chrCount()) * 0x400u + (addr & 0x3FFu);
    return true;
}
bool Mapper019::ppuMapWrite(uint16_t addr, uint32_t& mapped, uint8_t /*data*/) {
    return chrBanks == 0 && ppuMapRead(addr, mapped);
}

bool Mapper019::nametableRead(uint16_t addr, uint8_t& data) {
    addr &= 0x3FFFu;
    if (addr < 0x2000u || addr >= 0x3F00u) return false;
    const uint8_t slot = uint8_t(((addr - 0x2000u) >> 10u) & 3u) + 8u;
    if (!ciramSelected(slot)) return false;
    data = ciram[uint16_t((chrBank1k[slot] & 1u) * 1024u + (addr & 0x3FFu))];
    return true;
}
bool Mapper019::nametableWrite(uint16_t addr, uint8_t data) {
    addr &= 0x3FFFu;
    if (addr < 0x2000u || addr >= 0x3F00u) return false;
    const uint8_t slot = uint8_t(((addr - 0x2000u) >> 10u) & 3u) + 8u;
    if (!ciramSelected(slot)) return false;
    ciram[uint16_t((chrBank1k[slot] & 1u) * 1024u + (addr & 0x3FFu))] = data;
    return true;
}
bool Mapper019::nametableMapRead(uint16_t addr, uint32_t& mapped) {
    addr &= 0x3FFFu;
    if (addr < 0x2000u || addr >= 0x3F00u) return false;
    const uint8_t slot = uint8_t(((addr - 0x2000u) >> 10u) & 3u) + 8u;
    if (ciramSelected(slot)) return false;
    mapped = uint32_t(chrBank1k[slot] % chrCount()) * 0x400u + (addr & 0x3FFu);
    return true;
}
bool Mapper019::nametableMapWrite(uint16_t addr, uint32_t& mapped) {
    return chrBanks == 0 && nametableMapRead(addr, mapped);
}

void Mapper019::stepAudio() {
    if (soundDisabled || !usesExpansionAudio()) { heldOutput = 0; return; }
    const uint8_t channels = uint8_t(((chipRam[0x7F] >> 4u) & 7u) + 1u);
    const uint8_t first = uint8_t(8u - channels);
    if (currentChannel < first || currentChannel > 7u) currentChannel = 7;
    const uint8_t base = uint8_t(0x40u + currentChannel*8u);
    const uint32_t freq = uint32_t(chipRam[base]) |
        (uint32_t(chipRam[base + 2u]) << 8u) |
        (uint32_t(chipRam[base + 4u] & 3u) << 16u);
    const uint32_t length = 256u - (chipRam[base + 4u] & 0xFCu);
    uint32_t phase = uint32_t(chipRam[base + 1u]) |
        (uint32_t(chipRam[base + 3u]) << 8u) |
        (uint32_t(chipRam[base + 5u]) << 16u);
    phase = (phase + freq) % (length << 16u);
    chipRam[base + 1u] = uint8_t(phase);
    chipRam[base + 3u] = uint8_t(phase >> 8u);
    chipRam[base + 5u] = uint8_t(phase >> 16u);
    // Sound phase registers are chip-internal battery RAM, too.
    chipRamDirty = true;
    const uint8_t index = uint8_t((uint32_t(phase >> 16u) + chipRam[base + 6u]) & 0xFFu);
    const uint8_t waveByte = chipRam[index >> 1u];
    const int wave = (index & 1u) ? (waveByte >> 4u) : (waveByte & 0x0F);
    heldOutput = float((wave - 8) * (chipRam[base + 7u] & 0x0Fu));
    if (currentChannel == first) currentChannel = 7;
    else --currentChannel;
}
void Mapper019::cpuClock() {
    if (irqEnabled && irqCounter < 0x7FFFu) {
        ++irqCounter;
        if (irqCounter == 0x7FFFu) irqActive = true;
    }
    if (++soundDivider >= 15u) {
        soundDivider = 0;
        stepAudio();
    }
}
float Mapper019::audioSample() {
    if (!usesExpansionAudio() || soundDisabled) return 0;
    // Time-multiplexed N163 output; calibrated volume varies by PCB resistor.
    // Default is deliberately conservative until hardware-matched mixes exist.
    const float gain = submapper == 3 ? 0.00045f :
        (submapper == 4 ? 0.00068f : submapper == 5 ? 0.00085f : 0.00065f);
    return heldOutput * gain;
}
void Mapper019::copyPersistentRam(uint8_t* dst, size_t size) const {
    if (dst && size >= chipRam.size()) std::memcpy(dst, chipRam.data(), chipRam.size());
}
void Mapper019::restorePersistentRam(const uint8_t* src, size_t size) {
    if (src && size >= chipRam.size()) std::memcpy(chipRam.data(), src, chipRam.size());
    chipRamDirty = false;
}
void Mapper019::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    for (uint8_t value : prgBank8k) write_u8(out,value);
    for (uint8_t value : chrBank1k) write_u8(out,value);
    write_u8(out,wramProtect); write_u8(out,chrRamDisable);
    write_u16(out,irqCounter); write_bool(out,irqEnabled); write_bool(out,irqActive);
    for (uint8_t value : chipRam) write_u8(out,value);
    for (uint8_t value : ciram) write_u8(out,value);
    write_u8(out,chipAddr); write_bool(out,chipIncrement);
    write_bool(out,chipRamDirty); write_bool(out,soundDisabled);
    write_u8(out,soundDivider); write_u8(out,currentChannel);
    // Serialize held audio output without depending on host floating-point ABI.
    write_u16(out,uint16_t(int16_t(heldOutput)));
}
bool Mapper019::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    for (auto& value : prgBank8k) if (!read_u8(p,end,value)) return false;
    for (auto& value : chrBank1k) if (!read_u8(p,end,value)) return false;
    if (!read_u8(p,end,wramProtect) || !read_u8(p,end,chrRamDisable) ||
        !read_u16(p,end,irqCounter) || !read_bool(p,end,irqEnabled) ||
        !read_bool(p,end,irqActive)) return false;
    for (auto& value : chipRam) if (!read_u8(p,end,value)) return false;
    for (auto& value : ciram) if (!read_u8(p,end,value)) return false;
    if (!read_u8(p,end,chipAddr) || !read_bool(p,end,chipIncrement) ||
        !read_bool(p,end,chipRamDirty) || !read_bool(p,end,soundDisabled) ||
        !read_u8(p,end,soundDivider) || !read_u8(p,end,currentChannel)) return false;
    uint16_t level=0;
    if (!read_u16(p,end,level)) return false;
    heldOutput = float(int16_t(level));
    return chipAddr < 128u && soundDivider < 15u && currentChannel < 8u;
}
