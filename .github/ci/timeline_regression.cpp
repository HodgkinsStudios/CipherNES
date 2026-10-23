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
    history.configure(256,1800);
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
    std::cout << "TIMELINE TEST PASS: 45 reversible frames, exact state/pixels, branch truncation, new inputs and codec. "
        << "Stored bytes: " << history.usedBytes() << "\n";
    ciphernes_destroy(core);
    return 0;
}
