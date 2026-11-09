#pragma once
#include <cstdint>
#include "Mapper.h"
class Mapper080 final : public Mapper {
public:
    Mapper080(uint16_t prgBanks,uint16_t chrBanks,bool variant207=false):Mapper(prgBanks,chrBanks),variant207(variant207){reset();}
    uint32_t capabilities() const override { return variant207 ? MAPPER_CAP_NAMETABLE : MAPPER_CAP_MIRRORING; }
    void reset() override;
    bool cpuMapRead(uint16_t,uint32_t&) override; bool cpuMapWrite(uint16_t,uint32_t&,uint8_t) override;
    bool ppuMapRead(uint16_t,uint32_t&) override; bool ppuMapWrite(uint16_t,uint32_t&,uint8_t) override;
    uint8_t mirroringOverride() const override {return variant207 ? 0xFF : mirrorMode;}
    bool nametableRead(uint16_t addr, uint8_t& data) override;
    bool nametableWrite(uint16_t addr, uint8_t data) override;
    void saveState(std::vector<uint8_t>&) const override; bool loadState(const uint8_t*&,const uint8_t*) override;
private:
    uint8_t prg[3]{}, chr[8]{}; uint8_t ramPermission=0, mirrorMode=0xFF;
    // Mapper 207 routes the high bank bits of $7EF0/$7EF1 to CIRAM A10:
    // one choice for each 2-kiB pair of nametables, independent of $7EF6.
    bool variant207=false;
    uint8_t ntPage[2]{};
    uint8_t ciram[2048]{};
};
