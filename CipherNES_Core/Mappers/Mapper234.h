#pragma once
#include "Mapper.h"
#include <vector>
class Mapper234 final:public Mapper{
public:Mapper234(uint16_t p,uint16_t c):Mapper(p,c){reset();}
uint32_t capabilities()const override{return MAPPER_CAP_MIRRORING|MAPPER_CAP_BUS_CONFLICT|MAPPER_CAP_CPU_OBSERVE;}
void reset()override{r0=r1=0;update();}bool cpuMapRead(uint16_t,uint32_t&)override;bool cpuMapWrite(uint16_t,uint32_t&,uint8_t)override;bool ppuMapRead(uint16_t,uint32_t&)override;bool ppuMapWrite(uint16_t,uint32_t&,uint8_t)override;uint8_t mirroringOverride()const override{return mirrorMode;}void afterCpuRead(uint16_t,uint8_t)override;void saveState(std::vector<uint8_t>&)const override;bool loadState(const uint8_t*&,const uint8_t*)override;
private:uint8_t r0=0,r1=0,mirrorMode=1;uint16_t prg32=0,chr8=0;void latch(uint16_t,uint8_t);void update();};
