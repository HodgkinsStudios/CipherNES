#include "MapperVrc6.h"
#include "../state_io.h"

#include <algorithm>
#include <cstring>

void MapperVrc6::Pulse::reset() {
    volume = duty = step = shift = 0; ignoreDuty = enabled = false; frequency = 1; timer = 1;
}
void MapperVrc6::Pulse::write(uint8_t reg, uint8_t data) {
    switch (reg) {
        case 0: volume=data&0x0F; duty=(data>>4)&7; ignoreDuty=(data&0x80)!=0; break;
        case 1: frequency=uint16_t((frequency&0x0F00)|data); break;
        case 2:
            frequency=uint16_t((frequency&0x00FF)|((uint16_t(data&0x0F))<<8));
            enabled=(data&0x80)!=0; if(!enabled) step=0; break;
    }
}
void MapperVrc6::Pulse::clock() {
    if (!enabled) return;
    if (--timer <= 0) { step=uint8_t((step+1)&0x0F); timer=int32_t((frequency>>shift)+1u); }
}
uint8_t MapperVrc6::Pulse::output() const {
    if (!enabled) return 0;
    return (ignoreDuty || step <= duty) ? volume : 0;
}

void MapperVrc6::Saw::reset() {
    rate=accumulator=step=shift=0; enabled=false; frequency=1; timer=1;
}
void MapperVrc6::Saw::write(uint8_t reg, uint8_t data) {
    switch (reg) {
        case 0: rate=data&0x3F; break;
        case 1: frequency=uint16_t((frequency&0x0F00)|data); break;
        case 2:
            frequency=uint16_t((frequency&0x00FF)|((uint16_t(data&0x0F))<<8));
            enabled=(data&0x80)!=0; if(!enabled){accumulator=0;step=0;} break;
    }
}
void MapperVrc6::Saw::clock() {
    if (!enabled) return;
    if (--timer <= 0) {
        step=uint8_t((step+1)%14u); timer=int32_t((frequency>>shift)+1u);
        if(step==0) accumulator=0; else if((step&1u)==0) accumulator=uint8_t(accumulator+rate);
    }
}
uint8_t MapperVrc6::Saw::output() const { return enabled ? uint8_t(accumulator>>3) : 0; }

MapperVrc6::MapperVrc6(uint16_t p, uint16_t c, uint16_t mapper)
    : Mapper(p,c), mapperNumber(mapper) { reset(); }

uint16_t MapperVrc6::prgCount8() const { return std::max<uint16_t>(1u,uint16_t(prgBanks*2u)); }
uint16_t MapperVrc6::chrCount1() const { return chrBanks ? uint16_t(chrBanks*8u) : 8u; }
uint16_t MapperVrc6::translate(uint16_t addr) const {
    if(mapperNumber==26u) addr=uint16_t((addr&0xFFFCu)|((addr&1u)<<1)|((addr&2u)>>1));
    return addr;
}

void MapperVrc6::reset() {
    std::memset(chrReg,0,sizeof(chrReg)); std::memset(chrPage,0,sizeof(chrPage)); std::memset(ntRam,0,sizeof(ntRam));
    const uint16_t pc=prgCount8(); prg8[0]=0;prg8[1]=1;prg8[2]=0;prg8[3]=uint16_t(pc-1u);
    bankingMode=0; ntUsesChr=false; setNt(0,0,1,1);
    irqLatch=irqCounter=0; irqPrescaler=0; irqEnabled=irqEnableAfterAck=irqCycleMode=irqActive=false;
    pulse[0].reset();pulse[1].reset();saw.reset();haltAudio=false;frequencyShift=0;
    updatePpuBanking();
}

