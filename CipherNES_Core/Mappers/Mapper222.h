#pragma once
#include "Mapper.h"
class Mapper222 final : public Mapper {
public:
    Mapper222(uint16_t p,uint16_t c):Mapper(p,c){reset();}
    uint32_t capabilities() const override{return MAPPER_CAP_MIRRORING|MAPPER_CAP_IRQ|MAPPER_CAP_PPU_ADDRESS_BUS;}
    void reset() override; bool cpuMapRead(uint16_t,uint32_t&) override; bool cpuMapWrite(uint16_t,uint32_t&,uint8_t) override; bool ppuMapRead(uint16_t,uint32_t&) override; bool ppuMapWrite(uint16_t,uint32_t&,uint8_t) override;
    uint8_t mirroringOverride()const override{return mirrorMode;} bool irqState()const override{return irqActive;}void irqClear()override{irqActive=false;}void ppuAddressBus(uint16_t)override;
    void saveState(std::vector<uint8_t>&)const override;bool loadState(const uint8_t*&,const uint8_t*)override;
private:uint16_t prg8[4]{},chr1[8]{};uint16_t irqCounter=0;uint8_t mirrorMode=1,a12Low=0;bool lastA12=false,irqActive=false;
};
