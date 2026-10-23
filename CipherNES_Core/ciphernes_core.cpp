#include "ciphernes_core.h"

#include <memory>
#include <vector>
#include <cstring>
#include <string>
#include <fstream>

#include <cerrno>
#include <cstdio>
#include "platform_compat.h"

#if defined(__linux__)
#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>
#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001U
#endif
#elif defined(__APPLE__)
#include <fcntl.h>
#include <unistd.h>
#endif

#include "bus.h"
#include "cpu6502.h"
#include "cartridge.h"

#include "state_io.h"
#include "game_genie.h"

struct CIPHERNES_Handle {
    Bus bus;
    CPU6502 cpu;
    std::shared_ptr<Cartridge> cart;
    bool romLoaded = false;

    double audioSampleRate = 44100.0;

    std::string toastText;
    uint64_t toastUntilUtcMs = 0;
    std::vector<uint8_t> saveScratch;
    uint64_t frameCounter = 0;

    CIPHERNES_Handle() {
        cpu.connect(&bus);
        bus.cpu = &cpu;
        bus.apu.setSampleRate(audioSampleRate);
        bus.reset(true);
    }
};

static inline void safe_set_controller(Bus& bus, int port, uint8_t buttons) {
    if (port < 0) return;
    if (port > 1) return;
    bus.controller[port] = buttons;
}

CIPHERNES_API CIPHERNES_Handle* ciphernes_create() {
    return new CIPHERNES_Handle();
}

CIPHERNES_API void ciphernes_destroy(CIPHERNES_Handle* h) {
    delete h;
}

CIPHERNES_API int ciphernes_get_api_version() {
    return CIPHERNES_API_VERSION;
}

CIPHERNES_API const char* ciphernes_get_core_version_string() {
    // Keep stable for UI display and debugging.
    #if defined(_WIN32)
    return "CipherNES Core 1.0.0 (Windows, NTSC)";
#elif defined(__APPLE__)
    return "CipherNES Core 1.0.0 (macOS, NTSC)";
#else
    return "CipherNES Core 1.0.0 (Ubuntu, NTSC)";
#endif
}

static int ciphernes_install_cartridge(CIPHERNES_Handle* h, const std::shared_ptr<Cartridge>& c) {
    if (!h || !c || !c->imageValid()) return 0;
    c->setExpansionAudioSampleRate(h->audioSampleRate);
    h->cart = c;
    h->bus.insertCartridge(h->cart);
    h->romLoaded = true;
    h->bus.reset(true);
    return 1;
}

static void ciphernes_flush_replaced_cartridge(CIPHERNES_Handle* h) {
    if (h && h->cart) h->cart->flushSave();
}

CIPHERNES_API int ciphernes_load_rom(CIPHERNES_Handle* h, const char* nes_file_path) {
    if (!h || !nes_file_path) return 0;
    // Flush the currently loaded battery SRAM before constructing/loading the
    // replacement cartridge. Without this, reloading the same ROM could read
    // the old .SAV before the old cartridge destructor had a chance to write it.
    ciphernes_flush_replaced_cartridge(h);
    return ciphernes_install_cartridge(h, std::make_shared<Cartridge>(std::string(nes_file_path)));
}

CIPHERNES_API int ciphernes_load_rom_image(CIPHERNES_Handle* h, const uint8_t* image_data, uint32_t image_bytes, const char* virtual_path) {
    if (!h || !image_data || image_bytes < 16u) return 0;
    const std::string identity = (virtual_path && *virtual_path) ? std::string(virtual_path) : std::string("InMemoryROM.nes");

#if defined(_WIN32)
    char tempDirectory[MAX_PATH + 1] = {};
    const DWORD tempLength = GetTempPathA(MAX_PATH, tempDirectory);
    if (tempLength == 0 || tempLength > MAX_PATH) return 0;

    char tempPath[MAX_PATH + 1] = {};
    if (!GetTempFileNameA(tempDirectory, "PNS", 0, tempPath)) return 0;

    {
        std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);
        if (!out) {
            DeleteFileA(tempPath);
            return 0;
        }
        out.write(reinterpret_cast<const char*>(image_data), static_cast<std::streamsize>(image_bytes));
        if (!out.good()) {
            out.close();
            DeleteFileA(tempPath);
            return 0;
        }
    }

    ciphernes_flush_replaced_cartridge(h);
    std::shared_ptr<Cartridge> c = std::make_shared<Cartridge>(std::string(tempPath), identity);
    DeleteFileA(tempPath);
    return ciphernes_install_cartridge(h, c);
