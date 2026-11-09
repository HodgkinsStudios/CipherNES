#pragma once
// Xicor EEPROM used on the Bandai LZ93D50 family. Original X24C01 and
// X24C02 parts have four-byte page writes and different command prefixes.
#include "../state_io.h"
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

class BandaiSerialEeprom {
public:
    enum Kind : uint8_t { C01 = 1, C02 = 2 };
    // Datasheets specify approximately 5ms typical at 25C, up to 10ms.
    // 8950 NTSC CPU cycles is a deterministic *typical* duration, not
    // temperature/voltage-accurate analogue simulation.
    static constexpr uint32_t kWriteCycles = 8950;
    explicit BandaiSerialEeprom(Kind type=C02) : type_(type) { bytes_.fill(0xFF); resetBus(); }
    size_t size() const { return type_ == C01 ? 128u : 256u; }
    bool dirty() const { return dirty_; }
    void clearDirty() { dirty_ = false; }
    uint32_t busyCycles() const { return busy_; }
    void tick() { if(busy_) --busy_; }
    void resetBus() {
        scl_=sda_=true; phase_=Idle; next_=Idle;
        ackLow_=ackClockHigh_=masterAck_=false;
        shift_=bits_=address_=0; pageMask_=pageBase_=0;
    }
    void copy(uint8_t* dst,size_t n) const { if(dst && n>=size()) std::memcpy(dst,bytes_.data(),size()); }
    void restore(const uint8_t* src,size_t n) { if(src && n>=size()){std::memcpy(bytes_.data(),src,size());dirty_=false;busy_=0;resetBus();} }
    bool sdaHigh() const {
        const uint8_t bit = uint8_t(scl_ && bits_ ? bits_-1u : bits_);
        bool pulled = (phase_==Ack && ackLow_) ||
            (phase_==Send && bit<8u && ((bytes_[address_]>>(7-bit))&1u)==0);
        return sda_ && !pulled;
    }
    void lines(bool newClock,bool newSda) {
        // Host's changing SDA with SCL high means START or STOP; a repeated
        // START aborts an uncommitted page without touching the stored bytes.
        if(newClock && scl_ && newSda!=sda_) {
            if(!newSda) { pageMask_=0; phase_=Control; bits_=shift_=0; ackClockHigh_=false; }
            else { commit(); phase_=Idle; ackClockHigh_=false; }
        }
        if(!scl_ && newClock) {
            if(phase_==Control || phase_==Word || phase_==Write) {
                shift_=uint8_t((shift_<<1)|uint8_t(newSda));
                if(++bits_==8) received(shift_);
            } else if(phase_==Ack) ackClockHigh_=true;
            else if(phase_==Send) ++bits_;
            else if(phase_==MasterAck) { masterAck_=!newSda; ackClockHigh_=true; }
        } else if(scl_ && !newClock) {
            if(phase_==Ack && ackClockHigh_) { phase_=next_; bits_=shift_=0; ackClockHigh_=false; }
            else if(phase_==Send && bits_==8) {
                address_=uint8_t((address_+1u)%size());
                phase_=MasterAck; ackClockHigh_=false;
            } else if(phase_==MasterAck && ackClockHigh_) {
                phase_=masterAck_ ? Send : Ignore; bits_=0; ackClockHigh_=false;
            }
        }
        scl_=newClock; sda_=newSda;
    }
    void save(std::vector<uint8_t>& out) const {
        using namespace ciphernes_state;
        for(size_t i=0;i<size();++i) write_u8(out,bytes_[i]);
        write_bool(out,dirty_); write_u32(out,busy_);
        write_bool(out,scl_); write_bool(out,sda_);
        write_u8(out,uint8_t(phase_)); write_u8(out,uint8_t(next_));
        write_bool(out,ackLow_); write_bool(out,ackClockHigh_); write_bool(out,masterAck_);
        write_u8(out,shift_); write_u8(out,bits_); write_u8(out,address_);
        for(uint8_t b:page_)write_u8(out,b);
        write_u8(out,pageMask_);write_u8(out,pageBase_);
    }
    bool load(const uint8_t*& p,const uint8_t* end) {
        using namespace ciphernes_state;
        for(size_t i=0;i<size();++i) if(!read_u8(p,end,bytes_[i]))return false;
        if(!read_bool(p,end,dirty_) || !read_u32(p,end,busy_) ||
           !read_bool(p,end,scl_) || !read_bool(p,end,sda_))return false;
        uint8_t ph=0,nx=0;
        if(!read_u8(p,end,ph)||!read_u8(p,end,nx)||ph>Ignore||nx>Ignore)return false;
        phase_=Phase(ph);next_=Phase(nx);
        if(!read_bool(p,end,ackLow_)||!read_bool(p,end,ackClockHigh_)||
           !read_bool(p,end,masterAck_)||!read_u8(p,end,shift_)||
           !read_u8(p,end,bits_)||!read_u8(p,end,address_))return false;
        for(uint8_t& b:page_) if(!read_u8(p,end,b))return false;
        return read_u8(p,end,pageMask_) && read_u8(p,end,pageBase_) &&
               bits_<=8 && address_<size() && pageMask_<=15 && busy_<=kWriteCycles;
    }
private:
    enum Phase : uint8_t { Idle,Control,Word,Write,Ack,Send,MasterAck,Ignore };
    Kind type_;
    std::array<uint8_t,256> bytes_{};
    bool dirty_=false;
    uint32_t busy_=0;
    bool scl_=true,sda_=true,ackLow_=false,ackClockHigh_=false,masterAck_=false;
    Phase phase_=Idle,next_=Idle;
    uint8_t shift_=0,bits_=0,address_=0;
    uint8_t page_[4]{},pageMask_=0,pageBase_=0;
    void commit() {
        if(pageMask_) {
            for(unsigned i=0;i<4;i++) if(pageMask_&(1u<<i)) {
                uint8_t a=uint8_t(pageBase_|i);
                if(bytes_[a]!=page_[i]) {bytes_[a]=page_[i];dirty_=true;}
            }
            busy_=kWriteCycles;
        }
        pageMask_=0;
    }
    void received(uint8_t value) {
        ackLow_=true;
        switch(phase_) {
            case Control:
                if(busy_) {ackLow_=false;next_=Ignore;break;}
                if(type_==C02) {
                    if((value&0xFEu)!=0xA0u) {ackLow_=false;next_=Ignore;}
                    else next_=(value&1u)?Send:Word;
                } else {
                    // X24C01: command = seven-bit memory address + R/W,
                    // NOT a conventional 0xA0/0xA1 device address.
                    address_=uint8_t(value>>1);
                    next_=(value&1u)?Send:Write;
                    pageBase_=uint8_t(address_&0xFCu);
                    pageMask_=0;
                }
                break;
            case Word:
                address_=value;pageBase_=uint8_t(value&0xFCu);
                pageMask_=0;next_=Write;break;
            case Write:
                page_[address_&3u]=value;
                pageMask_=uint8_t(pageMask_|(1u<<(address_&3u)));
                address_=uint8_t(pageBase_|((address_+1u)&3u));
                next_=Write;break;
            default:ackLow_=false;next_=Ignore;break;
        }
        phase_=Ack;ackClockHigh_=false;bits_=shift_=0;
    }
};
