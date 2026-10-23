// Created by Jacob Hodgkins
// Measures how quickly the host can emulate and retain COMPLETE reversible frames.
// Distinct from the headless core-only --benchmark and not a GUI FPS guarantee.
#include "../../CipherNES_Core/ciphernes_core.h"
#include "../../CipherNES_Core/rewind_history.h"
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <vector>
#include <cstdint>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: benchmark_timeline <owned-rom.nes> [frames, default 360]\n";
        return 2;
    }
    int frames = argc > 2 ? std::atoi(argv[2]) : 360;
    if (frames < 1 || frames > 3600) return 2;
    CIPHERNES_Handle* core = ciphernes_create();
    if (!core || !ciphernes_load_rom(core, argv[1])) {
        std::cerr << "ROM failed to load\n";
        return 3;
    }
    ciphernes_set_audio_sample_rate(core, 44100.0);
    ciphernes_rewind::History history;
    history.configure(256, 1800);
    const size_t maxStateBytes = ciphernes_get_state_size(core) + 4096u;
    std::vector<uint8_t> state(maxStateBytes);
    std::vector<int16_t> audio;
    audio.reserve(1024);
    int16_t block[4096];
    uint64_t ticks=0;
    auto before=std::chrono::steady_clock::now();
    for (int i=0;i<frames;++i) {
        ticks += ciphernes_run_frame_with_input(core, uint8_t(i % 40 == 0 ? 0x10 : 0), 0);
        audio.clear();
        int n;
        do { n = ciphernes_take_audio_samples_i16(core, block, 4096);
             if (n > 0) audio.insert(audio.end(),block,block+n);
        } while (n == 4096);
        uint32_t written = ciphernes_save_state_transient(core, state.data(),uint32_t(state.size()));
        if (!written || written > maxStateBytes) return 4;
        state.resize(written);
        if (!history.append(state,ciphernes_get_framebuffer(core),audio,uint64_t(i))) return 4;
        state.resize(maxStateBytes);
    }
    const double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-before).count();
    std::cout << std::fixed << std::setprecision(2)
        << "Frames with exact state+image+audio history: " << frames << "\n"
        << "NTSC throughput with rewind: " << frames / elapsed << " fps ("
        << frames / elapsed / 60.098813897440515 << "x real time)\n"
        << "History retained: " << history.size() << " frames, "
        << (history.usedBytes() / 1048576.0) << " MiB\n"
        << "Master ticks: " << ticks << "\n";
    ciphernes_destroy(core);
    return 0;
}
