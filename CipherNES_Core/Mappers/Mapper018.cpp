#include "Mapper018.h"
#include "../state_io.h"

const uint16_t Mapper018::irqMasks[4] = { 0xFFFFu, 0x0FFFu, 0x00FFu, 0x000Fu };

void Mapper018::reset() {
    for (uint8_t& v : prg) v = 0;
    for (uint8_t& v : chr) v = 0;
    for (uint8_t& v : irqReloadNibble) v = 0;
    irqCounter = 0;
    irqSize = 0;
    irqEnabled = false;
    irqActive = false;
    mirrorMode = 0xFF;
}

void Mapper018::writeNibble(uint8_t& target, bool high, uint8_t data) {
    if (high) target = uint8_t((target & 0x0Fu) | ((data & 0x0Fu) << 4));
    else target = uint8_t((target & 0xF0u) | (data & 0x0Fu));
}

void Mapper018::reloadIrq() {
    irqCounter = uint16_t(irqReloadNibble[0] |
                          (uint16_t(irqReloadNibble[1]) << 4) |
                          (uint16_t(irqReloadNibble[2]) << 8) |
                          (uint16_t(irqReloadNibble[3]) << 12));
}

bool Mapper018::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr >= 0x6000u && addr <= 0x7FFFu) {
        mappedAddr = PRG_RAM_FLAG | uint32_t(addr & 0x1FFFu);
        return true;
    }
    if (addr < 0x8000u) return false;
    const uint16_t count = prgBanks ? uint16_t(prgBanks * 2u) : 1u;
    const uint8_t slot = uint8_t((addr - 0x8000u) >> 13);
    const uint16_t page = slot < 3u ? uint16_t(prg[slot] % count) : uint16_t(count - 1u);
    mappedAddr = uint32_t(page) * 0x2000u + uint32_t(addr & 0x1FFFu);
    return true;
}

bool Mapper018::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    if (addr >= 0x6000u && addr <= 0x7FFFu) {
        mappedAddr = PRG_RAM_FLAG | uint32_t(addr & 0x1FFFu);
        return true;
    }
    if (addr < 0x8000u) return false;

    const uint16_t reg = uint16_t(addr & 0xF003u);
    const bool high = (addr & 1u) != 0;
    if (reg == 0x8000u || reg == 0x8001u) writeNibble(prg[0], high, data);
    else if (reg == 0x8002u || reg == 0x8003u) writeNibble(prg[1], high, data);
    else if (reg == 0x9000u || reg == 0x9001u) writeNibble(prg[2], high, data);
    else if (reg >= 0xA000u && reg <= 0xD003u) {
        const uint8_t group = uint8_t((reg >> 12) - 0x0Au);
        const uint8_t pair = uint8_t((reg >> 1) & 1u);
        const uint8_t index = uint8_t(group * 2u + pair);
        writeNibble(chr[index], high, data);
    } else if (reg >= 0xE000u && reg <= 0xE003u) {
        irqReloadNibble[reg & 3u] = uint8_t(data & 0x0Fu);
    } else if (reg == 0xF000u) {
        irqActive = false;
        reloadIrq();
    } else if (reg == 0xF001u) {
        irqActive = false;
        irqEnabled = (data & 1u) != 0;
        irqSize = (data & 0x08u) ? 3u : ((data & 0x04u) ? 2u : ((data & 0x02u) ? 1u : 0u));
    } else if (reg == 0xF002u) {
        switch (data & 3u) {
            case 0: mirrorMode = 0; break;
            case 1: mirrorMode = 1; break;
            case 2: mirrorMode = 3; break;
            default: mirrorMode = 4; break;
        }
    }
    mappedAddr = 0;
    return true;
}

bool Mapper018::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr > 0x1FFFu) return false;
    const uint16_t count = chrBanks ? uint16_t(chrBanks * 8u) : 8u;
    const uint8_t slot = uint8_t(addr >> 10);
    mappedAddr = uint32_t(uint16_t(chr[slot]) % count) * 0x0400u + uint32_t(addr & 0x03FFu);
    return true;
}

bool Mapper018::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (chrBanks != 0 || addr > 0x1FFFu) return false;
    return ppuMapRead(addr, mappedAddr);
}

void Mapper018::cpuClock() {
    if (!irqEnabled) return;
    const uint16_t mask = irqMasks[irqSize & 3u];
    uint16_t counter = uint16_t(irqCounter & mask);
    counter = uint16_t((counter - 1u) & mask);
    irqCounter = uint16_t((irqCounter & uint16_t(~mask)) | counter);
    if (counter == 0u) irqActive = true;
}

void Mapper018::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    for (uint8_t v : prg) write_u8(out, v);
    for (uint8_t v : chr) write_u8(out, v);
    for (uint8_t v : irqReloadNibble) write_u8(out, v);
    write_u16(out, irqCounter);
    write_u8(out, irqSize);
    write_bool(out, irqEnabled);
    write_bool(out, irqActive);
    write_u8(out, mirrorMode);
}

bool Mapper018::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    for (uint8_t& v : prg) if (!read_u8(p, end, v)) return false;
    for (uint8_t& v : chr) if (!read_u8(p, end, v)) return false;
    for (uint8_t& v : irqReloadNibble) if (!read_u8(p, end, v)) return false;
    return read_u16(p, end, irqCounter) && read_u8(p, end, irqSize) &&
           read_bool(p, end, irqEnabled) && read_bool(p, end, irqActive) &&
           read_u8(p, end, mirrorMode);
}
