#include "MapperJY.h"
#include "../state_io.h"
#include <algorithm>
#include <cstring>

MapperJY::MapperJY(uint16_t p, uint16_t c, uint16_t m)
    : Mapper(p,c), mapperNumber(m) { reset(); }

uint32_t MapperJY::capabilities() const {
    return MAPPER_CAP_IRQ | MAPPER_CAP_MIRRORING | MAPPER_CAP_NAMETABLE |
           MAPPER_CAP_CPU_DIRECT | MAPPER_CAP_PPU_ADDRESS_BUS |
           MAPPER_CAP_PPU_FETCH_HOOKS;
}

uint32_t MapperJY::prgCount8() const { return std::max<uint32_t>(1u, uint32_t(prgBanks) * 2u); }
uint32_t MapperJY::chrCount1() const { return std::max<uint32_t>(1u, chrBanks ? uint32_t(chrBanks) * 8u : 8u); }

bool MapperJY::advancedNametables() const {
    return mapperNumber == 211u || (mapperNumber != 90u && advancedNtControl);
}

void MapperJY::reset() {
    std::memset(prgRegs,0,sizeof(prgRegs));
    std::memset(chrLow,0,sizeof(chrLow));
    std::memset(chrHigh,0,sizeof(chrHigh));
    std::memset(ntLow,0,sizeof(ntLow));
    std::memset(ntHigh,0,sizeof(ntHigh));
    ntRam.fill(0);
    prgMode=0; enablePrg6000=false; chrMode=0; chrBlockMode=false;
    chrBlock=0; mirrorChr=false; mirroringReg=0; advancedNtControl=false;
    disableNtRam=false; ntRamSelectBit=0; mirrorMode=1;
    irqEnabled=false; irqSource=CpuClock; irqDirection=0; irqFunkyMode=false;
    irqFunkyReg=0; irqSmallPrescaler=false; irqPrescaler=0; irqCounter=0;
    irqXor=0; irqActive=false; lastPpuAddr=0;
    multiply1=multiply2=regRam=0;
    updateState();
}

uint8_t MapperJY::invertPrg(uint8_t v) const {
    if ((prgMode & 3u) != 3u) return v;
    return uint8_t(((v & 0x01u) << 6) | ((v & 0x02u) << 4) |
                   ((v & 0x04u) << 2) | ((v & 0x10u) >> 2) |
                   ((v & 0x20u) >> 4) | ((v & 0x40u) >> 6));
}

void MapperJY::updatePrg() {
    uint16_t r[4] = { invertPrg(prgRegs[0]), invertPrg(prgRegs[1]),
                      invertPrg(prgRegs[2]), invertPrg(prgRegs[3]) };
    switch (prgMode & 3u) {
        case 0: {
            const uint16_t bank32 = (prgMode & 4u) ? r[3] : 0x3Cu;
            for (uint8_t i=0;i<4;i++) prgPage[i]=uint16_t(bank32*4u+i);
            break;
        }
        case 1: {
            const uint16_t lower = r[1];
            const uint16_t upper = (prgMode & 4u) ? r[3] : 0x3Eu;
            prgPage[0]=uint16_t(lower*2u); prgPage[1]=uint16_t(lower*2u+1u);
            prgPage[2]=uint16_t(upper*2u); prgPage[3]=uint16_t(upper*2u+1u);
            break;
        }
        default:
            prgPage[0]=r[0]; prgPage[1]=r[1]; prgPage[2]=r[2];
            prgPage[3]=(prgMode&4u)?r[3]:0x3Fu;
            break;
    }
}

uint16_t MapperJY::getChrReg(uint8_t index) const {
    if (chrMode >= 2u && mirrorChr && (index==2u || index==3u)) index=uint8_t(index-2u);
    if (chrBlockMode) {
        uint8_t mask, shift;
        switch (chrMode) {
            default: case 0: mask=0x1F; shift=5; break;
            case 1: mask=0x3F; shift=6; break;
            case 2: mask=0x7F; shift=7; break;
            case 3: mask=0xFF; shift=8; break;
        }
        return uint16_t((chrLow[index]&mask) | (uint16_t(chrBlock)<<shift));
    }
    return uint16_t(chrLow[index] | (uint16_t(chrHigh[index])<<8));
}

