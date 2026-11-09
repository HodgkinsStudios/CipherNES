#include "MapperNamco108.h"
#include "../state_io.h"
#include <algorithm>
MapperNamco108::MapperNamco108(uint16_t p,uint16_t c,uint16_t m):Mapper(p,c),mapperNumber(m){reset();}
void MapperNamco108::reset(){select=0;for(uint8_t&v:reg)v=0;ntRam.fill(0);update();}
void MapperNamco108::update(){uint16_t pc=prgBanks?uint16_t(prgBanks*2):1;prgPage[0]=uint8_t(reg[6]%pc);prgPage[1]=uint8_t(reg[7]%pc);prgPage[2]=uint8_t(pc>1?pc-2:0);prgPage[3]=uint8_t(pc-1);
 uint16_t cc=chrBanks?uint16_t(chrBanks*8):8;
 if(mapperNumber==76){for(int i=0;i<4;i++){uint16_t b=reg[2+i];chrPage[i*2]=uint16_t((b*2)%cc);chrPage[i*2+1]=uint16_t((b*2+1)%cc);}}
 else{uint16_t r0=reg[0]&0xFE,r1=reg[1]&0xFE;uint16_t r[6]={r0,r1,reg[2],reg[3],reg[4],reg[5]};if(mapperNumber==88){r[0]&=0x3F;r[1]&=0x3F;for(int i=2;i<6;i++)r[i]|=0x40;}
 chrPage[0]=r[0]%cc;chrPage[1]=(r[0]+1)%cc;chrPage[2]=r[1]%cc;chrPage[3]=(r[1]+1)%cc;for(int i=0;i<4;i++)chrPage[4+i]=r[2+i]%cc;}}
bool MapperNamco108::cpuMapRead(uint16_t a,uint32_t&m){if(a<0x8000)return false;uint8_t s=(a-0x8000)>>13;m=uint32_t(prgPage[s])*0x2000+(a&0x1FFF);return true;}
bool MapperNamco108::cpuMapWrite(uint16_t a,uint32_t&m,uint8_t d){if(a<0x8000)return false;uint16_t r=a&0x8001;if(r==0x8000)select=d&7;else{reg[select&7]=d;update();}m=0;return true;}
bool MapperNamco108::ppuMapRead(uint16_t a,uint32_t&m){if(a>0x1FFF)return false;uint8_t s=a>>10;m=uint32_t(chrPage[s])*0x400+(a&0x3FF);return true;}bool MapperNamco108::ppuMapWrite(uint16_t a,uint32_t&m,uint8_t){return chrBanks==0&&ppuMapRead(a,m);}
bool MapperNamco108::nametableRead(uint16_t a,uint8_t&d){if(mapperNumber!=95)return false;uint16_t x=(a-0x2000)&0xFFF;uint8_t t=x>>10;uint8_t page=(t<2)?((reg[0]>>5)&1):((reg[1]>>5)&1);d=ntRam[page*0x400+(x&0x3FF)];return true;}
bool MapperNamco108::nametableWrite(uint16_t a,uint8_t d){if(mapperNumber!=95)return false;uint16_t x=(a-0x2000)&0xFFF;uint8_t t=x>>10;uint8_t page=(t<2)?((reg[0]>>5)&1):((reg[1]>>5)&1);ntRam[page*0x400+(x&0x3FF)]=d;return true;}
void MapperNamco108::saveState(std::vector<uint8_t>&o)const{using namespace ciphernes_state;write_u8(o,select);for(uint8_t v:reg)write_u8(o,v);write_bytes(o,ntRam.data(),ntRam.size());}
bool MapperNamco108::loadState(const uint8_t*&p,const uint8_t*e){using namespace ciphernes_state;if(!read_u8(p,e,select))return false;for(uint8_t&v:reg)if(!read_u8(p,e,v))return false;if(!read_bytes(p,e,ntRam.data(),ntRam.size()))return false;update();return true;}
