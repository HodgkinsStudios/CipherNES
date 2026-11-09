#include "Mapper023.h"
#include "../state_io.h"

namespace {
static bool isVrc2Submapper(uint16_t mapper, uint8_t submapper) {
    return (mapper == 23u || mapper == 25u) && submapper == 3u;
}
}

Mapper023::Mapper023(uint16_t prgBanks16k, uint16_t chrBanks8k, uint16_t mapper, uint8_t submapper)
    : Mapper(prgBanks16k, chrBanks8k), mapperNumber(mapper), submapperNumber(submapper),
      vrc2Only(isVrc2Submapper(mapper, submapper)) {
    prgBanks8k = uint32_t(prgBanks16k) * 2u;
    reset();
}

void Mapper023::reset() {
    prg0 = 0;
    prg1 = 1;
    swapMode = false;
    for (int i = 0; i < 8; i++) chr[i] = uint16_t(i);
    irqEnable = false;
    irqEnableAfterAck = false;
    irqModeCycle = false;
    irqLatch = 0;
    irqCounter = 0;
    prescaler = 341;
    irqActive = false;
    mirrorMode = 0xFF;
}

uint16_t Mapper023::translateAddress(uint16_t addr) const {
    uint8_t a0 = 0;
    uint8_t a1 = 0;

    if (submapperNumber == 0u) {
        // Legacy iNES headers cannot disambiguate the wiring. The aliases for
        // each pair do not overlap in commercial software, so OR both forms.
        if (mapperNumber == 21u) {
            a0 = uint8_t(((addr >> 1) | (addr >> 6)) & 1u);
            a1 = uint8_t(((addr >> 2) | (addr >> 7)) & 1u);
        } else if (mapperNumber == 25u) {
            a0 = uint8_t(((addr >> 1) | (addr >> 3)) & 1u);
            a1 = uint8_t((addr | (addr >> 2)) & 1u);
        } else {
            a0 = uint8_t((addr | (addr >> 2)) & 1u);
            a1 = uint8_t(((addr >> 1) | (addr >> 3)) & 1u);
        }
    } else if (mapperNumber == 21u) {
        if (submapperNumber == 2u) { a0 = uint8_t((addr >> 6) & 1u); a1 = uint8_t((addr >> 7) & 1u); }
        else { a0 = uint8_t((addr >> 1) & 1u); a1 = uint8_t((addr >> 2) & 1u); }
    } else if (mapperNumber == 25u) {
        if (submapperNumber == 2u) { a0 = uint8_t((addr >> 3) & 1u); a1 = uint8_t((addr >> 2) & 1u); }
        else { a0 = uint8_t((addr >> 1) & 1u); a1 = uint8_t(addr & 1u); }
    } else {
        if (submapperNumber == 2u) { a0 = uint8_t((addr >> 2) & 1u); a1 = uint8_t((addr >> 3) & 1u); }
        else { a0 = uint8_t(addr & 1u); a1 = uint8_t((addr >> 1) & 1u); }
    }
    return uint16_t((addr & 0xFF00u) | (uint16_t(a1) << 1) | a0);
}

void Mapper023::setMirroring(uint8_t v) {
    if (vrc2Only) {
        mirrorMode = (v & 1u) ? 0u : 1u;
        return;
    }
    switch (v & 3u) {
        case 0: mirrorMode = 1; break;
        case 1: mirrorMode = 0; break;
        case 2: mirrorMode = 3; break;
        default: mirrorMode = 4; break;
    }
}

void Mapper023::writeChrLow(int idx, uint8_t v) { chr[idx] = uint16_t((chr[idx] & 0x1F0u) | (v & 0x0Fu)); }
void Mapper023::writeChrHigh(int idx, uint8_t v) {
    const uint8_t mask = vrc2Only ? 0x0Fu : 0x1Fu;
    chr[idx] = uint16_t((chr[idx] & 0x00Fu) | (uint16_t(v & mask) << 4));
}

bool Mapper023::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr >= 0x6000u && addr <= 0x7FFFu && !vrc2Only) {
        mappedAddr = PRG_RAM_FLAG | uint32_t(addr & 0x1FFFu);
        return true;
    }
    if (addr < 0x8000u) return false;

    const uint32_t offset = addr & 0x1FFFu;
    const uint32_t slot = (addr - 0x8000u) >> 13;
    const uint32_t last = prgBanks8k ? prgBanks8k - 1u : 0u;
    const uint32_t secondLast = prgBanks8k > 1u ? prgBanks8k - 2u : 0u;
    uint32_t page = 0;
    switch (slot) {
        case 0: page = swapMode ? secondLast : prg0; break;
        case 1: page = prg1; break;
        case 2: page = swapMode ? prg0 : secondLast; break;
        default: page = last; break;
    }
    if (prgBanks8k) page %= prgBanks8k;
    mappedAddr = page * 0x2000u + offset;
    return true;
}

