#include "Mapper016.h"
#include "../state_io.h"
#include <algorithm>
#include <cstring>
#include <cctype>

Mapper016::Mapper016(uint16_t prgBanks,uint16_t chrBanks,uint8_t submapper,
                     bool eepromPresent,uint16_t board,bool externalEeprom)
    : Mapper(prgBanks,chrBanks),submapper(submapper),board(board),
      hasEeprom(board==157 || (board!=153 && submapper!=4 && eepromPresent)),
      hasExternal(board==157 && externalEeprom),
      internal(board==159?BandaiSerialEeprom::C01:BandaiSerialEeprom::C02),
      external(BandaiSerialEeprom::C01) { reset(); }

uint32_t Mapper016::capabilities() const {
    return MAPPER_CAP_MIRRORING|MAPPER_CAP_IRQ|
           (board==157 ? MAPPER_CAP_PPU_ADDRESS_BUS : 0u)|
           (hasEeprom ? MAPPER_CAP_CPU_DIRECT : 0u);
}

void Mapper016::reset() {
    prgBank=outerBank=0;
    std::memset(chrBank,0,sizeof(chrBank));
    mirrorMode=0xFF;wramEnabled=false;
    irqLatch=irqCounter=0;irqEnabled=irqActive=false;
    internalScl=externalScl=masterSda=true;
    std::fill(std::begin(datachClockRegister),std::end(datachClockRegister),true);
    datachPpuAddr=0;
    // Reset line resets serial transceiver, not stored chip contents or busy cycle.
    internal.resetBus();external.resetBus();
    barcodeWave.clear();barcodeStep=barcodeCycle=0;
}

bool Mapper016::cpuMapRead(uint16_t addr,uint32_t& mapped) {
    if(board==153 && addr>=0x6000 && addr<0x8000 && wramEnabled) {
        mapped=PRG_RAM_FLAG|uint32_t(addr&0x1FFFu);return true;
    }
    if(addr<0x8000)return false;
    const uint32_t count=std::max<uint16_t>(1,prgBanks);
    if(board==153) {
        const uint32_t local=addr<0xC000 ? (prgBank&0x0Fu) : 15u;
        const uint32_t bank=(uint32_t(outerBank)*16u+local)%count;
        mapped=bank*0x4000u+uint32_t(addr&0x3FFFu);
    } else {
        const uint32_t bank=addr<0xC000 ? uint32_t(prgBank)%count : count-1u;
        mapped=bank*0x4000u+uint32_t(addr&0x3FFFu);
    }
    return true;
}

bool Mapper016::cpuMapWrite(uint16_t addr,uint32_t& mapped,uint8_t data) {
    mapped=0;
    if(board==153 && addr>=0x6000 && addr<0x8000) {
        if(!wramEnabled)return false;
        mapped=PRG_RAM_FLAG|uint32_t(addr&0x1FFFu);return true;
    }
    if(board==16) {
        // FCG-1/2: $6000-$7FFF; LZ93D50: $8000-$FFFF.
        // Submapper 0 denotes an unspecified board, so allow either.
        if(addr<0x6000 || (submapper==4 && addr>=0x8000) ||
           (submapper==5 && addr<0x8000)) return false;
    } else if(addr<0x8000) return false;
    const uint8_t reg=uint8_t(addr&0x0Fu);
    if(reg<=7) {
        if(board==153 && reg<=3) outerBank=data&1u;
        else if(board==157 && reg<=3) {
            // Datach cartridge's X24C01 is clocked by CHR A13 output.
            datachClockRegister[reg]=(data&0x08u)!=0;
            externalScl=datachClockRegister[(datachPpuAddr>>10)&3u];
            if(hasExternal) external.lines(externalScl,masterSda);
        } else if(board==16 || board==159) chrBank[reg]=data;
        return true;
    }
    switch(reg) {
        case 8: prgBank=data&0x0Fu;break;
        case 9:
            switch(data&3u) {
                case 0:mirrorMode=1;break;
                case 1:mirrorMode=0;break;
                case 2:mirrorMode=3;break;
                case 3:mirrorMode=4;break;
            }break;
        case 10:
            irqActive=false;irqEnabled=(data&1u)!=0;
            if(board!=16 || submapper!=4)irqCounter=irqLatch;
            break;
        case 11:
            irqLatch=uint16_t((irqLatch&0xFF00u)|data);
            if(board==16 && submapper==4)irqCounter=irqLatch;
            break;
        case 12:
            irqLatch=uint16_t((uint16_t(data)<<8)|(irqLatch&0x00FFu));
            if(board==16 && submapper==4)irqCounter=irqLatch;
            break;
        case 13:
            if(board==153)wramEnabled=(data&0x20u)!=0;
            else if(hasEeprom) {
                internalScl=(data&0x20u)!=0;
                masterSda=(data&0x80u)!=0||(data&0x40u)!=0;
                internal.lines(internalScl,masterSda);
                if(hasExternal)external.lines(externalScl,masterSda);
            }
            break;
        default:break;
    }
    return true;
}

