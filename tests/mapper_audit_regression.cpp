// Offline mapper sanity/regression suite; no copyrighted ROMs required.
// Build and run using tests/run_mapper_audit.sh.
#include "Mappers/MapperCommonBoards.h"
#include "Mappers/MapperMmc3Variant.h"
#include "Mappers/Mapper004.h"
#include "Mappers/MapperVrc6.h"
#include "state_io.h"
#include "Mappers/Mapper016.h"
#include "Mappers/Mapper001.h"
#include "Mappers/Mapper000.h"
#include "Mappers/Mapper005.h"
#include "Mappers/Mapper019.h"
#include "Mappers/Mapper064.h"
#include "Mappers/Mapper028.h"
#include "Mappers/Mapper032.h"
#include "Mappers/Mapper069.h"
#include "Mappers/Mapper067.h"
#include "Mappers/Mapper080.h"
#include "Mappers/Mapper082.h"
#include "Mappers/Mapper099.h"
#include "Mappers/Mapper007.h"
#include "cartridge.h"
#include "bus.h"
#include <cmath>
#include <memory>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static int failures=0, assertions=0;
#define CHECK(x) do { ++assertions; if(!(x)){ ++failures; std::cerr<<"FAIL "<<__FILE__<<":"<<__LINE__<<" "<<#x<<"\n"; }} while(0)

template<class T> static void roundtrip(T& m, uint16_t writeAddress, uint8_t value){
    std::vector<uint8_t> before,after;
    m.saveState(before);
    uint32_t map=0;
    m.cpuMapWrite(writeAddress,map,value);
    const uint8_t* cur=before.data();
    CHECK(m.loadState(cur, cur+before.size()));
    CHECK(cur==before.data()+before.size());
    m.saveState(after);
    CHECK(after==before);
}

// CP010: independently spec-derived tests for the existing 242, 243 and 246
// board modes.  No commercial cartridge data is used.
static void focusedDiscreteBoards() {
    uint32_t mapped=0;
    uint8_t value=0;
    MapperCommonBoards unL43272(32,0,242); // 512 KiB PRG, 8 KiB CHR RAM
    CHECK(unL43272.cpuMapRead(0x8000,mapped) && mapped == 0u);
    CHECK(unL43272.cpuMapRead(0xC000,mapped) && mapped == 0x4000u);
    CHECK(unL43272.ppuMapWrite(0,mapped,0x56));
    // UNROM fixed bank #7 in outer 128 KiB region (L=1,O=0).
    CHECK(unL43272.cpuMapWrite(0x8204,mapped,0xA9));
    CHECK(unL43272.cpuMapRead(0x8000,mapped) && mapped == 1u*0x4000u);
    CHECK(unL43272.cpuMapRead(0xC000,mapped) && mapped == 7u*0x4000u);
    CHECK(unL43272.ppuMapWrite(0,mapped,0));
    // A5 selects outer PRG bank region. S=1 forces even 16 KiB bank.
    CHECK(unL43272.cpuMapWrite(0x8025,mapped,0));
    CHECK(unL43272.cpuMapRead(0x8000,mapped) && mapped == 8u*0x4000u);
    CHECK(unL43272.cpuMapRead(0xC000,mapped) && mapped == 8u*0x4000u);
    // O=1 S=0 => 16 KiB switchable bank mirrored, CHR-RAM write inhibited.
    CHECK(unL43272.cpuMapWrite(0x8084,mapped,0));
    CHECK(unL43272.cpuMapRead(0x8000,mapped) && mapped == 1u*0x4000u);
    CHECK(unL43272.cpuMapRead(0xC000,mapped) && mapped == 1u*0x4000u);
    CHECK(!unL43272.ppuMapWrite(0,mapped,0));
    // O=1 S=1 => 32 KiB PRG banking; outer QQ + inner PP.
    CHECK(unL43272.cpuMapWrite(0x8099,mapped,0));
    CHECK(unL43272.cpuMapRead(0x8000,mapped) && mapped == 3u*0x8000u);
    CHECK(unL43272.cpuMapRead(0xC000,mapped) && mapped == 3u*0x8000u + 0x4000u);
    CHECK(unL43272.mirroringOverride() == 1u);
    CHECK(unL43272.cpuMapWrite(0x809B,mapped,0));
    CHECK(unL43272.mirroringOverride() == 0u);
    std::vector<uint8_t> before, after;
    unL43272.saveState(before);
    CHECK(unL43272.cpuMapWrite(0x8000,mapped,0));
    CHECK(unL43272.ppuMapWrite(0,mapped,0));
    const uint8_t* ptr=before.data();
    CHECK(unL43272.loadState(ptr,ptr+before.size()));
    CHECK(ptr == before.data()+before.size());
    CHECK(!unL43272.ppuMapWrite(0,mapped,0));
    CHECK(unL43272.cpuMapRead(0x8000,mapped) && mapped == 3u*0x8000u);
    unL43272.saveState(after);
    CHECK(before==after);

    // Sachen SA-020A (243): 8 readable registers, CHR bank bits, and
    // the unusual S0-S0-S0-S1 nametable arrangement.
    MapperCommonBoards sachen(8,16,243);
    auto writeSachen = [&](uint8_t reg, uint8_t data) {
        CHECK(sachen.cpuWriteDirect(0x4100u,reg));
        CHECK(sachen.cpuWriteDirect(0x4101u,data));
    };
    writeSachen(2,1); writeSachen(4,0); writeSachen(6,2);
    CHECK(sachen.ppuMapRead(0,mapped) && mapped == 9u*8192u);
    writeSachen(4,1);
    CHECK(sachen.ppuMapRead(0,mapped) && mapped == 11u*8192u);
    writeSachen(5,2);
    CHECK(sachen.cpuMapRead(0x8000,mapped) && mapped == 2u*32768u);
    CHECK(sachen.cpuWriteDirect(0x4102u,6));
    CHECK(sachen.cpuReadDirect(0x4103u,value) && value==2);
    CHECK(sachen.nametableWrite(0x2000,0x11));
    CHECK(sachen.nametableWrite(0x2C00,0x22));
    CHECK(sachen.nametableRead(0x2800,value) && value==0x11);
    CHECK(sachen.nametableRead(0x2C00,value) && value==0x22);
    // Vertical mode routes 2000/2800 and 2400/2C00 together.
    writeSachen(7,4);
    CHECK(sachen.nametableRead(0x2800,value) && value==0x11);
    CHECK(sachen.nametableRead(0x2400,value) && value==0x22);
    writeSachen(7,6); // single-screen B
    CHECK(sachen.nametableRead(0x2000,value) && value==0x22);
    before.clear();after.clear();sachen.saveState(before);
    writeSachen(7,0);
    ptr=before.data();
    CHECK(sachen.loadState(ptr,ptr+before.size()));
    CHECK(ptr==before.data()+before.size());
    CHECK(sachen.nametableRead(0x2000,value) && value==0x22);
    sachen.saveState(after);CHECK(after==before);

    // Fong Shen Bang (246): eight bank slots, only 2K SRAM at $6800,
    // plus the PAL's fixed-A17 override for selected $FFEx-$FFFx reads.
    MapperCommonBoards fong(16,32,246); // 512K PRG, 256K CHR
    CHECK(!fong.cpuMapRead(0x6000,mapped));
    CHECK(!fong.cpuMapRead(0x7000,mapped));
    CHECK(fong.cpuMapRead(0x6800,mapped) && mapped==Mapper::PRG_RAM_FLAG);
    CHECK(fong.cpuMapRead(0x6FFF,mapped) && mapped==(Mapper::PRG_RAM_FLAG|0x7FFu));
    CHECK(fong.cpuMapWrite(0x6803,mapped,0xBB) && mapped==(Mapper::PRG_RAM_FLAG|3u));
    CHECK(!fong.cpuMapWrite(0x7000,mapped,0xBB));
    CHECK(fong.cpuMapWrite(0x6000,mapped,3));
    CHECK(fong.cpuMapRead(0x8000,mapped) && mapped==3u*0x2000u);
    CHECK(fong.cpuMapWrite(0x6003,mapped,0));
    CHECK(fong.cpuMapRead(0xFFE3,mapped) && mapped==0x1FE3u);
    CHECK(fong.cpuMapRead(0xFFE4,mapped) && mapped==16u*0x2000u+0x1FE4u);
    CHECK(fong.cpuMapRead(0xFFEC,mapped) && mapped==16u*0x2000u+0x1FECu);
    CHECK(fong.cpuMapRead(0xFFF4,mapped) && mapped==16u*0x2000u+0x1FF4u);
    CHECK(fong.cpuMapRead(0xFFFC,mapped) && mapped==16u*0x2000u+0x1FFCu);
    CHECK(fong.cpuMapRead(0xFFE8,mapped) && mapped==0x1FE8u);
    CHECK(fong.cpuMapWrite(0x6005,mapped,4));
    CHECK(fong.ppuMapRead(0x0800,mapped) && mapped==4u*0x800u);
    CHECK(fong.ppuMapRead(0x0FFF,mapped) && mapped==4u*0x800u+0x7FFu);
    before.clear();after.clear();fong.saveState(before);
    CHECK(fong.cpuMapWrite(0x6003,mapped,6));
    ptr=before.data();CHECK(fong.loadState(ptr,ptr+before.size()));
    CHECK(ptr==before.data()+before.size());
    CHECK(fong.cpuMapRead(0xFFFC,mapped) && mapped==16u*0x2000u+0x1FFCu);
    fong.saveState(after);CHECK(before==after);
}

static void focused() {
    focusedDiscreteBoards();
    uint32_t map=0;
    Mapper001 surom(32,0);
    auto serialWrite = [](Mapper001& m, uint16_t a, uint8_t data) {
        uint32_t mapped=0;
        for(int i=0;i<5;i++) CHECK(m.cpuMapWrite(a,mapped,uint8_t((data>>i)&1u)));
    };
    CHECK((surom.capabilities() & MAPPER_CAP_PPU_ADDRESS_BUS)!=0);
    CHECK(surom.cpuMapRead(0xC000,map) && map==15u*0x4000u);
    serialWrite(surom,0xA000,0x10);
    CHECK(surom.cpuMapRead(0x8000,map) && map==16u*0x4000u);
    CHECK(surom.cpuMapRead(0xC000,map) && map==31u*0x4000u);
    serialWrite(surom,0xC000,0x00);
    surom.ppuAddressBus(0x1000);
    CHECK(surom.cpuMapRead(0xC000,map) && map==15u*0x4000u);
    surom.ppuAddressBus(0x0000);
    CHECK(surom.cpuMapRead(0xC000,map) && map==31u*0x4000u);
    serialWrite(surom,0x8000,0x08); // 16 KiB mode: low bank fixed
    CHECK(surom.cpuMapRead(0x8000,map) && map==16u*0x4000u);
    serialWrite(surom,0x8000,0x00); // 32 KiB mode
    CHECK(surom.cpuMapRead(0x8000,map) && map==16u*0x4000u);
    Mapper001 tinyMmc1(1,0);
    serialWrite(tinyMmc1,0x8000,0x00);
    CHECK(tinyMmc1.cpuMapRead(0xFFFF,map) && map==0x3FFFu);

    Mapper016 fcg(8,4,4);
    CHECK(fcg.cpuMapWrite(0x6008,map,3));
    CHECK(fcg.cpuMapRead(0x8000,map) && map==3u*0x4000u);
    CHECK(!fcg.cpuMapWrite(0x8008,map,4));
    CHECK(fcg.cpuMapRead(0x8000,map) && map==3u*0x4000u);
    CHECK(!fcg.cpuMapRead(0x6000,map));
    roundtrip(fcg,0x6008,5);
    Mapper016 lz(8,4,5);
    CHECK(!lz.cpuMapWrite(0x6008,map,2));
    CHECK(lz.cpuMapWrite(0x8008,map,2));
    CHECK(lz.cpuMapRead(0x8000,map) && map==2u*0x4000u);
    roundtrip(lz,0x8008,4);
    Mapper016 oldHeader(8,4,0);
    CHECK(oldHeader.cpuMapWrite(0x6008,map,4));
    CHECK(oldHeader.cpuMapRead(0x8000,map) && map==4u*0x4000u);
    CHECK(oldHeader.cpuMapWrite(0x8008,map,2));
    CHECK(oldHeader.cpuMapRead(0x8000,map) && map==2u*0x4000u);

    Mapper067 sunsoft3(8,0);
    CHECK(sunsoft3.cpuMapWrite(0x8800,map,0x3F));
    CHECK(sunsoft3.ppuMapWrite(0x0000,map,0) && map==0x1800u);
    roundtrip(sunsoft3,0x8800,0);

    Mapper080 x1005(8,8);
    CHECK(x1005.cpuMapWrite(0x7E70,map,3)); // A7 ignored
    CHECK(x1005.ppuMapRead(0x0000,map) && map==2u*0x400u);
    CHECK(x1005.ppuMapRead(0x0400,map) && map==3u*0x400u);
    CHECK(x1005.cpuMapWrite(0x7E76,map,0));
    CHECK(x1005.mirroringOverride()==1);
    CHECK(x1005.cpuMapWrite(0x7EF6,map,1));
    CHECK(x1005.mirroringOverride()==0);
    CHECK(x1005.cpuMapWrite(0x7EF7,map,0)); // not a mirroring mirror
    CHECK(x1005.mirroringOverride()==0);
    CHECK(x1005.cpuMapWrite(0x7E78,map,0xA3));
    CHECK(x1005.cpuMapRead(0x7F80,map) && map==(Mapper::PRG_RAM_FLAG|0u));
    CHECK(x1005.cpuMapWrite(0x7E7A,map,6));
    CHECK(x1005.cpuMapRead(0x8000,map) && map==6u*0x2000u);
    roundtrip(x1005,0x7E70,7);
    // X1-005 mapper 207: $7EF0/$7EF1 bit 7 independently select two
    // 2KiB rows of nametables. $7EF6 must be ignored on this board.
    Mapper080 x1005alt(8,16,true);
    uint8_t ntByte=0;
    CHECK((x1005alt.capabilities() & MAPPER_CAP_NAMETABLE)!=0);
    CHECK((x1005alt.capabilities() & MAPPER_CAP_MIRRORING)==0);
    CHECK(x1005alt.cpuMapWrite(0x7EF0,map,0x84));
    CHECK(x1005alt.cpuMapWrite(0x7E71,map,0x02));
    CHECK(x1005alt.ppuMapRead(0x0000,map) && map==4u*0x400u);
    CHECK(x1005alt.ppuMapRead(0x0800,map) && map==2u*0x400u);
    CHECK(x1005alt.nametableWrite(0x2000,0x33));
    CHECK(x1005alt.nametableWrite(0x2800,0x44));
    CHECK(x1005alt.nametableRead(0x2400,ntByte) && ntByte==0x33);
    CHECK(x1005alt.nametableRead(0x2C00,ntByte) && ntByte==0x44);
    CHECK(x1005alt.cpuMapWrite(0x7EF6,map,1));
    CHECK(x1005alt.nametableRead(0x2400,ntByte) && ntByte==0x33);
    CHECK(x1005alt.cpuMapWrite(0x7EF1,map,0x82));
    CHECK(x1005alt.nametableRead(0x2C00,ntByte) && ntByte==0x33);
    CHECK(x1005alt.cpuMapWrite(0x7EF0,map,0x04));
    CHECK(x1005alt.nametableRead(0x2000,ntByte) && ntByte==0x44);
    CHECK(x1005alt.cpuMapWrite(0x7EF8,map,0xA3));
    CHECK(x1005alt.cpuMapRead(0x7F80,map) && map==Mapper::PRG_RAM_FLAG);
    std::vector<uint8_t> x1005snapshot, x1005after;
    x1005alt.saveState(x1005snapshot);
    CHECK(x1005alt.nametableWrite(0x2000,0x99));
    CHECK(x1005alt.cpuMapWrite(0x7EF0,map,0x80));
    const uint8_t* x1005ptr=x1005snapshot.data();
    CHECK(x1005alt.loadState(x1005ptr,x1005ptr+x1005snapshot.size()));
    CHECK(x1005ptr==x1005snapshot.data()+x1005snapshot.size());
    CHECK(x1005alt.nametableRead(0x2000,ntByte) && ntByte==0x44);
    x1005alt.saveState(x1005after);
    CHECK(x1005after==x1005snapshot);

    Mapper082 x1017(8,8);
    CHECK(x1017.cpuMapWrite(0x7EF6,map,0));
    CHECK(x1017.mirroringOverride()==1);
    CHECK(x1017.cpuMapWrite(0x7EF6,map,1));
    CHECK(x1017.mirroringOverride()==0);
    CHECK(x1017.cpuMapWrite(0x7EFA,map,0x3C));
    CHECK(x1017.cpuMapRead(0x8000,map) && map==15u*0x2000u);
    CHECK(x1017.cpuMapWrite(0x7EFD,map,1));
    CHECK(x1017.cpuMapWrite(0x7EFE,map,0)); // latch + 2 times 16 = 48
    CHECK(x1017.cpuMapWrite(0x7EFE,map,3));
    for(int i=0;i<47;i++) x1017.cpuClock();
    CHECK(!x1017.irqState());
    x1017.cpuClock(); CHECK(x1017.irqState());
    CHECK(x1017.cpuMapWrite(0x7EFF,map,0)); CHECK(!x1017.irqState());
    for(int i=0;i<32;i++) x1017.cpuClock();
    CHECK(x1017.irqState());
    CHECK(x1017.cpuMapWrite(0x7EFE,map,0));
    CHECK(!x1017.irqState());
    roundtrip(x1017,0x7EFA,0);

    Mapper019 n163(4,8);
    CHECK((n163.capabilities() & MAPPER_CAP_CPU_DIRECT) != 0);
    CHECK(n163.cpuMapWrite(0x5000,map,0xFA));
    CHECK(n163.cpuMapWrite(0x5800,map,0xFF));
    uint8_t irqValue=0;
    CHECK(n163.cpuReadDirect(0x5A00,irqValue) && irqValue==0xFF);
    CHECK(n163.cpuReadDirect(0x5200,irqValue) && irqValue==0xFA);
    for(int i=0;i<5;i++) n163.cpuClock();
    CHECK(n163.irqState());
    CHECK(n163.cpuReadDirect(0x5800,irqValue) && irqValue==0xFF);
    CHECK(n163.irqState()); // reading IRQ counter must not acknowledge IRQ
    CHECK(n163.cpuMapWrite(0x5000,map,0)); CHECK(!n163.irqState());
    CHECK(n163.cpuReadDirect(0x5000,irqValue) && irqValue==0);
    roundtrip(n163,0x5800,0x80);

    // N163 chip-internal RAM port, auto-increment, CIRAM-vs-CHR and audio.
    Mapper019 n163advanced(8,32,5);
    uint8_t chipRead=0;
    CHECK(n163advanced.cpuMapWrite(0xF800,map,0xFE)); // address $7E, increment
    CHECK(n163advanced.cpuMapWrite(0x4800,map,0xA1));
    CHECK(n163advanced.cpuMapWrite(0x4800,map,0xB2));
    CHECK(n163advanced.cpuMapWrite(0x4800,map,0xC3)); // $7F saturates
    CHECK(n163advanced.cpuMapWrite(0xF800,map,0x7E));
    CHECK(n163advanced.cpuReadDirect(0x4800,chipRead) && chipRead==0xA1);
    CHECK(n163advanced.cpuMapWrite(0xF800,map,0x7F));
    CHECK(n163advanced.cpuReadDirect(0x4FFF,chipRead) && chipRead==0xC3);
    CHECK(n163advanced.persistentRamSize()==128);
    uint8_t backup[128]{};
    n163advanced.copyPersistentRam(backup,sizeof backup);
    CHECK(backup[0x7E]==0xA1 && backup[0x7F]==0xC3);
    n163advanced.clearPersistentRamDirty();
    CHECK(!n163advanced.persistentRamDirty());
    CHECK(n163advanced.cpuMapWrite(0x8000,map,0xE0));
    CHECK(n163advanced.ppuWriteDirect(0x0011,0x39));
    CHECK(n163advanced.ppuReadDirect(0x0011,chipRead) && chipRead==0x39);
    CHECK(n163advanced.cpuMapWrite(0xC000,map,0xE0));
    CHECK(n163advanced.nametableRead(0x2011,chipRead) && chipRead==0x39);
    CHECK(n163advanced.cpuMapWrite(0xC800,map,0xE1));
    CHECK(n163advanced.nametableWrite(0x2411,0x61));
    CHECK(n163advanced.nametableRead(0x2411,chipRead) && chipRead==0x61);
    CHECK(n163advanced.nametableRead(0x2011,chipRead) && chipRead==0x39);
    CHECK(n163advanced.cpuMapWrite(0x8000,map,0xE1));
    CHECK(n163advanced.ppuReadDirect(0x0011,chipRead) && chipRead==0x61);
    CHECK(n163advanced.cpuMapWrite(0xE800,map,0xC0)); // disable pattern-CIRAM mode
    CHECK(!n163advanced.ppuReadDirect(0x0011,chipRead));
    CHECK(n163advanced.ppuMapRead(0x0011,map) && map==((0xE1u%256u)*0x400u+0x11u));
    CHECK(n163advanced.cpuMapWrite(0xC800,map,0x0E)); // NT mapped to CHR ROM
    CHECK(!n163advanced.nametableRead(0x2411,chipRead));
    CHECK(n163advanced.nametableMapRead(0x2411,map) && map==(14u*1024u+0x11u));
    CHECK(!n163advanced.nametableMapWrite(0x2411,map)); // CHR ROM write ignored
    n163advanced.restorePersistentRam(backup,sizeof backup);
    CHECK(!n163advanced.persistentRamDirty());
    CHECK(n163advanced.cpuMapWrite(0xF800,map,0x7E));
    CHECK(n163advanced.cpuReadDirect(0x4800,chipRead) && chipRead==0xA1);
    // Wave at offset 0 = 0xF; channel 8 with zero frequency and length 4.
    CHECK(n163advanced.cpuMapWrite(0xF800,map,0x00));
    CHECK(n163advanced.cpuMapWrite(0x4800,map,0xFF));
    const uint8_t audioReg[8]={0,0,0,0,0xFC,0,0,0x0F};
    CHECK(n163advanced.cpuMapWrite(0xF800,map,0xF8));
    for (uint8_t val:audioReg) CHECK(n163advanced.cpuMapWrite(0x4800,map,val));
    for(int i=0;i<15;i++) n163advanced.cpuClock();
    CHECK(n163advanced.audioSample()>0.01f);
    roundtrip(n163advanced,0xF800,0x93);
    Mapper019 n163silent(8,8,2);
    CHECK(!n163silent.usesExpansionAudio());
    CHECK(n163silent.audioSample()==0.0f);

    // RAMBO-1: CPU IRQ clocks every 4 M2 and asserts on the following M2.
    Mapper064 rambo(8,8);
    CHECK((rambo.capabilities() & MAPPER_CAP_PPU_ADDRESS_BUS)!=0);
    CHECK((rambo.capabilities() & MAPPER_CAP_SCANLINE)==0);
    CHECK(rambo.cpuMapWrite(0xC000,map,2));
    CHECK(rambo.cpuMapWrite(0xC001,map,1));
    CHECK(rambo.cpuMapWrite(0xE001,map,0));
    for(int i=0;i<16;i++) rambo.cpuClock();
    CHECK(!rambo.irqState());
    rambo.cpuClock();
    CHECK(rambo.irqState());
    CHECK(rambo.cpuMapWrite(0xE000,map,0));
    CHECK(!rambo.irqState());
    CHECK(rambo.cpuMapWrite(0xC000,map,1));
    CHECK(rambo.cpuMapWrite(0xC001,map,0)); // PPU-A12 mode
    CHECK(rambo.cpuMapWrite(0xE001,map,0));
    for(int i=0;i<8;i++) rambo.ppuAddressBus(0x0000);
    rambo.ppuAddressBus(0x1000); // latch 1
    for(int i=0;i<7;i++) rambo.ppuAddressBus(0x0000);
    rambo.ppuAddressBus(0x1000); // short low gap ignored
    CHECK(!rambo.irqState());
    for(int i=0;i<8;i++) rambo.ppuAddressBus(0x0000);
    rambo.ppuAddressBus(0x1000); // decrement 1 to 0, delay 2 CPU cycles
    rambo.cpuClock();
    CHECK(!rambo.irqState());
    rambo.cpuClock();
    CHECK(rambo.irqState());
    roundtrip(rambo,0x8000,0xE9);

    Mapper099 vs(4,2);
    CHECK(vs.cpuMapRead(0x8000,map) && map==0);
    CHECK(vs.cpuMapRead(0xA000,map) && map==5u*0x2000u);
    CHECK(vs.cpuMapRead(0xC000,map) && map==6u*0x2000u);
    CHECK(vs.cpuMapRead(0xE000,map) && map==7u*0x2000u);
    CHECK(vs.cpuMapWrite(0x4016,map,4));
    CHECK(vs.cpuMapRead(0x8000,map) && map==4u*0x2000u);
    roundtrip(vs,0x4016,0);
    Mapper007 ax(8,0);
    CHECK(ax.cpuMapWrite(0x8000,map,0x12));
    CHECK(ax.mirroringOverride()==4);
    roundtrip(ax,0x8000,0x00);
}