bool Mapper023::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    if (addr >= 0x6000u && addr <= 0x7FFFu && !vrc2Only) {
        mappedAddr = PRG_RAM_FLAG | uint32_t(addr & 0x1FFFu);
        return true;
    }
    if (addr < 0x8000u) return false;

    const uint16_t r = uint16_t(translateAddress(addr) & 0xF003u);
    switch (r) {
        case 0x8000: case 0x8001: case 0x8002: case 0x8003: prg0 = uint8_t(data & 0x1Fu); break;
        case 0xA000: case 0xA001: case 0xA002: case 0xA003: prg1 = uint8_t(data & 0x1Fu); break;
        case 0x9000: case 0x9001: setMirroring(data); break;
        case 0x9002: case 0x9003: if (!vrc2Only) swapMode = (data & 0x02u) != 0; break;

        case 0xB000: writeChrLow(0, data); break; case 0xB001: writeChrHigh(0, data); break;
        case 0xB002: writeChrLow(1, data); break; case 0xB003: writeChrHigh(1, data); break;
        case 0xC000: writeChrLow(2, data); break; case 0xC001: writeChrHigh(2, data); break;
        case 0xC002: writeChrLow(3, data); break; case 0xC003: writeChrHigh(3, data); break;
        case 0xD000: writeChrLow(4, data); break; case 0xD001: writeChrHigh(4, data); break;
        case 0xD002: writeChrLow(5, data); break; case 0xD003: writeChrHigh(5, data); break;
        case 0xE000: writeChrLow(6, data); break; case 0xE001: writeChrHigh(6, data); break;
        case 0xE002: writeChrLow(7, data); break; case 0xE003: writeChrHigh(7, data); break;

        case 0xF000:
            if (!vrc2Only) irqLatch = uint8_t((irqLatch & 0xF0u) | (data & 0x0Fu));
            break;
        case 0xF001:
            if (!vrc2Only) irqLatch = uint8_t((irqLatch & 0x0Fu) | ((data & 0x0Fu) << 4));
            break;
        case 0xF002:
            if (!vrc2Only) {
                irqModeCycle = (data & 0x04u) != 0;
                irqEnable = (data & 0x02u) != 0;
                irqEnableAfterAck = (data & 0x01u) != 0;
                irqActive = false;
                prescaler = 341;
                if (irqEnable) irqCounter = irqLatch;
            }
            break;
        case 0xF003:
            if (!vrc2Only) { irqActive = false; irqEnable = irqEnableAfterAck; }
            break;
        default: break;
    }
    mappedAddr = 0;
    return true;
}

bool Mapper023::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr > 0x1FFFu) return false;
    const uint16_t count = chrBanks ? uint16_t(chrBanks * 8u) : 8u;
    const uint16_t bank = uint16_t(chr[(addr >> 10) & 7u] % count);
    mappedAddr = uint32_t(bank) * 0x0400u + uint32_t(addr & 0x03FFu);
    return true;
}

bool Mapper023::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (chrBanks != 0 || addr > 0x1FFFu) return false;
    return ppuMapRead(addr, mappedAddr);
}

void Mapper023::cpuClock() {
    if (vrc2Only || !irqEnable) return;
    bool tick = irqModeCycle;
    if (!irqModeCycle) {
        prescaler -= 3;
        if (prescaler <= 0) { prescaler += 341; tick = true; }
    }
    if (!tick) return;
    if (irqCounter == 0xFFu) { irqCounter = irqLatch; irqActive = true; }
    else ++irqCounter;
}

void Mapper023::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    write_u8(out, mirrorMode); write_u8(out, prg0); write_u8(out, prg1); write_bool(out, swapMode);
    for (uint16_t v : chr) write_u16(out, v);
    write_bool(out, irqEnable); write_bool(out, irqEnableAfterAck); write_bool(out, irqModeCycle);
    write_u8(out, irqLatch); write_u8(out, irqCounter); write_pod(out, prescaler); write_bool(out, irqActive);
}

bool Mapper023::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    if (!read_u8(p, end, mirrorMode) || !read_u8(p, end, prg0) || !read_u8(p, end, prg1) || !read_bool(p, end, swapMode)) return false;
    for (uint16_t& v : chr) if (!read_u16(p, end, v)) return false;
    return read_bool(p, end, irqEnable) && read_bool(p, end, irqEnableAfterAck) && read_bool(p, end, irqModeCycle) &&
           read_u8(p, end, irqLatch) && read_u8(p, end, irqCounter) && read_pod(p, end, prescaler) && read_bool(p, end, irqActive);
}
