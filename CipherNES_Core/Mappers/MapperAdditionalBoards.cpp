#include "MapperAdditionalBoards.h"
#include "../state_io.h"

#include <algorithm>
#include <cstring>

MapperAdditionalBoards::MapperAdditionalBoards(uint16_t p, uint16_t c, uint16_t m)
: Mapper(p, c), mapperNumber(m) { reset(); if (mapperNumber == 60) suppressInitialReset = true; }

uint16_t MapperAdditionalBoards::prgPageCount8() const {
    return std::max<uint16_t>(1u, uint16_t(prgBanks * 2u));
}

uint16_t MapperAdditionalBoards::chrPageCount1() const {
    if (chrBanks) return std::max<uint16_t>(1u, uint16_t(chrBanks * 8u));
    if (mapperNumber == 8) return 32u;   // FFE F3xxx: 32 KiB CHR-RAM
    if (mapperNumber == 13) return 16u;  // CPROM: 16 KiB CHR-RAM
    if (mapperNumber == 96) return 32u;  // Oeka Kids: eight 4 KiB pages
    return 8u;
}

uint32_t MapperAdditionalBoards::capabilities() const {
    uint32_t caps = 0;
    switch (mapperNumber) {
        case 8: case 13: case 42: case 43: case 50: case 51:
        case 58: case 61: case 62: case 93: case 96: case 117:
            caps |= MAPPER_CAP_MIRRORING;
            break;
        default: break;
    }
    switch (mapperNumber) {
        case 8: case 42: case 43: case 50: case 117:
            caps |= MAPPER_CAP_IRQ;
            break;
        default: break;
    }
    if (mapperNumber == 117 || mapperNumber == 96) caps |= MAPPER_CAP_PPU_ADDRESS_BUS;
    if (mapperNumber == 96) caps |= MAPPER_CAP_BUS_CONFLICT;
    return caps;
}

bool MapperAdditionalBoards::usesCpuClock() const {
    return mapperNumber == 8 || mapperNumber == 42 || mapperNumber == 43 || mapperNumber == 50;
}

void MapperAdditionalBoards::setPrg32(uint16_t bank32) {
    const uint16_t base = uint16_t(bank32 * 4u);
    for (uint8_t i = 0; i < 4; ++i) prg8[i] = uint16_t(base + i);
}

void MapperAdditionalBoards::setPrg16(int slot16, uint16_t bank16) {
    const uint16_t base = uint16_t(bank16 * 2u);
    prg8[slot16 * 2] = base;
    prg8[slot16 * 2 + 1] = uint16_t(base + 1u);
}

void MapperAdditionalBoards::setChr8(uint16_t bank8) {
    const uint16_t base = uint16_t(bank8 * 8u);
    for (uint8_t i = 0; i < 8; ++i) chr1[i] = uint16_t(base + i);
}

void MapperAdditionalBoards::reset() { reset(true); }