// Action 53 supports 8 MiB PRG ROM in NES 2.0; older 6-bit $81
// implementations silently aliased 2 MiB/4 MiB banks. Verify all 8 outer
// register bits, the 12 combinations of PRG window configuration, and
// power-on reset bank selection without relying on commercial multicarts.
static void action53Focused() {
    uint32_t mapped = 0;
    Mapper028 m(512,0); // 512 x 16 KiB PRG = 8 MiB; 4 x 8 KiB CHR RAM
    CHECK(m.cpuMapRead(0xFFFC,mapped) && mapped == 511u*0x4000u+0x3FFCu);
    CHECK(m.cpuMapWrite(0x5000,mapped,0x81));
    CHECK(m.cpuMapWrite(0x8000,mapped,0xFF));
    for (int mode = 0; mode < 4; ++mode) {
        const uint8_t modeByte = uint8_t(0x30 | (mode<<2) | 0x02);
        CHECK(m.cpuMapWrite(0x5000,mapped,0x80));
        CHECK(m.cpuMapWrite(0x8000,mapped,modeByte));
        CHECK(m.cpuMapWrite(0x5000,mapped,0x01));
        for(int inner : {0, 1, 5, 10, 15}) {
            CHECK(m.cpuMapWrite(0x8000,mapped,uint8_t(inner)));
            CHECK(m.cpuMapWrite(0x5000,mapped,0x81));
            for (int outer : {0, 31, 63, 64, 127, 128, 191, 192, 254, 255}) {
                CHECK(m.cpuMapWrite(0x8000,mapped,uint8_t(outer)));
                const unsigned out = unsigned(outer) << 1;
                // Outer size = 256 KiB: 4 inner bits + 4 outer upper bits.
                // Fixed half always passes full outer bank bits through.
                unsigned low = 0, high = 0;
                if(mode == 0 || mode == 1) {
                    low = (out & ~14u) | ((unsigned(inner)<<1) & 14u);
                    high = low | 1u;
                } else if (mode == 2) {
                    low = out;
                    high = (out & ~14u) | (unsigned(inner) & 15u);
                } else {
                    low = (out & ~14u) | (unsigned(inner) & 15u);
                    high = out | 1u;
                }
                CHECK(m.cpuMapRead(0x8000,mapped) && mapped==low*0x4000u);
                CHECK(m.cpuMapRead(0xC000,mapped) && mapped==high*0x4000u);
            }
            CHECK(m.cpuMapWrite(0x5000,mapped,0x01));
        }
    }
    // Mirroring write-through in one-screen mode, ignored in H/V mode.
    CHECK(m.cpuMapWrite(0x5000,mapped,0x80));
    CHECK(m.cpuMapWrite(0x8000,mapped,0));
    CHECK(m.mirroringOverride()==3);
    CHECK(m.cpuMapWrite(0x5000,mapped,0x00));
    CHECK(m.cpuMapWrite(0x8000,mapped,0x10));
    CHECK(m.mirroringOverride()==4);
    CHECK(m.cpuMapWrite(0x5000,mapped,0x80));
    CHECK(m.cpuMapWrite(0x8000,mapped,0x02));
    CHECK(m.cpuMapWrite(0x5000,mapped,0x00));
    CHECK(m.cpuMapWrite(0x8000,mapped,0x00));
    CHECK(m.mirroringOverride()==1);
    // Four distinct 8 KiB banks fit in the cartridge's 32 KiB CHR RAM.
    for(int bank=0;bank<4;++bank) {
        CHECK(m.cpuMapWrite(0x8000,mapped,uint8_t(bank)));
        CHECK(m.ppuMapRead(0x1FFF,mapped) && mapped==uint32_t(bank)*0x2000u+0x1FFFu);
        CHECK(m.ppuMapWrite(0x0012,mapped,0xA5) && mapped==uint32_t(bank)*0x2000u+0x12u);
    }
    std::vector<uint8_t> saved,after;
    m.saveState(saved);
    CHECK(m.cpuMapWrite(0x5000,mapped,0x81));
    CHECK(m.cpuMapWrite(0x8000,mapped,0));
    const uint8_t *ptr=saved.data();
    CHECK(m.loadState(ptr,ptr+saved.size()) && ptr==saved.data()+saved.size());
    m.saveState(after);
    CHECK(saved==after);
    // The physical /RESET line does not reset Action 53 latch state.
    m.reset(); after.clear(); m.saveState(after); CHECK(saved==after);
}

// Mapper 32 Irem G-101: decode all four register windows by A15..A12,
// never treating odd $8000 writes as mirroring/mode writes. NES2
// submapper 1 (Major League) has hardwired one-screen high mirroring.
static void iremG101Focused() {
    uint32_t map=0;
    Mapper032 g101(8,8);
    CHECK(g101.cpuMapRead(0x8000,map) && map==0);
    CHECK(g101.cpuMapRead(0xC000,map) && map==14u*0x2000u);
    CHECK(g101.cpuMapWrite(0x8001,map,7));
    CHECK(g101.cpuMapRead(0x8000,map) && map==7u*0x2000u);
    CHECK(g101.cpuMapRead(0xC000,map) && map==14u*0x2000u);
    CHECK(g101.cpuMapWrite(0x9000,map,3));
    CHECK(g101.mirroringOverride()==0); // H
    CHECK(g101.cpuMapRead(0x8000,map) && map==14u*0x2000u);
    CHECK(g101.cpuMapRead(0xC000,map) && map==7u*0x2000u);
    CHECK(g101.cpuMapWrite(0xAFFF,map,0xFF));
    CHECK(g101.cpuMapRead(0xA000,map) && map==15u*0x2000u); // 5-bit PRG register
    CHECK(g101.cpuMapWrite(0xB007,map,23));
    CHECK(g101.ppuMapRead(0x1C00,map) && map==23u*0x400u);
    CHECK(g101.cpuMapWrite(0x9FFF,map,0));
    CHECK(g101.mirroringOverride()==1); // V
    CHECK(g101.cpuMapRead(0x8000,map) && map==7u*0x2000u);
    CHECK(g101.cpuMapRead(0xC000,map) && map==14u*0x2000u);
    roundtrip(g101,0x9000,3);
    Mapper032 major(8,8,1);
    CHECK(major.mirroringOverride()==4);
    CHECK(major.cpuMapWrite(0x8001,map,13));
    CHECK(major.cpuMapWrite(0x9000,map,3));
    CHECK(major.mirroringOverride()==4);
    CHECK(major.cpuMapRead(0x8000,map) && map==13u*0x2000u);
    CHECK(major.cpuMapRead(0xC000,map) && map==14u*0x2000u);
    roundtrip(major,0x9000,1);
}

// All configured iNES mapper IDs in Cartridge's switch; FDS mapper 20 is a
// separate disk/BIOS path and is not an ordinary iNES ROM.
static const int ids[]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,15,16,17,18,19,21,22,23,24,25,26,28,32,33,34,41,42,43,44,45,46,47,49,50,51,52,53,56,57,58,60,61,62,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,82,83,85,86,87,88,89,90,91,92,93,94,95,96,97,99,105,107,111,112,113,114,115,117,118,119,120,123,128,129,130,133,140,142,151,153,157,158,159,160,161,162,164,165,166,180,182,184,185,187,188,189,193,195,196,199,200,201,202,203,204,205,207,210,211,222,224,225,226,227,228,229,230,231,232,233,234,240,241,242,243,244,245,246,248,249,250,251,252,254,255,552};
static void createRom(const char* path, unsigned mapper, unsigned prg16=8, unsigned chr8=8, unsigned header9=0, bool nes2=false, unsigned submapper=0){
    unsigned char header[16]={0x4E,0x45,0x53,0x1A};
    header[4]=prg16&0xFF; header[5]=chr8&0xFF;
    header[6]=static_cast<uint8_t>(((mapper&0xF)<<4));
    header[7]=static_cast<uint8_t>((mapper&0xF0)|(nes2?0x08:0));
    header[8]=nes2 ? static_cast<uint8_t>((submapper<<4) | (mapper>>8)) : 0;
    header[9]=static_cast<uint8_t>(header9);
    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    out.write(reinterpret_cast<const char*>(header),16);
    const int prgPages=prg16*2,chrPages=chr8*8;
    std::vector<char> page(0x2000);
    for(int i=0;i<prgPages;i++){std::fill(page.begin(),page.end(),static_cast<char>(i));out.write(page.data(),page.size());}
    page.resize(0x400);
    for(int i=0;i<chrPages;i++){std::fill(page.begin(),page.end(),static_cast<char>(i));out.write(page.data(),page.size());}
}

// CP021: NROM-368 is a 46 KiB linear PRG board with 2 KiB leading iNES
// padding; it must not be mistaken for ordinary 16/32 KiB mapper-0 NROM.
static void nrom368AndSnromHardwareFocused(const char* basename) {
    uint32_t mapped=0;
    Mapper000 nrom368(3,1);
    CHECK(!nrom368.cpuMapRead(0x47FF,mapped));
    CHECK(nrom368.cpuMapRead(0x4800,mapped) && mapped == 0x0800u);
    CHECK(nrom368.cpuMapRead(0x5FFF,mapped) && mapped == 0x1FFFu);
    CHECK(nrom368.cpuMapRead(0x6000,mapped) && mapped == 0x2000u);
    CHECK(nrom368.cpuMapRead(0x7FFF,mapped) && mapped == 0x3FFFu);
    CHECK(nrom368.cpuMapRead(0x8000,mapped) && mapped == 0x4000u);
    CHECK(nrom368.cpuMapRead(0xC000,mapped) && mapped == 0x8000u);
    CHECK(nrom368.cpuMapRead(0xFFFF,mapped) && mapped == 0xBFFFu);
    CHECK(nrom368.cpuMapWrite(0x6000,mapped,0xAA) && mapped == 0x2000u);
    CHECK(!nrom368.cpuMapWrite(0x47FF,mapped,0xAA));
    Mapper000 normalNrom(2,1);
    CHECK(!normalNrom.cpuMapRead(0x4800,mapped));
    CHECK(normalNrom.cpuMapRead(0x8000,mapped) && mapped == 0u);
    CHECK(normalNrom.cpuMapRead(0x6000,mapped) &&
          mapped == (Mapper::PRG_RAM_FLAG | 0u));
    const std::string path=std::string(basename)+".nrom368.nes";
    createRom(path.c_str(),0,3,1);
    {
        Cartridge c(path);
        uint8_t byte=0;
        CHECK(c.imageValid());
        CHECK(c.cpuRead(0x4800,byte) && byte == 0u);
        CHECK(c.cpuRead(0x6000,byte) && byte == 1u);
        CHECK(c.cpuRead(0x8000,byte) && byte == 2u);
        CHECK(c.cpuRead(0xC000,byte) && byte == 4u);
        CHECK(c.cpuRead(0xFFFF,byte) && byte == 5u);
        CHECK(c.cpuWrite(0x6000,0xF1));
        CHECK(c.cpuRead(0x6000,byte) && byte == 1u); // ROM is read-only
    }
    std::remove(path.c_str());
    Mapper001 snrom(8,0);
    CHECK((snrom.capabilities() & MAPPER_CAP_PPU_ADDRESS_BUS) != 0);
    auto reg=[](Mapper001& m,uint16_t a,uint8_t v){
        uint32_t tmp=0;
        for(unsigned i=0;i<5;++i) CHECK(m.cpuMapWrite(a,tmp,(v>>i)&1u));
    };
    reg(snrom,0x8000,0x1E); // CHR 4 KiB mode: A12 selects register 0/1
    reg(snrom,0xA000,0x00);
    reg(snrom,0xC000,0x10); // CHR1 high bit disables WRAM
    snrom.ppuAddressBus(0x0000);
    CHECK(snrom.cpuMapRead(0x6000,mapped));
    snrom.ppuAddressBus(0x1000);
    CHECK(!snrom.cpuMapRead(0x6000,mapped));
    snrom.ppuAddressBus(0x2000);
    CHECK(snrom.cpuMapRead(0x6000,mapped));
    roundtrip(snrom,0xA000,0x00);
}