#elif defined(__APPLE__)
    char tempPath[] = "/tmp/ciphernes-inmemory-XXXXXX";
    int fd = ::mkstemp(tempPath);
    if (fd < 0) return 0;

    uint32_t written = 0;
    while (written < image_bytes) {
        const ssize_t n = ::write(fd, image_data + written, static_cast<size_t>(image_bytes - written));
        if (n < 0) { if (errno == EINTR) continue; ::close(fd); ::unlink(tempPath); return 0; }
        if (n == 0) { ::close(fd); ::unlink(tempPath); return 0; }
        written += static_cast<uint32_t>(n);
    }
    if (::close(fd) != 0) { ::unlink(tempPath); return 0; }

    ciphernes_flush_replaced_cartridge(h);
    std::shared_ptr<Cartridge> c = std::make_shared<Cartridge>(std::string(tempPath), identity);
    ::unlink(tempPath);
    return ciphernes_install_cartridge(h, c);
#else
#if defined(SYS_memfd_create)
    int fd = static_cast<int>(::syscall(SYS_memfd_create, "CipherNES_InMemory_ROM", MFD_CLOEXEC));
#else
    int fd = -1;
#endif
    if (fd < 0) return 0;
    uint32_t written = 0;
    while (written < image_bytes) {
        const ssize_t n = ::write(fd, image_data + written, static_cast<size_t>(image_bytes - written));
        if (n < 0) { if (errno == EINTR) continue; ::close(fd); return 0; }
        if (n == 0) { ::close(fd); return 0; }
        written += static_cast<uint32_t>(n);
    }
    if (::lseek(fd, 0, SEEK_SET) < 0) { ::close(fd); return 0; }
    char fdPath[64];
    std::snprintf(fdPath, sizeof(fdPath), "/proc/self/fd/%d", fd);
    ciphernes_flush_replaced_cartridge(h);
    std::shared_ptr<Cartridge> c = std::make_shared<Cartridge>(std::string(fdPath), identity);
    ::close(fd);
    return ciphernes_install_cartridge(h, c);
#endif
}

CIPHERNES_API void ciphernes_unload_rom(CIPHERNES_Handle* h) {
    if (!h) return;
    if (h->cart) h->cart->flushSave();
    h->cart.reset();
    h->bus.insertCartridge(std::shared_ptr<Cartridge>());
    h->romLoaded = false;
    h->bus.clearPatches();
    h->bus.reset(true);
}

CIPHERNES_API void ciphernes_reset_soft(CIPHERNES_Handle* h) {
    if (!h) return;
    // Preserve battery saves before resetting.
    if (h->cart) h->cart->flushSave();
    // Preserve APU register state that survives the physical RESET line.
    h->bus.reset(false);
}

CIPHERNES_API void ciphernes_reset_hard(CIPHERNES_Handle* h) {
    if (!h) return;
    // Preserve battery saves before resetting.
    if (h->cart) h->cart->flushSave();
    // Approximate power cycle: clear RAM and reset all devices.
    h->bus.ram.fill(0);
    h->bus.reset(true);
}

CIPHERNES_API int ciphernes_is_fds(CIPHERNES_Handle* h) {
    return h && h->cart && h->cart->isDiskSystem() ? 1 : 0;
}

CIPHERNES_API int ciphernes_fds_get_side_count(CIPHERNES_Handle* h) {
    if (!h || !h->cart || !h->cart->isDiskSystem()) return 0;
    return int(h->cart->diskSideCount());
}

CIPHERNES_API int ciphernes_fds_get_current_side(CIPHERNES_Handle* h) {
    if (!h || !h->cart || !h->cart->isDiskSystem()) return -1;
    const uint32_t side = h->cart->currentDiskSide();
    return side == 0xFFFFFFFFu ? -1 : int(side);
}