void MapperAdditionalBoards::reset(bool hardReset) {
    if (mapperNumber == 60u && !hardReset) {
        resetCounter = uint8_t((resetCounter + 1u) & 3u);
        setPrg16(0, resetCounter);
        setPrg16(1, resetCounter);
        setChr8(resetCounter);
        resetInitialized = true;
        suppressInitialReset = false;
        return;
    }
    std::memset(prg8, 0, sizeof(prg8));
    std::memset(chr1, 0, sizeof(chr1));
    std::memset(regs, 0, sizeof(regs));
    mirrorMode = 0xFF;
    irqCounter = 0;
    irqReload = 0;
    irqEnabled = irqEnabledAlt = irqActive = false;
    bank = mode = resetCounter = outerChr = innerChr = 0;
    lastPpuAddress = 0;
    swap = false;
    chrEnabled = true;
    a12Low = 0; lastA12 = false;

    const uint16_t pc = prgPageCount8();
    switch (mapperNumber) {
        case 8:
            setPrg32(0); setChr8(0); mirrorMode = 0xFF;
            break;
        case 13:
            setPrg32(0);
            for (uint8_t i=0;i<4;++i) { chr1[i]=i; chr1[4+i]=i; }
            mirrorMode = 1;
            break;
        case 42:
            prg8[0] = pc > 3u ? uint16_t(pc - 4u) : 0u;
            prg8[1] = pc > 2u ? uint16_t(pc - 3u) : 0u;
            prg8[2] = pc > 1u ? uint16_t(pc - 2u) : 0u;
            prg8[3] = uint16_t(pc - 1u);
            setChr8(0); mirrorMode = 1;
            break;
        case 43:
            prg8[0] = 1; prg8[1] = 0; prg8[2] = 4; prg8[3] = 9;
            setChr8(0); mirrorMode = 0xFF;
            break;
        case 46:
            setPrg32(0); setChr8(0);
            break;
        case 50:
            prg8[0] = 0x08; prg8[1] = 0x09; prg8[2] = 0x0A; prg8[3] = 0x0B;
            setChr8(0); mirrorMode = 0xFF;
            break;
        case 51:
            bank = 0; mode = 1; setChr8(0); update51();
            break;
        case 58:
        case 61:
        case 62:
            setPrg32(0); setChr8(0); mirrorMode = 1;
            break;
        case 60:
            setPrg16(0,0); setPrg16(1,0); setChr8(0);
            break;
        case 93:
            setPrg16(0,0); setPrg16(1, prgBanks ? uint16_t(prgBanks - 1u) : 0u);
            setChr8(0); chrEnabled = true; mirrorMode = 0xFF;
            break;
        case 94:
            setPrg16(0,0); setPrg16(1, prgBanks ? uint16_t(prgBanks - 1u) : 0u); setChr8(0);
            break;
        case 96:
            setPrg32(0); outerChr = innerChr = 0; mirrorMode = 0xFF; update96();
            break;
        case 107:
            setPrg32(0); setChr8(0);
            break;
        case 117:
            prg8[0] = pc > 3u ? uint16_t(pc - 4u) : 0u;
            prg8[1] = pc > 2u ? uint16_t(pc - 3u) : 0u;
            prg8[2] = pc > 1u ? uint16_t(pc - 2u) : 0u;
            prg8[3] = uint16_t(pc - 1u);
            mirrorMode = 1;
            break;
        case 120:
            prg8[0]=8; prg8[1]=9; prg8[2]=10; prg8[3]=11; setChr8(0); bank=0;
            break;
        case 133:
            setPrg32(0); setChr8(0);
            break;
        case 162:
            regs[0]=3; regs[1]=0; regs[2]=0; regs[3]=7; setChr8(0); update162();
            break;
        case 164:
            bank=0x0F; setPrg32(bank); setChr8(0);
            break;
        case 166:
            setChr8(0); update166();
            break;
        default:
            setPrg32(0); setChr8(0);
            break;
    }
    resetInitialized = (mapperNumber == 60u);
    suppressInitialReset = false;
}

void MapperAdditionalBoards::update51() {
    const uint16_t b = uint16_t(bank << 2);
    if (mode & 1u) {
        for (uint8_t i=0;i<4;++i) prg8[i]=uint16_t(b+i);
    } else {
        const uint16_t first=uint16_t(b|mode);
        prg8[0]=first; prg8[1]=uint16_t(first+1u);
        const uint16_t second=uint16_t(b|0x0Eu);
        prg8[2]=second; prg8[3]=uint16_t(second+1u);
    }
    mirrorMode = (mode == 3u) ? 0u : 1u;
}

void MapperAdditionalBoards::update96() {
    // 4 KiB pages: lower half follows the nametable-derived inner bank,
    // upper half is fixed to outer|3.
    const uint16_t lo4 = uint16_t(outerChr | innerChr);
    const uint16_t hi4 = uint16_t(outerChr | 3u);
    for (uint8_t i=0;i<4;++i) chr1[i]=uint16_t(lo4*4u+i);
    for (uint8_t i=0;i<4;++i) chr1[4+i]=uint16_t(hi4*4u+i);
}

void MapperAdditionalBoards::update162() {
    uint16_t p = 0;
    switch (regs[3] & 5u) {
        case 0: p = uint16_t((regs[0]&0x0Cu)|(regs[1]&2u)|((regs[2]&0x0Fu)<<4)); break;
        case 1: p = uint16_t((regs[0]&0x0Cu)|((regs[2]&0x0Fu)<<4)); break;
        case 4: p = uint16_t((regs[0]&0x0Eu)|((regs[1]>>1)&1u)|((regs[2]&0x0Fu)<<4)); break;
        default:p = uint16_t((regs[0]&0x0Fu)|((regs[2]&0x0Fu)<<4)); break;
    }
    setPrg32(p);
}

void MapperAdditionalBoards::update166() {
    const uint16_t outer = uint16_t(((regs[0]^regs[1])&0x10u)<<1);
    const uint16_t inner = uint16_t(regs[2]^regs[3]);
    if (regs[1]&8u) {
        const uint16_t p=uint16_t((outer|inner)&0xFEu); setPrg16(0,p); setPrg16(1,uint16_t(p+1u));
    } else if (regs[1]&4u) {
        setPrg16(0,0x1Fu); setPrg16(1,uint16_t(outer|inner));
    } else {
        setPrg16(0,uint16_t(outer|inner)); setPrg16(1,7u);
    }
}