// CP016 end-to-end NES 2.0: 32 KiB MMC5 PRG RAM must route its 4 physical
// banks through the cartridge, including out-of-range open-bus chip selects,
// write protection, and a full cartridge state restore.
static void mmc5RamCartridgeEndToEnd(const char* basename) {
    const std::string path=std::string(basename)+".mmc5_ram.nes";
    createRom(path.c_str(),5,2,0,0,true,0);
    // NES2 byte 10, volatile PRG RAM shift=9: 64 << 9 = 32768 bytes.
    { std::fstream file(path.c_str(), std::ios::in|std::ios::out|std::ios::binary);
      const uint8_t ramHeader=9u; file.seekp(10); file.put(char(ramHeader)); }
    {
        Cartridge cart(path);
        uint8_t data=0;
        CHECK(cart.imageValid());
        CHECK(cart.prgRamMem().size()==32768u);
        CHECK(cart.cpuWrite(0x5102,2));
        CHECK(cart.cpuWrite(0x5103,1));
        for(int bank=0; bank<4; ++bank) {
            CHECK(cart.cpuWrite(0x5113,uint8_t(bank)));
            CHECK(cart.cpuWrite(0x6000,uint8_t(0xA0+bank)));
        }
        for(int bank=0; bank<4; ++bank) {
            CHECK(cart.cpuWrite(0x5113,uint8_t(bank)));
            CHECK(cart.cpuRead(0x6000,data) && data==uint8_t(0xA0+bank));
        }
        for(int bank=4; bank<8; ++bank) {
            CHECK(cart.cpuWrite(0x5113,uint8_t(bank)));
            CHECK(!cart.cpuRead(0x6000,data));
            CHECK(cart.cpuWrite(0x6000,0x55));
        }
        CHECK(cart.cpuWrite(0x5113,2));
        CHECK(cart.cpuRead(0x6000,data)&&data==0xA2);
        CHECK(cart.cpuWrite(0x5102,0)); // RAM write protect
        CHECK(cart.cpuWrite(0x6000,0x33));
        CHECK(cart.cpuRead(0x6000,data)&&data==0xA2);
        std::vector<uint8_t> before,after;
        cart.saveState(before);
        CHECK(cart.cpuWrite(0x5113,0));
        const uint8_t* cursor=before.data();
        CHECK(cart.loadState(cursor,cursor+before.size()));
        CHECK(cursor==before.data()+before.size());
        CHECK(cart.cpuRead(0x6000,data)&&data==0xA2);
        cart.saveState(after);
        CHECK(before==after);
    }
    std::remove(path.c_str());
    // NES 2.0 explicitly declares no MMC5 SRAM on ELROM boards.
    createRom(path.c_str(),5,2,0,0,true,0);
    {
        Cartridge noRam(path);
        uint8_t value=0;
        CHECK(noRam.imageValid());
        CHECK(noRam.prgRamMem().empty());
        CHECK(!noRam.cpuRead(0x6000,value));
        CHECK(noRam.cpuWrite(0x6000,0xA5));
    }
    std::remove(path.c_str());
    // iNES 1.0 omitting RAM size retains the historical 8 KiB default.
    createRom(path.c_str(),5,2,0,0,false,0);
    {
        Cartridge legacy(path);
        CHECK(legacy.imageValid());
        CHECK(legacy.prgRamMem().size()==8192u);
    }
    std::remove(path.c_str());
}

// Tengen board 800037 (mapper 158): RAMBO-1 with CHR A17 -> CIRAM A10.
// FME-7 register $D has independent timer-clock and IRQ output enables.
static void fme7Focused() {
    Mapper069 fme(16,16);
    uint32_t addr=0;
    // All four 8 KiB CPU windows, especially the fixed last bank at
    // $E000-$FFFF containing RESET/IRQ/NMI vectors, must stay in bounds.
    CHECK(fme.cpuMapRead(0x8000,addr) && addr==29u*0x2000u);
    CHECK(fme.cpuMapRead(0xA000,addr) && addr==30u*0x2000u);
    CHECK(fme.cpuMapRead(0xC000,addr) && addr==31u*0x2000u);
    CHECK(fme.cpuMapRead(0xE000,addr) && addr==31u*0x2000u);
    CHECK(fme.cpuMapRead(0xFFFC,addr) && addr==31u*0x2000u+0x1FFCu);
    CHECK(fme.cpuMapWrite(0x8000,addr,9));
    CHECK(fme.cpuMapWrite(0xA000,addr,3));
    CHECK(fme.cpuMapRead(0x8000,addr) && addr==3u*0x2000u);
    CHECK(fme.cpuMapWrite(0x8000,addr,10));
    CHECK(fme.cpuMapWrite(0xA000,addr,4));
    CHECK(fme.cpuMapRead(0xA000,addr) && addr==4u*0x2000u);
    CHECK(fme.cpuMapWrite(0x8000,addr,11));
    CHECK(fme.cpuMapWrite(0xA000,addr,5));
    CHECK(fme.cpuMapRead(0xC000,addr) && addr==5u*0x2000u);
    CHECK(fme.cpuMapRead(0xFFFC,addr) && addr==31u*0x2000u+0x1FFCu);
    CHECK(fme.cpuMapWrite(0x8000,addr,8));
    CHECK(fme.cpuMapWrite(0xA000,addr,0x40));
    CHECK(!fme.cpuMapRead(0x6000,addr)); // RAM selected, disabled: open bus.
    CHECK(fme.cpuMapWrite(0xA000,addr,0xC0));
    CHECK(fme.cpuMapRead(0x6000,addr) && addr==Mapper::PRG_RAM_FLAG);
    CHECK(fme.cpuMapWrite(0xA000,addr,0x03));
    CHECK(fme.cpuMapRead(0x6000,addr) && addr==0x6000u);
    CHECK(fme.cpuMapWrite(0x8000,addr,0x0D));
    CHECK(fme.cpuMapWrite(0xA000,addr,0x80)); // Timer counts; IRQ disabled.
    CHECK(fme.cpuMapWrite(0x8000,addr,0x0E));
    CHECK(fme.cpuMapWrite(0xA000,addr,1));
    CHECK(fme.cpuMapWrite(0x8000,addr,0x0F));
    CHECK(fme.cpuMapWrite(0xA000,addr,0));
    fme.cpuClock(); // 1 -> 0: no IRQ yet.
    CHECK(!fme.irqState());
    fme.cpuClock(); // 0 -> $FFFF: no IRQ because disabled.
    CHECK(!fme.irqState());
    CHECK(fme.cpuMapWrite(0x8000,addr,0x0D));
    CHECK(fme.cpuMapWrite(0xA000,addr,0x81)); // Counter and IRQ enabled.
    CHECK(fme.cpuMapWrite(0x8000,addr,0x0E));
    CHECK(fme.cpuMapWrite(0xA000,addr,1));
    CHECK(fme.cpuMapWrite(0x8000,addr,0x0F));
    CHECK(fme.cpuMapWrite(0xA000,addr,0));
    fme.cpuClock();CHECK(!fme.irqState());
    fme.cpuClock();CHECK(fme.irqState());
    CHECK(fme.cpuMapWrite(0x8000,addr,0x0D));
    CHECK(fme.cpuMapWrite(0xA000,addr,0x01)); // IRQ permitted but timer stopped; ACK.
    CHECK(!fme.irqState());
    for(int i=0;i<10;i++)fme.cpuClock();
    CHECK(!fme.irqState());
    roundtrip(fme,0xA000,0x81);
}

// Sunsoft 5B output must tick from the CPU clock, not the host audio callback.
// Exercises all three sound blocks, register-port gating, envelope phases,
// continued sound when IRQs are stopped, and bit-identical state rollback.
static void sunsoft5bAudioFocused() {
    Mapper069 chip(16,16);
    uint32_t mapped=0;
    auto set=[&](uint8_t reg,uint8_t val) {
        CHECK(chip.cpuMapWrite(0xC000,mapped,reg));
        CHECK(chip.cpuMapWrite(0xE000,mapped,val));
    };
    CHECK((chip.capabilities() & MAPPER_CAP_EXPANSION_AUDIO)!=0);
    CHECK(chip.usesExpansionAudio());
    CHECK(chip.audioSample()==0.0f);
    set(7,0x3Eu);  // A tone only, B/C off, noise gated out.
    set(0,2);set(1,0);set(8,15);
    float lo=10.0f,hi=-10.0f;
    for(int i=0;i<512;++i) {
        chip.cpuClock();
        if(i%8==0){const float x=chip.audioSample();lo=std::min(lo,x);hi=std::max(hi,x);}
    }
    CHECK(hi>0.04f && lo==0.0f); // Period 2 -> regular 64-CPU-clock square cycle.
    // Nonzero upper latch nibble inhibits data writes to sound registers.
    set(7,0x3Fu); // both gates disabled = constant level.
    CHECK(chip.audioSample()>0.04f);
    CHECK(chip.cpuMapWrite(0xC000,mapped,0x18));
    CHECK(chip.cpuMapWrite(0xE000,mapped,0));
    CHECK(chip.audioSample()>0.04f);
    // Envelope DAC has 32 levels and all 16 shape codes are accepted.
    set(8,0x10u);set(11,1);set(12,0);
    for(uint8_t shape=0;shape<16;++shape) {
        set(13,shape);
        std::vector<float> levels;
        for(unsigned i=0;i<1100u;++i){chip.cpuClock();if(i%16==15)levels.push_back(chip.audioSample());}
        const auto range=std::minmax_element(levels.begin(),levels.end());
        CHECK(*range.second>*range.first);
    }
    // Envelope shape 0xB: falling attack followed by a held high output.
    set(13,0x0Bu);
    for(int i=0;i<2048;i++) chip.cpuClock();
    CHECK(chip.audioSample()>0.04f);
    // Noise-only ch A: no other tone or channel influences the sample.
    set(8,0x0Fu);set(7,0x37u);set(6,1);
    lo=10.0f;hi=-10.0f;
    for(int i=0;i<8192;++i){chip.cpuClock();const float x=chip.audioSample();lo=std::min(lo,x);hi=std::max(hi,x);}
    CHECK(hi>0.04f && lo==0.0f);
    // Save while tone/noise/envelope and IRQ have all advanced; restoring must
    // reproduce the exact subsequent waveform and serialized register state.
    set(7,0x38u);set(0,3);set(2,5);set(4,7);
    set(8,0x10);set(9,0x0E);set(10,0x0C);set(13,0x0E);
    for(int i=0;i<123;++i) chip.cpuClock();
    std::vector<uint8_t> snapshot, restored;
    chip.saveState(snapshot);
    std::vector<float> predicted;
    for(int i=0;i<1024;++i){chip.cpuClock();if(i%5==0)predicted.push_back(chip.audioSample());}
    const uint8_t* ptr=snapshot.data();
    CHECK(chip.loadState(ptr,ptr+snapshot.size()));
    CHECK(ptr==snapshot.data()+snapshot.size());
    chip.saveState(restored);
    CHECK(restored==snapshot);
    size_t j=0;
    for(int i=0;i<1024;++i){chip.cpuClock();if(i%5==0)CHECK(chip.audioSample()==predicted[j++]);}
}

static void rambo158Focused() {
    Mapper064 m(16,16,true);
    uint32_t mapped=0;
    uint8_t data=0;
    CHECK((m.capabilities() & MAPPER_CAP_NAMETABLE)!=0);
    CHECK((m.capabilities() & MAPPER_CAP_MIRRORING)==0);
    CHECK(m.nametableWrite(0x2000,0x11));
    CHECK(m.nametableRead(0x2400,data) && data==0x11);
    CHECK(m.cpuMapWrite(0x8000,mapped,1));
    CHECK(m.cpuMapWrite(0x8001,mapped,0x80));
    CHECK(m.nametableWrite(0x2800,0x22));
    CHECK(m.nametableRead(0x2C00,data) && data==0x22);
    // R0 -> first two quadrants, R1 -> latter two; bit 7 is not CHR-ROM bank.
    CHECK(m.cpuMapWrite(0x8000,mapped,0));
    CHECK(m.cpuMapWrite(0x8001,mapped,0x80));
    CHECK(m.nametableWrite(0x2000,0x83));
    CHECK(m.nametableRead(0x2400,data) && data==0x83);
    CHECK(m.nametableRead(0x2800,data) && data==0x83);
    CHECK(m.ppuMapRead(0x0000,mapped) && mapped==0); // $80 -> bank 0 for 128 KiB CHR
    CHECK(m.cpuMapWrite(0x8000,mapped,1));
    CHECK(m.cpuMapWrite(0x8001,mapped,0x80));
    CHECK(m.nametableRead(0x2800,data) && data==0x83);
    // $A000 even has no effect on mapper 158 mirroring.
    CHECK(m.cpuMapWrite(0xA000,mapped,1));
    CHECK(m.nametableRead(0x2000,data) && data==0x83);
    // K=1 makes R8/R9 control quadrants 1 and 3 independently.
    CHECK(m.cpuMapWrite(0x8000,mapped,0x20|8));
    CHECK(m.cpuMapWrite(0x8001,mapped,0));
    CHECK(m.nametableRead(0x2400,data) && data==0x11);
    CHECK(m.cpuMapWrite(0x8000,mapped,0x20|9));
    CHECK(m.cpuMapWrite(0x8001,mapped,0x80));
    CHECK(m.nametableRead(0x2C00,data) && data==0x83);
    // Inversion maps R2-R5 onto the four nametable outputs.
    CHECK(m.cpuMapWrite(0x8000,mapped,0x80|2));
    CHECK(m.cpuMapWrite(0x8001,mapped,0x80));
    CHECK(m.nametableRead(0x2000,data) && data==0x83);
    CHECK(m.cpuMapWrite(0x8000,mapped,0x80|3));
    CHECK(m.cpuMapWrite(0x8001,mapped,0));
    CHECK(m.nametableRead(0x2400,data) && data==0x11);
    // Serialize both active bank selection and the genuine 2 KiB CIRAM image.
    std::vector<uint8_t> saved,after;
    m.saveState(saved);
    CHECK(m.nametableWrite(0x2000,0x44));
    CHECK(m.cpuMapWrite(0x8000,mapped,0));
    CHECK(m.cpuMapWrite(0x8001,mapped,0));
    const uint8_t* p=saved.data();
    CHECK(m.loadState(p,p+saved.size()));
    CHECK(p==saved.data()+saved.size());
    CHECK(m.nametableRead(0x2000,data) && data==0x83);
    m.saveState(after);
    CHECK(after==saved);
}

// End-to-end NES cartridge path: the mapper's custom CIRAM read/write hooks
// must be exposed through Cartridge's cached MAPPER_CAP_NAMETABLE flag.
static void taito207EndToEnd(const char* romPath) {
    createRom(romPath,207,8,16);
    Cartridge cart(romPath);
    CHECK(cart.imageValid() && cart.mapperId()==207u);
    CHECK(cart.prgRamMem().size()==128u);
    uint8_t value=0;
    CHECK(cart.cpuWrite(0x7EF0,0x80));
    CHECK(cart.cpuWrite(0x7EF1,0x00));
    CHECK(cart.ppuWrite(0x2009,0xA5));
    CHECK(cart.ppuWrite(0x2809,0x5A));
    CHECK(cart.ppuRead(0x2409,value) && value==0xA5);
    CHECK(cart.ppuRead(0x2C09,value) && value==0x5A);
    CHECK(cart.cpuWrite(0x7EF6,0x01));
    CHECK(cart.ppuRead(0x2409,value) && value==0xA5);
    // Switching both nametable halves to physical CIRAM page 0 aliases them.
    CHECK(cart.cpuWrite(0x7EF0,0x00));
    CHECK(cart.ppuRead(0x2009,value) && value==0x5A);
    std::vector<uint8_t> before,after;
    cart.saveState(before);
    CHECK(cart.cpuWrite(0x7EF1,0x80));
    CHECK(cart.ppuWrite(0x2009,0x11));
    const uint8_t* cur=before.data();
    CHECK(cart.loadState(cur,cur+before.size()));
    CHECK(cur==before.data()+before.size());
    cart.saveState(after);
    CHECK(after==before);
    CHECK(cart.ppuRead(0x2009,value) && value==0x5A);

    // Battery-backed X1-005 saves only its physically exposed 128-byte SRAM.
    // A legacy 8KiB save's first 128 bytes must remain readable, too.
    createRom(romPath,207,8,16);
    {std::fstream f(romPath,std::ios::binary|std::ios::in|std::ios::out);
     f.seekp(6); const uint8_t flags6=0xF2;
     f.write(reinterpret_cast<const char*>(&flags6),1);}
    std::string leaf=romPath;
    const auto slash=leaf.find_last_of("/\\");
    if(slash!=std::string::npos) leaf.erase(0,slash+1);
    const auto dot=leaf.find_last_of('.');
    if(dot!=std::string::npos) leaf.erase(dot);
    const std::string sav="Saves/"+leaf+".SAV";
    {std::ofstream f(sav,std::ios::binary|std::ios::trunc);
     std::vector<uint8_t> old(8192,0);
     old[0]=0x73;
     f.write(reinterpret_cast<const char*>(old.data()),std::streamsize(old.size()));}
    {
        Cartridge battery(romPath);
        CHECK(battery.imageValid() && battery.prgRamMem().size()==128u);
        CHECK(battery.cpuWrite(0x7EF8,0xA3));
        CHECK(battery.cpuRead(0x7F80,value) && value==0x73);
        CHECK(battery.cpuWrite(0x7F80,0x42));
        battery.flushSave();
    }
    {std::ifstream f(sav,std::ios::binary|std::ios::ate);
     CHECK(f.good() && f.tellg()==std::streampos(128));}
    {
        Cartridge battery(romPath);
        CHECK(battery.imageValid());
        CHECK(battery.cpuWrite(0x7EF8,0xA3));
        CHECK(battery.cpuRead(0x7F00,value) && value==0x42);
    }
    std::remove(sav.c_str());
}

