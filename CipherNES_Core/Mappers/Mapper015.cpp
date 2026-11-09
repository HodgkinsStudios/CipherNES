#include "Mapper015.h"
#include "../state_io.h"

void Mapper015::reset() {
    latch = 0;
    mode = 0;
    mirrorMode = 1; // vertical
    updateBanks();
}

void Mapper015::updateBanks() {
    const uint16_t count = prgBanks ? uint16_t(prgBanks * 2u) : 1u;
    const uint16_t sub = uint16_t(latch >> 7);
    uint16_t bank = uint16_t(latch & 0x7Fu) << 1;

    switch (mode & 3u) {
        case 0:
            pages[0] = uint16_t((bank + 0u) ^ sub);
            pages[1] = uint16_t((bank + 1u) ^ sub);
            pages[2] = uint16_t((bank + 2u) ^ sub);
            pages[3] = uint16_t((bank + 3u) ^ sub);
            break;
        case 1: {
            bank = uint16_t(bank | sub);
            pages[0] = bank;
            pages[1] = uint16_t(bank + 1u);
            const uint16_t fixed = uint16_t((bank | 0x0Eu) | sub);
            pages[2] = fixed;
            pages[3] = uint16_t(fixed + 1u);
            break;
        }
        case 2:
            bank = uint16_t(bank | sub);
            pages[0] = pages[1] = pages[2] = pages[3] = bank;
            break;
        case 3:
            bank = uint16_t(bank | sub);
            pages[0] = bank;
            pages[1] = uint16_t(bank + 1u);
            pages[2] = bank;
            pages[3] = uint16_t(bank + 1u);
            break;
    }
    for (uint16_t& page : pages) page = uint16_t(page % count);
}

bool Mapper015::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr < 0x8000u) return false;
    const uint8_t slot = uint8_t((addr - 0x8000u) >> 13);
    mappedAddr = uint32_t(pages[slot]) * 0x2000u + uint32_t(addr & 0x1FFFu);
    return true;
}

bool Mapper015::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    if (addr < 0x8000u) return false;
    latch = data;
    mode = uint8_t(addr & 3u);
    mirrorMode = (data & 0x40u) ? 0u : 1u;
    updateBanks();
    mappedAddr = 0;
    return true;
}

bool Mapper015::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr > 0x1FFFu) return false;
    mappedAddr = addr;
    return true;
}

bool Mapper015::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (addr > 0x1FFFu || chrBanks != 0) return false;
    // Real K-1029 hardware write-protects CHR RAM in modes 0 and 3.
    if (mode == 0u || mode == 3u) return false;
    mappedAddr = addr;
    return true;
}

void Mapper015::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    write_u8(out, latch);
    write_u8(out, mode);
    write_u8(out, mirrorMode);
}

bool Mapper015::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    if (!read_u8(p, end, latch) || !read_u8(p, end, mode) || !read_u8(p, end, mirrorMode)) return false;
    updateBanks();
    return true;
}