void MapperAdditionalBoards::writeRegister(uint16_t addr, uint8_t data) {
    switch (mapperNumber) {
        case 8:
            if (addr==0x42FEu) mirrorMode=(data&0x10u)?4u:3u;
            else if (addr==0x42FFu) mirrorMode=(data&0x10u)?0u:1u;
            else if (addr==0x4501u) { irqEnabled=false; irqActive=false; }
            else if (addr==0x4502u) { irqCounter=uint16_t((irqCounter&0xFF00u)|data); irqActive=false; }
            else if (addr==0x4503u) { irqCounter=uint16_t((irqCounter&0x00FFu)|(uint16_t(data)<<8)); irqEnabled=true; irqActive=false; }
            else if (addr>=0x8000u) {
                const uint16_t first=uint16_t((data&0xF8u)>>2); prg8[0]=first; prg8[1]=uint16_t(first+1u); setChr8(uint16_t(data&7u));
            }
            break;
        case 13:
            if (addr>=0x8000u) {
                const uint16_t p4=uint16_t(data&3u); for(uint8_t i=0;i<4;++i) chr1[4+i]=uint16_t(p4*4u+i);
            }
            break;
        case 42:
            switch(addr&0xE003u){
                case 0x8000u:if(chrBanks)setChr8(uint16_t(data&0x0Fu));break;
                case 0xE000u:bank=uint8_t(data&0x0Fu);break;
                case 0xE001u:mirrorMode=(data&8u)?0u:1u;break;
                case 0xE002u:irqEnabled=(data==2u);if(!irqEnabled){irqActive=false;irqCounter=0;}break;
                default:break;
            }
            break;
        case 43: {
            static const uint8_t lut[8]={4,3,5,3,6,3,7,3};
            switch(addr&0xF1FFu){
                case 0x4022u:bank=lut[data&7u];prg8[2]=bank;break;
                case 0x4120u:swap=(data&1u)!=0;break;
                case 0x4122u:case 0x8122u:irqEnabled=(data&1u)!=0;irqActive=false;irqCounter=0;break;
                default:break;
            }
            prg8[3]=swap?8u:9u;
            break;
        }
        case 46:
            regs[addr<0x8000u?0:1]=data;
            setPrg32(uint16_t(((regs[0]&0x0Fu)<<1)|(regs[1]&1u)));
            setChr8(uint16_t(((regs[0]&0xF0u)>>1)|((regs[1]&0x70u)>>4)));
            break;
        case 50:
            switch(addr&0x4120u){
                case 0x4020u:prg8[2]=uint16_t((data&8u)|((data&1u)<<2)|((data&6u)>>1));break;
                case 0x4120u:irqEnabled=(data&1u)!=0;if(!irqEnabled){irqActive=false;irqCounter=0;}break;
            }
            break;
        case 51:
            if(addr<=0x7FFFu) mode=uint8_t(((data>>3)&2u)|((data>>1)&1u));
            else if(addr>=0xC000u&&addr<=0xDFFFu){bank=uint8_t(data&0x0Fu);mode=uint8_t(((data>>3)&2u)|(mode&1u));}
            else bank=uint8_t(data&0x0Fu);
            update51();
            break;
        case 58: {
            const uint16_t p=uint16_t(addr&7u);if(addr&0x40u){setPrg16(0,p);setPrg16(1,p);}else{setPrg16(0,uint16_t(p&6u));setPrg16(1,uint16_t((p&6u)+1u));}
            setChr8(uint16_t((addr>>3)&7u));mirrorMode=(addr&0x80u)?0u:1u;break;
        }
        case 61: {
            const uint16_t p=uint16_t(((addr&0x0Fu)<<1)|((addr>>5)&1u));if(addr&0x10u){setPrg16(0,p);setPrg16(1,p);}else{setPrg16(0,uint16_t(p&0xFEu));setPrg16(1,uint16_t((p&0xFEu)+1u));}
            mirrorMode=(addr&0x80u)?0u:1u;break;
        }
        case 62: {
            const uint16_t p=uint16_t(((addr&0x3F00u)>>8)|(addr&0x40u));const uint16_t c=uint16_t(((addr&0x1Fu)<<2)|(data&3u));if(addr&0x20u){setPrg16(0,p);setPrg16(1,p);}else{setPrg16(0,uint16_t(p&0xFEu));setPrg16(1,uint16_t((p&0xFEu)+1u));}setChr8(c);mirrorMode=(addr&0x80u)?0u:1u;break;
        }
        case 93:
            setPrg16(0,uint16_t((data>>4)&7u));chrEnabled=(data&1u)!=0;if(chrEnabled)setChr8(0);break;
        case 94:
            setPrg16(0,uint16_t((data>>2)&7u));break;
        case 96:
            setPrg32(uint16_t(data&3u));outerChr=uint8_t(data&4u);update96();break;
        case 107:
            setPrg32(uint16_t(data>>1));setChr8(data);break;
        case 117:
            if(addr>=0x8000u&&addr<=0x8003u)prg8[addr&3u]=data;
            else if(addr>=0xA000u&&addr<=0xA007u)chr1[addr&7u]=data;
            else if(addr==0xC001u)irqReload=data;
            else if(addr==0xC002u)irqActive=false;
            else if(addr==0xC003u){irqCounter=irqReload;irqEnabledAlt=true;}
            else if(addr==0xD000u)mirrorMode=(data&1u)?0u:1u;
            else if(addr==0xE000u){irqEnabled=(data&1u)!=0;irqActive=false;}
            break;
        case 120:
            if (addr == 0x41FFu) {
                bank = data;
            }
            break;
        case 133:
            if((addr&0x6100u)==0x4100u){setPrg32(uint16_t((data>>2)&1u));setChr8(uint16_t(data&3u));}break;
        case 162:
            if(addr>=0x5000u&&addr<=0x53FFu){regs[(addr>>8)&3u]=data;update162();}break;
        case 164:
            switch(addr&0x7300u){case 0x5000u:bank=uint8_t((bank&0xF0u)|(data&0x0Fu));setPrg32(bank);break;case 0x5100u:bank=uint8_t((bank&0x0Fu)|((data&0x0Fu)<<4));setPrg32(bank);break;default:break;}break;
        case 166:
            switch(addr&0xE000u){case 0x8000u:regs[0]=data&0x10u;break;case 0xA000u:regs[1]=data&0x1Cu;break;case 0xC000u:regs[2]=data&0x1Fu;break;case 0xE000u:regs[3]=data&0x1Fu;break;}update166();break;
        default: break;
    }
}