// CP012: standard MMC3 banking, WRAM protection and filtered PPU A12 IRQ.
static void mmc3CoreFocused() {
    Mapper004 m(16,16);
    uint32_t map=0;
    CHECK(m.cpuMapRead(0xE000,map) && map==31u*8192u);
    CHECK(m.cpuMapWrite(0x8000,map,0x06));
    CHECK(m.cpuMapWrite(0x8001,map,0x05));
    CHECK(m.cpuMapRead(0x8000,map) && map==5u*8192u);
    CHECK(m.cpuMapWrite(0x8000,map,0x46)); // PRG mode inversion
    CHECK(m.cpuMapRead(0xC000,map) && map==5u*8192u);
    CHECK(m.cpuMapRead(0x8000,map) && map==30u*8192u);
    CHECK(m.cpuMapRead(0x6001,map) && map==(Mapper::PRG_RAM_FLAG|1u));
    CHECK(m.cpuMapWrite(0xA001,map,0xC0)); // RAM read enabled, write protected
    CHECK(m.cpuMapRead(0x6001,map) && map==(Mapper::PRG_RAM_FLAG|1u));
    map=0; CHECK(m.cpuMapWrite(0x6001,map,0xA5) && !(map&Mapper::PRG_RAM_FLAG));
    CHECK(m.cpuMapWrite(0xA001,map,0x00));
    CHECK(!m.cpuMapRead(0x6001,map));
    CHECK(m.cpuMapWrite(0xA001,map,0x80));
    CHECK(m.cpuMapWrite(0x6001,map,0xA5) && (map&Mapper::PRG_RAM_FLAG));
    CHECK(m.cpuMapWrite(0xA000,map,0)); CHECK(m.mirroringOverride()==1u);
    CHECK(m.cpuMapWrite(0xA000,map,1)); CHECK(m.mirroringOverride()==0u);
    // IRQ counter increments only after A12 has spent at least 8 PPU dots
    // low. The default MMC3 revision asserts for reload latch=0.
    CHECK(m.cpuMapWrite(0xC000,map,0));
    CHECK(m.cpuMapWrite(0xC001,map,0));
    CHECK(m.cpuMapWrite(0xE001,map,0));
    for (int i=0;i<7;++i) m.ppuAddressBus(0);
    m.ppuAddressBus(0x1000); m.ppuAddressBus(0x1000);
    CHECK(!m.irqState()); // edge rejected
    for (int i=0;i<8;++i) m.ppuAddressBus(0);
    m.ppuAddressBus(0x1000);
    CHECK(!m.irqState()); // delayed a further 2 PPU dots
    m.ppuAddressBus(0x1000);m.ppuAddressBus(0x1000);
    CHECK(m.irqState());
    CHECK(m.cpuMapWrite(0xE000,map,0)); // disable/acknowledge
    CHECK(!m.irqState());
    std::vector<uint8_t> before,after;
    m.saveState(before);
    CHECK(m.cpuMapWrite(0x8001,map,9));
    const uint8_t* cur=before.data();
    CHECK(m.loadState(cur,cur+before.size()));
    CHECK(cur==before.data()+before.size());
    m.saveState(after);CHECK(after==before);
}

// CP013: IRQ silicon profiles must not be conflated. A requested reload
// may assert on both profiles, while a zero->zero *natural* reload must
// assert only on later Sharp MMC3B/C hardware. Filtering and propagation
// remain at real PPU dot granularity, with no per-game or CRC checks.
static void mmc3IrqRevisionFocused(const char* basename) {
    auto edge = [](Mapper004& m) {
        for (int i=0;i<9;++i) m.ppuAddressBus(0x0000);
        for (int i=0;i<4;++i) m.ppuAddressBus(0x1000);
    };
    for (int variant=0;variant<2;++variant) {
        Mapper004 m(8,8);
        m.setMmc3AlternateIrqRevision(variant != 0);
        uint32_t mapped=0;
        CHECK(m.cpuMapWrite(0xC000,mapped,0));
        CHECK(m.cpuMapWrite(0xC001,mapped,0));
        CHECK(m.cpuMapWrite(0xE001,mapped,0));
        edge(m);
        CHECK(m.irqState()); // an explicit reload of zero triggers either chip
        CHECK(m.cpuMapWrite(0xE000,mapped,0));
        CHECK(!m.irqState());
        CHECK(m.cpuMapWrite(0xE001,mapped,0));
        edge(m); // previous IRQ counter was zero, automatic reload to zero
        CHECK(m.irqState() == (variant==0));
        CHECK(m.cpuMapWrite(0xE000,mapped,0));
        CHECK(m.cpuMapWrite(0xE001,mapped,0));
        CHECK(m.cpuMapWrite(0xC001,mapped,0)); // explicit reload reinstates IRQ
        edge(m);
        CHECK(m.irqState());
        // Save-state restores the counter, while board/silicon selection stays
        // fixed by the loaded cartridge or the caller's explicit override.
        std::vector<uint8_t> saved,after;
        m.saveState(saved);
        CHECK(m.cpuMapWrite(0xE000,mapped,0));
        const uint8_t* cur=saved.data();
        CHECK(m.loadState(cur,cur+saved.size()));
        CHECK(cur==saved.data()+saved.size());
        m.saveState(after);
        CHECK(after==saved && m.irqState());
    }
    // NES 2.0 submapper 4 must select the early silicon automatically.
    // Legacy iNES 1.0 without submapper metadata must retain the established
    // late-silicon default. Both are test-generated cartridges.
    const std::string rom=std::string(basename)+".mmc3rev4.nes";
    for (int nes2=0;nes2<2;++nes2) {
        createRom(rom.c_str(),4,8,8,0,nes2!=0,4);
        Cartridge cart(rom);
        CHECK(cart.imageValid() && cart.mapperId()==4u);
        CHECK(cart.cpuWrite(0xC000,0));
        CHECK(cart.cpuWrite(0xC001,0));
        CHECK(cart.cpuWrite(0xE001,0));
        for (int i=0;i<9;++i) cart.ppuAddressBus(0);
        for (int i=0;i<4;++i) cart.ppuAddressBus(0x1000);
        CHECK(cart.irqState());
        cart.irqClear();
        CHECK(cart.cpuWrite(0xE000,0));
        CHECK(cart.cpuWrite(0xE001,0));
        for (int i=0;i<9;++i) cart.ppuAddressBus(0);
        for (int i=0;i<4;++i) cart.ppuAddressBus(0x1000);
        CHECK(cart.irqState() == (nes2==0));
    }
    std::remove(rom.c_str());
}

// TQROM (119) CHR A16 selects RAM at both $40 and $C0 banks; A17 is
// unconnected to the 64 KiB CHR ROM. All eight RAM pages are mirrored.
static void tqrom119Focused(const char* basename) {
    MapperMmc3Variant m(8,8,119);
    uint32_t map=0;
    auto bank = [&](uint8_t reg,uint8_t value) {
        CHECK(m.cpuMapWrite(0x8000,map,reg));
        CHECK(m.cpuMapWrite(0x8001,map,value));
    };
    bank(2,0x41); // 1K RAM page 1 in pattern slot 4
    CHECK(m.ppuMapRead(0x100B,map) && map==(Mapper::CHR_RAM_FLAG|0x40Bu));
    CHECK(m.ppuMapWrite(0x100B,map,0x56) && map==(Mapper::CHR_RAM_FLAG|0x40Bu));
    bank(3,0xC1); // A17 must not disable the CHR-RAM chip
    CHECK(m.ppuMapRead(0x140B,map) && map==(Mapper::CHR_RAM_FLAG|0x40Bu));
    CHECK(m.ppuMapWrite(0x140B,map,0x78) && map==(Mapper::CHR_RAM_FLAG|0x40Bu));
    bank(4,0x81); // bit 6 cleared selects CHR-ROM page 1
    CHECK(m.ppuMapRead(0x180B,map) && map==0x40Bu);
    CHECK(!m.ppuMapWrite(0x180B,map,0x99));
    std::vector<uint8_t> before,after;
    m.saveState(before); bank(3,3);
    const uint8_t* cur=before.data();
    CHECK(m.loadState(cur,cur+before.size()));
    CHECK(cur==before.data()+before.size());
    m.saveState(after); CHECK(after==before);
    const std::string rom=std::string(basename)+".tqrom119.nes";
    createRom(rom.c_str(),119,8,8);
    {
        Cartridge cart(rom);
        uint8_t d=0;
        CHECK(cart.imageValid());
        CHECK(cart.cpuWrite(0x8000,3)); CHECK(cart.cpuWrite(0x8001,0xC1));
        CHECK(cart.ppuWrite(0x140B,0x66));
        CHECK(cart.ppuRead(0x140B,d) && d==0x66);
        CHECK(cart.cpuWrite(0x8000,2));CHECK(cart.cpuWrite(0x8001,0x41));
        CHECK(cart.ppuRead(0x100B,d) && d==0x66);
    }
    std::remove(rom.c_str());
}

// CP012: ET-113 640 KiB second-chip selection, reset mapping and physical
// 43272 solder-pad PRG A4..A0 override. Source: NESdev mapper 242.
static void et113242Focused(const char* basename) {
    uint32_t mapped=0;
    MapperCommonBoards et113(40,0,242,false,0x15);
    CHECK(!et113.cpuMapRead(0x6000,mapped));
    CHECK(et113.cpuMapRead(0x8000,mapped) && mapped==512u*1024u);
    CHECK(et113.cpuMapRead(0xC000,mapped) && mapped==512u*1024u);
    // C=0: 128 KiB replacement chip, even if outer QQ bits are set.
    CHECK(et113.cpuMapWrite(0x8264,mapped,0));
    CHECK(et113.cpuMapRead(0x8000,mapped) && mapped==uint32_t(64u+((3u*8u+1u)%8u)*2u)*8192u);
    CHECK(et113.cpuMapRead(0xC000,mapped) && mapped==uint32_t(64u+((3u*8u+7u)%8u)*2u)*8192u);
    // C=1: 512 KiB primary chip. This maps to the same physical bank as
    // the unmodified 43272 with no 128 KiB offset.
    CHECK(et113.cpuMapWrite(0x8664,mapped,0));
    CHECK(et113.cpuMapRead(0x8000,mapped) && mapped==uint32_t(3u*8u+1u)*16384u);
    CHECK(et113.cpuMapRead(0xC000,mapped) && mapped==uint32_t(3u*8u+7u)*16384u);
    // m=1 drives ROM A4..0 from five physical solder pads, not CPU A4..0.
    CHECK(et113.cpuMapWrite(0x85E4,mapped,0));
    CHECK(et113.cpuMapRead(0x802E,mapped) && (mapped&0x1FFFu)==0x35u);
    CHECK(et113.cpuMapRead(0x8011,mapped) && (mapped&0x1FFFu)==0x15u);
    CHECK(et113.cpuMapWrite(0x84E4,mapped,0));
    CHECK(et113.cpuMapRead(0x8011,mapped) && (mapped&0x1FFFu)==0x11u);
    std::vector<uint8_t> before,after;
    CHECK(et113.cpuMapWrite(0x85E4,mapped,0));
    et113.saveState(before);
    CHECK(et113.cpuMapWrite(0x8000,mapped,0));
    const uint8_t* cur=before.data();
    CHECK(et113.loadState(cur,cur+before.size()));
    CHECK(cur==before.data()+before.size());
    CHECK(et113.cpuMapRead(0x8011,mapped) && (mapped&0x1FFFu)==0x15u);
    et113.saveState(after); CHECK(after==before);
    // The 640 KiB header size selects ET-113 through genuine capacity,
    // rather than ROM title, filename or CRC.
    const std::string rom=std::string(basename)+".et113242.nes";
    createRom(rom.c_str(),242,40,0);
    {
        Cartridge cart(rom);
        uint8_t d=0;
        CHECK(cart.imageValid() && cart.mapperId()==242u);
        CHECK(cart.prgRamMem().empty());
        CHECK(cart.cpuRead(0x8000,d) && d==64u); // synthetic 8K page marker
        CHECK(cart.cpuWrite(0x8400,0));
        CHECK(cart.cpuRead(0x8000,d) && d==0u); // C=1 main chip bank 0
    }
    std::remove(rom.c_str());
}

// CP012: TLSROM/TKSROM select CIRAM A10 from the *currently active* MMC3
// CHR register wiring, including a bank inversion without a bank-data write.
static void tlsrom118Focused() {
    MapperMmc3Variant m(16,16,118);
    uint32_t mapped=0;
    uint8_t d=0;
    CHECK((m.capabilities() & MAPPER_CAP_NAMETABLE) != 0);
    auto bank = [&](uint8_t reg,uint8_t v) {
        CHECK(m.cpuMapWrite(0x8000,mapped,reg));
        CHECK(m.cpuMapWrite(0x8001,mapped,v));
    };
    bank(0,0x80); // normal: NT0 and NT1 -> CIRAM page 1
    bank(1,0x00); // normal: NT2 and NT3 -> CIRAM page 0
    CHECK(m.nametableWrite(0x200A,0xA1));
    CHECK(m.nametableWrite(0x280A,0xB2));
    CHECK(m.nametableRead(0x240A,d) && d==0xA1);
    CHECK(m.nametableRead(0x2C0A,d) && d==0xB2);
    bank(2,0x00); bank(3,0x80); bank(4,0x00); bank(5,0x80);
    CHECK(m.nametableRead(0x200A,d) && d==0xA1); // R2-R5 ignored in normal mode
    CHECK(m.cpuMapWrite(0x8000,mapped,0x80)); // inversion alone flips CIRAM routing
    CHECK(m.nametableRead(0x200A,d) && d==0xB2);
    CHECK(m.nametableRead(0x240A,d) && d==0xA1);
    CHECK(m.nametableRead(0x280A,d) && d==0xB2);
    CHECK(m.nametableRead(0x2C0A,d) && d==0xA1);
    // In inverted mode, only R2-R5 can change their respective quadrants.
    bank(4,0x80); // bank() above switches mode to normal, flip back
    CHECK(m.cpuMapWrite(0x8000,mapped,0x80));
    CHECK(m.nametableRead(0x280A,d) && d==0xA1);
    CHECK(m.cpuMapWrite(0xA000,mapped,1)); // ignored for mapper 118
    CHECK(m.nametableRead(0x280A,d) && d==0xA1);
    std::vector<uint8_t> before,after;
    m.saveState(before);
    CHECK(m.cpuMapWrite(0x8000,mapped,0));
    CHECK(m.nametableRead(0x280A,d) && d==0xB2);
    const uint8_t* cur=before.data();
    CHECK(m.loadState(cur,cur+before.size()));
    CHECK(cur==before.data()+before.size());
    CHECK(m.nametableRead(0x280A,d) && d==0xA1);
    m.saveState(after); CHECK(after==before);
    // Switching inversion after loading re-derives correct R0/R1 routing.
    CHECK(m.cpuMapWrite(0x8000,mapped,0x00));
    CHECK(m.nametableRead(0x200A,d) && d==0xA1);
    CHECK(m.nametableRead(0x280A,d) && d==0xB2);
}

// CP016: MMC5 PRG-RAM chip-select decoding, independent of PRG-ROM size.
// NESdev physical board layouts: EKROM 8K (chip0 only), ETROM 16K (two
// independent 8K chips), EWROM 32K (chip0 with 4 banks), and 64K superset.
// Changing the RAM-bank register must not wrap by ROM size or alias an absent
// second RAM chip. These are hardware invariants, not game/CRC special cases.
static void mmc5PrgRamChipSelectFocused() {
    for (unsigned bytes : {8192u, 16384u, 32768u, 65536u}) {
        Mapper005 m(2,0,bytes); // 32KB PRG-ROM deliberately smaller than SRAM.
        uint32_t map=0;
        for (unsigned bank=0;bank<8;++bank) {
            CHECK(m.cpuWriteDirect(0x5113,uint8_t(bank)));
            const bool populated = bytes>=65536u ||
                (bytes==16384u) || (bank < 4u);
            uint32_t physical= bytes==65536u ? bank :
                   bytes==32768u ? (bank&3u) :
                   bytes==16384u ? (bank>>2) : 0u;
            map=0;
            const bool read=m.cpuMapRead(0x6000,map);
            CHECK(read==populated);
            if(populated) CHECK(map==(Mapper::PRG_RAM_FLAG|physical*8192u));
            // A disabled or unpopulated work-RAM bank must swallow CPU writes.
            CHECK(m.cpuMapWrite(0x6000,map,0xA5) && (map&Mapper::PRG_RAM_FLAG)==0);
            CHECK(m.cpuWriteDirect(0x5102,2)); CHECK(m.cpuWriteDirect(0x5103,1));
            CHECK(m.cpuMapWrite(0x7FFF,map,0xA5));
            CHECK(((map&Mapper::PRG_RAM_FLAG)!=0)==populated);
            if(populated) CHECK(map==(Mapper::PRG_RAM_FLAG|physical*8192u|0x1FFFu));
            CHECK(m.cpuWriteDirect(0x5102,0));
        }
        // $5114 is one of three switchable RAM/ROM windows. RAM address bits
        // D6..D3 are not PRG-ROM bank bits and must be ignored for SRAM.
        CHECK(m.cpuWriteDirect(0x5114,0x37));
        map=0;
        CHECK(m.cpuMapRead(0x8000,map)==(bytes==16384u || bytes==65536u));
        if(bytes==16384u || bytes==65536u)
            CHECK(map==(Mapper::PRG_RAM_FLAG|(bytes>=65536u?7u*8192u:bytes==32768u?0u:8192u)));
        CHECK(m.cpuWriteDirect(0x5114,0x84));
        CHECK(m.cpuMapRead(0x8000,map) && (map&Mapper::PRG_RAM_FLAG)==0);
        // 16KB mode: register D0 ignored, CPU A13 selects next bank.
        CHECK(m.cpuWriteDirect(0x5100,1));
        CHECK(m.cpuWriteDirect(0x5115,2));
        CHECK(m.cpuMapRead(0x8000,map) && (map&Mapper::PRG_RAM_FLAG));
        const uint32_t low=map;
        CHECK(m.cpuMapRead(0xA000,map) && (map&Mapper::PRG_RAM_FLAG));
        if(bytes==32768u || bytes==65536u)
            CHECK(map==low+8192u);
        else CHECK(map==low);
        std::vector<uint8_t> saved,after;
        m.saveState(saved);
        CHECK(m.cpuWriteDirect(0x5115,6));
        const uint8_t* p=saved.data();
        CHECK(m.loadState(p,p+saved.size()) && p==saved.data()+saved.size());
        m.saveState(after);
        CHECK(saved==after);
    }
}

