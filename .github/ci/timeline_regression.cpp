// Created by Jacob Hodgkins
// Standalone deterministic timeline regression. Supply a legally obtained .nes ROM.
#include "../../CipherNES_Core/ciphernes_core.h"
#include "../../CipherNES_Core/rewind_history.h"
#include <cstdint>
#include <iostream>
#include <vector>
#include <cstring>

static std::vector<uint8_t> stateOf(CIPHERNES_Handle* core) {
    std::vector<uint8_t> out(ciphernes_get_state_size(core) + 4096);
    const uint32_t n = ciphernes_save_state_transient(core, out.data(), static_cast<uint32_t>(out.size()));
    out.resize(n);
    return out;
}
static void sample(CIPHERNES_Handle* core, std::vector<int16_t>& audio) {
    audio.clear(); int16_t block[4096]; int n;
    do { n = ciphernes_take_audio_samples_i16(core,block,4096); if (n>0) audio.insert(audio.end(),block,block+n); } while(n>0);
}
int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "Usage: timeline_regression <owned-rom.nes>\n"; return 2; }
    CIPHERNES_Handle* core = ciphernes_create();
    if (!core || !ciphernes_load_rom(core, argv[1])) { std::cerr << "ROM load failed\n"; return 2; }
    ciphernes_rewind::History history;
    history.configure(ciphernes_rewind::kRewindMemoryMiB, ciphernes_rewind::kRewindMaxFrames);
    std::vector<std::vector<uint8_t>> states;
    std::vector<std::vector<uint32_t>> images;
    std::vector<int16_t> audio;
    for (int i=0;i<91;++i) {
        if (i) ciphernes_run_frame_with_input(core, i%3==0?0x80:0,0);
        sample(core,audio);
        std::vector<uint8_t> state = stateOf(core);
        if (state.empty() || !history.append(state,ciphernes_get_framebuffer(core),audio,i)) return 3;
        states.push_back(state);
        const uint32_t* px=ciphernes_get_framebuffer(core);
        images.emplace_back(px,px+256*240);
    }
    if (history.size()!=91 || history.cursor()!=90) return 4;
    std::vector<uint8_t> state;
    for(int i=89;i>=45;--i) {
        if (!history.stepBack(state,audio) || history.tasCursor()!=uint64_t(i) || state!=states[i]) {
            std::cerr << "State/timeline mismatch at " << i << "\n"; return 5;
        }
        if (std::memcmp(history.image(),images[i].data(),256*240*4)!=0) {
            std::cerr << "Rewind picture mismatch at " << i << "\n"; return 6;
        }
        if (!ciphernes_load_state(core,state.data(),static_cast<uint32_t>(state.size()))) return 7;
    }
    history.commit();
    if (history.size()!=46 || history.cursor()!=45) return 8;
    for(int i=0;i<5;++i) {
        ciphernes_run_frame_with_input(core,0x40,0);
        sample(core,audio);
        if(!history.append(stateOf(core),ciphernes_get_framebuffer(core),audio,46+i)) return 9;
    }
    if (history.size()!=51 || history.tasCursor()!=50) return 10;
    std::vector<uint8_t> full(8192,0); for(size_t i=0;i<full.size();i+=29)full[i]=uint8_t(i);
    std::vector<uint8_t> uncompressed;
    if (!ciphernes_rewind::unpack(ciphernes_rewind::pack(full.data(),full.size()),uncompressed,full.size()) || uncompressed!=full) return 11;
    // v1.0.1: Long-running capture must never retain more than 600 frames,
    // must correctly reverse the earliest retained frame, and must remove
    // abandoned future data when recording after a backward step.
    ciphernes_rewind::History bounded;
    bounded.configure(ciphernes_rewind::kRewindMemoryMiB, ciphernes_rewind::kRewindMaxFrames);
    std::vector<uint32_t> frame(256u*240u, 0xff123456u);
    std::vector<int16_t> tinyAudio(8, 16);
    for (uint64_t i = 0; i < 750; ++i) {
        frame[0] = 0xff000000u | uint32_t(i);
        std::vector<uint8_t> syntheticState(32, 0);
        for (size_t j = 0; j < syntheticState.size(); ++j)
            syntheticState[j] = uint8_t((i >> ((j % 8) * 8)) & 0xff);
        if (!bounded.append(syntheticState, frame.data(), tinyAudio, i)) return 12;
        if (bounded.size() > ciphernes_rewind::kRewindMaxFrames ||
            bounded.usedBytes() > ciphernes_rewind::kRewindMemoryMiB * 1048576u) return 13;
    }
    if (bounded.size() != 600 || bounded.tasCursor() != 749) return 14;
    for (int i = 748; i >= 150; --i) {
        if (!bounded.stepBack(state, audio) || bounded.tasCursor() != uint64_t(i) ||
            !bounded.image() || bounded.image()[0] != (0xff000000u | uint32_t(i))) return 15;
        for (size_t j = 0; j < state.size(); ++j)
            if (state[j] != uint8_t((uint64_t(i) >> ((j % 8) * 8)) & 0xff)) return 16;
    }
    if (bounded.canStepBack() || bounded.cursor() != 0) return 17;
    bounded.commit();
    if (bounded.size() != 1) return 18;
    frame[0] = 0xff99aacc;
    const std::vector<uint8_t> rebranch(13, 91);
    if (!bounded.append(rebranch.data(), rebranch.size(), frame.data(), tinyAudio, 999) ||
        !bounded.stepBack(state, audio) || bounded.tasCursor() != 150 ||
        bounded.image()[0] != (0xff000000u | 150u)) return 19;

    // Compressibility-independent allocation cap: large, mostly incompressible
    // serialized states must evict earlier frames *before* 600 entries.
    ciphernes_rewind::History memoryBounded;
    memoryBounded.configure(ciphernes_rewind::kRewindMemoryMiB, ciphernes_rewind::kRewindMaxFrames);
    std::vector<uint8_t> largeState(512u * 1024u);
    uint32_t rng = 0x12345678u;
    for (size_t i=0; i<largeState.size(); ++i) {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        largeState[i] = uint8_t(rng);
    }
    for (uint64_t i=0; i<150; ++i) {
        largeState[i] ^= uint8_t(i + 1);
        if (!memoryBounded.append(largeState.data(), largeState.size(), frame.data(), tinyAudio, i)) return 20;
        if (memoryBounded.usedBytes() > ciphernes_rewind::kRewindMemoryMiB * 1048576u) return 21;
    }
    if (memoryBounded.size() >= 150 || !memoryBounded.canStepBack() ||
        !memoryBounded.stepBack(state, audio) || memoryBounded.tasCursor() != 148) return 22;
    std::cout << "REWIND BOUNDS PASS: 600 frames, oldest state/video, branch, 64 MiB heap budget.\n";
    std::cout << "TIMELINE TEST PASS: 45 reversible frames, exact state/pixels, branch truncation, new inputs and codec. "
        << "Stored bytes: " << history.usedBytes() << "\n";
    ciphernes_destroy(core);
    return 0;
}
