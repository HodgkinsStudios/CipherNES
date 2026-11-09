#include "Mapper160.h"
#include "../state_io.h"
#include <algorithm>

Mapper160::Mapper160(uint16_t p,uint16_t c):Mapper(p,c){reset();}
void Mapper160::reset(){chrBank=0;}
bool Mapper160::cpuMapRead(uint16_t a,uint32_t&m){if(a<0x8000u)return false;const uint32_t size=std::max<uint32_t>(1u,uint32_t(prgBanks))*0x4000u;m=uint32_t(a-0x8000u)%size;return true;}
bool Mapper160::cpuMapWrite(uint16_t a,uint32_t&m,uint8_t d){
    if(a>=0x4100u&&a<=0x5FFFu){if(a&0x0100u)chrBank=uint8_t(d&1u);m=0;return true;}
    return false;
}
bool Mapper160::ppuMapRead(uint16_t a,uint32_t&m){if(a>0x1FFFu)return false;const uint32_t count=std::max<uint32_t>(1u,chrBanks);m=(uint32_t(chrBank)%count)*0x2000u+a;if(chrBanks==0u)m|=CHR_RAM_FLAG;return true;}
bool Mapper160::ppuMapWrite(uint16_t a,uint32_t&m,uint8_t){if(chrBanks!=0u||a>0x1FFFu)return false;m=CHR_RAM_FLAG+uint32_t(a);return true;}
void Mapper160::saveState(std::vector<uint8_t>&o)const{ciphernes_state::write_u8(o,chrBank);}
bool Mapper160::loadState(const uint8_t*&p,const uint8_t*e){return ciphernes_state::read_u8(p,e,chrBank);}