// CP012: verify both VRC6 register address permutations and IRQ divider
// modes, including long cycle-mode runs that previously overflowed phase.
static void vrc6IrqFocused() {
    for (uint16_t id : {uint16_t(24),uint16_t(26)}) {
        MapperVrc6 m(16,8,id);
        uint32_t mapped=0;
        const uint16_t control = id == 26 ? 0xF002u : 0xF001u;
        const uint16_t ack = id == 26 ? 0xF001u : 0xF002u;
        CHECK(m.cpuMapWrite(0xF000,mapped,0xFF));
        CHECK(m.cpuMapWrite(control,mapped,0x02)); // scanline, enabled
        for (int i=0;i<113;++i) m.cpuClock();
        CHECK(!m.irqState());
        m.cpuClock(); // 114th CPU cycle: 341 - 114*3 <= 0
        CHECK(m.irqState());
        CHECK(m.cpuMapWrite(ack,mapped,0)); // A=0 disables
        CHECK(!m.irqState());
        for (int i=0;i<350;++i) m.cpuClock();
        CHECK(!m.irqState());
        CHECK(m.cpuMapWrite(control,mapped,0x07)); // cycle, enable, retain on ack
        CHECK(!m.irqState());
        m.cpuClock(); // cycle mode bypasses divider
        CHECK(m.irqState());
        CHECK(m.cpuMapWrite(ack,mapped,0));
        CHECK(!m.irqState());
        m.cpuClock();
        CHECK(m.irqState());
        m.irqClear();
        for (int i=0;i<50000;++i) m.cpuClock();
        std::vector<uint8_t> before,after;
        m.saveState(before);
        // Offset is the documented VRC6 mapper-state layout: 4 PRG u16s,
        // 8 pairs of CHR u16s, banking byte, 4 nametable bytes, NT select,
        // 2 KiB CIRAM, IRQ latch and counter. Cycle mode never advances its
        // 341-dot prescaler, even over > 32k CPU clocks.
        const size_t phaseOffset=4u*2u + 8u*4u + 1u + 4u + 1u + 0x800u + 2u;
        CHECK(before.size()>phaseOffset+1u);
        CHECK((uint16_t(before[phaseOffset]) | (uint16_t(before[phaseOffset+1u])<<8)) == 341u);
        for (int i=0;i<500;++i) m.cpuClock();
        const uint8_t* cur=before.data();
        CHECK(m.loadState(cur,cur+before.size()));
        CHECK(cur==before.data()+before.size());
        m.saveState(after);
        CHECK(before==after);
        // Counter clocks at every cycle; IRQ phase must reproduce from state.
        std::vector<uint8_t> expected;
        for (int i=0;i<1024;++i) m.cpuClock();
        m.saveState(expected);
        cur=before.data(); CHECK(m.loadState(cur,cur+before.size()));
        for (int i=0;i<1024;++i) m.cpuClock();
        after.clear(); m.saveState(after); CHECK(after==expected);
    }
}

static void registrySmoke(const char* path){
    size_t success=0;
    for(int id:ids){
        // NES 2.0 is required to encode mapper IDs above 255.
        createRom(path,id,8,8,0,id>255);
        Cartridge cart(path);
        if(!cart.imageValid() || cart.mapperId()!=id) { ++failures; std::cerr<<"REGISTRY FAIL mapper "<<id<<"\n"; continue; }
        ++success;
        uint8_t value=0;
        for(uint16_t a:{uint16_t(0x8000),uint16_t(0xA000),uint16_t(0xC000),uint16_t(0xE000)}) (void)cart.cpuRead(a,value);
        for(uint16_t a:{uint16_t(0),uint16_t(0x0800),uint16_t(0x1000),uint16_t(0x1800)}) (void)cart.ppuRead(a,value);
        std::vector<uint8_t> snapshot, restored;
        cart.saveState(snapshot);
        // Exercise bank/register writes and restore the complete mutable state.
        for(uint16_t a:{uint16_t(0x6000),uint16_t(0x6008),uint16_t(0x7EF0),uint16_t(0x8000),uint16_t(0x8001),uint16_t(0xA000),uint16_t(0xC000),uint16_t(0xE000)})
            (void)cart.cpuWrite(a,0x03);
        const uint8_t* p=snapshot.data();
        CHECK(cart.loadState(p,p+snapshot.size()));
        CHECK(p==snapshot.data()+snapshot.size());
        cart.saveState(restored);
        CHECK(snapshot==restored);
    }
    std::cout<<"Registry smoke: "<<success<<"/"<<sizeof(ids)/sizeof(ids[0])<<" iNES mappers load\n";
}
static void discreteBusConflictTests(const char* path){
    for(int mapper:{2,3,7,11}){
        // NES 2.0 submapper 1 bypasses ROM-data conflicts; submapper 2
        // must AND the written bank number with the byte visible in ROM.
        createRom(path,mapper,8,8,0,true,1);
        Cartridge clean(path);
        CHECK(clean.imageValid());
        uint8_t value=0;
        CHECK(clean.cpuWrite(0x8000,mapper==11?0x11:3));
        if(mapper==3){CHECK(clean.ppuRead(0,value) && value==24);} 
        else if(mapper==2){CHECK(clean.cpuRead(0x8000,value) && value==6);}
        else if(mapper==7){CHECK(clean.cpuRead(0x8000,value) && value==12);}
        else if(mapper==11){CHECK(clean.cpuRead(0x8000,value) && value==4);}
        createRom(path,mapper,8,8,0,true,2);
        Cartridge conflict(path);
        CHECK(conflict.imageValid());
        CHECK(conflict.cpuWrite(0x8000,mapper==11?0x11:3));
        if(mapper==3){CHECK(conflict.ppuRead(0,value) && value==0);} 
        else{CHECK(conflict.cpuRead(0x8000,value) && value==0);}
    }
    createRom(path,66);
    Cartridge gnrom(path);
    CHECK(gnrom.imageValid());
    uint8_t value=99;
    CHECK(gnrom.cpuWrite(0x8000,0x10));
    CHECK(gnrom.cpuRead(0x8000,value) && value==0);
}

static void namcoEndToEndPersistenceAndPpu(const char* uniqueBase){
    const std::string rom = std::string(uniqueBase) + ".n163_battery.nes";
    std::string stem = rom.substr(rom.find_last_of("/\\") + 1);
    stem.resize(stem.find_last_of('.'));
    const std::string sav = "Saves/" + stem + ".SAV";
    std::remove(rom.c_str()); std::remove(sav.c_str());
    createRom(rom.c_str(),19,2,2,0,true,2);
    // NES 2.0 chip-only battery: no external PRG RAM/NVRAM allocated.
    {
        std::fstream io(rom.c_str(),std::ios::binary|std::ios::in|std::ios::out);
        unsigned char battery = static_cast<unsigned char>((19u & 15u)<<4u | 0x02u);
        io.seekp(6);io.write(reinterpret_cast<char*>(&battery),1);
    }
    {
        Cartridge cart(rom);
        CHECK(cart.imageValid());
        CHECK(cart.prgRamMem().empty());
        uint8_t value=0;
        CHECK(!cart.cpuRead(0x6000,value));
        CHECK(cart.cpuWrite(0xF800,0x07));
        CHECK(cart.cpuWrite(0x4800,0x93));
        CHECK(cart.cpuWrite(0x8000,0xE0));
        CHECK(cart.ppuWrite(0x0038,0x44));
        CHECK(cart.cpuWrite(0xC000,0xE0));
        CHECK(cart.ppuRead(0x2038,value) && value==0x44);
        CHECK(cart.cpuWrite(0xC800,0x03));
        CHECK(cart.ppuRead(0x2400,value) && value==3);
        CHECK(!cart.ppuWrite(0x2400,0x92)); // ignored by CHR ROM
        CHECK(cart.ppuRead(0x2400,value) && value==3);
        cart.flushSave();
    }
    {
        Cartridge cart(rom);
        CHECK(cart.imageValid());
        uint8_t value=0;
        CHECK(cart.cpuWrite(0xF800,0x07));
        CHECK(cart.cpuRead(0x4800,value) && value==0x93);
    }
    std::ifstream in(sav.c_str(),std::ios::binary|std::ios::ate);
    CHECK(in.good() && in.tellg() == std::streampos(128));
    in.close();
    std::remove(sav.c_str());
    std::remove(rom.c_str());
}

static void namcoAudioIntegration(const char* uniqueBase){
    const std::string rom=std::string(uniqueBase)+".n163_audio.nes";
    createRom(rom.c_str(),19,2,1,0,true,5);
    auto cart=std::make_shared<Cartridge>(rom);
    CHECK(cart->imageValid());
    Bus bus;
    bus.insertCartridge(cart);
    bus.reset(true);
    bus.apu.setSampleRate(44100.0);
    // Two different nibble samples: frequency selects a new nibble each update.
    CHECK(cart->cpuWrite(0xF800,0x00));
    CHECK(cart->cpuWrite(0x4800,0x0F));
    const uint8_t audioRegs[8]={0x00,0,0x00,0,0xFD,0,0,0x0F};
    CHECK(cart->cpuWrite(0xF800,0xF8));
    for(uint8_t v:audioRegs) CHECK(cart->cpuWrite(0x4800,v));
    // 0x1FFFF frequency; length is 4 samples. Phase varies after updates.
    CHECK(cart->cpuWrite(0xF800,0x78));
    CHECK(cart->cpuWrite(0x4800,0xFF));
    CHECK(cart->cpuWrite(0xF800,0x7A));
    CHECK(cart->cpuWrite(0x4800,0xFF));
    CHECK(cart->cpuWrite(0xF800,0x7C));
    CHECK(cart->cpuWrite(0x4800,0xFF));
    CHECK(cart->cpuWrite(0xE000,0x00));
    for(int i=0;i<240000;i++) bus.clock();
    int16_t pcm[4096]{};
    const int count=bus.apu.copyAndClearSamplesI16(pcm,4096);
    CHECK(count>100);
    int low=32767, high=-32768;
    for(int i=0;i<count;i++){low=std::min(low,int(pcm[i]));high=std::max(high,int(pcm[i]));}
    CHECK(high-low>40); // Ensures per-sample expansion mixing, not one DC value per frame.
    cart.reset();
    std::remove(rom.c_str());
}


// Taito X1-017's two established header numbers encode different ROM
// wiring. Test both directly and through the complete NES 2.0 loader.
static void taitoX1017WiringFocused(const char* base) {
    uint32_t mapped=0;
    Mapper082 legacy(32,32,false), actual(32,32,true);
    CHECK(legacy.cpuMapWrite(0x7EFA,mapped,0x3C));
    CHECK(legacy.cpuMapRead(0x8000,mapped) && mapped==15u*0x2000u);
    CHECK(actual.cpuMapWrite(0x7EFA,mapped,0x3C)); // 0b111100 -> reverse6 = 0b001111
    CHECK(actual.cpuMapRead(0x8000,mapped) && mapped==15u*0x2000u);
    CHECK(actual.cpuMapWrite(0x7EFA,mapped,0x01)); // D0 -> PRG bank bit 5
    CHECK(actual.cpuMapRead(0x8000,mapped) && mapped==32u*0x2000u);
    CHECK(legacy.cpuMapWrite(0x7EFA,mapped,0x01));
    CHECK(legacy.cpuMapRead(0x8000,mapped) && mapped==0u);
    CHECK(actual.cpuMapWrite(0x7EFB,mapped,0x20)); // D5 -> PRG bank bit 0
    CHECK(actual.cpuMapRead(0xA000,mapped) && mapped==0x2000u);
    CHECK(actual.cpuMapRead(0xE000,mapped) && mapped==63u*0x2000u);
    CHECK(actual.cpuMapWrite(0x7EF7,mapped,0xCA));
    CHECK(actual.cpuMapWrite(0x7EF8,mapped,0x69));
    CHECK(actual.cpuMapWrite(0x7EF9,mapped,0x84));
    CHECK(actual.cpuMapRead(0x6000,mapped) && mapped==Mapper::PRG_RAM_FLAG);
    CHECK(actual.cpuMapRead(0x6FFF,mapped) && mapped==(Mapper::PRG_RAM_FLAG|0xFFFu));
    CHECK(actual.cpuMapRead(0x73FF,mapped) && mapped==(Mapper::PRG_RAM_FLAG|0x13FFu));
    CHECK(!actual.cpuMapRead(0x7400,mapped));
    CHECK(actual.cpuMapWrite(0x7EF9,mapped,0x00));
    CHECK(!actual.cpuMapRead(0x7000,mapped));
    roundtrip(actual,0x7EFA,0x07);
    roundtrip(legacy,0x7EFA,0x07);

    const std::string rom=std::string(base)+".x1017_552.nes";
    createRom(rom.c_str(),552,32,32,0,true);
    {
        Cartridge c(rom);
        CHECK(c.imageValid() && c.mapperId()==552 && c.prgRamMem().size()==5120u);
        uint8_t value=0;
        CHECK(c.cpuWrite(0x7EFA,0x01));
        CHECK(c.cpuRead(0x8000,value) && value==32);
        CHECK(c.cpuWrite(0x7EFB,0x20));
        CHECK(c.cpuRead(0xA000,value) && value==1);
        CHECK(c.cpuWrite(0x7EF7,0xCA));
        CHECK(c.cpuWrite(0x7EF8,0x69));
        CHECK(c.cpuWrite(0x7EF9,0x84));
        CHECK(c.cpuWrite(0x73FF,0x92));
        CHECK(c.cpuRead(0x73FF,value) && value==0x92);
        CHECK(c.cpuWrite(0x7EF9,0x00));
        CHECK(c.cpuRead(0x73FF,value) && value==0x00);
        CHECK(c.cpuRead(0x7EFD,value) && value==0x00);
        std::vector<uint8_t> state,after;
        c.saveState(state);
        CHECK(c.cpuWrite(0x7EFA,0x20));
        const uint8_t* ptr=state.data();
        CHECK(c.loadState(ptr,ptr+state.size()) && ptr==state.data()+state.size());
        CHECK(c.cpuRead(0x8000,value) && value==32);
        c.saveState(after); CHECK(state==after);
    }
    createRom(rom.c_str(),82,8,8);
    {
        Cartridge c(rom);
        CHECK(c.imageValid() && c.mapperId()==82 && c.prgRamMem().size()==5120u);
        CHECK(c.cpuWrite(0x7EFA,0x3C));
        uint8_t value=0;
        CHECK(c.cpuRead(0x8000,value) && value==15);
    }
    std::remove(rom.c_str());
}

static void nes2HeaderTests(const char* path){
    // 0xF is the size MSB sentinel, NOT a comparison against the low count.
    createRom(path,0,15,1,1,true);
    std::ostringstream expectedError;
    std::streambuf* const originalError = std::cerr.rdbuf(expectedError.rdbuf());
    Cartridge extended(path);
    std::cerr.rdbuf(originalError);
    CHECK(!extended.imageValid()); // fixture has 15 banks while header requests 271
    CHECK(expectedError.str().find("Failed reading PRG data") != std::string::npos);
    // An exponent of 18 and multiplier 1 is 256 KiB / 16 KiB = 16 banks.
    createRom(path,0,16,0,0x0F,true);
    // Construct above using an exponent value in the PRG LSB instead:
    std::fstream patch(path,std::ios::binary|std::ios::in|std::ios::out);
    unsigned char b=18u*4u; patch.seekp(4);patch.write(reinterpret_cast<char*>(&b),1);patch.close();
    Cartridge exponent(path);
    CHECK(exponent.imageValid() && exponent.prgBanks()==16);
    // NES 2.0 8 MiB PRG, mapper 28: end-to-end cartridge allocation and
    // physical CHR-RAM mapping, beyond standalone mapper register tests.
    createRom(path,28,512,0,2,true);
    Cartridge action53(path);
    CHECK(action53.imageValid() && action53.prgBanks()==512);
    CHECK(action53.chrMem().size()==32768u);
    CHECK(action53.cpuWrite(0x5000,0x81));
    CHECK(action53.cpuWrite(0x8000,0xFF));
    uint8_t value=0;
    CHECK(action53.cpuRead(0xC000,value) && value==254u);
    CHECK(action53.cpuWrite(0x5000,0x00));
    CHECK(action53.cpuWrite(0x8000,0x03));
    CHECK(action53.ppuWrite(0x0010,0xA5));
    CHECK(action53.cpuWrite(0x8000,0x00));
    CHECK(action53.ppuRead(0x0010,value) && value==0);
    CHECK(action53.cpuWrite(0x8000,0x03));
    CHECK(action53.ppuRead(0x0010,value) && value==0xA5);
    // NES 2.0 submapper 1 of mapper 32 is an electrically different board.
    createRom(path,32,8,8,0,true,1);
    Cartridge majorLeague(path);
    CHECK(majorLeague.imageValid());
    CHECK(majorLeague.mirroring()==Cartridge::Mirror::ONE_SCREEN_HI);
    CHECK(majorLeague.cpuWrite(0x9000,0x03));
    CHECK(majorLeague.mirroring()==Cartridge::Mirror::ONE_SCREEN_HI);
}

