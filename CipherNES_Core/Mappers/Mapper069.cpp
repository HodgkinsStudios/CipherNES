#include "Mapper069.h"
#include "../state_io.h"

void Mapper069::reset() {
    command = 0;
    for (uint8_t i = 0; i < 8; ++i) chr[i] = i;
    const uint8_t count = prg8kCount();
    prg[0] = 0;
    prg[1] = count > 3 ? uint8_t(count - 3) : 0;
    prg[2] = count > 2 ? uint8_t(count - 2) : 0;
    prg[3] = count > 1 ? uint8_t(count - 1) : 0;
    wramEnabled = false;
    wramSelected = false;
    mirrorMode = 0xFF;
    irqEnabled = irqCounterEnabled = irqActive = false;
    irqCounter = 0;
}

bool Mapper069::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        if (wramSelected && wramEnabled) {
            mappedAddr = Mapper::PRG_RAM_FLAG | uint32_t(addr & 0x1FFF);
            return true;
        }
        const uint8_t count = prg8kCount() ? prg8kCount() : 1;
        mappedAddr = uint32_t(prg[0] % count) * 0x2000u + uint32_t(addr & 0x1FFF);
        return true;
    }
    if (addr >= 0x8000) {
        const uint8_t count = prg8kCount() ? prg8kCount() : 1;
        const uint8_t slot = uint8_t((addr - 0x8000) / 0x2000) + 1;
        mappedAddr = uint32_t(prg[slot] % count) * 0x2000u + uint32_t(addr & 0x1FFF);
        return true;
    }
    return false;
}

bool Mapper069::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        if (wramSelected && wramEnabled) {
            mappedAddr = Mapper::PRG_RAM_FLAG | uint32_t(addr & 0x1FFF);
            return true;
        }
        mappedAddr = 0;
        return true;
    }

    if (addr >= 0x8000 && addr <= 0x9FFF) {
        command = data & 0x0F;
        mappedAddr = 0;
        return true;
    }

    if (addr >= 0xA000 && addr <= 0xBFFF) {
        const uint8_t count = prg8kCount() ? prg8kCount() : 1;
        const uint16_t ccount = chr1kCount() ? chr1kCount() : 1;
        switch (command) {
            case 0x0: case 0x1: case 0x2: case 0x3:
            case 0x4: case 0x5: case 0x6: case 0x7:
                chr[command] = uint8_t(data % ccount);
                break;
            case 0x8:
                wramSelected = (data & 0x40) != 0;
                wramEnabled = (data & 0x80) != 0;
                prg[0] = data & 0x3F;
                if (count) prg[0] %= count;
                break;
            case 0x9: case 0xA: case 0xB:
                prg[command - 0x8] = data & 0x3F;
                if (count) prg[command - 0x8] %= count;
                break;
            case 0xC:
                switch (data & 0x03) {
                    case 0: mirrorMode = 1; break;
                    case 1: mirrorMode = 0; break;
                    case 2: mirrorMode = 3; break;
                    case 3: mirrorMode = 4; break;
                }
                break;
            case 0xD:
                irqCounterEnabled = (data & 0x01) != 0;
                irqEnabled = (data & 0x80) != 0;
                irqActive = false;
                break;
            case 0xE:
                irqCounter = uint16_t((irqCounter & 0xFF00) | data);
                break;
            case 0xF:
                irqCounter = uint16_t((irqCounter & 0x00FF) | (uint16_t(data) << 8));
                break;
        }
        mappedAddr = 0;
        return true;
    }

    if (addr >= 0xC000) {
        mappedAddr = 0;
        return true;
    }

    return false;
}

bool Mapper069::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr > 0x1FFF) return false;
    if (chrBanks == 0) { mappedAddr = addr; return true; }
    const uint8_t slot = uint8_t(addr >> 10);
    const uint16_t count = chr1kCount() ? chr1kCount() : 1;
    mappedAddr = uint32_t(chr[slot] % count) * 0x400u + uint32_t(addr & 0x03FF);
    return true;
}

bool Mapper069::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (addr > 0x1FFF) return false;
    mappedAddr = addr;
    return chrBanks == 0;
}

void Mapper069::cpuClock() {
    if (!irqCounterEnabled) return;
    if (irqCounter == 0) {
        if (irqEnabled) irqActive = true;
    } else {
        irqCounter--;
    }
}

void Mapper069::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    write_u8(out, command);
    for (uint8_t i = 0; i < 8; ++i) write_u8(out, chr[i]);
    for (uint8_t i = 0; i < 4; ++i) write_u8(out, prg[i]);
    write_bool(out, wramEnabled); write_bool(out, wramSelected); write_u8(out, mirrorMode);
    write_bool(out, irqEnabled); write_bool(out, irqCounterEnabled); write_bool(out, irqActive);
    write_u16(out, irqCounter);
}

bool Mapper069::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    if (!read_u8(p, end, command)) return false;
    for (uint8_t i = 0; i < 8; ++i) if (!read_u8(p, end, chr[i])) return false;
    for (uint8_t i = 0; i < 4; ++i) if (!read_u8(p, end, prg[i])) return false;
    return read_bool(p, end, wramEnabled) && read_bool(p, end, wramSelected) &&
           read_u8(p, end, mirrorMode) && read_bool(p, end, irqEnabled) &&
           read_bool(p, end, irqCounterEnabled) && read_bool(p, end, irqActive) &&
           read_u16(p, end, irqCounter);
}
