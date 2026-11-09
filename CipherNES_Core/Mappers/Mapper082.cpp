#include "Mapper082.h"
#include "../state_io.h"
void Mapper082::reset(){prg[0]=0;prg[1]=1;prg[2]=2;for(uint8_t&v:chrReg)v=0;for(uint8_t&v:perm)v=0;chrMode=0;mirrorMode=0xFF;irqLatch=irqControl=0;irqCounter=0;irqActive=false;updateChr();}
void Mapper082::updateChr(){if(!chrMode){chrPage[0]=chrReg[0]&0xFE;chrPage[1]=uint8_t(chrPage[0]+1);chrPage[2]=chrReg[1]&0xFE;chrPage[3]=uint8_t(chrPage[2]+1);for(int i=0;i<4;i++)chrPage[4+i]=chrReg[2+i];}else{for(int i=0;i<4;i++)chrPage[i]=chrReg[2+i];chrPage[4]=chrReg[0]&0xFE;chrPage[5]=uint8_t(chrPage[4]+1);chrPage[6]=chrReg[1]&0xFE;chrPage[7]=uint8_t(chrPage[6]+1);}}
bool Mapper082::ramMap(uint16_t a,uint32_t&m)const{if(a>=0x6000&&a<=0x67FF&&perm[0]==0xCA){m=PRG_RAM_FLAG|uint32_t(a-0x6000);return true;}if(a>=0x6800&&a<=0x6FFF&&perm[1]==0x69){m=PRG_RAM_FLAG|uint32_t(0x800+a-0x6800);return true;}if(a>=0x7000&&a<=0x73FF&&perm[2]==0x84){m=PRG_RAM_FLAG|uint32_t(0x1000+a-0x7000);return true;}return false;}
// The X1-017 ASIC has unusually strong pull-downs: CPU reads from its
// disabled WRAM and write-only register region are electrically zero rather
// than unrelated CPU open-bus data. Enabled windows must still be served by
// Cartridge's real PRG-RAM array, not by this direct-read hook.
bool Mapper082::cpuReadDirect(uint16_t addr, uint8_t& data) {
    if (addr < 0x6000u || addr >= 0x8000u) return false;
    uint32_t mapped=0;
    if (ramMap(addr,mapped)) return false;
    data=0;
    return true;
}
bool Mapper082::cpuMapRead(uint16_t a,uint32_t&m){if(a<0x8000)return ramMap(a,m);uint16_t c=prgBanks?uint16_t(prgBanks*2):1;uint8_t s=(a-0x8000)>>13;uint16_t p=s<3?prg[s]:uint16_t(c-1);m=uint32_t(p%c)*0x2000+(a&0x1FFF);return true;}
bool Mapper082::cpuMapWrite(uint16_t a,uint32_t&m,uint8_t d){if(a<0x7EF0)return ramMap(a,m);if(a>0x7EFF)return false;if(a<=0x7EF5){chrReg[a&0xF]=d;updateChr();}else if(a==0x7EF6){mirrorMode=(d&1u)?0u:1u;chrMode=(d>>1)&1;updateChr();}else if(a<=0x7EF9){perm[(a&0xF)-7]=d;}else if(a<=0x7EFC){// Legacy iNES 82 exposes four PRG bank bits at D2..D5.
        // NES 2.0 mapper 552 reflects the real six reverse-wired ROM
        // address lines: D0 -> A18 (bank bit 5), D5 -> A13 (bank bit 0).
        // Do not silently reinterpret legacy mapper 82 dumps.
        if (actualWiring) {
            const uint8_t bits = uint8_t(d & 0x3Fu);
            prg[a-0x7EFA] = uint8_t(((bits & 0x01u) << 5u) |
                ((bits & 0x02u) << 3u) | ((bits & 0x04u) << 1u) |
                ((bits & 0x08u) >> 1u) | ((bits & 0x10u) >> 3u) |
                ((bits & 0x20u) >> 5u));
        } else prg[a-0x7EFA] = uint8_t((d>>2)&0x0Fu);}
    else if(a==0x7EFD){irqLatch=d;}
    else if(a==0x7EFE){
        irqControl=d;
        if(!(d&1u))irqCounter=irqLatch?uint16_t((uint16_t(irqLatch)+2u)*16u):17u;
        if(!(d&2u))irqActive=false;
    }else if(a==0x7EFF){
        irqActive=false;
        irqCounter=irqLatch?uint16_t((uint16_t(irqLatch)+1u)*16u):1u;
    }
    m=0;return true;}
bool Mapper082::ppuMapRead(uint16_t a,uint32_t&m){if(a>0x1FFF)return false;uint16_t c=chrBanks?uint16_t(chrBanks*8):8;uint8_t s=a>>10;m=uint32_t(chrPage[s]%c)*0x400+(a&0x3FF);return true;}bool Mapper082::ppuMapWrite(uint16_t a,uint32_t&m,uint8_t){return chrBanks==0&&ppuMapRead(a,m);}
void Mapper082::saveState(std::vector<uint8_t>&o)const{using namespace ciphernes_state;for(uint8_t v:prg)write_u8(o,v);for(uint8_t v:chrReg)write_u8(o,v);for(uint8_t v:perm)write_u8(o,v);write_u8(o,chrMode);write_u8(o,mirrorMode);write_u8(o,irqLatch);write_u8(o,irqControl);write_u16(o,irqCounter);write_bool(o,irqActive);}
bool Mapper082::loadState(const uint8_t*&p,const uint8_t*e){using namespace ciphernes_state;for(uint8_t&v:prg)if(!read_u8(p,e,v))return false;for(uint8_t&v:chrReg)if(!read_u8(p,e,v))return false;for(uint8_t&v:perm)if(!read_u8(p,e,v))return false;if(!read_u8(p,e,chrMode)||!read_u8(p,e,mirrorMode))return false;if(!read_u8(p,e,irqLatch)||!read_u8(p,e,irqControl)||!read_u16(p,e,irqCounter)||!read_bool(p,e,irqActive))return false;updateChr();return true;}

void Mapper082::cpuClock(){
    if((irqControl&0x05u)!=0x01u || irqCounter==0) return;
    if(--irqCounter==0 && (irqControl&0x02u)) irqActive=true;
}