// CPU bit-banged master connected to an LZ93D50 X24C02. The readback
// path is deliberately independent of direct EEPROM-memory inspection.
class BandaiI2cMaster {
    std::function<bool(uint8_t)> writePort;
    std::function<bool(uint8_t&)> readPort;
public:
    BandaiI2cMaster(std::function<bool(uint8_t)> w, std::function<bool(uint8_t&)> r)
        : writePort(w),readPort(r) {}
    void line(bool clock, bool data, bool release=false) {
        CHECK(writePort(uint8_t((clock?0x20:0) | (data?0x40:0) | (release?0x80:0))));
    }
    bool sampled() {
        uint8_t value=0;
        CHECK(readPort(value));
        return (value & 0x10u)!=0;
    }
    void start() {
        line(false,true); line(true,true); line(true,false); line(false,false);
    }
    void stop() {
        line(false,false); line(true,false); line(true,true);
    }
    bool byteAck(uint8_t value) {
        for(int i=7;i>=0;--i) {
            const bool bit=((value>>i)&1u)!=0;
            line(false,bit); line(true,bit); line(false,bit);
        }
        line(false,true,true);
        line(true,true,true);
        const bool acknowledged = !sampled();
        line(false,true,true);
        return acknowledged;
    }
    void byte(uint8_t value) { CHECK(byteAck(value)); }
    uint8_t readByte(bool acknowledge) {
        uint8_t result=0;
        for(int i=7;i>=0;--i) {
            line(false,true,true); line(true,true,true);
            result = uint8_t((result<<1) | uint8_t(sampled()));
            line(false,true,true);
        }
        line(false,!acknowledge);
        line(true,!acknowledge);
        line(false,!acknowledge);
        return result;
    }
    void writeByte(uint8_t address,uint8_t value) {
        start(); byte(0xA0); byte(address); byte(value); stop();
    }
    uint8_t readAt(uint8_t address) {
        start(); byte(0xA0); byte(address);
        start(); byte(0xA1);
        const uint8_t value=readByte(false);
        stop(); return value;
    }
};

static void bandaiI2cFocused(){
    uint32_t ignored=0;
    Mapper016 oldBoard(8,4,4,true); // FCG-1/2 never gains an EEPROM.
    CHECK(oldBoard.persistentRamSize()==0);
    Mapper016 noChip(8,4,5,false);
    CHECK(noChip.persistentRamSize()==0);
    Mapper016 eeprom(8,4,5,true);
    CHECK(eeprom.persistentRamSize()==256);
    CHECK((eeprom.capabilities()&MAPPER_CAP_CPU_DIRECT)!=0);
    BandaiI2cMaster i2c(
        [&](uint8_t v){return eeprom.cpuMapWrite(0x800D,ignored,v);},
        [&](uint8_t& v){return eeprom.cpuReadDirect(0x6000,v);});
    i2c.writeByte(0x63,0x57);
    for(int i=0;i<8950;i++)eeprom.cpuClock();
    CHECK(eeprom.persistentRamDirty());
    CHECK(i2c.readAt(0x63)==0x57);
    CHECK(i2c.readAt(0x64)==0xFF);
    i2c.start();
    CHECK(!i2c.byteAck(0xA2)); // not device address 000
    i2c.stop();
    // Do not commit a partial write on repeated START; only STOP commits.
    i2c.start(); i2c.byte(0xA0); i2c.byte(0x65); i2c.byte(0x44);
    i2c.start(); i2c.byte(0xA0); i2c.byte(0x64);
    i2c.start(); i2c.byte(0xA1);
    CHECK(i2c.readByte(false)==0xFF);
    i2c.stop();
    CHECK(i2c.readAt(0x65)==0xFF);
    // Xicor 24C02 uses four-byte *pages*, not the eight-byte
    // page found in some generic 24C02 devices. Crossing $FF wraps
    // back to $FC during a page write.
    i2c.start(); i2c.byte(0xA0); i2c.byte(0xFE);
    i2c.byte(0x12); i2c.byte(0x34); i2c.byte(0x56); i2c.stop();
    for(int i=0;i<8950;i++)eeprom.cpuClock();
    CHECK(i2c.readAt(0xFE)==0x12);
    CHECK(i2c.readAt(0xFF)==0x34);
    CHECK(i2c.readAt(0xFC)==0x56);
    CHECK(i2c.readAt(0x00)==0xFF);
    // Sequential read: ACK first byte to continue, NACK second to stop.
    i2c.start(); i2c.byte(0xA0); i2c.byte(0xFE);
    i2c.start(); i2c.byte(0xA1);
    CHECK(i2c.readByte(true)==0x12);
    CHECK(i2c.readByte(false)==0x34);
    i2c.stop();
    // Reset keeps EEPROM bytes but resets the serial transaction.
    eeprom.reset();
    CHECK(i2c.readAt(0x63)==0x57);
    std::vector<uint8_t> snap,after;
    eeprom.saveState(snap);
    i2c.writeByte(0x63,0xA5);
    for(int i=0;i<8950;i++)eeprom.cpuClock();
    const uint8_t* p=snap.data();
    CHECK(eeprom.loadState(p,p+snap.size()));
    CHECK(p==snap.data()+snap.size());
    eeprom.saveState(after);
    CHECK(snap==after);
    CHECK(i2c.readAt(0x63)==0x57);
    // Mid-byte save-state round-trip must retain the serial shift register.
    i2c.start(); i2c.byte(0xA0); i2c.byte(0x62);
    // Send the first four high bits (0xB...); freeze transaction at SCL low.
    for (int i=7; i>=4; --i) {
        const bool bit=((0xB5u>>i)&1u)!=0;
        i2c.line(false,bit);i2c.line(true,bit);i2c.line(false,bit);
    }
    std::vector<uint8_t> midState;
    eeprom.saveState(midState);
    for (int i=3;i>=0;--i) {
        const bool bit=((0xB5u>>i)&1u)!=0;
        i2c.line(false,bit);i2c.line(true,bit);i2c.line(false,bit);
    }
    // Restart and restore the state from the original byte boundary.
    i2c.start();
    const uint8_t* restore=midState.data();
    CHECK(eeprom.loadState(restore,restore+midState.size()));
    // Continue the half-transmitted byte, checking the ACK and committed data.
    for (int i=3;i>=0;--i) {
        const bool bit=((0xB5u>>i)&1u)!=0;
        i2c.line(false,bit);i2c.line(true,bit);i2c.line(false,bit);
    }
    i2c.line(false,true,true);i2c.line(true,true,true);
    CHECK(!i2c.sampled());i2c.line(false,true,true);
    i2c.stop();
    for(int i=0;i<8950;i++)eeprom.cpuClock();
    CHECK(i2c.readAt(0x62)==0xB5);
    uint8_t buf[256]{};
    eeprom.copyPersistentRam(buf,sizeof(buf));
    CHECK(buf[0x63]==0x57 && buf[0xFE]==0x12 && buf[0xFF]==0x34 && buf[0xFC]==0x56);
    eeprom.clearPersistentRamDirty();
    CHECK(!eeprom.persistentRamDirty());
    Mapper016 newlyLoaded(8,4,5,true);
    newlyLoaded.restorePersistentRam(buf,sizeof(buf));
    CHECK(!newlyLoaded.persistentRamDirty());
    BandaiI2cMaster second(
        [&](uint8_t v){return newlyLoaded.cpuMapWrite(0x800D,ignored,v);},
        [&](uint8_t& v){return newlyLoaded.cpuReadDirect(0x7FFF,v);});
    CHECK(second.readAt(0x63)==0x57);
    // IRQ: LZ latches on $A; FCG counter writes are live. Underflow only.
    CHECK(eeprom.cpuMapWrite(0x800B,ignored,1));
    CHECK(eeprom.cpuMapWrite(0x800C,ignored,0));
    CHECK(eeprom.cpuMapWrite(0x800A,ignored,1));
    CHECK(!eeprom.irqState());
    eeprom.cpuClock(); CHECK(!eeprom.irqState());
    eeprom.cpuClock(); CHECK(eeprom.irqState());
    CHECK(eeprom.cpuMapWrite(0x800A,ignored,0)); CHECK(!eeprom.irqState());
}

static void bandaiSaveRoundtrip(const char* basename){
    const std::string rom=std::string(basename)+".bandai24c02.nes";
    std::string stem=rom.substr(rom.find_last_of("/\\")+1);
    stem.resize(stem.find_last_of('.'));
    const std::string sav="Saves/"+stem+".SAV";
    std::remove(rom.c_str()); std::remove(sav.c_str());
    createRom(rom.c_str(),16,4,4,0,true,5);
    {std::fstream file(rom.c_str(),std::ios::in|std::ios::out|std::ios::binary);
      unsigned char nvramByte=0x20;file.seekp(10);file.write(reinterpret_cast<const char*>(&nvramByte),1);}
    {
        Cartridge cart(rom);
        CHECK(cart.imageValid());
        CHECK(cart.prgRamMem().empty());
        BandaiI2cMaster i2c(
            [&](uint8_t v){return cart.cpuWrite(0x800D,v);},
            [&](uint8_t& v){return cart.cpuRead(0x6000,v);});
        i2c.writeByte(0x35,0x9D);
        for(int i=0;i<8950;i++)cart.cpuClock();
        CHECK(i2c.readAt(0x35)==0x9D);
        cart.flushSave();
    }
    {
        Cartridge cart(rom);
        CHECK(cart.imageValid());
        BandaiI2cMaster i2c(
            [&](uint8_t v){return cart.cpuWrite(0x800D,v);},
            [&](uint8_t& v){return cart.cpuRead(0x6000,v);});
        CHECK(i2c.readAt(0x35)==0x9D);
    }
    std::ifstream input(sav.c_str(),std::ios::binary|std::ios::ate);
    CHECK(input.good() && input.tellg()==std::streampos(256));
    input.close();
    std::remove(sav.c_str());std::remove(rom.c_str());
}


// Dedicated board contracts: no copyrighted ROMs or internal EEPROM pokes.
static void bandaiBoardVariantTests(const char* basename) {
    uint32_t mapped=0;
    // Famicom Jump II, mapper 153. $8000..$8003 select 256K half;
    // $8008 selects lower 16K; upper 16K fixed within that half.
    Mapper016 jump2(32,0,5,false,153);
    CHECK(jump2.cpuMapRead(0x8000,mapped) && mapped==0);
    CHECK(jump2.cpuMapRead(0xC000,mapped) && mapped==15u*0x4000u);
    CHECK(jump2.cpuMapWrite(0x8001,mapped,1));
    CHECK(jump2.cpuMapRead(0xC000,mapped) && mapped==31u*0x4000u);
    CHECK(jump2.cpuMapWrite(0x8008,mapped,5));
    CHECK(jump2.cpuMapRead(0x8000,mapped) && mapped==21u*0x4000u);
    CHECK(jump2.ppuMapRead(0x1507,mapped) && mapped==0x1507u);
    CHECK(jump2.ppuMapWrite(0x1507,mapped,0x70) && mapped==0x1507u);
    CHECK(!jump2.cpuMapRead(0x6000,mapped));
    CHECK(!jump2.cpuMapWrite(0x6000,mapped,0xC7));
    CHECK(jump2.cpuMapWrite(0x800D,mapped,0x20));
    CHECK(jump2.cpuMapRead(0x6000,mapped) && mapped==Mapper::PRG_RAM_FLAG);
    CHECK(jump2.cpuMapWrite(0x7FFF,mapped,0x44) && mapped==(Mapper::PRG_RAM_FLAG|0x1FFFu));
    CHECK(jump2.cpuMapWrite(0x800D,mapped,0));
    CHECK(!jump2.cpuMapRead(0x6000,mapped));
    roundtrip(jump2,0x8008,11);

    // Mapper 159: original X24C01, 128 bytes, word-address/RW command.
    Mapper016 x01(8,8,5,true,159);
    CHECK(x01.persistentRamSize()==128);
    BandaiI2cMaster i2c01(
        [&](uint8_t v){return x01.cpuMapWrite(0x800D,mapped,v);},
        [&](uint8_t& v){return x01.cpuReadDirect(0x6000,v);});
    i2c01.start();i2c01.byte(uint8_t(0x52u<<1));i2c01.byte(0xA6);i2c01.stop();
    CHECK(x01.persistentRamDirty());
    // 5 ms write-busy window NACKs the command on the ninth SCL edge.
    i2c01.start();CHECK(!i2c01.byteAck(uint8_t(0x52u<<1)));i2c01.stop();
    for(int i=0;i<8949;i++)x01.cpuClock();
    i2c01.start();CHECK(!i2c01.byteAck(uint8_t(0x52u<<1)));i2c01.stop();
    x01.cpuClock();
    i2c01.start();CHECK(i2c01.byteAck(uint8_t((0x52u<<1)|1)));CHECK(i2c01.readByte(false)==0xA6);i2c01.stop();
    // Four-byte page wrap $7F->$7C for X24C01, not at $80.
    i2c01.start();i2c01.byte(uint8_t(0x7Eu<<1));i2c01.byte(0x41);i2c01.byte(0x42);
    i2c01.byte(0x43);i2c01.stop();
    for(int i=0;i<8950;i++)x01.cpuClock();
    i2c01.start();i2c01.byte(uint8_t((0x7Cu<<1)|1));
    CHECK(i2c01.readByte(false)==0x43);i2c01.stop();
    uint8_t eeprom01[128]{};
    x01.copyPersistentRam(eeprom01,sizeof(eeprom01));
    CHECK(eeprom01[0x52]==0xA6&&eeprom01[0x7E]==0x41&&eeprom01[0x7F]==0x42&&eeprom01[0x7C]==0x43);
    roundtrip(x01,0x8008,2);

    // Datach 157: internal X24C02, external X24C01 with independent
    // clocks but wired-AND D4. Unbanked 8 KiB CHR RAM.
    Mapper016 datach(8,0,5,true,157,true);
    CHECK(datach.persistentRamSize()==128);
    CHECK(datach.sharedPersistentRamSize()==256);
    CHECK(datach.ppuMapWrite(0x1ABC,mapped,0x33)&&mapped==0x1ABCu);
    BandaiI2cMaster mainChip(
        [&](uint8_t v){return datach.cpuMapWrite(0x800D,mapped,v);},
        [&](uint8_t& v){return datach.cpuReadDirect(0x6000,v);});
    mainChip.writeByte(0x24,0x9E);
    for(int i=0;i<8950;i++)datach.cpuClock();
    CHECK(mainChip.readAt(0x24)==0x9E);
    uint8_t mainBacking[256]{};
    datach.copySharedPersistentRam(mainBacking,256);
    CHECK(mainBacking[0x24]==0x9E);
    BandaiI2cMaster subChip(
        [&](uint8_t v){
            // SCL for main unit held high (bit 5), while $8000.3
            // toggles only the subcartridge EEPROM's SCL.
            bool ok=datach.cpuMapWrite(0x800D,mapped,uint8_t(v|0x20u));
            return datach.cpuMapWrite(0x8000,mapped,(v&0x20u)?8:0)&&ok;
        },
        [&](uint8_t& v){return datach.cpuReadDirect(0x6FFF,v);});
    subChip.start();subChip.byte(uint8_t(0x35u<<1));subChip.byte(0x7B);subChip.stop();
    for(int i=0;i<8950;i++)datach.cpuClock();
    subChip.start();subChip.byte(uint8_t((0x35u<<1)|1));
    CHECK(subChip.readByte(false)==0x7B);subChip.stop();
    uint8_t subBacking[128]{};
    datach.copyPersistentRam(subBacking,128);
    CHECK(subBacking[0x35]==0x7B);
    datach.copySharedPersistentRam(mainBacking,256);
    CHECK(mainBacking[0x24]==0x9E);
    // Digital barcode input, valid UPC-A checksum and transitions.
    CHECK(!datach.queueDatachBarcode("012345678901"));
    CHECK(datach.queueDatachBarcode("036000291452"));
    CHECK(datach.queueDatachBarcode("4006381333931")); // EAN-13 example
    CHECK(datach.queueDatachBarcode("96385074")); // EAN-8 example
    CHECK(datach.queueDatachBarcode("036000291452")); // UPC-A as final stimulus
    uint8_t busData=0;
    CHECK(datach.cpuReadDirect(0x6000,busData)&&(busData&0x08u)!=0);
    for(int i=0;i<11000;i++)datach.cpuClock(); // 11 quiet-zone modules
    CHECK(datach.cpuReadDirect(0x6000,busData)&&(busData&0x08u)==0); // start guard bar
    for(int i=0;i<1000;i++)datach.cpuClock();
    CHECK(datach.cpuReadDirect(0x6000,busData)&&(busData&0x08u)!=0); // second guard module
    roundtrip(datach,0x8008,9);

    // Mapper 153 WRAM survives a cartridge destruction and reload.
    const std::string jumpRom=std::string(basename)+".m153.nes";
    std::string stem=jumpRom.substr(jumpRom.find_last_of("/\\")+1);
    stem.resize(stem.find_last_of('.'));
    std::string save="Saves/"+stem+".SAV";
    std::remove(save.c_str());
    createRom(jumpRom.c_str(),153,32,0,0,true);
    { std::fstream h(jumpRom.c_str(),std::ios::in|std::ios::out|std::ios::binary);
      uint8_t nvram=0x70;h.seekp(10);h.write(reinterpret_cast<char*>(&nvram),1); }
    {
        Cartridge c(jumpRom);CHECK(c.imageValid()&&c.prgRamMem().size()==8192);
        CHECK(c.prgRamMem()[0]==0xFF);
        CHECK(c.cpuWrite(0x800D,0x20));CHECK(c.cpuWrite(0x600F,0xBE));
        c.flushSave();
    }
    {
        Cartridge c(jumpRom);uint8_t v=0;
        CHECK(c.imageValid());CHECK(c.cpuWrite(0x800D,0x20));
        CHECK(c.cpuRead(0x600F,v)&&v==0xBE);
    }
    {std::ifstream f(save.c_str(),std::ios::binary|std::ios::ate);CHECK(f.good()&&f.tellg()==std::streampos(8192));}
    std::remove(save.c_str());std::remove(jumpRom.c_str());

    // X24C01 battery save is 128 bytes, not a fabricated 8 KiB PRG RAM.
    const std::string dbzRom=std::string(basename)+".m159.nes";
    stem=dbzRom.substr(dbzRom.find_last_of("/\\")+1);
    stem.resize(stem.find_last_of('.'));save="Saves/"+stem+".SAV";
    std::remove(save.c_str());createRom(dbzRom.c_str(),159,4,4,0,true);
    {std::fstream h(dbzRom.c_str(),std::ios::in|std::ios::out|std::ios::binary);
     uint8_t nvram=0x10;h.seekp(10);h.write(reinterpret_cast<char*>(&nvram),1);}
    {
        Cartridge c(dbzRom);CHECK(c.imageValid()&&c.prgRamMem().empty());
        BandaiI2cMaster m(
            [&](uint8_t v){return c.cpuWrite(0x800D,v);},
            [&](uint8_t& v){return c.cpuRead(0x6000,v);});
        m.start();m.byte(uint8_t(0x45u<<1));m.byte(0xB1);m.stop();
        for(int i=0;i<8950;i++)c.cpuClock();
        c.flushSave();
    }
    {
        Cartridge c(dbzRom);CHECK(c.imageValid());
        BandaiI2cMaster m(
            [&](uint8_t v){return c.cpuWrite(0x800D,v);},
            [&](uint8_t& v){return c.cpuRead(0x6000,v);});
        m.start();m.byte(uint8_t((0x45u<<1)|1));
        CHECK(m.readByte(false)==0xB1);m.stop();
    }
    {std::ifstream f(save.c_str(),std::ios::binary|std::ios::ate);CHECK(f.good()&&f.tellg()==std::streampos(128));}
    std::remove(save.c_str());std::remove(dbzRom.c_str());

    // Datach main unit save MUST be shared and separate from the optional
    // game/subcartridge 128-byte .SAV. Use a private override for the test.
    const std::string shared=std::string(basename)+".datach_unit.sav";
    const std::string datachRom=std::string(basename)+".datach157.nes";
    stem=datachRom.substr(datachRom.find_last_of("/\\")+1);
    stem.resize(stem.find_last_of('.'));save="Saves/"+stem+".SAV";
    std::remove(shared.c_str());std::remove(save.c_str());
    const char* previousDatach = std::getenv("CIPHERNES_DATACH_SAVE_PATH");
    const std::string previousDatachPath=previousDatach ? previousDatach : "";
    setenv("CIPHERNES_DATACH_SAVE_PATH",shared.c_str(),1);
    createRom(datachRom.c_str(),157,4,0,0,true);
    {std::fstream h(datachRom.c_str(),std::ios::in|std::ios::out|std::ios::binary);
     uint8_t nvram=0x10;h.seekp(10);h.write(reinterpret_cast<char*>(&nvram),1);}
    {
        Cartridge c(datachRom);CHECK(c.imageValid()&&c.prgRamMem().empty());
        BandaiI2cMaster m(
            [&](uint8_t v){return c.cpuWrite(0x800D,v);},
            [&](uint8_t& v){return c.cpuRead(0x6000,v);});
        m.writeByte(0x17,0xA3);
        for(int i=0;i<8950;i++)c.cpuClock();
        c.flushSave();
    }
    {
        Cartridge c(datachRom);CHECK(c.imageValid());
        BandaiI2cMaster m(
            [&](uint8_t v){return c.cpuWrite(0x800D,v);},
            [&](uint8_t& v){return c.cpuRead(0x6000,v);});
        CHECK(m.readAt(0x17)==0xA3);
    }
    {std::ifstream f(shared.c_str(),std::ios::binary|std::ios::ate);CHECK(f.good()&&f.tellg()==std::streampos(256));}
    {std::ifstream f(save.c_str(),std::ios::binary|std::ios::ate);CHECK(f.good()&&f.tellg()==std::streampos(128));}
    // A second Datach game has no cartridge EEPROM, but must see the same
    // separate main-adapter save written by the first game.
    const std::string otherRom=std::string(basename)+".datach157_other.nes";
    std::string otherStem=otherRom.substr(otherRom.find_last_of("/\\")+1);
    otherStem.resize(otherStem.find_last_of('.'));
    const std::string otherSav="Saves/"+otherStem+".SAV";
    std::remove(otherSav.c_str());createRom(otherRom.c_str(),157,4,0,0,true);
    {
        Cartridge c(otherRom);CHECK(c.imageValid() && c.prgRamMem().empty());
        BandaiI2cMaster m(
            [&](uint8_t v){return c.cpuWrite(0x800D,v);},
            [&](uint8_t& v){return c.cpuRead(0x6000,v);});
        CHECK(m.readAt(0x17)==0xA3);
        CHECK(c.queueDatachBarcode("036000291452"));
    }
    {std::ifstream f(otherSav.c_str(),std::ios::binary);CHECK(!f.good());}
    if(previousDatach) setenv("CIPHERNES_DATACH_SAVE_PATH",previousDatachPath.c_str(),1);
    else unsetenv("CIPHERNES_DATACH_SAVE_PATH");
    std::remove(shared.c_str());std::remove(save.c_str());std::remove(datachRom.c_str());
    std::remove(otherSav.c_str());std::remove(otherRom.c_str());
}