CIPHERNES_API void ciphernes_fds_eject(CIPHERNES_Handle* h) {
    if (h && h->cart && h->cart->isDiskSystem()) h->cart->ejectDisk();
}

CIPHERNES_API int ciphernes_fds_insert_next_side(CIPHERNES_Handle* h) {
    if (!h || !h->cart || !h->cart->isDiskSystem()) return 0;
    const uint32_t count = h->cart->diskSideCount();
    if (count == 0) return 0;
    const uint32_t current = h->cart->currentDiskSide();
    const uint32_t next = current == 0xFFFFFFFFu ? 0u : ((current + 1u) % count);
    return h->cart->insertDiskSide(next) ? 1 : 0;
}

CIPHERNES_API uint64_t ciphernes_run_frame(CIPHERNES_Handle* h) {
    if (!h) return 0;

    uint64_t ticks = 0;
    // Clear any stale completion flag so callers can run exactly one frame.
    h->bus.ppu.frame_complete = false;

    while (!h->bus.ppu.frame_complete) {
        h->bus.clock();
        ticks++;
        // Safety: if ROM isn't loaded, don't spin forever
        if (!h->romLoaded && ticks > 1000000ULL) break;
    }
    // Caller can read framebuffer after this returns.
    return ticks;
}

CIPHERNES_API uint64_t ciphernes_run_frame_with_input(CIPHERNES_Handle* h, uint8_t port1_buttons, uint8_t port2_buttons) {
    if (!h) return 0;
    safe_set_controller(h->bus, 0, port1_buttons);
    safe_set_controller(h->bus, 1, port2_buttons);
    return ciphernes_run_frame(h);
}

CIPHERNES_API void ciphernes_set_controller(CIPHERNES_Handle* h, int controller_port, uint8_t buttons) {
    if (!h) return;
    safe_set_controller(h->bus, controller_port, buttons);
}

CIPHERNES_API const uint32_t* ciphernes_get_framebuffer(CIPHERNES_Handle* h) {
    if (!h) return nullptr;
    return h->bus.ppu.getFrame().data();
}

// Framebuffer interop helpers. These are constants for the current core.
CIPHERNES_API void ciphernes_set_audio_sample_rate(CIPHERNES_Handle* h, double sample_rate_hz) {
    if (!h) return;
    if (sample_rate_hz <= 0.0) return;
    h->audioSampleRate = sample_rate_hz;
    h->bus.apu.setSampleRate(sample_rate_hz);
    if (h->cart) h->cart->setExpansionAudioSampleRate(sample_rate_hz);
}

CIPHERNES_API int ciphernes_take_audio_samples_i16(CIPHERNES_Handle* h, int16_t* out_samples, int max_samples) {
    if (!h || !out_samples || max_samples <= 0) return 0;
    int n = h->bus.apu.copyAndClearSamplesI16(out_samples, max_samples);
    if (n > 0 && h->cart) h->cart->processCartridgeAudioPathI16(out_samples, n);
    return n;
}

CIPHERNES_API void ciphernes_set_audio_enabled(CIPHERNES_Handle* h, int enabled) {
    if (!h) return;
    h->bus.apu.setAudioEnabled(enabled != 0);
}

CIPHERNES_API void ciphernes_set_apu_channel_mask(CIPHERNES_Handle* h, uint8_t mask) {
    if (!h) return;
    h->bus.apu.setChannelMask(mask);
}

static inline std::vector<uint8_t>& build_state_blob(CIPHERNES_Handle* h, bool flushBattery) {
    // Flush battery PRG-RAM before state capture so SRAM is persisted
    // before the user manually saves state.
    if (flushBattery && h && h->cart) h->cart->flushSave();
    std::vector<uint8_t>& tmp = h->saveScratch;
    tmp.clear();
    if (tmp.capacity() < 128u * 1024u)
        tmp.reserve(128u * 1024u);

    const uint32_t magic = 0x53545047u; // Save-state file marker
    ciphernes_state::write_u32(tmp, magic);
    ciphernes_state::write_u32(tmp, CIPHERNES_STATE_VERSION);
    ciphernes_state::write_bool(tmp, false); // State v6 legacy turbo field
    ciphernes_state::write_u64(tmp, h->toastUntilUtcMs);
    ciphernes_state::write_string(tmp, h->toastText);
    h->bus.saveState(tmp);
    return tmp;
}