void MapperJY::updateChr() {
    uint16_t r[8]; for(uint8_t i=0;i<8;i++) r[i]=getChrReg(i);
    switch(chrMode) {
        case 0:
            for(uint8_t i=0;i<8;i++) chrPage[i]=uint16_t(r[0]*8u+i);
            break;
        case 1:
            for(uint8_t i=0;i<4;i++) chrPage[i]=uint16_t(r[0]*4u+i);
            for(uint8_t i=0;i<4;i++) chrPage[4u+i]=uint16_t(r[4]*4u+i);
            break;
        case 2:
            for(uint8_t q=0;q<4;q++) { chrPage[q*2]=uint16_t(r[q*2]*2u); chrPage[q*2+1]=uint16_t(r[q*2]*2u+1u); }
            break;
        default:
            for(uint8_t i=0;i<8;i++) chrPage[i]=r[i];
            break;
    }
}

void MapperJY::updateMirroring() {
    if (advancedNametables()) { mirrorMode=0xFF; return; }
    switch(mirroringReg&3u) {
        case 0: mirrorMode=1; break;
        case 1: mirrorMode=0; break;
        case 2: mirrorMode=3; break;
        default: mirrorMode=4; break;
    }
}
void MapperJY::updateState(){updatePrg();updateChr();updateMirroring();}

bool MapperJY::cpuReadDirect(uint16_t a,uint8_t& d) {
    switch(a&0xF803u) {
        case 0x5000u: d=0; return true;
        case 0x5800u: d=uint8_t(uint16_t(multiply1)*multiply2); return true;
        case 0x5801u: d=uint8_t((uint16_t(multiply1)*multiply2)>>8); return true;
        case 0x5803u: d=regRam; return true;
        default: return false;
    }
}

bool MapperJY::cpuMapRead(uint16_t a,uint32_t& m) {
    if(a>=0x6000u&&a<=0x7FFFu) {
        if(!enablePrg6000) return false;
        uint32_t page;
        switch(prgMode&3u) {
            case 0: page=uint32_t(invertPrg(prgRegs[3]))*4u+3u; break;
            case 1: page=uint32_t(invertPrg(prgRegs[3]))*2u+1u; break;
            default: page=invertPrg(prgRegs[3]); break;
        }
        m=(page%prgCount8())*0x2000u+(a&0x1FFFu); return true;
    }
    if(a<0x8000u)return false;
    const uint8_t slot=uint8_t((a-0x8000u)>>13);
    m=(uint32_t(prgPage[slot])%prgCount8())*0x2000u+(a&0x1FFFu); return true;
}

