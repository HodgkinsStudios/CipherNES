#pragma once
#include "Mapper.h"
#include <vector>
class Mapper077 final : public Mapper {
public:
    Mapper077(uint16_t p,uint16_t c):Mapper(p,c){reset();}
    uint32_t capabilities()const override{return MAPPER_CAP_BUS_CONFLICT|MAPPER_CAP_MIRRORING;}
    void reset()override{prg32=0;chr2=0;}
    bool cpuMapRead(uint16_t,uint32_t&)override;bool cpuMapWrite(uint16_t,uint32_t&,uint8_t)override;
    bool ppuMapRead(uint16_t,uint32_t&)override;bool ppuMapWrite(uint16_t,uint32_t&,uint8_t)override;
    uint8_t mirroringOverride()const override{return 2;}
    void saveState(std::vector<uint8_t>&)const override;bool loadState(const uint8_t*&,const uint8_t*)override;
private:uint8_t prg32=0,chr2=0;
};
