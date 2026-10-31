#include "MapperKaiser202.h"
#include "../state_io.h"
#include <algorithm>
#include <cstring>
MapperKaiser202::MapperKaiser202(uint16_t p,uint16_t c,uint16_t m):Mapper(p,c),mapperNumber(m){reset();}
uint32_t MapperKaiser202::capabilities()const{return MAPPER_CAP_IRQ|(mapperNumber==56?MAPPER_CAP_MIRRORING:0u);}
uint16_t MapperKaiser202::prgCount8()const{return std::max<uint16_t>(1,uint16_t(prgBanks*2u));}
void MapperKaiser202::reset(){irqReload=irqCounter=0;irqControl=selectedReg=0;std::memset(prgRegs,0,sizeof(prgRegs));for(int i=0;i<8;i++)chr1[i]=i;mirrorMode=0xFF;useRom=irqActive=false;prgRegs[3]=uint8_t(prgCount8()-1u);}
bool MapperKaiser202::cpuMapRead(uint16_t a,uint32_t&m){if(a>=0x6000u&&a<=0x7FFFu){if(useRom)m=uint32_t(prgRegs[3]%prgCount8())*0x2000u+(a&0x1FFFu);else m=PRG_RAM_FLAG|uint32_t(a&0x1FFFu);return true;}if(a<0x8000u)return false;uint8_t s=uint8_t((a-0x8000u)>>13);uint16_t p=s<3?prgRegs[s]:uint16_t(prgCount8()-1u);m=uint32_t(p%prgCount8())*0x2000u+(a&0x1FFFu);return true;}
bool MapperKaiser202::cpuMapWrite(uint16_t a,uint32_t&m,uint8_t d){if(a>=0x6000u&&a<=0x7FFFu&&!useRom){m=PRG_RAM_FLAG|uint32_t(a&0x1FFFu);return true;}if(a<0x8000u)return false;switch(a&0xF000u){case 0x8000:irqReload=uint16_t((irqReload&0xFFF0u)|(d&0x0Fu));break;case 0x9000:irqReload=uint16_t((irqReload&0xFF0Fu)|((d&0x0Fu)<<4));break;case 0xA000:irqReload=uint16_t((irqReload&0xF0FFu)|((d&0x0Fu)<<8));break;case 0xB000:irqReload=uint16_t((irqReload&0x0FFFu)|((d&0x0Fu)<<12));break;case 0xC000:irqControl=d;if(irqControl&2u)irqCounter=irqReload;irqActive=false;break;case 0xD000:irqActive=false;break;case 0xE000:selectedReg=uint8_t((d&7u)-1u);break;case 0xF000:
    if(selectedReg<4)prgRegs[selectedReg]=uint8_t((prgRegs[selectedReg]&0x10u)|(d&0x0Fu));else if(selectedReg==4)useRom=(d&4u)!=0;
    if(mapperNumber==56){switch(a&0xFC00u){case 0xF000u:{uint8_t b=uint8_t(a&3u);prgRegs[b]=uint8_t((d&0x10u)|(prgRegs[b]&0x0Fu));break;}case 0xF800u:mirrorMode=(d&1u)?1u:0u;break;case 0xFC00u:chr1[a&7u]=d;break;default:break;}}
    break;default:break;}m=0;return true;}
bool MapperKaiser202::ppuMapRead(uint16_t a,uint32_t&m){if(a>0x1FFFu)return false;uint16_t c=std::max<uint16_t>(1,uint16_t(chrBanks?chrBanks*8u:8u));m=uint32_t(chr1[a>>10]%c)*0x400u+(a&0x3FFu);return true;}
bool MapperKaiser202::ppuMapWrite(uint16_t a,uint32_t&m,uint8_t){return chrBanks==0&&ppuMapRead(a,m);}
void MapperKaiser202::cpuClock(){if(irqControl&2u){++irqCounter;if(irqCounter==0xFFFFu){irqCounter=irqReload;irqControl=uint8_t(irqControl&~2u);irqActive=true;}}}
void MapperKaiser202::saveState(std::vector<uint8_t>&o)const{using namespace ciphernes_state;write_u16(o,irqReload);write_u16(o,irqCounter);write_u8(o,irqControl);write_u8(o,selectedReg);write_bytes(o,prgRegs,sizeof(prgRegs));for(uint16_t v:chr1)write_u16(o,v);write_u8(o,mirrorMode);write_bool(o,useRom);write_bool(o,irqActive);}
bool MapperKaiser202::loadState(const uint8_t*&p,const uint8_t*e){using namespace ciphernes_state;if(!read_u16(p,e,irqReload)||!read_u16(p,e,irqCounter)||!read_u8(p,e,irqControl)||!read_u8(p,e,selectedReg)||!read_bytes(p,e,prgRegs,sizeof(prgRegs)))return false;for(uint16_t&v:chr1)if(!read_u16(p,e,v))return false;return read_u8(p,e,mirrorMode)&&read_bool(p,e,useRom)&&read_bool(p,e,irqActive);}
