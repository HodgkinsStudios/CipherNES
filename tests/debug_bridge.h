#pragma once
#include <cstdint>
// Test-only readback of existing CPU, PPU and bus fields. Not in installed ABI.
struct CoreDebugSnapshot {
    uint16_t pc;
    uint16_t cpu_address_bus;
    uint8_t a, x, y, sp, status, cpu_divider;
    int16_t scanline, ppu_cycle;
    uint64_t master_clock;
    bool nmi_pending, dma_active;
};
extern "C" int ciphernes_debug_sample(struct CIPHERNES_Handle* h, CoreDebugSnapshot* out);
