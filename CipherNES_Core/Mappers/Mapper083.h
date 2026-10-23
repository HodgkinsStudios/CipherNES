#pragma once
#include <array>
#include "Mapper.h"
class Mapper083 final : public Mapper {
public:
    Mapper083(uint16_t p,uint16_t c):Mapper(p,c){reset();}
    uint32_t capabilities() const override { return MAPPER_CAP_CPU_DIRECT|MAPPER_CAP_MIRRORING|MAPPER_CAP_IRQ; }
    void reset() override;
    bool cpuReadDirect(uint16_t addr,uint8_t& data) override;
    bool cpuWriteDirect(uint16_t addr,uint8_t data) override;
    bool cpuMapRead(uint16_t addr,uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr,uint32_t& mappedAddr,uint8_t data) override;
    bool ppuMapRead(uint16_t addr,uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr,uint32_t& mappedAddr,uint8_t data) override;
    bool usesCpuClock() const override{return true;} void cpuClock() override;
    uint8_t mirroringOverride() const override{return mirrorMode;}
    bool irqState() const override{return irqActive;} void irqClear() override{irqActive=false;}
    void saveState(std::vector<uint8_t>& out) const override; bool loadState(const uint8_t*&p,const uint8_t*e) override;
private:
    uint8_t regs[11]{}; uint8_t exRegs[4]{}; uint16_t prg8[4]{}; uint16_t chr1[8]{};
    bool is2k=false,isNot2k=false,irqEnabled=false,irqActive=false; uint8_t mode=0,bank=0,mirrorMode=1; uint16_t irqCounter=0;
    void update();
};
