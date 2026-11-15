#include "Mapper004.h"

#include "../state_io.h"

#include <algorithm>
#include <iterator>

#ifndef CIPHERNES_MMC3_IRQ_DELAY_DOTS
#define CIPHERNES_MMC3_IRQ_DELAY_DOTS 2
#endif

namespace {
constexpr uint16_t kA12SampleBit = 0x1000u;
constexpr uint16_t kIrqDelayMask = 0x0007u;
static_assert(CIPHERNES_MMC3_IRQ_DELAY_DOTS >= 0 && CIPHERNES_MMC3_IRQ_DELAY_DOTS <= 7,
              "MMC3 IRQ delay must fit in the saved low bits of lastA12");
}

// -----------------------------------------------------------------------------
// Mapper004 (MMC3)
// -----------------------------------------------------------------------------
Mapper004::Mapper004(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {
    reset();
}

void Mapper004::reset() {
    bankSelect = 0;
    std::fill(std::begin(bankData), std::end(bankData), 0);
    prgMode = 0;
    chrMode = 0;

    mirrorMode = 0xFF;
    // Standard mapper-4 boards expose PRG-RAM at power-on; $A001 can then
    // disable or write-protect it. Blargg's status channel relies on this.
    prgRamEnabled = true;
    prgRamWriteProtected = false;

    irqLatch = 0;
    irqCounter = 0;
    irqReload = false;
    irqEnabled = false;
    irqActive = false;

    lastA12 = 0;
    a12LowCounter = 0;

    updateBanks();
}

uint8_t Mapper004::prgBankCount8k() const {
    return uint8_t(prgBanks * 2); // 16KB units -> 8KB units
}

uint16_t Mapper004::chrBankCount1k() const {
    // CHR RAM carts report chrBanks == 0 but still expose 8KB of CHR.
    if (chrBanks == 0) return 8;
    return uint16_t(chrBanks * 8); // 8KB units -> 1KB units
}

void Mapper004::updateBanks() {
    // --- PRG banks (8KB each)
    const uint8_t prgCount = prgBankCount8k();
    const uint8_t lastBank = prgCount ? (prgCount - 1) : 0;
    const uint8_t secondLast = prgCount > 1 ? (prgCount - 2) : 0;

    if (prgMode == 0) {
        prgBank[0] = bankData[6] % (prgCount ? prgCount : 1);
        prgBank[1] = bankData[7] % (prgCount ? prgCount : 1);
        prgBank[2] = secondLast;
        prgBank[3] = lastBank;
    } else {
        prgBank[0] = secondLast;
        prgBank[1] = bankData[7] % (prgCount ? prgCount : 1);
        prgBank[2] = bankData[6] % (prgCount ? prgCount : 1);
        prgBank[3] = lastBank;
    }

    // --- CHR banks (1KB each)
    // Registers 0 and 1 select 2KB banks (mask even), 2..5 select 1KB banks.
    uint16_t r0 = uint16_t(bankData[0] & 0xFE);
    uint16_t r1 = uint16_t(bankData[1] & 0xFE);
    uint16_t r2 = uint16_t(bankData[2]);
    uint16_t r3 = uint16_t(bankData[3]);
    uint16_t r4 = uint16_t(bankData[4]);
    uint16_t r5 = uint16_t(bankData[5]);

    const uint16_t chrCount = std::max<uint16_t>(1, chrBankCount1k());

    if (chrMode == 0) {
        chrBank[0] = (r0 + 0) % chrCount;
        chrBank[1] = (r0 + 1) % chrCount;
        chrBank[2] = (r1 + 0) % chrCount;
        chrBank[3] = (r1 + 1) % chrCount;
        chrBank[4] = (r2) % chrCount;
        chrBank[5] = (r3) % chrCount;
        chrBank[6] = (r4) % chrCount;
        chrBank[7] = (r5) % chrCount;
    } else {
        chrBank[0] = (r2) % chrCount;
        chrBank[1] = (r3) % chrCount;
        chrBank[2] = (r4) % chrCount;
        chrBank[3] = (r5) % chrCount;
        chrBank[4] = (r0 + 0) % chrCount;
        chrBank[5] = (r0 + 1) % chrCount;
        chrBank[6] = (r1 + 0) % chrCount;
        chrBank[7] = (r1 + 1) % chrCount;
    }
}

bool Mapper004::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        if (prgRamEnabled) {
            mappedAddr = Mapper::PRG_RAM_FLAG | uint32_t(addr & 0x1FFF);
            return true;
        }
        return false;
    }

    if (addr >= 0x8000 && addr <= 0x9FFF) {
        mappedAddr = (uint32_t(prgBank[0]) * 0x2000) + (addr & 0x1FFF);
        return true;
    }
    if (addr >= 0xA000 && addr <= 0xBFFF) {
        mappedAddr = (uint32_t(prgBank[1]) * 0x2000) + (addr & 0x1FFF);
        return true;
    }
    if (addr >= 0xC000 && addr <= 0xDFFF) {
        mappedAddr = (uint32_t(prgBank[2]) * 0x2000) + (addr & 0x1FFF);
        return true;
    }
    if (addr >= 0xE000) {
        mappedAddr = (uint32_t(prgBank[3]) * 0x2000) + (addr & 0x1FFF);
        return true;
    }
    return false;
}

