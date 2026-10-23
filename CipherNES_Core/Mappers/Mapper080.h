#pragma once
#include <cstdint>
#include "Mapper.h"
class Mapper080 final : public Mapper {
public:
    Mapper080(uint16_t prgBanks,uint16_t chrBanks):Mapper(prgBanks,chrBanks){reset();}
    uint32_t capabilities() const override { return MAPPER_CAP_MIRRORING; }
    void reset() override;
    bool cpuMapRead(uint16_t,uint32_t&) override; bool cpuMapWrite(uint16_t,uint32_t&,uint8_t) override;
    bool ppuMapRead(uint16_t,uint32_t&) override; bool ppuMapWrite(uint16_t,uint32_t&,uint8_t) override;
    uint8_t mirroringOverride() const override {return mirrorMode;}
    void saveState(std::vector<uint8_t>&) const override; bool loadState(const uint8_t*&,const uint8_t*) override;
private:
    uint8_t prg[3]{}, chr[8]{}; uint8_t ramPermission=0, mirrorMode=0xFF;
};
