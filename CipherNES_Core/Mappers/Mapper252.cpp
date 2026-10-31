#include "Mapper252.h"
#include "../state_io.h"
#include <algorithm>

Mapper252::Mapper252(uint16_t p, uint16_t c) : Mapper(p, c) {
    prgPages8k = std::max<uint32_t>(1, uint32_t(p) * 2u);
    chrPages1k = std::max<uint32_t>(1, c ? uint32_t(c) * 8u : 8u);
    reset();
}

void Mapper252::reset() {
    prg[0] = 0;
    prg[1] = 1;
    for (uint8_t i = 0; i < 8; ++i) chr[i] = i;
    irqLatch = irqCounter = 0;
    irqEnable = irqEnableAfterAck = irqCycleMode = irqActive = false;
    irqPrescaler = 341;
}

bool Mapper252::cpuMapRead(uint16_t a, uint32_t& m) {
    if (a < 0x8000u) return false;
    const uint8_t slot = uint8_t((a - 0x8000u) >> 13);
    uint32_t page;
    if (slot == 0) page = prg[0];
    else if (slot == 1) page = prg[1];
    else if (slot == 2) page = prgPages8k > 1 ? prgPages8k - 2u : 0u;
    else page = prgPages8k - 1u;
    m = (page % prgPages8k) * 0x2000u + (a & 0x1FFFu);
    return true;
}

bool Mapper252::cpuMapWrite(uint16_t a, uint32_t& m, uint8_t d) {
    if (a < 0x8000u) return false;

    if (a <= 0x8FFFu) {
        prg[0] = d;
    } else if (a >= 0xA000u && a <= 0xAFFFu) {
        prg[1] = d;
    } else if (a >= 0xB000u && a <= 0xEFFFu) {
        const uint8_t shift = uint8_t(a & 0x0004u);
        const uint8_t bank = uint8_t(((((a - 0xB000u) >> 1) & 0x1800u)
            | ((uint32_t(a) << 7) & 0x0400u)) / 0x0400u);
        const uint8_t keepMask = uint8_t(0xF0u >> shift);
        chr[bank & 7u] = uint8_t((chr[bank & 7u] & keepMask) | ((d & 0x0Fu) << shift));
    } else {
        switch (a & 0xF00Cu) {
            case 0xF000u: irqLatch = uint8_t((irqLatch & 0xF0u) | (d & 0x0Fu)); break;
            case 0xF004u: irqLatch = uint8_t((irqLatch & 0x0Fu) | ((d & 0x0Fu) << 4)); break;
            case 0xF008u:
                irqCycleMode = (d & 0x04u) != 0;
                irqEnable = (d & 0x02u) != 0;
                irqEnableAfterAck = (d & 0x01u) != 0;
                irqActive = false;
                irqPrescaler = 341;
                if (irqEnable) irqCounter = irqLatch;
                break;
            case 0xF00Cu:
                irqActive = false;
                irqEnable = irqEnableAfterAck;
                break;
            default: break;
        }
    }
    m = 0;
    return true;
}

bool Mapper252::ppuMapRead(uint16_t a, uint32_t& m) {
    if (a > 0x1FFFu) return false;
    const uint32_t page = uint32_t(chr[(a >> 10) & 7u]) % chrPages1k;
    m = page * 0x400u + (a & 0x3FFu);
    return true;
}

bool Mapper252::ppuMapWrite(uint16_t a, uint32_t& m, uint8_t) {
    // This board deliberately leaves the selected CHR memory writeable.
    return ppuMapRead(a, m);
}

void Mapper252::cpuClock() {
    if (!irqEnable) return;
    bool tick = irqCycleMode;
    if (!irqCycleMode) {
        irqPrescaler -= 3;
        if (irqPrescaler <= 0) {
            irqPrescaler += 341;
            tick = true;
        }
    }
    if (!tick) return;
    if (irqCounter == 0xFFu) {
        irqCounter = irqLatch;
        irqActive = true;
    } else {
        ++irqCounter;
    }
}

void Mapper252::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    write_bytes(out, prg, sizeof(prg));
    write_bytes(out, chr, sizeof(chr));
    write_u8(out, irqLatch);
    write_u8(out, irqCounter);
    write_bool(out, irqEnable);
    write_bool(out, irqEnableAfterAck);
    write_bool(out, irqCycleMode);
    write_pod(out, irqPrescaler);
    write_bool(out, irqActive);
}

bool Mapper252::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    return read_bytes(p, end, prg, sizeof(prg))
        && read_bytes(p, end, chr, sizeof(chr))
        && read_u8(p, end, irqLatch)
        && read_u8(p, end, irqCounter)
        && read_bool(p, end, irqEnable)
        && read_bool(p, end, irqEnableAfterAck)
        && read_bool(p, end, irqCycleMode)
        && read_pod(p, end, irqPrescaler)
        && read_bool(p, end, irqActive);
}