bool Mapper004::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    // PRG-RAM
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        if (prgRamEnabled && !prgRamWriteProtected) {
            mappedAddr = Mapper::PRG_RAM_FLAG | uint32_t(addr & 0x1FFF);
            return true;
        }
        return true; // handled (ignored)
    }

    if (addr >= 0x8000 && addr <= 0x9FFF) {
        if ((addr & 0x0001) == 0) {
            // Bank select
            bankSelect = data;
            prgMode = (data >> 6) & 0x01;
            chrMode = (data >> 7) & 0x01;
            updateBanks();
        } else {
            // Bank data
            uint8_t reg = bankSelect & 0x07;
            bankData[reg] = data;
            updateBanks();
        }
        mappedAddr = 0;
        return true;
    }

    if (addr >= 0xA000 && addr <= 0xBFFF) {
        if ((addr & 0x0001) == 0) {
            // Mirroring: MMC3 bit0 => 0=vertical, 1=horizontal
            // Cartridge::mirroringOverride() expects 0=HORIZONTAL, 1=VERTICAL.
            mirrorMode = (data & 0x01) ? 0 : 1;
        } else {
            // PRG-RAM protect
            prgRamEnabled = (data & 0x80) != 0;
            prgRamWriteProtected = (data & 0x40) != 0;
        }
        mappedAddr = 0;
        return true;
    }

    if (addr >= 0xC000 && addr <= 0xDFFF) {
        if ((addr & 0x0001) == 0) {
            irqLatch = data;
        } else {
            // Request IRQ counter reload on next eligible A12 rising edge.
            // Many MMC3 implementations also clear the counter here.
            irqReload = true;
            irqCounter = 0;
        }
        mappedAddr = 0;
        return true;
    }

    if (addr >= 0xE000) {
        if ((addr & 0x0001) == 0) {
            irqEnabled = false;
            irqActive = false;
            lastA12 = uint16_t(lastA12 & ~kIrqDelayMask);
        } else {
            irqEnabled = true;
        }
        mappedAddr = 0;
        return true;
    }

    return false;
}

bool Mapper004::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr <= 0x1FFF) {
        // 1KB CHR bank mapping
        uint8_t bank = uint8_t((addr >> 10) & 0x07);
        mappedAddr = (uint32_t(chrBank[bank]) * 0x0400) + (addr & 0x03FF);
        return true;
    }
    return false;
}

bool Mapper004::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (addr <= 0x1FFF) {
        // If CHR RAM, allow writes
        if (chrBanks == 0) {
            uint8_t bank = uint8_t((addr >> 10) & 0x07);
            mappedAddr = (uint32_t(chrBank[bank]) * 0x0400) + (addr & 0x03FF);
            return true;
        }
        return false;
    }
    return false;
}

