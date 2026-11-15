// Read-only test-only non-mutating C++ debug bridge. This file is linked into
// diagnostics, never compiled into the normal CipherNES core or frontend.
#include "../CipherNES_Core/ciphernes_core.cpp"
#include "debug_bridge.h"
extern "C" int ciphernes_debug_sample(CIPHERNES_Handle* h, CoreDebugSnapshot* s) {
    if (!h || !s) return 0;
    s->pc=h->cpu.PC;s->a=h->cpu.A;s->x=h->cpu.X;s->y=h->cpu.Y;
    s->sp=h->cpu.SP;s->status=h->cpu.P;
    s->cpu_address_bus=h->bus.cpuAddressBus;
    s->cpu_divider=h->bus.cpuClockDivider;
    s->scanline=h->bus.ppu.currentScanline();
    s->ppu_cycle=h->bus.ppu.currentCycle();
    s->master_clock=h->bus.systemClockCounter;
    s->nmi_pending=h->bus.nmiPending;
    s->dma_active=h->bus.dmaTransfer||h->bus.dmcDmaTransfer;
    return 1;
}