bool MapperAdditionalBoards::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (mapperNumber==42 && addr>=0x6000u && addr<=0x7FFFu) {
        mappedAddr=uint32_t(bank%prgPageCount8())*0x2000u+(addr&0x1FFFu);return true;
    }
    if (mapperNumber==43) {
        if(addr>=0x5000u&&addr<=0x5FFFu){mappedAddr=uint32_t(8u%prgPageCount8())*0x2000u+(addr&0x0FFFu);return true;}
        if(addr>=0x6000u&&addr<=0x7FFFu){const uint16_t p=swap?0u:2u;mappedAddr=uint32_t(p%prgPageCount8())*0x2000u+(addr&0x1FFFu);return true;}
    }
    if (mapperNumber==50 && addr>=0x6000u&&addr<=0x7FFFu) {mappedAddr=uint32_t(0x0Fu%prgPageCount8())*0x2000u+(addr&0x1FFFu);return true;}
    if (mapperNumber==51 && addr>=0x6000u&&addr<=0x7FFFu) {const uint16_t p=(mode&1u)?uint16_t(0x23u|(bank<<2)):uint16_t(0x2Fu|(bank<<2));mappedAddr=uint32_t(p%prgPageCount8())*0x2000u+(addr&0x1FFFu);return true;}
    if (mapperNumber==120 && addr>=0x6000u&&addr<=0x7FFFu) {mappedAddr=uint32_t(bank%prgPageCount8())*0x2000u+(addr&0x1FFFu);return true;}
    if(addr<0x8000u)return false;
    const uint8_t slot=uint8_t((addr-0x8000u)>>13);
    mappedAddr=uint32_t(prg8[slot]%prgPageCount8())*0x2000u+(addr&0x1FFFu);
    return true;
}

bool MapperAdditionalBoards::cpuMapWrite(uint16_t addr,uint32_t& mappedAddr,uint8_t data) {
    bool handled=false;
    switch(mapperNumber){
        case 8: handled=(addr>=0x42FEu&&addr<=0x4517u)||addr>=0x8000u;break;
        case 13:case 42:case 58:case 61:case 62:case 93:case 94:case 96:case 107:case 117:case 166:handled=addr>=0x8000u;break;
        case 43:handled=(addr>=0x4020u);break;
        case 46:case 51:handled=addr>=0x6000u;break;
        case 50:handled=addr>=0x4020u&&addr<=0x5FFFu;break;
        case 60:handled=false;break;
        case 120:handled=addr==0x41FFu;break;
        case 133:handled=addr>=0x4100u;break;
        case 162:case 164:handled=addr>=0x5000u&&addr<=0x5FFFu;break;
        default:handled=addr>=0x8000u;break;
    }
    if (!handled) {
        return false;
    }
    writeRegister(addr, data);
    mappedAddr = 0;
    return true;
}

