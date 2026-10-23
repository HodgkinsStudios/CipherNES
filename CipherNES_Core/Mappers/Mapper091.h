#pragma once
#include <cstdint>
#include "Mapper.h"
class Mapper091 final:public Mapper{
public:Mapper091(uint16_t p,uint16_t c):Mapper(p,c){reset();}uint32_t capabilities()const override{return MAPPER_CAP_IRQ|MAPPER_CAP_PPU_ADDRESS_BUS;}void reset()override;
bool cpuMapRead(uint16_t,uint32_t&)override;bool cpuMapWrite(uint16_t,uint32_t&,uint8_t)override;bool ppuMapRead(uint16_t,uint32_t&)override;bool ppuMapWrite(uint16_t,uint32_t&,uint8_t)override;
bool irqState()const override{return irqActive;}void irqClear()override{irqActive=false;}void ppuAddressBus(uint16_t)override;void saveState(std::vector<uint8_t>&)const override;bool loadState(const uint8_t*&,const uint8_t*)override;
private:uint8_t prg[2]{},chr2k[4]{};uint8_t irqCounter=0;bool irqReload=false,irqEnabled=false,irqActive=false;bool lastA12=false;uint8_t lowDots=0;void clockIrq();};
