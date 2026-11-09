// Test-only bridge to inspect CPU RAM for ROM-defined $6000 verdicts.
// Not compiled into CipherNES or its installed core library. The probe
// intentionally reuses the unmodified emulator's CPU bus read semantics.
#include "../CipherNES_Core/ciphernes_core.cpp"
extern "C" int ciphernes_test_probe_cpu(CIPHERNES_Handle* h, unsigned address) {
    return h ? int(h->bus.cpuRead(uint16_t(address), true)) : -1;
}
