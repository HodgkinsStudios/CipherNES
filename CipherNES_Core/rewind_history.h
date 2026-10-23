// Created by Jacob Hodgkins
// CipherNES 1.0: bounded, reversible per-frame history (frontend utility).
// No third-party dependencies. Stores key state per frame and lossless XOR video deltas.
#ifndef CIPHERNES_REWIND_HISTORY_H
#define CIPHERNES_REWIND_HISTORY_H
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <vector>

namespace ciphernes_rewind {
// A literal / zero-run packet stream. Control byte's high bit distinguishes
// zero-runs from literal runs (length: low seven bits + 1).
inline std::vector<uint8_t> pack(const uint8_t* src, size_t size) {
    std::vector<uint8_t> result;
    result.reserve(size / 2);
    for (size_t i = 0; i < size;) {
        if (src[i] == 0) {
            size_t run = 1;
            while (run < 128 && i + run < size && src[i + run] == 0) ++run;
            result.push_back(static_cast<uint8_t>(0x80u | (run - 1)));
            i += run;
        } else {
            const size_t start = i++;
            while (i - start < 128 && i < size && src[i] != 0) ++i;
            result.push_back(static_cast<uint8_t>((i - start) - 1));
            result.insert(result.end(), src + start, src + i);
        }
    }
    return result;
}
inline bool unpack(const std::vector<uint8_t>& packed, std::vector<uint8_t>& out, size_t expected) {
    if (expected > 64u * 1024u * 1024u) return false;
    out.resize(expected);
    size_t at = 0, offset = 0;
    while (at < packed.size()) {
        const uint8_t cmd = packed[at++];
        const size_t count = (cmd & 127u) + 1u;
        if (count > expected - offset) return false;
        if (cmd & 128u) std::memset(out.data() + offset, 0, count);
        else {
            if (count > packed.size() - at) return false;
            std::memcpy(out.data() + offset, packed.data() + at, count);
            at += count;
        }
        offset += count;
    }
    return offset == expected;
}

struct Entry {
    std::vector<uint8_t> state, videoDiff;
    std::vector<int16_t> audio;
    size_t uncompressedState = 0;
    uint64_t tasCursor = 0;
    size_t storage() const { return state.size() + videoDiff.size() + audio.size() * sizeof(int16_t) + sizeof(Entry); }
};

class History {
    std::deque<Entry> frames_;
    std::vector<uint8_t> pixels_;  // video framebuffer at the selected cursor
    std::vector<uint8_t> deltaScratch_; // avoid reallocating a 240 KiB buffer every frame
    size_t cursor_ = 0;
    size_t bytes_ = 0;
    size_t frameBytes_ = 256u * 240u * 4u;
    size_t limitBytes_ = 256u * 1024u * 1024u;
    size_t limitFrames_ = 1800;
public:
    void clear() { frames_.clear(); pixels_.clear(); cursor_ = bytes_ = 0; }
    void configure(size_t memoryMegabytes, size_t frames) {
        limitBytes_ = memoryMegabytes * 1024u * 1024u;
        limitFrames_ = frames;
        trim();
    }
    void trim() {
        while (frames_.size() > 1 && (frames_.size() > limitFrames_ || bytes_ > limitBytes_)) {
            bytes_ -= frames_.front().storage(); frames_.pop_front();
            if (cursor_ > 0) --cursor_;
        }
    }
    size_t size() const { return frames_.size(); }
    size_t cursor() const { return cursor_; }
    size_t usedBytes() const { return bytes_; }
    bool canStepBack() const { return frames_.size() >= 2 && cursor_ > 0; }
    uint64_t tasCursor() const { return frames_.empty() ? 0 : frames_[cursor_].tasCursor; }
    const uint32_t* image() const { return pixels_.size() == frameBytes_ ? reinterpret_cast<const uint32_t*>(pixels_.data()) : nullptr; }
    void commit() {
        while (frames_.size() > cursor_ + 1) { bytes_ -= frames_.back().storage(); frames_.pop_back(); }
    }
    bool append(const std::vector<uint8_t>& state, const uint32_t* image, const std::vector<int16_t>& audio, uint64_t tasCursor) {
        if (state.empty() || !image) return false;
        commit();
        Entry e;
        e.uncompressedState = state.size();
        e.state = pack(state.data(), state.size());
        const uint8_t* now = reinterpret_cast<const uint8_t*>(image);
        deltaScratch_.resize(frameBytes_);
        if (pixels_.size() == frameBytes_) {
            for (size_t i = 0; i < frameBytes_; ++i) deltaScratch_[i] = now[i] ^ pixels_[i];
        } else std::fill(deltaScratch_.begin(), deltaScratch_.end(), uint8_t(0));
        e.videoDiff = pack(deltaScratch_.data(), deltaScratch_.size());
        e.audio = audio;
        e.tasCursor = tasCursor;
        bytes_ += e.storage();
        frames_.push_back(std::move(e));
        cursor_ = frames_.size() - 1;
        pixels_.assign(now, now + frameBytes_);
        trim();
        return true;
    }
    bool stepBack(std::vector<uint8_t>& state, std::vector<int16_t>& backwardsAudio) {
        if (!canStepBack()) return false;
        if (!unpack(frames_[cursor_].videoDiff, deltaScratch_, frameBytes_)) return false;
        const Entry& previous = frames_[cursor_ - 1];
        if (!unpack(previous.state, state, previous.uncompressedState)) return false;
        // Frame N's audio spans the interval N-1 -> N, hence reverse N when leaving N.
        backwardsAudio.assign(frames_[cursor_].audio.rbegin(), frames_[cursor_].audio.rend());
        // Short endpoint envelope prevents discontinuities at direction changes.
        const size_t fade = std::min<size_t>(48, backwardsAudio.size() / 2);
        for (size_t i = 0; i < fade; ++i) {
            backwardsAudio[i] = static_cast<int16_t>(int(backwardsAudio[i]) * int(i) / int(fade));
            backwardsAudio[backwardsAudio.size() - i - 1] =
                static_cast<int16_t>(int(backwardsAudio[backwardsAudio.size() - i - 1]) * int(i) / int(fade));
        }
        for (size_t i = 0; i < frameBytes_; ++i) pixels_[i] ^= deltaScratch_[i];
        --cursor_;
        return true;
    }
};
}
#endif