CIPHERNES_API uint32_t ciphernes_get_state_size(CIPHERNES_Handle* h) {
    if (!h || !h->romLoaded) return 0;
    std::vector<uint8_t>& tmp = build_state_blob(h, false);
    return (uint32_t)tmp.size();
}

CIPHERNES_API uint32_t ciphernes_save_state(CIPHERNES_Handle* h, uint8_t* out_state, uint32_t out_capacity) {
    if (!h || !h->romLoaded || !out_state || out_capacity == 0) return 0;
    std::vector<uint8_t>& tmp = build_state_blob(h, true);
    if (tmp.size() > out_capacity) return 0;
    std::memcpy(out_state, tmp.data(), tmp.size());
    return (uint32_t)tmp.size();
}

// Used for per-frame rewind snapshots; do not repeatedly persist battery RAM.
CIPHERNES_API uint32_t ciphernes_save_state_transient(CIPHERNES_Handle* h, uint8_t* out_state, uint32_t out_capacity) {
    if (!h || !h->romLoaded || !out_state || !out_capacity) return 0;
    std::vector<uint8_t>& tmp = build_state_blob(h, false);
    if (tmp.size() > out_capacity) return 0;
    std::memcpy(out_state, tmp.data(), tmp.size());
    return static_cast<uint32_t>(tmp.size());
}

CIPHERNES_API int ciphernes_load_state(CIPHERNES_Handle* h, const uint8_t* state, uint32_t state_size) {
    if (!h || !h->romLoaded || !state || state_size < 8) return 0;
    const uint8_t* p = state;
    const uint8_t* end = state + state_size;

    uint32_t magic=0, ver=0;
    if (!ciphernes_state::read_u32(p,end,magic)) return 0;
    if (!ciphernes_state::read_u32(p,end,ver)) return 0;
    if (magic != 0x53545047u) return 0;
    if (ver != CIPHERNES_STATE_VERSION) return 0;

    bool turbo=false;
    uint64_t toastUntil=0;
    std::string toast;
    if (!ciphernes_state::read_bool(p,end,turbo)) return 0;
    if (!ciphernes_state::read_u64(p,end,toastUntil)) return 0;
    if (!ciphernes_state::read_string(p,end,toast)) return 0;

    if (!h->bus.loadState(p,end)) return 0;

    (void)turbo; // reserved legacy host-control bit
    h->toastUntilUtcMs = toastUntil;
    h->toastText = toast;
    return 1;
}

CIPHERNES_API int ciphernes_set_mmc3_alternate_irq_revision(CIPHERNES_Handle* h, int alternate) {
    if (!h || !h->cart) return 0;
    return h->cart->setMmc3AlternateIrqRevision(alternate != 0) ? 1 : 0;
}

CIPHERNES_API int ciphernes_set_game_genie_codes(CIPHERNES_Handle* h, const char** codes_utf8, int count) {
    if (!h || !codes_utf8 || count <= 0) return 0;
    int ok = 0;
    h->bus.clearPatches();
    for (int i = 0; i < count; i++) {
        if (!codes_utf8[i]) continue;
        GameGeniePatch gp;
        if (!parse_nes_game_genie(std::string(codes_utf8[i]), gp)) continue;
        Bus::Patch p;
        p.addr = gp.addr;
        p.value = gp.value;
        p.hasCompare = gp.hasCompare;
        p.compare = gp.compare;
        h->bus.patches.push_back(p);
        ok++;
    }
    h->bus.rebuildPatchLookup();
    return ok;
}

CIPHERNES_API void ciphernes_clear_game_genie(CIPHERNES_Handle* h) {
    if (!h) return;
    h->bus.clearPatches();
}