// End-to-end cartridge backend checks plus CP009 legacy SRAM migration.

// CP011 FW-01 Waixing mapper 242 versus non-battery 43272 multicart.
// Source: NESdev mapper 242 board variants; NES Specifications, Waixing
// Zhan Shi memory-control address-line map.

// CP011: independent register-interface assertions for the existing 200-204
// discrete-board family. These are focused behavioral regressions, *not*
// evidence of pixel-perfect or exhaustive commercial-game accuracy.
static void common200to204Focused() {
    uint32_t mapped=0;
    MapperCommonBoards m200(64,64,200);
    CHECK(m200.cpuMapWrite(0x8003,mapped,0xAA));
    CHECK(m200.cpuMapRead(0x8000,mapped) && mapped==3u*0x4000u);
    CHECK(m200.cpuMapRead(0xC000,mapped) && mapped==3u*0x4000u);
    CHECK(m200.ppuMapRead(0x1000,mapped) && mapped==3u*0x2000u+0x1000u);
    CHECK(m200.mirroringOverride()==0u);
    CHECK(m200.cpuMapWrite(0x800B,mapped,0x00));
    CHECK(m200.mirroringOverride()==1u);

    MapperCommonBoards m201(64,64,201);
    CHECK(m201.cpuMapWrite(0x8005,mapped,0));
    CHECK(m201.cpuMapRead(0x8000,mapped) && mapped==1u*0x8000u);
    CHECK(m201.cpuMapRead(0xE000,mapped) && mapped==1u*0x8000u+0x6000u);
    CHECK(m201.ppuMapRead(0x0000,mapped) && mapped==5u*0x2000u);
    CHECK(m201.mirroringOverride()==0xFFu);

    MapperCommonBoards m202(64,64,202);
    CHECK(m202.cpuMapWrite(0x800B,mapped,0));
    CHECK(m202.cpuMapRead(0x8000,mapped) && mapped==5u*0x4000u);
    CHECK(m202.cpuMapRead(0xC000,mapped) && mapped==6u*0x4000u);
    CHECK(m202.ppuMapRead(0x0100,mapped) && mapped==5u*0x2000u+0x100u);
    CHECK(m202.mirroringOverride()==0u);
    CHECK(m202.cpuMapWrite(0x8004,mapped,0));
    CHECK(m202.cpuMapRead(0x8000,mapped) && mapped==2u*0x4000u);
    CHECK(m202.cpuMapRead(0xC000,mapped) && mapped==2u*0x4000u);
    CHECK(m202.mirroringOverride()==1u);

    MapperCommonBoards m203(64,64,203);
    CHECK(m203.cpuMapWrite(0x8000,mapped,0x0Du));
    CHECK(m203.cpuMapRead(0x8000,mapped) && mapped==3u*0x4000u);
    CHECK(m203.cpuMapRead(0xC000,mapped) && mapped==3u*0x4000u);
    CHECK(m203.ppuMapRead(0x1C00,mapped) && mapped==1u*0x2000u+0x1C00u);

    MapperCommonBoards m204(64,64,204);
    CHECK(m204.cpuMapWrite(0x8003,mapped,0));
    CHECK(m204.cpuMapRead(0x8000,mapped) && mapped==3u*0x4000u);
    CHECK(m204.cpuMapRead(0xC000,mapped) && mapped==3u*0x4000u);
    CHECK(m204.ppuMapRead(0,mapped) && mapped==3u*0x2000u);
    CHECK(m204.cpuMapWrite(0x8017,mapped,0));
    CHECK(m204.cpuMapRead(0x8000,mapped) && mapped==6u*0x4000u);
    CHECK(m204.cpuMapRead(0xC000,mapped) && mapped==7u*0x4000u);
    CHECK(m204.ppuMapRead(0,mapped) && mapped==6u*0x2000u);
    CHECK(m204.mirroringOverride()==0u);
    std::vector<uint8_t> old,now;
    m204.saveState(old);
    CHECK(m204.cpuMapWrite(0x8000,mapped,0));
    const uint8_t* ptr=old.data();
    CHECK(m204.loadState(ptr,ptr+old.size()));
    CHECK(ptr==old.data()+old.size());
    CHECK(m204.cpuMapRead(0xC000,mapped) && mapped==7u*0x4000u);
    m204.saveState(now);CHECK(old==now);
}

static void waixing242Focused(const char* base) {
    uint32_t mapped=0;
    MapperCommonBoards multicart(32,0,242);
    MapperCommonBoards waixing(32,0,242,true);
    CHECK(!multicart.cpuMapRead(0x6000,mapped));
    CHECK(!multicart.cpuMapWrite(0x7FFF,mapped,0x99));
    CHECK(waixing.cpuMapRead(0x6000,mapped) && mapped==Mapper::PRG_RAM_FLAG);
    CHECK(waixing.cpuMapRead(0x7FFF,mapped) && mapped==(Mapper::PRG_RAM_FLAG|0x1FFFu));
    CHECK(waixing.cpuMapWrite(0x6ABC,mapped,0x53) && mapped==(Mapper::PRG_RAM_FLAG|0xABCu));
    CHECK(waixing.ppuMapWrite(0x0000,mapped,0x66));
    // 16 possible 32 KiB banks; A6..A3, A1 sets H/V. No switchable 16K mode.
    for (uint16_t bank=0;bank<16u;++bank) {
        const uint16_t addr=uint16_t(0x8081u|(bank<<3));
        CHECK(waixing.cpuMapWrite(addr,mapped,0xFF));
        CHECK(waixing.cpuMapRead(0x8000,mapped) && mapped==uint32_t(bank)*0x8000u);
        CHECK(waixing.cpuMapRead(0xC000,mapped) && mapped==uint32_t(bank)*0x8000u+0x4000u);
        CHECK(waixing.ppuMapWrite(0,mapped,0)); // no CHR-RAM /WE gate
        CHECK(waixing.mirroringOverride()==1);
        CHECK(waixing.cpuMapWrite(uint16_t(addr|2u),mapped,0));
        CHECK(waixing.mirroringOverride()==0);
    }
    CHECK(multicart.cpuMapWrite(0x8099u,mapped,0));
    CHECK(!multicart.ppuMapWrite(0x0000,mapped,0)); // multicart NROM gate still works
    std::vector<uint8_t> saved,after;
    waixing.saveState(saved);
    CHECK(waixing.cpuMapWrite(0x8001u,mapped,0));
    const uint8_t* ptr=saved.data();
    CHECK(waixing.loadState(ptr,ptr+saved.size()));
    CHECK(ptr==saved.data()+saved.size());
    CHECK(waixing.cpuMapRead(0xC000,mapped) && mapped==15u*0x8000u+0x4000u);
    waixing.saveState(after);CHECK(saved==after);

    const std::string rom=std::string(base)+".waixing242.nes";
    createRom(rom.c_str(),242,32,0); // original synthetic 512 KiB image
    std::string stem=rom.substr(rom.find_last_of("/\\")+1);
    stem.resize(stem.find_last_of('.'));
    const std::string save="Saves/"+stem+".SAV";
    std::remove(save.c_str());
    {
        Cartridge plain(rom);
        CHECK(plain.imageValid() && plain.mapperId()==242u);
        CHECK(plain.prgRamMem().empty());
        uint8_t data=0;
        CHECK(!plain.cpuWrite(0x6000,0x77));
        CHECK(!plain.cpuRead(0x6000,data));
    }
    // Set the *board hardware flag* in a private synthetic header, not a
    // game-title/CRC hack. iNES mapper bits remain identical.
    {std::fstream f(rom.c_str(),std::ios::binary|std::ios::in|std::ios::out);
     uint8_t flags=0;f.seekg(6);f.read(reinterpret_cast<char*>(&flags),1);
     flags=uint8_t(flags|2u);f.seekp(6);f.write(reinterpret_cast<const char*>(&flags),1);}
    {
        Cartridge c(rom);uint8_t data=0;
        CHECK(c.imageValid() && c.prgRamMem().size()==8192u);
        CHECK(c.cpuWrite(0x807Bu,0)); // bank 15, horizontal
        CHECK(c.cpuRead(0x8000,data) && data==60u);
        CHECK(c.cpuRead(0xC000,data) && data==62u);
        CHECK(c.mirroring()==Cartridge::Mirror::HORIZONTAL);
        CHECK(c.ppuWrite(0x0FFF,0x33));
        CHECK(c.cpuWrite(0x8081,0)); // old multicart's CHR write gate address
        CHECK(c.ppuWrite(0x0FFF,0x44));
        CHECK(c.ppuRead(0x0FFF,data) && data==0x44u);
        CHECK(c.cpuWrite(0x6000,0x56));
        CHECK(c.cpuWrite(0x7FFF,0xA9));
        CHECK(c.cpuRead(0x6000,data) && data==0x56u);
        CHECK(c.cpuRead(0x7FFF,data) && data==0xA9u);
        std::vector<uint8_t> snapshot,after;
        c.saveState(snapshot);
        CHECK(c.cpuWrite(0x6000,0));
        CHECK(c.cpuWrite(0x8001,0));
        const uint8_t* ptr=snapshot.data();
        CHECK(c.loadState(ptr,ptr+snapshot.size()));
        CHECK(ptr==snapshot.data()+snapshot.size());
        c.saveState(after);CHECK(snapshot==after);
        CHECK(c.cpuRead(0x6000,data) && data==0x56u);
        c.flushSave();
    }
    {std::ifstream file(save,std::ios::binary|std::ios::ate);
     CHECK(file.good()&&file.tellg()==std::streampos(8192u));}
    {
        Cartridge restored(rom);uint8_t data=0;
        CHECK(restored.imageValid() && restored.prgRamMem().size()==8192u);
        CHECK(restored.cpuRead(0x6000,data) && data==0x56u);
        CHECK(restored.cpuRead(0x7FFF,data) && data==0xA9u);
        CHECK(restored.cpuWrite(0x8001,0));
        CHECK(restored.cpuRead(0x8000,data) && data==0u);
    }
    std::remove(save.c_str());std::remove(rom.c_str());
}

static void mapper242And246CartridgeTests(const char* basename) {
    const std::string modeRom=std::string(basename)+".m242.nes";
    createRom(modeRom.c_str(),242,32,0);
    {
        Cartridge cart(modeRom);
        uint8_t data=0;
        CHECK(cart.imageValid());
        CHECK(cart.cpuWrite(0x8204,0));
        CHECK(cart.cpuRead(0x8000,data) && data==2u);
        CHECK(cart.cpuRead(0xC000,data) && data==14u);
        CHECK(cart.ppuWrite(0x0000,0xA5));
        CHECK(cart.ppuRead(0x0000,data) && data==0xA5u);
        CHECK(cart.cpuWrite(0x8099,0));
        CHECK(!cart.ppuWrite(0x0000,0xCC));
        CHECK(cart.ppuRead(0x0000,data) && data==0xA5u);
        CHECK(cart.cpuRead(0x8000,data) && data==12u);
        CHECK(cart.cpuRead(0xC000,data) && data==14u);
        CHECK(cart.cpuWrite(0x8000,0));
        CHECK(cart.ppuWrite(0x0000,0xCC));
    }
    std::remove(modeRom.c_str());

    const std::string fongRom=std::string(basename)+".m246.nes";
    createRom(fongRom.c_str(),246,16,0);
    // Legacy iNES battery-backed flag to exercise on-disk SRAM migration.
    {std::fstream file(fongRom.c_str(),std::ios::in|std::ios::out|std::ios::binary);
     char flags=0; file.seekg(6);file.read(&flags,1);
     flags=static_cast<char>(static_cast<unsigned char>(flags)|2u);
     file.seekp(6);file.write(&flags,1);}
    std::string stem=fongRom.substr(fongRom.find_last_of("/\\")+1);
    stem.resize(stem.find_last_of('.'));
    const std::string sav=std::string("Saves/")+stem+".SAV";
    std::remove(sav.c_str());
    {
        Cartridge cart(fongRom); uint8_t data=0;
        CHECK(cart.imageValid());
        CHECK(cart.prgRamMem().size()==2048u);
        CHECK(cart.cpuWrite(0x6003,0));
        CHECK(cart.cpuRead(0xFFE4,data)&&data==16u);
        CHECK(cart.cpuRead(0xFFFC,data)&&data==16u);
        CHECK(cart.cpuRead(0xFFF8,data)&&data==0u);
        CHECK(!cart.cpuRead(0x6000,data));
        CHECK(!cart.cpuRead(0x7000,data));
        CHECK(cart.cpuWrite(0x6800,0x31));
        CHECK(cart.cpuWrite(0x6FFF,0x32));
        CHECK(cart.cpuRead(0x6800,data)&&data==0x31u);
        CHECK(cart.cpuRead(0x6FFF,data)&&data==0x32u);
        cart.flushSave();
        {std::ifstream f(sav.c_str(),std::ios::binary|std::ios::ate);
         CHECK(f.good()&&f.tellg()==std::streampos(2048));}

        // Restore a serialized CP009 old oversized RAM vector from the
        // former $0800 offset. New states are always 2048 bytes.
        std::vector<uint8_t> current,legacy,ram(8192,0);
        cart.saveState(current);
        ram[0x800]=0xB1;ram[0xFFF]=0xB2;
        constexpr size_t prefix=1+2+2+2+1;
        CHECK(current.size()>prefix+4+2048u);
        ciphernes_state::write_bytes(legacy,current.data(),prefix);
        ciphernes_state::write_u32(legacy,8192u);
        ciphernes_state::write_bytes(legacy,ram.data(),ram.size());
        ciphernes_state::write_bytes(legacy,current.data()+prefix+4u+2048u,
                                    current.size()-(prefix+4u+2048u));
        const uint8_t* ptr=legacy.data();
        CHECK(cart.loadState(ptr,ptr+legacy.size()));
        CHECK(ptr==legacy.data()+legacy.size());
        CHECK(cart.prgRamMem().size()==2048u);
        CHECK(cart.cpuRead(0x6800,data)&&data==0xB1u);
        CHECK(cart.cpuRead(0x6FFF,data)&&data==0xB2u);
    }
    // A legacy 8K save stored RAM at offsets $800-$FFF. Keep its data.
    {std::vector<uint8_t> oldRam(8192,0); oldRam[0x800]=0x73;oldRam[0xFFF]=0x74;
     std::ofstream file(sav.c_str(),std::ios::binary|std::ios::trunc);
     file.write(reinterpret_cast<const char*>(oldRam.data()),oldRam.size());}
    {
        Cartridge cart(fongRom); uint8_t data=0;
        CHECK(cart.imageValid());
        CHECK(cart.cpuRead(0x6800,data)&&data==0x73u);
        CHECK(cart.cpuRead(0x6FFF,data)&&data==0x74u);
        CHECK(cart.cpuWrite(0x6801,0x75u));
        cart.flushSave();
    }
    {std::ifstream file(sav.c_str(),std::ios::binary|std::ios::ate);
     CHECK(file.good()&&file.tellg()==std::streampos(2048));}
    {
        Cartridge cart(fongRom); uint8_t data=0;
        CHECK(cart.imageValid());
        CHECK(cart.cpuRead(0x6800,data)&&data==0x73u);
        CHECK(cart.cpuRead(0x6801,data)&&data==0x75u);
        CHECK(cart.cpuRead(0x6FFF,data)&&data==0x74u);
    }
    std::remove(sav.c_str());std::remove(fongRom.c_str());
}


