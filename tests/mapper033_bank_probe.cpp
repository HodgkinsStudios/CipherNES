// Standalone diagnostic-only assertions based on 8 KiB PRG slot wiring.
// This probe deliberately does not alter the tested mapper implementation.
#include "Mappers/Mapper033.h"
#include <cstdint>
#include <cstdio>
#include <vector>
#include <string>
#include <iostream>
static int total=0, misses=0;
static void check(Mapper033& m, const char* phase, uint16_t addr, uint32_t expect, bool ppu=false) {
    uint32_t actual=0xFFFFFFFFu;
    const bool mapped = ppu ? m.ppuMapRead(addr,actual) : m.cpuMapRead(addr,actual);
    const bool pass = mapped && actual==expect;
    std::printf("%s,%s,%04X,%u,%u,%s\n",phase,ppu?"PPU":"CPU",addr,expect,actual,pass?"PASS":"MISMATCH");
    ++total; if (!pass) ++misses;
}
int main() {
    std::puts("phase,bus,address_hex,expected_offset,actual_offset,result");
    // Eight 16-KiB PRG banks => 16 physical 8-KiB pages.
    // Normal mapper 33: two switchable pages followed by last two fixed pages.
    Mapper033 m(8,4);
    m.reset();
    check(m,"power_on",0x8000,0u*8192);
    check(m,"power_on",0x9FFF,0u*8192+8191);
    check(m,"power_on",0xA000,1u*8192);
    check(m,"power_on",0xBFFF,1u*8192+8191);
    check(m,"power_on",0xC000,14u*8192);
    check(m,"power_on",0xDFFF,14u*8192+8191);
    check(m,"power_on",0xE000,15u*8192);
    check(m,"power_on",0xFFFC,15u*8192+8188);
    check(m,"power_on",0xFFFF,15u*8192+8191);
    uint32_t dummy=0;
    m.cpuMapWrite(0x8000,dummy,0x04);
    m.cpuMapWrite(0x8001,dummy,0x09);
    check(m,"bank_switch",0x8000,4u*8192);
    check(m,"bank_switch",0xA000,9u*8192);
    check(m,"bank_switch",0xC000,14u*8192);
    check(m,"bank_switch",0xE000,15u*8192);
    // Independent CHR mapping controls; useful for bounding the defect.
    check(m,"chr_default",0x0000,0u*2048,true);
    check(m,"chr_default",0x0800,1u*2048,true);
    m.cpuMapWrite(0x8002,dummy,3);
    m.cpuMapWrite(0xA000,dummy,5);
    check(m,"chr_switch",0x0000,3u*2048,true);
    check(m,"chr_switch",0x1000,5u*1024,true);
    std::vector<uint8_t> state; m.saveState(state);
    m.cpuMapWrite(0x8000,dummy,7);
    const uint8_t* ptr=state.data();
    const bool restored=m.loadState(ptr, state.data()+state.size()) && ptr==state.data()+state.size();
    check(m,"state_reload",0x8000,4u*8192);
    check(m,"state_reload",0xE000,15u*8192);
    std::fprintf(stderr,"Mapper 33 diagnostic: %d/%d mapping expectations met; %d mismatch(es); state_load=%s\n",
                 total-misses,total,misses,restored?"PASS":"FAIL");
    if (!restored) return 2;
    // Do not mask the mismatch as a passing regression test: success here means
    // the observation file was produced, and the CSV describes failures.
    return 0;
}