void MapperVrc6::setNt(uint8_t a,uint8_t b,uint8_t c,uint8_t d){ntPage[0]=a;ntPage[1]=b;ntPage[2]=c;ntPage[3]=d;}
void MapperVrc6::updateNametables(){
    ntUsesChr=(bankingMode&0x10u)!=0;
    const uint8_t m=bankingMode&0x2Fu;
    if(ntUsesChr){
        switch(m){
            case 0x20:case 0x27:setNt(chrReg[6]&0xFEu,(chrReg[6]&0xFEu)|1u,chrReg[7]&0xFEu,(chrReg[7]&0xFEu)|1u);return;
            case 0x23:case 0x24:setNt(chrReg[6]&0xFEu,chrReg[7]&0xFEu,(chrReg[6]&0xFEu)|1u,(chrReg[7]&0xFEu)|1u);return;
            case 0x28:case 0x2F:setNt(chrReg[6]&0xFEu,chrReg[6]&0xFEu,chrReg[7]&0xFEu,chrReg[7]&0xFEu);return;
            case 0x2B:case 0x2C:setNt((chrReg[6]&0xFEu)|1u,(chrReg[7]&0xFEu)|1u,(chrReg[6]&0xFEu)|1u,(chrReg[7]&0xFEu)|1u);return;
        }
        switch(bankingMode&7u){
            case 0:case 6:case 7:setNt(chrReg[6],chrReg[6],chrReg[7],chrReg[7]);break;
            case 1:case 5:setNt(chrReg[4],chrReg[5],chrReg[6],chrReg[7]);break;
            default:setNt(chrReg[6],chrReg[7],chrReg[6],chrReg[7]);break;
        }
    }else{
        switch(m){
            case 0x20:case 0x27:setNt(0,1,0,1);return;
            case 0x23:case 0x24:setNt(0,0,1,1);return;
            case 0x28:case 0x2F:setNt(0,0,0,0);return;
            case 0x2B:case 0x2C:setNt(1,1,1,1);return;
        }
        switch(bankingMode&7u){
            case 0:case 6:case 7:setNt(chrReg[6]&1u,chrReg[6]&1u,chrReg[7]&1u,chrReg[7]&1u);break;
            case 1:case 5:setNt(chrReg[4]&1u,chrReg[5]&1u,chrReg[6]&1u,chrReg[7]&1u);break;
            default:setNt(chrReg[6]&1u,chrReg[7]&1u,chrReg[6]&1u,chrReg[7]&1u);break;
        }
    }
}
void MapperVrc6::updatePpuBanking(){
    const uint16_t mask=(bankingMode&0x20u)?0xFEu:0xFFu; const uint16_t orv=(bankingMode&0x20u)?1u:0u;
    switch(bankingMode&3u){
        case 0: for(uint8_t i=0;i<8;i++) chrPage[i]=chrReg[i]; break;
        case 1:
            for(uint8_t i=0;i<4;i++){chrPage[i*2]=chrReg[i]&mask;chrPage[i*2+1]=(chrReg[i]&mask)|orv;} break;
        default:
            for(uint8_t i=0;i<4;i++)chrPage[i]=chrReg[i];
            chrPage[4]=chrReg[4]&mask;chrPage[5]=(chrReg[4]&mask)|orv;
            chrPage[6]=chrReg[5]&mask;chrPage[7]=(chrReg[5]&mask)|orv;break;
    }
    updateNametables();
}