bool MapperJY::cpuMapWrite(uint16_t a,uint32_t& m,uint8_t d) {
    m=0;
    if(a<0x8000u) {
        if(a<0x5000u||a>0x5FFFu)return false;
        switch(a&0xF803u) {
            case 0x5800u:multiply1=d;break;
            case 0x5801u:multiply2=d;break;
            case 0x5803u:regRam=d;break;
            default:break;
        }
        return true;
    }
    switch(a&0xF007u) {
        case 0x8000u:case 0x8001u:case 0x8002u:case 0x8003u:
        case 0x8004u:case 0x8005u:case 0x8006u:case 0x8007u: prgRegs[a&3u]=uint8_t(d&0x7Fu);break;
        case 0x9000u:case 0x9001u:case 0x9002u:case 0x9003u:
        case 0x9004u:case 0x9005u:case 0x9006u:case 0x9007u: chrLow[a&7u]=d;break;
        case 0xA000u:case 0xA001u:case 0xA002u:case 0xA003u:
        case 0xA004u:case 0xA005u:case 0xA006u:case 0xA007u: chrHigh[a&7u]=d;break;
        case 0xB000u:case 0xB001u:case 0xB002u:case 0xB003u: ntLow[a&3u]=d;break;
        case 0xB004u:case 0xB005u:case 0xB006u:case 0xB007u: ntHigh[a&3u]=d;break;
        case 0xC000u: irqEnabled=(d&1u)!=0; if(!irqEnabled)irqActive=false; break;
        case 0xC001u: irqDirection=uint8_t((d>>6)&3u); irqFunkyMode=(d&8u)!=0; irqSmallPrescaler=(d&4u)!=0; irqSource=IrqSource(d&3u); break;
        case 0xC002u: irqEnabled=false; irqActive=false; break;
        case 0xC003u: irqEnabled=true; break;
        case 0xC004u: irqPrescaler=uint8_t(d^irqXor); break;
        case 0xC005u: irqCounter=uint8_t(d^irqXor); break;
        case 0xC006u: irqXor=d; break;
        case 0xC007u: irqFunkyReg=d; break;
        case 0xD000u: prgMode=uint8_t(d&7u); chrMode=uint8_t((d>>3)&3u); advancedNtControl=(d&0x20u)!=0; disableNtRam=(d&0x40u)!=0; enablePrg6000=(d&0x80u)!=0; break;
        case 0xD001u: mirroringReg=uint8_t(d&3u); break;
        case 0xD002u: ntRamSelectBit=uint8_t(d&0x80u); break;
        case 0xD003u: mirrorChr=(d&0x80u)!=0; chrBlockMode=(d&0x20u)==0; chrBlock=uint8_t(((d&0x18u)>>2)|(d&1u)); break;
        default: break;
    }
    updateState(); return true;
}

bool MapperJY::ppuMapRead(uint16_t a,uint32_t& m) {
    a&=0x3FFFu;
    if(a<=0x1FFFu){const uint8_t s=uint8_t(a>>10);m=(uint32_t(chrPage[s])%chrCount1())*0x400u+(a&0x3FFu);if(chrBanks==0u)m|=CHR_RAM_FLAG;return true;}
    if(a>=0x2000u&&a<=0x3EFFu&&advancedNametables()) {
        const uint16_t o=uint16_t((a-0x2000u)&0x0FFFu);const uint8_t t=uint8_t(o>>10);
        const bool chrBacked=disableNtRam || ((ntLow[t]&0x80u)!=(ntRamSelectBit&0x80u));
        if(chrBacked){const uint32_t page=uint32_t(ntLow[t])|(uint32_t(ntHigh[t])<<8);m=(page%chrCount1())*0x400u+(o&0x3FFu);return true;}
    }
    return false;
}
bool MapperJY::ppuMapWrite(uint16_t a,uint32_t& m,uint8_t){if(chrBanks!=0u||a>0x1FFFu)return false;const uint8_t s=uint8_t(a>>10);m=CHR_RAM_FLAG|((uint32_t(chrPage[s])%chrCount1())*0x400u+(a&0x3FFu));return true;}

bool MapperJY::nametableRead(uint16_t a,uint8_t& d) {
    if(!advancedNametables()) return false;
    const uint16_t o=uint16_t((a-0x2000u)&0x0FFFu);
    const uint8_t t=uint8_t(o>>10);
    if(disableNtRam||((ntLow[t]&0x80u)!=(ntRamSelectBit&0x80u)))return false;
    d=ntRam[uint16_t(ntLow[t]&1u)*0x400u+(o&0x3FFu)];return true;
}
bool MapperJY::nametableWrite(uint16_t a,uint8_t d) {
    if(!advancedNametables()) return false;
    const uint16_t o=uint16_t((a-0x2000u)&0x0FFFu);
    const uint8_t t=uint8_t(o>>10);
    ntRam[uint16_t(ntLow[t]&1u)*0x400u+(o&0x3FFu)]=d;return true;
}

