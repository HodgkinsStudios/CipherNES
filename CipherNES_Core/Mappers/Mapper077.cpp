#include "Mapper077.h"
#include "../state_io.h"
#include <algorithm>
bool Mapper077::cpuMapRead(uint16_t a,uint32_t&m){if(a<0x8000)return false;uint16_t c=std::max<uint16_t>(1,uint16_t((prgBanks+1u)/2u));m=uint32_t(prg32%c)*0x8000u+(a&0x7FFFu);return true;}
bool Mapper077::cpuMapWrite(uint16_t a,uint32_t&m,uint8_t d){if(a<0x8000)return false;prg32=d&0x0Fu;chr2=(d>>4)&0x0Fu;m=0;return true;}
bool Mapper077::ppuMapRead(uint16_t a,uint32_t&m){if(a>0x1FFF)return false;if(a<0x0800u){uint16_t c=std::max<uint16_t>(1,uint16_t(chrBanks*4u));m=uint32_t(chr2%c)*0x0800u+(a&0x07FFu);}else m=CHR_RAM_FLAG|uint32_t(a-0x0800u);return true;}
bool Mapper077::ppuMapWrite(uint16_t a,uint32_t&m,uint8_t){if(a<0x0800u||a>0x1FFFu)return false;m=CHR_RAM_FLAG|uint32_t(a-0x0800u);return true;}
void Mapper077::saveState(std::vector<uint8_t>&o)const{ciphernes_state::write_u8(o,prg32);ciphernes_state::write_u8(o,chr2);}bool Mapper077::loadState(const uint8_t*&p,const uint8_t*e){return ciphernes_state::read_u8(p,e,prg32)&&ciphernes_state::read_u8(p,e,chr2);}