bool MapperVrc6::cpuMapRead(uint16_t addr,uint32_t& mappedAddr){
    if(addr>=0x6000u&&addr<=0x7FFFu){if(!(bankingMode&0x80u))return false;mappedAddr=PRG_RAM_FLAG|uint32_t(addr&0x1FFFu);return true;}
    if (addr < 0x8000u) return false;
    const uint8_t slot = uint8_t((addr - 0x8000u) >> 13);
    const uint16_t page = uint16_t(prg8[slot] % prgCount8());
    mappedAddr = uint32_t(page) * 0x2000u + uint32_t(addr & 0x1FFFu);
    return true;
}
bool MapperVrc6::cpuMapWrite(uint16_t addr,uint32_t& mappedAddr,uint8_t data){
    mappedAddr=0;if(addr>=0x6000u&&addr<=0x7FFFu){if(bankingMode&0x80u)mappedAddr=PRG_RAM_FLAG|uint32_t(addr&0x1FFFu);return true;}if(addr<0x8000u)return false;
    addr=translate(addr);switch(addr&0xF003u){
        case 0x8000:case 0x8001:case 0x8002:case 0x8003:{uint16_t b=uint16_t((data&0x0Fu)*2u);prg8[0]=b;prg8[1]=uint16_t(b+1u);break;}
        case 0x9000:case 0x9001:case 0x9002:pulse[0].write(uint8_t(addr&3u),data);break;
        case 0x9003:haltAudio=(data&1u)!=0;frequencyShift=(data&4u)?8u:((data&2u)?4u:0u);pulse[0].shift=pulse[1].shift=saw.shift=frequencyShift;break;
        case 0xA000:case 0xA001:case 0xA002:pulse[1].write(uint8_t(addr&3u),data);break;
        case 0xB000:case 0xB001:case 0xB002:saw.write(uint8_t(addr&3u),data);break;
        case 0xB003:bankingMode=data;updatePpuBanking();break;
        case 0xC000:case 0xC001:case 0xC002:case 0xC003:prg8[2]=data&0x1Fu;break;
        case 0xD000:case 0xD001:case 0xD002:case 0xD003:chrReg[addr&3u]=data;updatePpuBanking();break;
        case 0xE000:case 0xE001:case 0xE002:case 0xE003:chrReg[4u+(addr&3u)]=data;updatePpuBanking();break;
        case 0xF000:irqLatch=data;break;
        case 0xF001:irqEnableAfterAck=(data&1u)!=0;irqEnabled=(data&2u)!=0;irqCycleMode=(data&4u)!=0;if(irqEnabled){irqCounter=irqLatch;irqPrescaler=341;}irqActive=false;break;
        case 0xF002:irqEnabled=irqEnableAfterAck;irqActive=false;break;
    }return true;
}

bool MapperVrc6::ppuMapRead(uint16_t addr,uint32_t& mappedAddr){
    addr&=0x3FFFu;if(addr<=0x1FFFu){uint8_t s=uint8_t(addr>>10);mappedAddr=uint32_t(chrPage[s]%chrCount1())*0x400u+(addr&0x3FFu);return true;}
    if(addr>=0x2000u&&addr<=0x3EFFu&&ntUsesChr){uint16_t o=uint16_t((addr-0x2000u)&0x0FFFu);uint8_t t=uint8_t(o>>10);mappedAddr=uint32_t(ntPage[t]%chrCount1())*0x400u+(o&0x3FFu);return true;}return false;
}
bool MapperVrc6::ppuMapWrite(uint16_t addr,uint32_t& mappedAddr,uint8_t){
    addr&=0x3FFFu;if(addr<=0x1FFFu&&chrBanks==0u){uint8_t s=uint8_t(addr>>10);mappedAddr=uint32_t(chrPage[s]%chrCount1())*0x400u+(addr&0x3FFu);return true;}return false;
}
bool MapperVrc6::nametableRead(uint16_t addr,uint8_t& data){if(ntUsesChr)return false;uint16_t o=uint16_t((addr-0x2000u)&0x0FFFu);data=ntRam[uint16_t(ntPage[o>>10]&1u)*0x400u+(o&0x3FFu)];return true;}
bool MapperVrc6::nametableWrite(uint16_t addr,uint8_t data){if(ntUsesChr)return false;uint16_t o=uint16_t((addr-0x2000u)&0x0FFFu);ntRam[uint16_t(ntPage[o>>10]&1u)*0x400u+(o&0x3FFu)]=data;return true;}