bool Mapper016::ppuMapRead(uint16_t addr,uint32_t& mapped) {
    if(addr>0x1FFF)return false;
    if(board==153 || board==157) {mapped=addr;return true;}
    const uint16_t count=chrBanks ? uint16_t(chrBanks)*8u : 8u;
    const uint8_t slot=uint8_t((addr>>10)&7u);
    mapped=uint32_t(uint16_t(chrBank[slot])%count)*0x400u+(addr&0x3FFu);
    return true;
}

bool Mapper016::ppuMapWrite(uint16_t addr,uint32_t& mapped,uint8_t) {
    if(addr>0x1FFF || chrBanks!=0)return false;
    if(board==153 || board==157) {mapped=addr;return true;}
    const uint8_t slot=uint8_t((addr>>10)&7u);
    mapped=uint32_t(chrBank[slot]%8u)*0x400u+(addr&0x3FFu);
    return true;
}

bool Mapper016::cpuReadDirect(uint16_t addr,uint8_t& data) {
    if(!hasEeprom||addr<0x6000||addr>=0x8000)return false;
    const bool high=internal.sdaHigh() && (!hasExternal||external.sdaHigh());
    // $6000 D4: wired-AND EEPROM SDA. $6000 D3: Datach scanner output.
    const bool sensor=board==157 && (barcodeStep>=barcodeWave.size() || barcodeWave[barcodeStep]==0);
    data=uint8_t((high?0x10u:0u)|(sensor?0x08u:0u));
    return true;
}

void Mapper016::ppuAddressBus(uint16_t addr) {
    if(board!=157)return;
    datachPpuAddr=addr;
    const bool scl=datachClockRegister[(addr>>10)&3u];
    if(hasExternal && scl!=externalScl) external.lines(scl,masterSda);
    externalScl=scl;
}

void Mapper016::clockDatach() {
    if(board!=157 || barcodeStep>=barcodeWave.size())return;
    if(++barcodeCycle>=1000u) {barcodeCycle=0;++barcodeStep;}
}
void Mapper016::cpuClock() {
    if(hasEeprom)internal.tick();
    if(hasExternal)external.tick();
    clockDatach();
    if(!irqEnabled)return;
    if(irqCounter==0) {irqCounter=0xFFFFu;irqActive=true;}
    else --irqCounter;
}

size_t Mapper016::persistentRamSize() const {
    if(board==157)return hasExternal ? external.size() : 0u;
    return hasEeprom ? internal.size() : 0u;
}
void Mapper016::copyPersistentRam(uint8_t* dst,size_t n) const {
    if(board==157) {if(hasExternal)external.copy(dst,n);}
    else if(hasEeprom)internal.copy(dst,n);
}
void Mapper016::restorePersistentRam(const uint8_t* src,size_t n) {
    if(board==157) {if(hasExternal)external.restore(src,n);}
    else if(hasEeprom)internal.restore(src,n);
}
bool Mapper016::persistentRamDirty() const {
    return board==157 ? hasExternal && external.dirty() : hasEeprom && internal.dirty();
}
void Mapper016::clearPersistentRamDirty() {
    if(board==157)external.clearDirty();else internal.clearDirty();
}
size_t Mapper016::sharedPersistentRamSize() const {return board==157 ? internal.size() : 0u;}
void Mapper016::copySharedPersistentRam(uint8_t* dst,size_t n) const {if(board==157)internal.copy(dst,n);}
void Mapper016::restoreSharedPersistentRam(const uint8_t* src,size_t n) {if(board==157)internal.restore(src,n);}
bool Mapper016::sharedPersistentRamDirty() const {return board==157&&internal.dirty();}
void Mapper016::clearSharedPersistentRamDirty() {if(board==157)internal.clearDirty();}

