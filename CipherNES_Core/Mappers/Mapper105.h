#pragma once
#include "Mapper.h"
#include <vector>
class Mapper105 final : public Mapper {
public:
    Mapper105(uint16_t p,uint16_t c):Mapper(p,c){reset();}
    uint32_t capabilities()const override{return MAPPER_CAP_MIRRORING|MAPPER_CAP_IRQ;}
    void reset()override;
    bool cpuMapRead(uint16_t,uint32_t&)override;bool cpuMapWrite(uint16_t,uint32_t&,uint8_t)override;
    bool ppuMapRead(uint16_t,uint32_t&)override;bool ppuMapWrite(uint16_t,uint32_t&,uint8_t)override;
    uint8_t mirroringOverride()const override{return mirrorMode;}
    bool usesCpuClock()const override{return true;}void cpuClock()override;
    bool irqState()const override{return irqActive;}void irqClear()override{irqActive=false;}
    void saveState(std::vector<uint8_t>&)const override;bool loadState(const uint8_t*&,const uint8_t*)override;
private:
    uint8_t shiftReg=0x10,shiftCount=0,control=0x1C,chr0=0x10,chr1=0,prgReg=0;
    uint8_t mirrorMode=3,initState=0,dipSwitches=0;uint32_t irqCounter=0;bool irqEnabled=false,irqActive=false;
    void commit(uint16_t,uint8_t);void updateState();uint16_t selectedPrg16(uint16_t)const;
};