void MapperVrc6::cpuClock(){
    if(irqEnabled){irqPrescaler=int16_t(irqPrescaler-3);if(irqCycleMode||irqPrescaler<=0){if(irqCounter==0xFFu){irqCounter=irqLatch;irqActive=true;}else irqCounter++;irqPrescaler=int16_t(irqPrescaler+341);}}
    if(!haltAudio){pulse[0].clock();pulse[1].clock();saw.clock();}
}
float MapperVrc6::audioSample(){const float pulseStrength=0.149f;return float(pulse[0].output()+pulse[1].output()+saw.output())*(pulseStrength/15.0f);}

void MapperVrc6::saveState(std::vector<uint8_t>& out) const{
    using namespace ciphernes_state;for(uint8_t i=0;i<4;i++)write_u16(out,prg8[i]);for(uint8_t i=0;i<8;i++){write_u16(out,chrReg[i]);write_u16(out,chrPage[i]);}write_u8(out,bankingMode);for(uint8_t i=0;i<4;i++)write_u8(out,ntPage[i]);write_bool(out,ntUsesChr);for(uint16_t i=0;i<0x800;i++)write_u8(out,ntRam[i]);
    write_u8(out,irqLatch);write_u8(out,irqCounter);write_u16(out,uint16_t(irqPrescaler));write_bool(out,irqEnabled);write_bool(out,irqEnableAfterAck);write_bool(out,irqCycleMode);write_bool(out,irqActive);write_bool(out,haltAudio);write_u8(out,frequencyShift);
    for(uint8_t i=0;i<2;i++){const Pulse&p=pulse[i];write_u8(out,p.volume);write_u8(out,p.duty);write_u8(out,p.step);write_u8(out,p.shift);write_bool(out,p.ignoreDuty);write_bool(out,p.enabled);write_u16(out,p.frequency);write_u32(out,uint32_t(p.timer));}
    write_u8(out,saw.rate);write_u8(out,saw.accumulator);write_u8(out,saw.step);write_u8(out,saw.shift);write_bool(out,saw.enabled);write_u16(out,saw.frequency);write_u32(out,uint32_t(saw.timer));
}
bool MapperVrc6::loadState(const uint8_t*&p,const uint8_t*e){
    using namespace ciphernes_state;for(uint8_t i=0;i<4;i++)if(!read_u16(p,e,prg8[i]))return false;for(uint8_t i=0;i<8;i++)if(!read_u16(p,e,chrReg[i])||!read_u16(p,e,chrPage[i]))return false;if(!read_u8(p,e,bankingMode))return false;for(uint8_t i=0;i<4;i++)if(!read_u8(p,e,ntPage[i]))return false;if(!read_bool(p,e,ntUsesChr))return false;for(uint16_t i=0;i<0x800;i++)if(!read_u8(p,e,ntRam[i]))return false;
    uint16_t ps=0;if(!read_u8(p,e,irqLatch)||!read_u8(p,e,irqCounter)||!read_u16(p,e,ps)||!read_bool(p,e,irqEnabled)||!read_bool(p,e,irqEnableAfterAck)||!read_bool(p,e,irqCycleMode)||!read_bool(p,e,irqActive)||!read_bool(p,e,haltAudio)||!read_u8(p,e,frequencyShift))return false;irqPrescaler=int16_t(ps);
    for(uint8_t i=0;i<2;i++){Pulse&q=pulse[i];uint32_t t=0;if(!read_u8(p,e,q.volume)||!read_u8(p,e,q.duty)||!read_u8(p,e,q.step)||!read_u8(p,e,q.shift)||!read_bool(p,e,q.ignoreDuty)||!read_bool(p,e,q.enabled)||!read_u16(p,e,q.frequency)||!read_u32(p,e,t))return false;q.timer=int32_t(t);}
    uint32_t t=0;if(!read_u8(p,e,saw.rate)||!read_u8(p,e,saw.accumulator)||!read_u8(p,e,saw.step)||!read_u8(p,e,saw.shift)||!read_bool(p,e,saw.enabled)||!read_u16(p,e,saw.frequency)||!read_u32(p,e,t))return false;saw.timer=int32_t(t);return true;
}
