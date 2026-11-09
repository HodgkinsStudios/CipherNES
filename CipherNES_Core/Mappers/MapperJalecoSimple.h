#pragma once

#include <cstdint>
#include "Mapper.h"

// Jaleco discrete boards:
//   mapper 72/92  - JF-17/JF-19
//   mapper 86     - JF-13
//   mapper 140    - JF-11/JF-14
//
// Cartridge banking is implemented here. Some original cartridges include a
// separate sample-playback IC controlled by otherwise-unused data bits. That
// optional audio device is not emulated by this mapper class.
class MapperJalecoSimple final : public Mapper {
public:
    MapperJalecoSimple(uint16_t prgBanks, uint16_t chrBanks, uint16_t mapperNumber);

    uint32_t capabilities() const override {
        return (mapperNumber == 72 || mapperNumber == 92)
            ? MAPPER_CAP_BUS_CONFLICT : 0u;
    }

    void reset() override;
    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;

private:
    const uint16_t mapperNumber;
    uint16_t prg16[2]{};
    uint16_t chr8 = 0;
    bool prgFlag = false;
    bool chrFlag = false;
};
