#include "Mapper234.h"
#include "../state_io.h"
#include <algorithm>
void Mapper234::update(){if(r0&0x40u){prg32=uint16_t((r0&0x0Eu)|(r1&1u));chr8=uint16_t(((r0<<2)&0x38u)|((r1>>4)&7u));}else{prg32=r0&0x0Fu;chr8=uint16_t(((r0<<2)&0x3Cu)|((r1>>4)&3u));}mirrorMode=(r0&0x80u)?0u:1u;}
void Mapper234::latch(uint16_t a,uint8_t d){if(a>=0xFF80u&&a<=0xFF9Fu){if(!(r0&0x3Fu)){r0=d;update();}}else if(a>=0xFFE8u&&a<=0xFFF8u){r1=d&0x71u;update();}}
bool Mapper234::cpuMapRead(uint16_t a,uint32_t&m){if(a<0x8000u)return false;uint16_t c=std::max<uint16_t>(1,uint16_t((prgBanks+1u)/2u));m=uint32_t(prg32%c)*0x8000u+(a&0x7FFFu);return true;}
bool Mapper234::cpuMapWrite(uint16_t a,uint32_t&m,uint8_t d){if(!((a>=0xFF80u&&a<=0xFF9Fu)||(a>=0xFFE8u&&a<=0xFFF8u)))return false;latch(a,d);m=0;return true;}
void Mapper234::afterCpuRead(uint16_t a,uint8_t d){latch(a,d);}
bool Mapper234::ppuMapRead(uint16_t a,uint32_t&m){if(a>0x1FFFu)return false;uint16_t c=std::max<uint16_t>(1,chrBanks);m=uint32_t(chr8%c)*0x2000u+a;return true;}bool Mapper234::ppuMapWrite(uint16_t a,uint32_t&m,uint8_t){return chrBanks==0&&ppuMapRead(a,m);}
void Mapper234::saveState(std::vector<uint8_t>&o)const{using namespace ciphernes_state;write_u8(o,r0);write_u8(o,r1);write_u8(o,mirrorMode);write_u16(o,prg32);write_u16(o,chr8);}bool Mapper234::loadState(const uint8_t*&p,const uint8_t*e){using namespace ciphernes_state;return read_u8(p,e,r0)&&read_u8(p,e,r1)&&read_u8(p,e,mirrorMode)&&read_u16(p,e,prg32)&&read_u16(p,e,chr8);}
