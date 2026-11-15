// Mapper regression: TC0190 (iNES Mapper 33) hardware mapping regression.
// Synthetic bank geometry only. No game/ROM-specific behavior or source modifications.
#include "Mappers/Mapper033.h"
#include <cstdint>
#include <cstdio>
#include <vector>

static int checks = 0, failures = 0;
static void expect(bool cond, const char* label) {
    ++checks;
    if (!cond) { ++failures; std::fprintf(stderr, "FAIL %s\n", label); }
}
static void cpu(Mapper033& m, uint16_t addr, uint32_t expected) {
    uint32_t actual = UINT32_MAX;
    const bool mapped = m.cpuMapRead(addr, actual);
    char label[100];
    std::snprintf(label,sizeof(label),"CPU %04X expected %u got %u", addr,expected,actual);
    expect(mapped && actual == expected,label);
}
static void ppu(Mapper033& m, uint16_t addr, uint32_t expected) {
    uint32_t actual = UINT32_MAX;
    const bool mapped = m.ppuMapRead(addr, actual);
    char label[100];
    std::snprintf(label,sizeof(label),"PPU %04X expected %u got %u", addr,expected,actual);
    expect(mapped && actual == expected,label);
}
int main() {
    Mapper033 m(8,4); // sixteen distinct 8KiB PRG banks, four 8KiB CHR banks
    m.reset();
    uint32_t mapped = 0;
    expect(!m.cpuMapRead(0x7FFF,mapped),"CPU below PRG address unmapped");
    expect((m.capabilities() & MAPPER_CAP_MIRRORING)!=0,"mirroring capability");
    expect(m.mirroringOverride()==0xFF,"initial mirroring respects header");
    // TC0190: two 8KiB switched pages followed by the last two 8KiB fixed pages.
    cpu(m,0x8000,0); cpu(m,0x9FFF,8191);
    cpu(m,0xA000,8192); cpu(m,0xBFFF,16383);
    cpu(m,0xC000,14*8192); cpu(m,0xDFFF,15*8192-1);
    cpu(m,0xE000,15*8192); cpu(m,0xFFFC,16*8192-4); cpu(m,0xFFFF,16*8192-1);
    expect(m.cpuMapWrite(0x8000,mapped,0x03),"bank 0 register handles write");
    expect(m.cpuMapWrite(0x8001,mapped,0x05),"bank 1 register handles write");
    cpu(m,0x8000,3*8192); cpu(m,0xA000,5*8192);
    cpu(m,0xC000,14*8192); cpu(m,0xE000,15*8192);
    // $9000 mirrors $8000 under the documented address decoder ($A003 mask).
    expect(m.cpuMapWrite(0x9000,mapped,0x46),"mirrored $9000 write");
    cpu(m,0x8000,6*8192);
    expect(m.mirroringOverride()==0,"$8000 bit6 horizontal mirroring");
    expect(m.cpuMapWrite(0x8000,mapped,0x00),"clear mirroring bit");
    expect(m.mirroringOverride()==1,"$8000 bit6 clear vertical mirroring");
    // Unused high bits of PRG register are ignored, then wrapped to actual geometry.
    expect(m.cpuMapWrite(0x8001,mapped,0xFF),"bank1 PRG mask");
    cpu(m,0xA000,15*8192);
    // CHR: two 2KiB banks in 2KiB units, followed by four 1KiB banks.
    ppu(m,0x0000,0); ppu(m,0x07FF,2047);
    ppu(m,0x0800,2048); ppu(m,0x0FFF,4095);
    ppu(m,0x1000,0); ppu(m,0x1400,1024);
    ppu(m,0x1800,2048); ppu(m,0x1C00,3072);
    m.cpuMapWrite(0x8002,mapped,7); m.cpuMapWrite(0x8003,mapped,8);
    m.cpuMapWrite(0xA000,mapped,9); m.cpuMapWrite(0xA003,mapped,10);
    ppu(m,0x0000,7*2048); ppu(m,0x0800,8*2048);
    ppu(m,0x1000,9*1024); ppu(m,0x1FFF,10*1024+1023);
    std::vector<uint8_t> state;
    m.saveState(state);
    m.cpuMapWrite(0x8000,mapped,12);
    m.cpuMapWrite(0xA003,mapped,2);
    const uint8_t* ptr=state.data();
    expect(m.loadState(ptr,state.data()+state.size()),"state load accepted");
    expect(ptr==state.data()+state.size(),"state fully consumed");
    cpu(m,0x8000,0); ppu(m,0x1C00,10*1024);
    m.reset();cpu(m,0x8000,0);cpu(m,0xA000,8192);
    expect(m.mirroringOverride()==0xFF,"reset restores header mirroring");
    std::fprintf(stderr,"Mapper33 hardware gate: %d assertions, %d failures\n",checks,failures);
    return failures?1:0;
}
