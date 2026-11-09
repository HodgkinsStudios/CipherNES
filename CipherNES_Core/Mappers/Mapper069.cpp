#include "Mapper069.h"
#include "../state_io.h"
#include <cmath>

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
    audioAddress = 0;
    audioRegs.fill(0);
    audioClockDivider = 0;
    for (unsigned ch=0;ch<3;++ch) { toneCounter[ch]=0; toneHigh[ch]=true; }
    noiseCounter = 0;
    noiseShift = 0x1FFFFu;
    noiseHigh = true;
    resetEnvelope(0);
}

bool Mapper069::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        if (wramSelected) {
            if (!wramEnabled) return false; // Disabled RAM is electrically open bus.
            mappedAddr = Mapper::PRG_RAM_FLAG | uint32_t(addr & 0x1FFF);
            return true;
        }
        const uint8_t count = prg8kCount() ? prg8kCount() : 1;
        mappedAddr = uint32_t(prg[0] % count) * 0x2000u + uint32_t(addr & 0x1FFF);
        return true;
    }
    if (addr >= 0x8000) {
        const uint8_t count = prg8kCount() ? prg8kCount() : 1;
        const uint8_t slot = uint8_t((addr - 0x8000) / 0x2000);
        // $8000/$A000/$C000 use command registers 9/A/B; $E000
        // is hardwired to the final 8 KiB bank. Never index prg[4].
        const uint8_t bank = (slot == 3) ? uint8_t(count - 1) : prg[slot + 1u];
        mappedAddr = uint32_t(bank % count) * 0x2000u + uint32_t(addr & 0x1FFF);
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
                // FME-7: bit 7 clocks the timer, bit 0 enables the IRQ.
                // These controls are independent; old code reversed them.
                irqCounterEnabled = (data & 0x80) != 0;
                irqEnabled = (data & 0x01) != 0;
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
        // $C000-$DFFF address latch; nonzero high nibble disables data writes.
        // $E000-$FFFF data port; 5A/FME-7 boards ignore these pins.
        if (addr < 0xE000) audioAddress = data;
        else if ((audioAddress & 0xF0u) == 0) {
            const uint8_t reg = audioAddress & 15u;
            switch (reg) {
                case 1: case 3: case 5: case 13: data &= 0x0Fu; break;
                case 6: data &= 0x1Fu; break;
                case 8: case 9: case 10: data &= 0x1Fu; break;
                default: break;
            }
            audioRegs[reg] = data;
            if (reg == 13u) resetEnvelope(data);
        }
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

void Mapper069::resetEnvelope(uint8_t shape) {
    audioRegs[13] = shape & 15u;
    envelopeCounter = 0;
    envelopeStep = 31;
    envelopeAttack = (shape & 4u) != 0;
    envelopeHolding = false;
}

uint8_t Mapper069::envelopeLevel() const {
    return envelopeHolding && !(audioRegs[13] & 8u) ? 0u :
        uint8_t(envelopeStep ^ (envelopeAttack ? 31 : 0));
}

void Mapper069::clockSound() {
    // With YM2149F SEL held low, every 16 CPU clocks advances tone,
    // noise and envelope period counters. Noise takes 2 such ticks.
    if (++audioClockDivider < 16u) return;
    audioClockDivider = 0;
    for (unsigned ch = 0; ch < 3; ++ch) {
        const uint16_t period = uint16_t(audioRegs[ch*2]) |
            (uint16_t(audioRegs[ch*2 + 1] & 15u) << 8u);
        if (++toneCounter[ch] >= (period ? period : 1u)) {
            toneCounter[ch] = 0;
            toneHigh[ch] = !toneHigh[ch];
        }
    }
    const uint16_t np = audioRegs[6] & 31u;
    if (++noiseCounter >= 2u * (np ? np : 1u)) {
        noiseCounter = 0;
        // 17-bit LFSR: taps 16 and 13, linear feedback on the low bit.
        const uint32_t feedback = ((noiseShift >> 16u) ^ (noiseShift >> 13u)) & 1u;
        noiseShift = ((noiseShift << 1u) | feedback) & 0x1FFFFu;
        noiseHigh = ((noiseShift >> 16u) & 1u) != 0;
    }
    const uint16_t ep = uint16_t(audioRegs[11]) | (uint16_t(audioRegs[12]) << 8u);
    if (++envelopeCounter >= (ep ? ep : 1u)) {
        envelopeCounter = 0;
        if (!envelopeHolding) {
            --envelopeStep;
            if (envelopeStep < 0) {
                const uint8_t shape = audioRegs[13];
                if (!(shape & 8u)) {
                    envelopeStep = 0;
                    envelopeHolding = true;
                } else if (shape & 1u) {
                    if (shape & 2u) envelopeAttack = !envelopeAttack;
                    envelopeStep = 0;
                    envelopeHolding = true;
                } else {
                    if (shape & 2u) envelopeAttack = !envelopeAttack;
                    envelopeStep = 31;
                }
            }
        }
    }
}

float Mapper069::audioSample() {
    // 32-level YM2149 DAC: 1.5 dB per level; fixed-volume levels
    // map to the odd envelope levels (1 -> 3, 15 -> 31).
    static const std::array<float,32> dac = []() {
        std::array<float,32> v{};
        for (unsigned i=2;i<32;++i)
            v[i] = float(std::pow(10.0, (double(i)-31.0)*1.5/20.0));
        return v;
    }();
    const uint8_t mixer = audioRegs[7];
    const uint8_t env = envelopeLevel();
    float sum = 0.0f;
    for (unsigned ch=0;ch<3;++ch) {
        const bool toneGate = (mixer & (1u << ch)) != 0 || toneHigh[ch];
        const bool noiseGate = (mixer & (1u << (ch + 3u))) != 0 || noiseHigh;
        if (!toneGate || !noiseGate) continue;
        const uint8_t volume = audioRegs[8u + ch];
        const uint8_t level = (volume & 16u) ? env :
            ((volume & 15u) ? uint8_t((volume & 15u)*2u + 1u) : 0u);
        sum += dac[level];
    }
    // Conservative digital mixer gain; PCB analog amplifier/filter not modeled.
    return sum * 0.085f;
}

void Mapper069::cpuClock() {
    clockSound(); // Runs continuously, independent of $D IRQ counter enable.
    if (!irqCounterEnabled) return;
    const uint16_t previous = irqCounter;
    --irqCounter;
    if (previous == 0 && irqEnabled) irqActive = true;
}

void Mapper069::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    write_u8(out, command);
    for (uint8_t i = 0; i < 8; ++i) write_u8(out, chr[i]);
    for (uint8_t i = 0; i < 4; ++i) write_u8(out, prg[i]);
    write_bool(out, wramEnabled); write_bool(out, wramSelected); write_u8(out, mirrorMode);
    write_bool(out, irqEnabled); write_bool(out, irqCounterEnabled); write_bool(out, irqActive);
    write_u16(out, irqCounter);
    write_u8(out, audioAddress);
    for (uint8_t v : audioRegs) write_u8(out, v);
    write_u8(out, audioClockDivider);
    for (unsigned ch=0;ch<3;++ch) {
        write_u16(out,toneCounter[ch]);
        write_bool(out,toneHigh[ch]);
    }
    write_u16(out, noiseCounter);
    write_u32(out, noiseShift);
    write_bool(out, noiseHigh);
    write_u16(out, envelopeCounter);
    write_u8(out, uint8_t(envelopeStep));
    write_bool(out, envelopeAttack);
    write_bool(out, envelopeHolding);
}

