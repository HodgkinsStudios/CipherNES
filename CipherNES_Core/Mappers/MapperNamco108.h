#pragma once
#include <array>
#include <cstdint>
#include "Mapper.h"

// Namco 108 family variants used by mappers 76, 88 and 95.
class MapperNamco108 final : public Mapper {
public:
    MapperNamco108(uint16_t prgBanks,uint16_t chrBanks,uint16_t mapperNumber);
    uint32_t capabilities() const override { return mapperNumber==95 ? MAPPER_CAP_NAMETABLE : 0u; }
    void reset() override;
    bool cpuMapRead(uint16_t,uint32_t&) override; bool cpuMapWrite(uint16_t,uint32_t&,uint8_t) override;
    bool ppuMapRead(uint16_t,uint32_t&) override; bool ppuMapWrite(uint16_t,uint32_t&,uint8_t) override;
    bool nametableRead(uint16_t,uint8_t&) override; bool nametableWrite(uint16_t,uint8_t) override;
    void saveState(std::vector<uint8_t>&) const override; bool loadState(const uint8_t*&,const uint8_t*) override;
private:
    const uint16_t mapperNumber;
    uint8_t select=0,reg[8]{}; uint16_t chrPage[8]{}; uint8_t prgPage[4]{};
    std::array<uint8_t,2048> ntRam{};
    void update();
};
