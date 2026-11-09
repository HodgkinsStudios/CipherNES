// Created by Jacob Hodgkins
// CipherNES 1.0 host-only PCM resampler; does not affect emulated APU timing.
#ifndef CIPHERNES_HOST_AUDIO_RESAMPLER_H
#define CIPHERNES_HOST_AUDIO_RESAMPLER_H
#include <cstdint>
#include <cmath>
#include <vector>
#include <algorithm>

namespace ciphernes_audio {
class SpeedResampler {
    double phase_ = 0.0;
    int16_t previous_ = 0;
public:
    void reset(int16_t previous = 0) { phase_ = 0.0; previous_ = previous; }
    // Continuously interpolates previous -> current source samples. No fixed
    // per-frame rounding or fade that would cause 60-Hz periodic distortion.
    void append(const std::vector<int16_t>& input, double speed, std::vector<int16_t>& output) {
        output.clear();
        if (input.empty()) return;
        const double rate = std::max(1.0, std::min(8.0, speed));
        output.reserve(input.size() + 2u);
        double p = phase_;
        const size_t n = input.size();
        while (p < static_cast<double>(n)) {
            const size_t at = static_cast<size_t>(p);
            const double mix = p - static_cast<double>(at);
            const int16_t left = at == 0 ? previous_ : input[at - 1];
            const int16_t right = input[at];
            output.push_back(static_cast<int16_t>(std::lround(double(left) * (1.0 - mix) + double(right) * mix)));
            p += rate;
        }
        phase_ = p - static_cast<double>(n);
        previous_ = input.back();
    }
};
}
#endif