// Inject an optical barcode waveform into the scanner. This is a digital
// UPC-A/EAN-13/EAN-8 source, not an analogue swipe-speed model or GUI input.
bool Mapper016::queueDatachBarcode(const std::string& digits) {
    if(board!=157 || (digits.size()!=8 && digits.size()!=12 && digits.size()!=13))return false;
    for(char ch:digits)if(ch<'0'||ch>'9')return false;
    // EAN/UPC modulo-10 check digit, weights 3,1 from right to left.
    int sum=0,weight=3;
    for(int i=int(digits.size())-2;i>=0;--i) {sum+=(digits[size_t(i)]-'0')*weight;weight=(weight==3?1:3);}
    if((10-sum%10)%10!=digits.back()-'0')return false;
    static const char* leftA[]={"0001101","0011001","0010011","0111101","0100011","0110001","0101111","0111011","0110111","0001011"};
    static const char* leftB[]={"0100111","0110011","0011011","0100001","0011101","0111001","0000101","0010001","0001001","0010111"};
    static const char* right[]={"1110010","1100110","1101100","1000010","1011100","1001110","1010000","1000100","1001000","1110100"};
    static const char* parity[]={"AAAAAA","AABABB","AABBAB","AABBBA","ABAABB","ABBAAB","ABBBAA","ABABAB","ABABBA","ABBABA"};
    std::string code=digits;
    if(code.size()==12)code="0"+code;
    std::string modules(11,'0');modules+="101";
    if(code.size()==13) {
        for(int i=1;i<=6;i++)modules+=parity[code[0]-'0'][i-1]=='A'?leftA[code[size_t(i)]-'0']:leftB[code[size_t(i)]-'0'];
        modules+="01010";
        for(int i=7;i<=12;i++)modules+=right[code[size_t(i)]-'0'];
    } else {
        for(int i=0;i<4;i++)modules+=leftA[code[size_t(i)]-'0'];
        modules+="01010";
        for(int i=4;i<8;i++)modules+=right[code[size_t(i)]-'0'];
    }
    modules+="101";modules+=std::string(7,'0');
    barcodeWave.clear();barcodeWave.reserve(modules.size());
    for(char ch:modules)barcodeWave.push_back(uint8_t(ch=='1'));
    barcodeStep=barcodeCycle=0;
    return true;
}

void Mapper016::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    write_u16(out,board);write_u8(out,submapper);
    write_bool(out,hasEeprom);write_bool(out,hasExternal);
    write_u8(out,prgBank);write_u8(out,outerBank);
    for(uint8_t b:chrBank)write_u8(out,b);
    write_u8(out,mirrorMode);write_bool(out,wramEnabled);
    write_bool(out,internalScl);write_bool(out,externalScl);write_bool(out,masterSda);
    for(bool clock:datachClockRegister)write_bool(out,clock);
    write_u16(out,datachPpuAddr);
    write_u16(out,irqLatch);write_u16(out,irqCounter);
    write_bool(out,irqEnabled);write_bool(out,irqActive);
    if(hasEeprom)internal.save(out);
    if(hasExternal)external.save(out);
    write_u32(out,barcodeStep);write_u32(out,barcodeCycle);
    write_u16(out,uint16_t(barcodeWave.size()));
    for(uint8_t v:barcodeWave)write_u8(out,v);
}
bool Mapper016::loadState(const uint8_t*& p,const uint8_t* end) {
    using namespace ciphernes_state;
    uint16_t storedBoard=0;uint8_t storedSubmapper=0;
    bool storedEeprom=false,storedExternal=false;
    if(!read_u16(p,end,storedBoard)||!read_u8(p,end,storedSubmapper)||
       !read_bool(p,end,storedEeprom)||!read_bool(p,end,storedExternal) ||
       storedBoard!=board||storedSubmapper!=submapper||
       storedEeprom!=hasEeprom||storedExternal!=hasExternal)return false;
    if(!read_u8(p,end,prgBank)||!read_u8(p,end,outerBank))return false;
    for(uint8_t& b:chrBank)if(!read_u8(p,end,b))return false;
    if(!read_u8(p,end,mirrorMode)||!read_bool(p,end,wramEnabled)||
       !read_bool(p,end,internalScl)||!read_bool(p,end,externalScl)||
       !read_bool(p,end,masterSda))return false;
    for(bool& clock:datachClockRegister)if(!read_bool(p,end,clock))return false;
    if(!read_u16(p,end,datachPpuAddr)||!read_u16(p,end,irqLatch)||
       !read_u16(p,end,irqCounter)||!read_bool(p,end,irqEnabled)||
       !read_bool(p,end,irqActive))return false;
    if(hasEeprom&&!internal.load(p,end))return false;
    if(hasExternal&&!external.load(p,end))return false;
    uint16_t n=0;
    if(!read_u32(p,end,barcodeStep)||!read_u32(p,end,barcodeCycle)||
       !read_u16(p,end,n)||n>128)return false;
    barcodeWave.resize(n);
    for(uint8_t& v:barcodeWave)if(!read_u8(p,end,v)||v>1)return false;
    return barcodeStep<=barcodeWave.size() && barcodeCycle<1000u;
}