bool Mapper069::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    if (!read_u8(p, end, command)) return false;
    for (uint8_t i = 0; i < 8; ++i) if (!read_u8(p, end, chr[i])) return false;
    for (uint8_t i = 0; i < 4; ++i) if (!read_u8(p, end, prg[i])) return false;
    if (!read_bool(p, end, wramEnabled) || !read_bool(p, end, wramSelected) ||
        !read_u8(p, end, mirrorMode) || !read_bool(p, end, irqEnabled) ||
        !read_bool(p, end, irqCounterEnabled) || !read_bool(p, end, irqActive) ||
        !read_u16(p, end, irqCounter) || !read_u8(p, end, audioAddress)) return false;
    for (uint8_t& v : audioRegs) if (!read_u8(p, end, v)) return false;
    if (!read_u8(p, end, audioClockDivider)) return false;
    for (unsigned ch=0;ch<3;++ch) {
        if (!read_u16(p,end,toneCounter[ch]) || !read_bool(p,end,toneHigh[ch])) return false;
    }
    uint8_t step = 0;
    if (!read_u16(p,end,noiseCounter) || !read_u32(p,end,noiseShift) ||
        !read_bool(p,end,noiseHigh) || !read_u16(p,end,envelopeCounter) ||
        !read_u8(p,end,step) || !read_bool(p,end,envelopeAttack) ||
        !read_bool(p,end,envelopeHolding)) return false;
    envelopeStep = int8_t(step);
    return audioClockDivider < 16u && envelopeStep >= 0 && envelopeStep <= 31 &&
        noiseShift <= 0x1FFFFu;
}