bool MapperAdditionalBoards::ppuMapRead(uint16_t addr,uint32_t& mappedAddr) {
    if(addr>0x1FFFu||!chrEnabled)return false;
    const uint8_t slot=uint8_t(addr>>10);mappedAddr=uint32_t(chr1[slot]%chrPageCount1())*0x400u+(addr&0x3FFu);return true;
}

bool MapperAdditionalBoards::ppuMapWrite(uint16_t addr,uint32_t& mappedAddr,uint8_t) {
    if(chrBanks!=0)return false;
    return ppuMapRead(addr,mappedAddr);
}

void MapperAdditionalBoards::cpuClock() {
    switch(mapperNumber){
        case 8:
            if(irqEnabled){++irqCounter;if(irqCounter==0){irqActive=true;irqEnabled=false;}}break;
        case 42:
            if(irqEnabled){irqCounter=uint16_t((irqCounter+1u)&0x7FFFu);irqActive=irqCounter>=0x6000u;}break;
        case 43:
            if(irqEnabled&&++irqCounter>=4096u){irqEnabled=false;irqActive=true;}break;
        case 50:
            if(irqEnabled&&++irqCounter==0x1000u){irqEnabled=false;irqActive=true;}break;
        default:break;
    }
}

void MapperAdditionalBoards::ppuAddressBus(uint16_t addr) {
    if(mapperNumber==96){
        if((lastPpuAddress&0x3000u)!=0x2000u&&(addr&0x3000u)==0x2000u){innerChr=uint8_t((addr>>8)&3u);update96();}
        lastPpuAddress=addr;return;
    }
    if(mapperNumber==117){
        const bool hi=(addr&0x1000u)!=0;
        if(!hi){if(a12Low<0xFFu)++a12Low;}
        else{
            if(!lastA12&&a12Low>=8u&&irqEnabled&&irqEnabledAlt&&irqCounter){
                --irqCounter;
                if(irqCounter==0){irqActive=true;irqEnabledAlt=false;}
            }
            a12Low=0;
        }
        lastA12=hi;
    }
}

void MapperAdditionalBoards::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    for (uint16_t v : prg8) {
        write_u16(out, v);
    }
    for (uint16_t v : chr1) {
        write_u16(out, v);
    }
    write_bytes(out, regs, sizeof(regs));
    write_u8(out, mirrorMode);
    write_u16(out,irqCounter);write_u8(out,irqReload);write_bool(out,irqEnabled);write_bool(out,irqEnabledAlt);write_bool(out,irqActive);
    write_u8(out,bank);write_u8(out,mode);write_u8(out,resetCounter);write_u8(out,outerChr);write_u8(out,innerChr);write_u16(out,lastPpuAddress);write_bool(out,swap);write_bool(out,chrEnabled);write_u8(out,a12Low);write_bool(out,lastA12);write_bool(out,suppressInitialReset);write_bool(out,resetInitialized);
}

bool MapperAdditionalBoards::loadState(const uint8_t*& p,const uint8_t* end) {
    using namespace ciphernes_state;
    for (uint16_t& v : prg8) {
        if (!read_u16(p, end, v)) return false;
    }
    for (uint16_t& v : chr1) {
        if (!read_u16(p, end, v)) return false;
    }
    if (!read_bytes(p, end, regs, sizeof(regs)) || !read_u8(p, end, mirrorMode)) {
        return false;
    }
    return read_u16(p,end,irqCounter)&&read_u8(p,end,irqReload)&&read_bool(p,end,irqEnabled)&&read_bool(p,end,irqEnabledAlt)&&read_bool(p,end,irqActive)&&read_u8(p,end,bank)&&read_u8(p,end,mode)&&read_u8(p,end,resetCounter)&&read_u8(p,end,outerChr)&&read_u8(p,end,innerChr)&&read_u16(p,end,lastPpuAddress)&&read_bool(p,end,swap)&&read_bool(p,end,chrEnabled)&&read_u8(p,end,a12Low)&&read_bool(p,end,lastA12)&&read_bool(p,end,suppressInitialReset)&&read_bool(p,end,resetInitialized);
}