void Mapper004::clockIrqCounter() {
    const bool counterWasZero = irqCounter == 0;
    const bool reloadWasRequested = irqReload;

    if (counterWasZero || reloadWasRequested) {
        irqCounter = irqLatch;
        irqReload = false;
    } else {
        irqCounter--;
    }

    // Common MMC3B/C silicon asserts whenever the post-clock value is zero.
    // The alternate/revision-A behaviour only asserts after a nonzero-to-zero
    // decrement or an explicit $C001 reload request; repeatedly reloading zero
    // after the counter naturally reached zero does not assert again.
    const bool alternateCanAssert = reloadWasRequested || !counterWasZero;
    if (irqCounter == 0 && irqEnabled
            && (!alternateIrqRevision || alternateCanAssert)) {
#if CIPHERNES_MMC3_IRQ_DELAY_DOTS == 0
        irqActive = true;
#else
        lastA12 = uint16_t((lastA12 & ~kIrqDelayMask)
            | uint16_t(CIPHERNES_MMC3_IRQ_DELAY_DOTS));
#endif
    }
}

void Mapper004::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    write_u8(out, bankSelect);
    write_bytes(out, bankData, sizeof(bankData));
    write_u8(out, prgMode);
    write_u8(out, chrMode);
    write_u8(out, mirrorMode);
    write_bool(out, prgRamEnabled);
    write_bool(out, prgRamWriteProtected);

    write_u8(out, irqLatch);
    write_u8(out, irqCounter);
    write_bool(out, irqReload);
    write_bool(out, irqEnabled);
    write_bool(out, irqActive);

    write_u16(out, lastA12);
    write_u8(out, a12LowCounter);

    write_bytes(out, prgBank, sizeof(prgBank));
    write_bytes(out, chrBank, sizeof(chrBank));
}

bool Mapper004::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    if (!read_u8(p,end,bankSelect)) return false;
    if (!read_bytes(p,end,bankData,sizeof(bankData))) return false;
    if (!read_u8(p,end,prgMode)) return false;
    if (!read_u8(p,end,chrMode)) return false;
    if (!read_u8(p,end,mirrorMode)) return false;
    if (!read_bool(p,end,prgRamEnabled)) return false;
    if (!read_bool(p,end,prgRamWriteProtected)) return false;

    if (!read_u8(p,end,irqLatch)) return false;
    if (!read_u8(p,end,irqCounter)) return false;
    if (!read_bool(p,end,irqReload)) return false;
    if (!read_bool(p,end,irqEnabled)) return false;
    if (!read_bool(p,end,irqActive)) return false;

    if (!read_u16(p,end,lastA12)) return false;
    if (!read_u8(p,end,a12LowCounter)) return false;

    if (!read_bytes(p,end,prgBank,sizeof(prgBank))) return false;
    if (!read_bytes(p,end,chrBank,sizeof(chrBank))) return false;

    return true;
}

void Mapper004::ppuAddressLatch(uint16_t /*addr*/) {
    // MMC2/MMC4-style read latches are not used by MMC3.
}

void Mapper004::ppuAddressBus(uint16_t addr) {
#if CIPHERNES_MMC3_IRQ_DELAY_DOTS != 0
    uint16_t irqDelay = uint16_t(lastA12 & kIrqDelayMask);
    if (irqDelay != 0) {
        --irqDelay;
        lastA12 = uint16_t((lastA12 & ~kIrqDelayMask) | irqDelay);
        if (irqDelay == 0) irqActive = true;
    }
#endif

    const bool a12 = (addr & kA12SampleBit) != 0;
    const bool previousA12 = (lastA12 & kA12SampleBit) != 0;

    if (!a12) {
        // Reject short A12-low gaps between pattern-plane fetches. A valid
        // rising edge requires about three CPU cycles (eight PPU dots) low.
        if (a12LowCounter < 0xFF) ++a12LowCounter;
    } else {
        if (!previousA12 && a12LowCounter >= 8) clockIrqCounter();
        a12LowCounter = 0;
    }

    lastA12 = uint16_t((lastA12 & kIrqDelayMask)
        | (a12 ? kA12SampleBit : 0u));
}