void MapperJY::tickIrq(){
    bool clock=false;const uint8_t mask=irqSmallPrescaler?7u:0xFFu;uint8_t p=uint8_t(irqPrescaler&mask);
    if(irqDirection==1u){p=uint8_t(p+1u);if((p&mask)==0u)clock=true;}
    else if(irqDirection==2u){p=uint8_t(p-1u);if(p==0u)clock=true;}
    irqPrescaler=uint8_t((irqPrescaler&uint8_t(~mask))|(p&mask));
    if(clock){if(irqDirection==1u){irqCounter=uint8_t(irqCounter+1u);if(irqCounter==0u&&irqEnabled)irqActive=true;}else if(irqDirection==2u){irqCounter=uint8_t(irqCounter-1u);if(irqCounter==0xFFu&&irqEnabled)irqActive=true;}}
}
void MapperJY::cpuClockEx(uint64_t,uint16_t,bool write,bool){if(irqSource==CpuClock||(irqSource==CpuWrite&&write))tickIrq();}
void MapperJY::ppuAddressBus(uint16_t a){if(irqSource==PpuA12Rise&&(a&0x1000u)&&!(lastPpuAddr&0x1000u))tickIrq();lastPpuAddr=a;}
void MapperJY::beforePpuFetch(uint16_t,uint8_t,int16_t,int16_t,uint64_t){if(irqSource==PpuRead)tickIrq();}

void MapperJY::saveState(std::vector<uint8_t>&o)const{using namespace ciphernes_state;
    write_bytes(o,prgRegs,sizeof(prgRegs));write_bytes(o,chrLow,sizeof(chrLow));write_bytes(o,chrHigh,sizeof(chrHigh));write_bytes(o,ntLow,sizeof(ntLow));write_bytes(o,ntHigh,sizeof(ntHigh));
    write_u8(o,prgMode);write_bool(o,enablePrg6000);write_u8(o,chrMode);write_bool(o,chrBlockMode);write_u8(o,chrBlock);write_bool(o,mirrorChr);write_u8(o,mirroringReg);write_bool(o,advancedNtControl);write_bool(o,disableNtRam);write_u8(o,ntRamSelectBit);write_u8(o,mirrorMode);write_bytes(o,ntRam.data(),ntRam.size());
    write_bool(o,irqEnabled);write_u8(o,uint8_t(irqSource));write_u8(o,irqDirection);write_bool(o,irqFunkyMode);write_u8(o,irqFunkyReg);write_bool(o,irqSmallPrescaler);write_u8(o,irqPrescaler);write_u8(o,irqCounter);write_u8(o,irqXor);write_bool(o,irqActive);write_u16(o,lastPpuAddr);write_u8(o,multiply1);write_u8(o,multiply2);write_u8(o,regRam);
}
bool MapperJY::loadState(const uint8_t*&p,const uint8_t*e){using namespace ciphernes_state;uint8_t src=0;
    if(!read_bytes(p,e,prgRegs,sizeof(prgRegs))||!read_bytes(p,e,chrLow,sizeof(chrLow))||!read_bytes(p,e,chrHigh,sizeof(chrHigh))||!read_bytes(p,e,ntLow,sizeof(ntLow))||!read_bytes(p,e,ntHigh,sizeof(ntHigh)))return false;
    if(!read_u8(p,e,prgMode)||!read_bool(p,e,enablePrg6000)||!read_u8(p,e,chrMode)||!read_bool(p,e,chrBlockMode)||!read_u8(p,e,chrBlock)||!read_bool(p,e,mirrorChr)||!read_u8(p,e,mirroringReg)||!read_bool(p,e,advancedNtControl)||!read_bool(p,e,disableNtRam)||!read_u8(p,e,ntRamSelectBit)||!read_u8(p,e,mirrorMode)||!read_bytes(p,e,ntRam.data(),ntRam.size()))return false;
    if(!read_bool(p,e,irqEnabled)||!read_u8(p,e,src)||src>3u||!read_u8(p,e,irqDirection)||!read_bool(p,e,irqFunkyMode)||!read_u8(p,e,irqFunkyReg)||!read_bool(p,e,irqSmallPrescaler)||!read_u8(p,e,irqPrescaler)||!read_u8(p,e,irqCounter)||!read_u8(p,e,irqXor)||!read_bool(p,e,irqActive)||!read_u16(p,e,lastPpuAddr)||!read_u8(p,e,multiply1)||!read_u8(p,e,multiply2)||!read_u8(p,e,regRam))return false;
    irqSource=IrqSource(src);updateState();return true;
}