// CP017: MMC5 audio port IRQ, zero-byte DAC latch, both pulse status bits,
// ROM-read PCM mode and serialization; no ROM-content or title checks.
static void mmc5AudioFocused() {
    Mapper005 m(16,16,8192);
    uint8_t data=0;
    CHECK((m.capabilities() & MAPPER_CAP_EXPANSION_AUDIO)!=0);
    CHECK(m.audioSample()==0.0f);
    CHECK(m.cpuReadDirect(0x5010,data) && data==1u); // MMC5A revision
    CHECK(m.cpuWriteDirect(0x5010,0x80)); // IRQ enabled, PCM write mode
    CHECK(m.cpuWriteDirect(0x5011,0xA0));
    const float first=m.audioSample();
    CHECK(first < 0.0f);
    CHECK(!m.irqState());
    CHECK(m.cpuWriteDirect(0x5011,0));
    CHECK(m.irqState());
    CHECK(m.audioSample()==first); // Zero signals end; DAC unchanged
    CHECK(m.cpuReadDirect(0x5010,data) && (data&0x80u));
    CHECK(!m.irqState());
    CHECK(m.cpuWriteDirect(0x5011,0));
    CHECK(m.irqState());
    CHECK(m.cpuWriteDirect(0x5010,0x00)); // Disable IRQ output, flag retained
    CHECK(!m.irqState());
    CHECK(m.cpuReadDirect(0x5010,data) && !(data&0x80u));
    CHECK(m.cpuWriteDirect(0x5010,0x81)); // read mode
    CHECK(m.cpuWriteDirect(0x5011,0x40)); // ignored in read mode
    CHECK(m.audioSample()==first);
    m.afterCpuRead(0xC000,0); // not $8000-$BFFF
    CHECK(!m.irqState());
    CHECK(m.cpuWriteDirect(0x5114,0x80)); // ROM-backed slots for PCM read mode
    CHECK(m.cpuWriteDirect(0x5115,0x81));
    m.afterCpuRead(0x8000,0x72);
    CHECK(m.audioSample()!=first);
    CHECK(!m.irqState());
    m.afterCpuRead(0xBFFF,0);
    CHECK(m.irqState());
    CHECK(m.cpuReadDirect(0x5010,data) && (data&0x80u));
    CHECK(!m.irqState());
    CHECK(m.cpuWriteDirect(0x5015,3));
    CHECK(m.cpuWriteDirect(0x5000,0x9F));
    CHECK(m.cpuWriteDirect(0x5002,0x16));
    CHECK(m.cpuWriteDirect(0x5003,0x08));
    CHECK(m.cpuWriteDirect(0x5004,0x5F));
    CHECK(m.cpuWriteDirect(0x5006,0x28));
    CHECK(m.cpuWriteDirect(0x5007,0x08));
    CHECK(m.cpuReadDirect(0x5015,data) && (data&3u)==3u);
    for(int i=0;i<400;++i) m.cpuClock();
    CHECK(m.cpuReadDirect(0x5015,data) && (data&3u)==3u);
    std::vector<uint8_t> state,after;
    m.saveState(state);
    CHECK(m.cpuWriteDirect(0x5015,0));
    CHECK(m.cpuReadDirect(0x5015,data) && (data&3u)==0u);
    const uint8_t* ptr=state.data();
    CHECK(m.loadState(ptr,ptr+state.size()) && ptr==state.data()+state.size());
    m.saveState(after); CHECK(after==state);
    CHECK(m.cpuReadDirect(0x5015,data) && (data&3u)==3u);
    // A standalone pulse channel must produce a nonzero expansion signal,
    // independent of PCM writes, and silence immediately when disabled.
    Mapper005 tone(2,0,8192);
    CHECK(tone.cpuWriteDirect(0x5015,1));
    CHECK(tone.cpuWriteDirect(0x5000,0xDF)); // constant volume, duty3
    CHECK(tone.cpuWriteDirect(0x5002,0x20));
    CHECK(tone.cpuWriteDirect(0x5003,0x08));
    bool heard=false;
    for (int tick=0;tick<1000;++tick) {
        tone.cpuClock();
        if (tone.audioSample()!=0.0f) heard=true;
    }
    CHECK(heard);
    CHECK(tone.cpuWriteDirect(0x5015,0));
    CHECK(tone.audioSample()==0.0f);
    // CP016 mapper-state payload without the CP017 audio tag is still readable.
    const size_t tag=std::string("M5A2").size();
    auto it=std::search(state.begin(),state.end(),"M5A2","M5A2"+tag);
    CHECK(it!=state.end());
    if(it!=state.end()) {
        std::vector<uint8_t> legacy(state.begin(),it);
        ptr=legacy.data();
        CHECK(m.loadState(ptr,ptr+legacy.size()) && ptr==legacy.data()+legacy.size());
    }
}

// NES 2.0 ETROM has two 8KiB chips, battery on chip 0, volatile chip 1.
// Internal MMC5 ExRAM is a separate backed memory after the saved chip data.
static void mmc5MixedRamPersistenceFocused(const char* basename) {
    const std::string rom=std::string(basename)+".mmc5_mixed.nes";
    createRom(rom.c_str(),5,2,0,0,true,0);
    {
        std::fstream f(rom.c_str(),std::ios::in|std::ios::out|std::ios::binary);
        f.seekp(6); char flags=0; f.seekg(6);f.get(flags);
        f.seekp(6);f.put(char(uint8_t(flags)|2u));
        f.seekp(10);f.put(char(0x77)); // 8K PRG-RAM + 8K PRG-NVRAM
    }
    std::string stem=rom.substr(rom.find_last_of("/\\")+1);
    stem.resize(stem.find_last_of('.'));
    const std::string save="Saves/"+stem+".SAV";
    std::remove(save.c_str());
    {
        Cartridge c(rom); uint8_t value=0;
        CHECK(c.imageValid() && c.mapperId()==5 && c.prgRamMem().size()==16384u);
        CHECK(c.cpuWrite(0x5102,2) && c.cpuWrite(0x5103,1));
        CHECK(c.cpuWrite(0x5113,0) && c.cpuWrite(0x6000,0x37));
        CHECK(c.cpuWrite(0x5113,4) && c.cpuWrite(0x6000,0x6E));
        CHECK(c.cpuRead(0x6000,value) && value==0x6Eu);
        CHECK(c.cpuWrite(0x5104,2) && c.cpuWrite(0x5C05,0xBC));
        c.flushSave();
        std::ifstream f(save.c_str(),std::ios::binary|std::ios::ate);
        CHECK(f.good() && f.tellg()==std::streampos(8192+1024));
    }
    {
        Cartridge c(rom); uint8_t value=0;
        CHECK(c.imageValid() && c.prgRamMem().size()==16384u);
        CHECK(c.cpuWrite(0x5113,0));
        CHECK(c.cpuRead(0x6000,value) && value==0x37u);
        CHECK(c.cpuWrite(0x5113,4));
        CHECK(c.cpuRead(0x6000,value) && value==0u);
        CHECK(c.cpuWrite(0x5104,2));
        CHECK(c.cpuRead(0x5C05,value) && value==0xBCu);
    }
    // A front-panel reset must not erase battery-backed ExRAM.
    {
        Cartridge c(rom); uint8_t value=0;
        CHECK(c.cpuWrite(0x5104,2));
        CHECK(c.cpuRead(0x5C05,value) && value==0xBCu);
        c.resetMapper(false);
        CHECK(c.cpuWrite(0x5104,2));
        CHECK(c.cpuRead(0x5C05,value) && value==0xBCu);
    }
    // Read historical CP016 whole-RAM save without treating volatile bytes
    // as an ExRAM trailer. The first battery chip must still be recovered.
    {
        std::vector<uint8_t> old(16384u,0x7E);
        old[0]=0x93;
        std::ofstream f(save.c_str(),std::ios::binary|std::ios::trunc);
        f.write(reinterpret_cast<const char*>(old.data()),std::streamsize(old.size()));
    }
    {
        Cartridge c(rom); uint8_t value=0;
        CHECK(c.imageValid());
        CHECK(c.cpuWrite(0x5113,0));
        CHECK(c.cpuRead(0x6000,value) && value==0x93u);
        CHECK(c.cpuWrite(0x5113,4));
        CHECK(c.cpuRead(0x6000,value) && value==0u);
        CHECK(c.cpuWrite(0x5104,2));
        CHECK(c.cpuRead(0x5C05,value) && value==0u);
    }
    std::remove(save.c_str()); std::remove(rom.c_str());
}


static void mmc5MixedDifferentChipSizesFocused(const char* basename) {
    for (const uint8_t header : {uint8_t(0x79),uint8_t(0x97)}) {
        const uint32_t nvBytes = ((header >> 4u) == 7u) ? 8192u : 32768u;
        const std::string rom=std::string(basename)+".mmc5_mixed_"+std::to_string(unsigned(header))+".nes";
        createRom(rom.c_str(),5,2,0,0,true,0);
        {
            std::fstream f(rom.c_str(),std::ios::in|std::ios::out|std::ios::binary);
            f.seekg(6); char flags=0; f.get(flags);
            f.seekp(6); f.put(char(uint8_t(flags)|2u));
            f.seekp(10); f.put(char(header));
        }
        std::string stem=rom.substr(rom.find_last_of("/\\")+1);
        stem.resize(stem.find_last_of('.'));
        const std::string save="Saves/"+stem+".SAV";
        std::remove(save.c_str());
        {
            Cartridge c(rom); uint8_t data=0;
            CHECK(c.imageValid() && c.prgRamMem().size()==40960u);
            CHECK(c.cpuWrite(0x5102,2) && c.cpuWrite(0x5103,1));
            for (unsigned b=0;b<8;b++) {
                CHECK(c.cpuWrite(0x5113,uint8_t(b)));
                CHECK(c.cpuWrite(0x6000,uint8_t(0x20u+b)));
            }
            CHECK(c.cpuWrite(0x5113,0));
            CHECK(c.cpuRead(0x6000,data) && data==(nvBytes==8192u?0x23u:0x20u));
            CHECK(c.cpuWrite(0x5113,4));
            CHECK(c.cpuRead(0x6000,data) && data==(nvBytes==8192u?0x24u:0x27u));
            CHECK(c.cpuWrite(0x5104,2) && c.cpuWrite(0x5C08,0xE2));
            c.flushSave();
            std::ifstream f(save.c_str(),std::ios::binary|std::ios::ate);
            CHECK(f.good() && f.tellg()==std::streampos(nvBytes+1024u));
        }
        {
            Cartridge c(rom); uint8_t data=0;
            CHECK(c.imageValid());
            CHECK(c.cpuWrite(0x5113,0));
            CHECK(c.cpuRead(0x6000,data) && data==(nvBytes==8192u?0x23u:0x20u));
            CHECK(c.cpuWrite(0x5113,4));
            CHECK(c.cpuRead(0x6000,data) && data==0u); // chip 1 volatile
            CHECK(c.cpuWrite(0x5104,2));
            CHECK(c.cpuRead(0x5C08,data) && data==0xE2u);
        }
        std::remove(save.c_str());std::remove(rom.c_str());
    }
}


// CP017: MMC5 vertical split NT, attribute and 4KiB CHR substitution.
// Uses the PPU fetch hooks rather than title/CRC detection. Split mode is
// disabled whenever ExRAM is CPU readable (mode 2/3) or PPUMASK is off.
static void mmc5VerticalSplitFocused() {
    Mapper005 m(8,8,8192);
    uint8_t data=0;uint32_t mapped=0;
    CHECK(m.cpuWriteDirect(0x5104,2));
    // Split Y=16 (tile row2), tile columns3/9, nonuniform attributes.
    CHECK(m.cpuWriteDirect(0x5C43,0xA7)); // row 2 x 32 + col 3
    CHECK(m.cpuWriteDirect(0x5C49,0xB6)); // row 2 + col9
    CHECK(m.cpuWriteDirect(0x5FC0,0xD2)); // row2,col3 => palette3
    CHECK(m.cpuWriteDirect(0x5FC2,0x20)); // row2,col9 => palette2
    CHECK(m.cpuWriteDirect(0x5104,0));
    CHECK(m.cpuWriteDirect(0x5200,0x88)); // left eight tiles
    CHECK(m.cpuWriteDirect(0x5201,16));
    CHECK(m.cpuWriteDirect(0x5202,3));
    m.beforeCpuWrite(0x2001,0x18); // rendering on
    m.beforePpuFetch(0x2003,1,0,25,0); // tile 3
    CHECK(m.nametableRead(0x2003,data) && data==0xA7u);
    m.beforePpuFetch(0x23C0,2,0,27,0);
    CHECK(m.attributeRead(0x23C0,data) && data==0xFFu);
    m.beforePpuFetch(0x0000,3,0,29,0);
    CHECK(m.ppuMapRead(0x0000,mapped) && mapped==3u*4096u);
    CHECK(m.ppuMapRead(0x1402,mapped) && mapped==3u*4096u+0x402u);
    // Column 9 falls outside the left split, uses ordinary CIRAM0.
    m.beforePpuFetch(0x2009,1,0,73,0);
    CHECK(m.nametableRead(0x2009,data) && data==0u);
    CHECK(m.cpuWriteDirect(0x5200,0xC8)); // right eight+ tiles
    m.beforePpuFetch(0x2003,1,0,25,0);
    CHECK(m.nametableRead(0x2003,data) && data==0u);
    m.beforePpuFetch(0x2009,1,0,73,0);
    CHECK(m.nametableRead(0x2009,data) && data==0xB6u);
    m.beforePpuFetch(0x23C2,2,0,75,0);
    CHECK(m.attributeRead(0x23C2,data) && data==0xAAu);
    // Switching ExRAM to CPU-readable mode removes split substitutions.
    CHECK(m.cpuWriteDirect(0x5104,2));
    m.beforePpuFetch(0x2009,1,0,73,0);
    CHECK(m.nametableRead(0x2009,data) && data==0u);
    // PPUMASK $00 also disables split mode.
    CHECK(m.cpuWriteDirect(0x5104,0));
    m.beforeCpuWrite(0x2001,0);
    m.beforePpuFetch(0x2009,1,0,73,0);
    CHECK(m.nametableRead(0x2009,data) && data==0u);
    m.beforeCpuWrite(0x2001,0x18);
    m.beforePpuFetch(0x2009,1,0,73,0);
    std::vector<uint8_t> state,after;
    m.saveState(state);
    CHECK(m.cpuWriteDirect(0x5202,9));
    const uint8_t* ptr=state.data();
    CHECK(m.loadState(ptr,ptr+state.size()) && ptr==state.data()+state.size());
    m.saveState(after);CHECK(after==state);
    m.beforePpuFetch(0x0000,3,0,77,0);
    CHECK(m.ppuMapRead(0x0000,mapped) && mapped==3u*4096u);
}

int main(int argc, char**argv){
    if(argc!=2){std::cerr<<"usage: mapper_audit_regression /tmp/rompath.nes\n";return 2;}
    nrom368AndSnromHardwareFocused(argv[1]); focused(); mmc5VerticalSplitFocused(); mmc5AudioFocused(); mmc5MixedRamPersistenceFocused(argv[1]); mmc5MixedDifferentChipSizesFocused(argv[1]); mmc5PrgRamChipSelectFocused(); mmc5RamCartridgeEndToEnd(argv[1]); mmc3CoreFocused(); mmc3IrqRevisionFocused(argv[1]); tqrom119Focused(argv[1]); et113242Focused(argv[1]); tlsrom118Focused(); vrc6IrqFocused(); common200to204Focused(); waixing242Focused(argv[1]); mapper242And246CartridgeTests(argv[1]); taitoX1017WiringFocused(argv[1]); action53Focused(); iremG101Focused(); rambo158Focused(); fme7Focused(); sunsoft5bAudioFocused(); bandaiI2cFocused(); bandaiSaveRoundtrip(argv[1]); bandaiBoardVariantTests(argv[1]); taito207EndToEnd(argv[1]); registrySmoke(argv[1]); discreteBusConflictTests(argv[1]); nes2HeaderTests(argv[1]); namcoEndToEndPersistenceAndPpu(argv[1]); namcoAudioIntegration(argv[1]);
    std::remove(argv[1]);
    std::cout<<"Mapper audit checks: "<<assertions<<" assertions; "<<failures<<" failures\n";
    return failures?1:0;
}
