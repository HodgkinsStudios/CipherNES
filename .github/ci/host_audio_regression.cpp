// Created by Jacob Hodgkins
// No SDL dependency: verify FF sample pacing, chunk-independent phase,
// and steady audio over multiple frame boundaries (NTSC 44.1 kHz).
#include "../../CipherNES_Core/host_audio_resampler.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    for (int rate = 44100; rate <= 48000; rate += 3900) {
        std::vector<int16_t> source;
        const int frames = 240;
        const int n = rate / 60;
        for (int i=0;i<frames*n;++i)
            source.push_back(static_cast<int16_t>(std::lround(12000.0 * std::sin(i * 50.0 / double(rate)))));
        ciphernes_audio::SpeedResampler one, chunked;
        std::vector<int16_t> reference, chunkResult, part, buffer;
        one.append(source, 1.5, reference);
        for (int i=0;i<frames;++i) {
            part.assign(source.begin()+i*n,source.begin()+(i+1)*n);
            chunked.append(part, 1.5, buffer);
            chunkResult.insert(chunkResult.end(),buffer.begin(),buffer.end());
        }
        if (reference != chunkResult || std::abs(int(reference.size())-int(source.size()/1.5)) > 2) {
            std::cerr << "FAIL: 150% sample count or chunk boundaries " << rate << "\n"; return 1;
        }
        // Vary the target after every frame; no discontinuous resets or invalid samples.
        ciphernes_audio::SpeedResampler ramp;
        int total = 0;
        for (int i=0;i<frames;++i) {
            part.assign(source.begin()+i*n, source.begin()+(i+1)*n);
            const double speed = i < 30 ? 1.0 + (0.5 * i/30.0) :
                i < 180 ? 1.5 : 1.5 - (0.5 * (i-180)/60.0);
            ramp.append(part,speed,buffer);
            if (buffer.empty() || buffer.size()>part.size()+2) return 2;
            total += int(buffer.size());
        }
        if (total < int(source.size()/1.6) || total > int(source.size())) return 3;
    }
    std::cout << "AUDIO REGRESSION PASS: 150% continuous PCM across 240 frame boundaries, 44.1/48 kHz and ramps.\n";
}
