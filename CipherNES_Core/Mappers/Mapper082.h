#pragma once
#include <cstdint>
#include "Mapper.h"
class Mapper082 final:public Mapper{
public:Mapper082(uint16_t p,uint16_t c, bool nes2Wiring=false):Mapper(p,c),actualWiring(nes2Wiring){reset();}uint32_t capabilities()const override{return MAPPER_CAP_MIRRORING | MAPPER_CAP_IRQ | MAPPER_CAP_CPU_DIRECT;}void reset()override;
    bool usesCpuClock() const override { return true; }
    void cpuClock() override;
    bool irqState() const override { return irqActive; }
    void irqClear() override { irqActive = false; }
    uint32_t saveStateVersion() const override { return 2u; }
bool cpuReadDirect(uint16_t,uint8_t&)override;bool cpuMapRead(uint16_t,uint32_t&)override;bool cpuMapWrite(uint16_t,uint32_t&,uint8_t)override;bool ppuMapRead(uint16_t,uint32_t&)override;bool ppuMapWrite(uint16_t,uint32_t&,uint8_t)override;
uint8_t mirroringOverride()const override{return mirrorMode;}void saveState(std::vector<uint8_t>&)const override;bool loadState(const uint8_t*&,const uint8_t*)override;
private:const bool actualWiring = false; uint8_t prg[3]{},chrReg[6]{},chrPage[8]{},perm[3]{};uint8_t chrMode=0,mirrorMode=0xFF;void updateChr();bool ramMap(uint16_t,uint32_t&)const;
    uint8_t irqLatch = 0, irqControl = 0;
    uint16_t irqCounter = 0;
    bool irqActive = false;};
