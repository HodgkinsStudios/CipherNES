#include "Mapper188.h"
#include "../state_io.h"
#include <algorithm>

Mapper188::Mapper188(uint16_t p,uint16_t c):Mapper(p,c){reset();}
void Mapper188::reset(){lowerPrg=0;lowerOpenBus=false;mirrorMode=1;}

bool Mapper188::cpuReadDirect(uint16_t a,uint8_t& d){
    if(a>=0x6000u&&a<=0x7FFFu){
        // A/B are active-low and released by default; microphone is inactive.
        d=0x03u;
        return true;
    }
    return false;
}

bool Mapper188::cpuMapRead(uint16_t a,uint32_t&m){
    if(a<0x8000u)return false;
    const uint32_t count=std::max<uint32_t>(1u,prgBanks);
    if(a<0xC000u){if(lowerOpenBus)return false;m=(uint32_t(lowerPrg)%count)*0x4000u+(a&0x3FFFu);}
    else{m=(uint32_t(7u)%count)*0x4000u+(a&0x3FFFu);}
    return true;
}

bool Mapper188::cpuMapWrite(uint16_t a,uint32_t&m,uint8_t d){
    if(a<0x8000u)return false;
    if(d&0x10u){lowerPrg=uint16_t(d&7u);lowerOpenBus=false;}
    else if(prgBanks>=16u){lowerPrg=uint16_t((d&7u)|8u);lowerOpenBus=false;}
    else{lowerOpenBus=true;}
    mirrorMode=(d&0x20u)?0u:1u;
    m=0;return true;
}

bool Mapper188::ppuMapRead(uint16_t a,uint32_t&m){if(a>0x1FFFu)return false;m=a;if(chrBanks==0u)m|=CHR_RAM_FLAG;return true;}
bool Mapper188::ppuMapWrite(uint16_t a,uint32_t&m,uint8_t){if(chrBanks!=0u||a>0x1FFFu)return false;m=CHR_RAM_FLAG|uint32_t(a);return true;}
void Mapper188::saveState(std::vector<uint8_t>&o)const{using namespace ciphernes_state;write_u16(o,lowerPrg);write_bool(o,lowerOpenBus);write_u8(o,mirrorMode);}
bool Mapper188::loadState(const uint8_t*&p,const uint8_t*e){using namespace ciphernes_state;return read_u16(p,e,lowerPrg)&&read_bool(p,e,lowerOpenBus)&&read_u8(p,e,mirrorMode);}
