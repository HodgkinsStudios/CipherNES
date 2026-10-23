#include "apu2A03.h"

#include "state_io.h"

#include "bus.h"

#include <algorithm>
#include <cmath>

#if defined(CIPHERNES_DEBUG_4015_PHASE) || defined(CIPHERNES_DEBUG_ABORT_TABLES)
#include <cstdio>
#endif

// ------------------------------
// Static lookup tables (defined out-of-line for broad toolchain compatibility)
// ------------------------------

const uint8_t APU2A03::lengthTable[32] = {
    10, 254, 20,  2, 40,  4, 80,  6,
    160, 8, 60, 10, 14, 12, 26, 14,
    12, 16, 24, 18, 48, 20, 96, 22,
    192, 24, 72, 26, 16, 28, 32, 30
};

const uint8_t APU2A03::dutyTable[4][8] = {
    {0,1,0,0,0,0,0,0},
    {0,1,1,0,0,0,0,0},
    {0,1,1,1,1,0,0,0},
    {1,0,0,1,1,1,1,1}
};

const uint8_t APU2A03::triTable[32] = {
    15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
     0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15
};

const uint16_t APU2A03::noisePeriodTable[16] = {
    4, 8, 16, 32, 64, 96, 128, 160,
    202, 254, 380, 508, 762, 1016, 2034, 4068
};

const uint16_t APU2A03::dmcRateTable[16] = {
    428, 380, 340, 320, 286, 254, 226, 214,
    190, 160, 142, 128, 106,  84,  72,  54
};

// ------------------------------
// Helpers
// ------------------------------

static inline uint16_t lohi(uint8_t lo, uint8_t hi) {
    return uint16_t(lo) | (uint16_t(hi) << 8);
}

// ------------------------------
// Pulse
// ------------------------------

void APU2A03::Pulse::reset(uint8_t idx) {
    reg0 = reg1 = reg2 = reg3 = 0;
    timer = 0;
    timerCounter = 0;
    dutyStep = 0;

    envelopeStart = false;
    envelopeDivider = 0;
    envelopeDecay = 0;

    lengthCounter = 0;
    lengthHaltEffective = false;
    lengthHaltPending = false;
    lengthHaltPendingValue = false;
    suppressLengthClockOnce = false;

    sweepReload = false;
    sweepDivider = 0;

    enabled = false;
    channelIndex = idx;
}

uint16_t APU2A03::Pulse::targetPeriod() const {
    const uint16_t cur = timer;
    const uint16_t change = cur >> sweepShift();
    if (!sweepNegate()) {
        return uint16_t(cur + change);
    }
    // Pulse 1 uses one's complement; Pulse 2 uses two's complement.
    if (channelIndex == 0) {
        return uint16_t(cur - change - 1);
    }
    return uint16_t(cur - change);
}

bool APU2A03::Pulse::sweepMute() const {
    // Mute if period too small or target over 0x7FF
    if (timer < 8) return true;
    if (sweepEnabled() && sweepShift() != 0) {
        if (targetPeriod() > 0x07FF) return true;
    }
    return false;
}

void APU2A03::Pulse::clockTimer() {
    if (timerCounter == 0) {
        timerCounter = timer;
        dutyStep = (dutyStep + 1) & 7;
    } else {
        timerCounter--;
    }
}

void APU2A03::Pulse::clockEnvelope() {
    if (envelopeStart) {
        envelopeStart = false;
        envelopeDecay = 15;
        envelopeDivider = volumeOrEnvelope();
    } else {
        if (envelopeDivider == 0) {
            envelopeDivider = volumeOrEnvelope();
            if (envelopeDecay == 0) {
                if (lengthHalt()) envelopeDecay = 15;
            } else {
                envelopeDecay--;
            }
        } else {
            envelopeDivider--;
        }
    }
}

void APU2A03::Pulse::clockLength() {
    if (suppressLengthClockOnce) {
        suppressLengthClockOnce = false;
        return;
    }
    if (!counterHalted() && lengthCounter > 0) {
        lengthCounter--;
    }
}

void APU2A03::Pulse::clockSweep() {
    if (sweepDivider == 0) {
        sweepDivider = sweepPeriod();
        if (sweepEnabled() && sweepShift() != 0 && !sweepMute()) {
            timer = targetPeriod();
        }
    } else {
        sweepDivider--;
    }

    if (sweepReload) {
        sweepReload = false;
        sweepDivider = sweepPeriod();
    }
}

uint8_t APU2A03::Pulse::output() const {
    if (!enabled) return 0;
    if (lengthCounter == 0) return 0;
    if (sweepMute()) return 0;
    if (APU2A03::dutyTable[duty()][dutyStep] == 0) return 0;

    if (constantVolume()) return volumeOrEnvelope();
    return envelopeDecay;
}

// ------------------------------
// Triangle
// ------------------------------

void APU2A03::Triangle::reset() {
    reg0 = reg1 = reg2 = reg3 = 0;
    timer = 0;
    timerCounter = 0;
    seqStep = 0;
    lengthCounter = 0;
    lengthControlEffective = false;
    lengthControlPending = false;
    lengthControlPendingValue = false;
    suppressLengthClockOnce = false;
    linearCounter = 0;
    linearReload = false;
    enabled = false;
}

void APU2A03::Triangle::clockTimer() {
    if (timerCounter == 0) {
        timerCounter = timer;
        if (lengthCounter > 0 && linearCounter > 0) {
            seqStep = (seqStep + 1) & 31;
        }
    } else {
        timerCounter--;
    }
}

void APU2A03::Triangle::clockLength() {
    if (suppressLengthClockOnce) {
        suppressLengthClockOnce = false;
        return;
    }
    if (!lengthControlEffective && lengthCounter > 0) {
        lengthCounter--;
    }
}

void APU2A03::Triangle::clockLinear() {
    if (linearReload) {
        linearCounter = linearReloadValue();
    } else {
        if (linearCounter > 0) linearCounter--;
    }

    if (!controlFlag()) {
        linearReload = false;
    }
}

uint8_t APU2A03::Triangle::output() const {
    if (!enabled) return 0;
    if (lengthCounter == 0) return 0;
    if (linearCounter == 0) return 0;
    // Hardware mutes when timer < 2
    if (timer < 2) return 0;
    return APU2A03::triTable[seqStep];
}

// ------------------------------
// Noise
// ------------------------------

void APU2A03::Noise::reset() {
    reg0 = reg1 = reg2 = reg3 = 0;
    timerPeriod = 0;
    timerCounter = 0;
    shiftReg = 1;
    envelopeStart = false;
    envelopeDivider = 0;
    envelopeDecay = 0;
    lengthCounter = 0;
    lengthHaltEffective = false;
    lengthHaltPending = false;
    lengthHaltPendingValue = false;
    suppressLengthClockOnce = false;
    enabled = false;
    updatePeriod();
}

void APU2A03::Noise::updatePeriod() {
    timerPeriod = APU2A03::noisePeriodTable[periodIndex() & 0x0F];
}

void APU2A03::Noise::clockTimer() {
    if (timerCounter == 0) {
        timerCounter = timerPeriod > 0 ? uint16_t(timerPeriod - 1) : 0;

        // LFSR feedback
        const uint16_t bit0 = shiftReg & 0x0001;
        const uint16_t tap = mode() ? ((shiftReg >> 6) & 0x0001) : ((shiftReg >> 1) & 0x0001);
        const uint16_t feedback = bit0 ^ tap;
        shiftReg >>= 1;
        shiftReg |= (feedback << 14);
    } else {
        timerCounter--;
    }
}

void APU2A03::Noise::clockEnvelope() {
    if (envelopeStart) {
        envelopeStart = false;
        envelopeDecay = 15;
        envelopeDivider = volumeOrEnvelope();
    } else {
        if (envelopeDivider == 0) {
            envelopeDivider = volumeOrEnvelope();
            if (envelopeDecay == 0) {
                if (lengthHalt()) envelopeDecay = 15;
            } else {
                envelopeDecay--;
            }
        } else {
            envelopeDivider--;
        }
    }
}

void APU2A03::Noise::clockLength() {
    if (suppressLengthClockOnce) {
        suppressLengthClockOnce = false;
        return;
    }
    if (!counterHalted() && lengthCounter > 0) {
        lengthCounter--;
    }
}

uint8_t APU2A03::Noise::output() const {
    if (!enabled) return 0;
    if (lengthCounter == 0) return 0;
    if (shiftReg & 0x0001) return 0; // bit0 = 1 mutes
    if (constantVolume()) return volumeOrEnvelope();
    return envelopeDecay;
}

// ------------------------------
// DMC
// ------------------------------

void APU2A03::DMC::reset() {
    reg0 = reg1 = reg2 = reg3 = 0;
    enabled = false;
    pendingEnabled = false;
    statusDelay = 0;
    outputLevel = 0;

    sampleAddress = sampleLength = 0;
    currentAddress = 0;
    bytesRemaining = 0;
    sampleBuffer = 0;
    sampleBufferEmpty = true;

    timerPeriod = 0;
    timerCounter = 0;
    shiftReg = 0;
    bitsRemaining = 8;
    silence = true;
    irqFlag = false;
    dmaRequest = false;
    boundaryFetchRequest = false;
    boundaryFetchBlocked = false;
    loadDmaCountdown = 0;
    restartFetchDelay = 0;
    implicitAbortPending = false;
    implicitAbortQueued = false;
    implicitAbortActive = false;
    updateRate();
}

void APU2A03::DMC::updateRate() {
    timerPeriod = APU2A03::dmcRateTable[rateIndex() & 0x0F];
}

void APU2A03::DMC::startSample() {
    sampleAddress = 0xC000 | (uint16_t(reg2) << 6);
    sampleLength  = uint16_t(reg3) * 16 + 1;
    currentAddress = sampleAddress;
    bytesRemaining = sampleLength;
}

void APU2A03::DMC::requestSampleFetch() {
    if (loadDmaCountdown > 0) return;
    if (restartFetchDelay > 0) return;
    if (boundaryFetchBlocked) return;
    if (!sampleBufferEmpty) return;
    if (dmaRequest) return;
    if (bytesRemaining == 0) {
        if (loop()) {
            startSample();
        } else {
            if (irqEnable()) irqFlag = true;
            return;
        }
    }

    dmaRequest = true;
}

void APU2A03::DMC::completeSampleFetch(uint8_t data) {
    if (!dmaRequest) return;

    sampleBuffer = data;
    sampleBufferEmpty = false;
    dmaRequest = false;
    boundaryFetchRequest = false;
    boundaryFetchBlocked = false;
    currentAddress++;
    if (currentAddress == 0x0000) currentAddress = 0x8000; // wrap

    bytesRemaining--;
    if (bytesRemaining == 0) {
        if (loop()) {
            startSample();
        } else if (irqEnable()) {
            irqFlag = true;
        }
    }
}

void APU2A03::DMC::clockTimer(APU2A03& apu) {
    if (timerCounter == 0) {
        timerCounter = timerPeriod > 0 ? uint16_t(timerPeriod - 1) : 0;

        // Output unit
        if (!silence) {
            if (shiftReg & 0x01) {
                if (outputLevel <= 125) outputLevel += 2;
            } else {
                if (outputLevel >= 2) outputLevel -= 2;
            }
            shiftReg >>= 1;
        }

        // The verified DMC timing model reloads the bit counter and promotes the sample
        // buffer on the same output clock that consumes bit eight. Keeping
        // bitsRemaining at zero until the next timer tick shifts load-DMA
        // collisions by an entire DMC period.
        if (bitsRemaining > 0) bitsRemaining--;
        if (bitsRemaining == 0) {
            bitsRemaining = 8;
            boundaryFetchRequest = true;
            boundaryFetchBlocked = false;
            if (implicitAbortPending) {
                implicitAbortActive = true;
                implicitAbortPending = false;
            }
            if (sampleBufferEmpty) {
                silence = true;
            } else {
                silence = false;
                shiftReg = sampleBuffer;
                sampleBufferEmpty = true;
            }
        }
    } else {
        timerCounter--;
    }
}

// ------------------------------
// APU core
// ------------------------------

void APU2A03::reset() {
    cpuCycle = 0;
    irq = false;

    frameCounterReg = 0;
    frameIrqInhibit = false;
    frameIrqFlag = false;
    frameIrqLine = false;
    fiveStepMode = false;
    frameIrqClearPending = false;
    frameIrqInhibitTransient = 0;
    frameIrqPulseRemaining = 0;
    lastCpuBusGetCycle = false;
    frameSeqCounter = 0;
    frameSeqResetPending = false;
    frameSeqResetDelay = 0;

    pulse1.reset(0);
    pulse2.reset(1);
    tri.reset();
    noise.reset();
    dmc.reset();

    clearSampleQueue();
    sampleTimeAccumulator = 0.0;

    updateFilterCoeffs();

    // Reset filter state
    hp90_y = hp90_x1 = 0.0f;
    hp440_y = hp440_x1 = 0.0f;
    lp_y = 0.0f;
}

void APU2A03::softReset() {
    // The console RESET line does not erase the APU register latches. It acts
    // like $4015=$00 and rewrites the last $4017 value while the CPU performs
    // its reset sequence. Preserve the latches, but clear channel enables,
    // length counters, DMC activity, and IRQ flags.
    const Pulse oldPulse1 = pulse1;
    const Pulse oldPulse2 = pulse2;
    const Triangle oldTri = tri;
    const Noise oldNoise = noise;
    const DMC oldDmc = dmc;
    const uint8_t oldFrameCounterReg = frameCounterReg;

    reset();

    pulse1.reg0 = oldPulse1.reg0;
    pulse1.reg1 = oldPulse1.reg1;
    pulse1.reg2 = oldPulse1.reg2;
    pulse1.reg3 = oldPulse1.reg3;
    pulse1.timer = oldPulse1.timer;
    pulse1.lengthHaltEffective = pulse1.lengthHalt();

    pulse2.reg0 = oldPulse2.reg0;
    pulse2.reg1 = oldPulse2.reg1;
    pulse2.reg2 = oldPulse2.reg2;
    pulse2.reg3 = oldPulse2.reg3;
    pulse2.timer = oldPulse2.timer;
    pulse2.lengthHaltEffective = pulse2.lengthHalt();

    tri.reg0 = oldTri.reg0;
    tri.reg1 = oldTri.reg1;
    tri.reg2 = oldTri.reg2;
    tri.reg3 = oldTri.reg3;
    tri.timer = oldTri.timer;
    tri.lengthControlEffective = tri.controlFlag();

    noise.reg0 = oldNoise.reg0;
    noise.reg1 = oldNoise.reg1;
    noise.reg2 = oldNoise.reg2;
    noise.reg3 = oldNoise.reg3;
    noise.lengthHaltEffective = noise.lengthHalt();
    noise.updatePeriod();

    dmc.reg0 = oldDmc.reg0;
    dmc.reg1 = oldDmc.reg1;
    dmc.reg2 = oldDmc.reg2;
    dmc.reg3 = oldDmc.reg3;
    dmc.outputLevel = oldDmc.outputLevel;
    dmc.updateRate();

    frameCounterReg = oldFrameCounterReg;
    fiveStepMode = (frameCounterReg & 0x80) != 0;
    frameIrqInhibit = (frameCounterReg & 0x40) != 0;
    frameIrqFlag = false;
    frameIrqLine = false;
    frameIrqClearPending = false;
    frameIrqPulseRemaining = 0;
    frameSeqCounter = 0;
    frameSeqResetPending = false;
    frameSeqResetDelay = 0;

    // Rewriting a five-step value clocks both frame units immediately. The
    // channel lengths are zero after the implicit $4015 clear, but envelope,
    // sweep, and linear-counter latches still observe the write correctly.
    if (fiveStepMode) {
        clockQuarterFrame();
        clockHalfFrame();
    }

    irq = false;
}

void APU2A03::clearSampleQueue() {
    samples.clear();
    sampleReadOffset = 0;
}

std::size_t APU2A03::queuedSampleCount() const {
    return sampleReadOffset < samples.size() ? (samples.size() - sampleReadOffset) : 0;
}

void APU2A03::compactSampleQueueIfNeeded() {
    if (sampleReadOffset == 0) return;
    if (sampleReadOffset >= samples.size()) {
        clearSampleQueue();
        return;
    }

    // Avoid an O(n) erase on every small audio callback.  Compact only when
    // enough already-consumed samples have accumulated at the front.
    if (sampleReadOffset >= 4096 && sampleReadOffset * 2 >= samples.size()) {
        samples.erase(samples.begin(), samples.begin() + static_cast<std::ptrdiff_t>(sampleReadOffset));
        sampleReadOffset = 0;
    }
}

void APU2A03::enforceSampleQueueLimit() {
    // Keep at most roughly two seconds queued.  If the frontend is paused or
    // stops pulling audio, this prevents runaway latency/memory growth without
    // changing APU timing/state used by accuracy tests.
    const std::size_t maxQueued = static_cast<std::size_t>(sampleRate * 2.0);
    const std::size_t queued = queuedSampleCount();
    if (queued <= maxQueued) return;

    const std::size_t drop = queued - maxQueued;
    sampleReadOffset += drop;
    compactSampleQueueIfNeeded();
}

void APU2A03::setSampleRate(double sampleRateHz) {
    sampleRate = std::max(8000.0, sampleRateHz);
    timePerSample = 1.0 / sampleRate;
    timePerCpuTick = 1.0 / CPU_CLOCK_HZ;

    updateFilterCoeffs();
}

void APU2A03::updateFilterCoeffs() {
    // One-pole IIR coefficients derived from analog RC filters.
    // a = exp(-2*pi*fc/fs)
    const double fs = std::max(8000.0, sampleRate);
    auto a_from_fc = [fs](double fc) {
        constexpr double PI = 3.1415926535897932384626433832795;
        const double x = -2.0 * PI * fc / fs;
        return float(std::exp(x));
    };

    hp90_a  = a_from_fc(90.0);
    hp440_a = a_from_fc(440.0);
    lp14k_a = a_from_fc(14000.0);
}

float APU2A03::filterSample(float x) {
    // High-pass: y[n] = a * (y[n-1] + x[n] - x[n-1])
    hp90_y = hp90_a * (hp90_y + x - hp90_x1);
    hp90_x1 = x;

    hp440_y = hp440_a * (hp440_y + hp90_y - hp440_x1);
    hp440_x1 = hp90_y;

    // Low-pass: y[n] = a*y[n-1] + (1-a)*x[n]
    lp_y = lp14k_a * lp_y + (1.0f - lp14k_a) * hp440_y;
    return lp_y;
}

int APU2A03::copyAndClearSamplesI16(int16_t* outSamples, int maxSamples) {
    if (!outSamples || maxSamples <= 0) return 0;

    std::size_t available = queuedSampleCount();
    std::size_t n = available;
    if (n > static_cast<std::size_t>(maxSamples)) n = static_cast<std::size_t>(maxSamples);

    for (std::size_t i = 0; i < n; i++) {
        float x = samples[sampleReadOffset + i];
        if (x > 1.0f) x = 1.0f;
        if (x < -1.0f) x = -1.0f;
        int v = (int)(x * 32767.0f);
        if (v > 32767) v = 32767;
        if (v < -32768) v = -32768;
        outSamples[i] = (int16_t)v;
    }

    if (n > 0) {
        sampleReadOffset += n;
        compactSampleQueueIfNeeded();
    }
    return static_cast<int>(n);
}

void APU2A03::setAudioEnabled(bool enabled) {
    audioEnabled = enabled;
    if (!audioEnabled) {
        clearSampleQueue();
    }
}

// Frame sequencer clocks envelopes/linear (quarter frame) and length/sweep (half frame)
void APU2A03::clockQuarterFrame() {
    pulse1.clockEnvelope();
    pulse2.clockEnvelope();
    tri.clockLinear();
    noise.clockEnvelope();
}

void APU2A03::clockHalfFrame() {
    pulse1.clockLength();
    pulse2.clockLength();
    tri.clockLength();
    noise.clockLength();
    pulse1.clockSweep();
    pulse2.clockSweep();
}

bool APU2A03::halfFrameWillClockOnNextCpuCycle() const {
    // A register write occurs after the APU's work for the current CPU cycle.
    // When cpuCycle is odd, the next APU clock advances the CPU/2 frame
    // divider. This identifies the late write phase that collides with a
    // half-frame clock on real hardware.
    if ((cpuCycle & 1ULL) == 0ULL) return false;
    if (frameSeqResetPending && frameSeqResetDelay == 0 && fiveStepMode) return true;
    const uint32_t next = frameSeqCounter + 1;
    if (fiveStepMode) return next == 7457 || next == 18641;
    return next == 7457 || next == 14915;
}

void APU2A03::applyPendingLengthControls() {
    if (pulse1.lengthHaltPending) {
        pulse1.lengthHaltEffective = pulse1.lengthHaltPendingValue;
        pulse1.lengthHaltPending = false;
    }
    if (pulse2.lengthHaltPending) {
        pulse2.lengthHaltEffective = pulse2.lengthHaltPendingValue;
        pulse2.lengthHaltPending = false;
    }
    if (tri.lengthControlPending) {
        tri.lengthControlEffective = tri.lengthControlPendingValue;
        tri.lengthControlPending = false;
    }
    if (noise.lengthHaltPending) {
        noise.lengthHaltEffective = noise.lengthHaltPendingValue;
        noise.lengthHaltPending = false;
    }
}

void APU2A03::clockFrameSequencer() {
    // Sequencer ticks at CPU/2 (one tick every 2 CPU cycles)
    // Implement using a simple step counter.

    // The exact step timing is based on the APU's internal divider.
    // We model the canonical 4-step and 5-step sequences.

    // Step scheduling (APU sequencer tick domain):
    // 4-step: 3729(Q), 7457(Q+H), 11186(Q), 14915(Q+H + IRQ)
    // 5-step: 3729(Q), 7457(Q+H), 11186(Q), 14915(Q+H), 18641(Q)

    frameSeqCounter++;

    if (!fiveStepMode) {
        if (frameSeqCounter == 3729) clockQuarterFrame();
        if (frameSeqCounter == 7457) { clockQuarterFrame(); clockHalfFrame(); }
        if (frameSeqCounter == 11186) clockQuarterFrame();
        if (frameSeqCounter == 14914) {
            frameIrqInhibitTransient = frameIrqInhibit ? 2 : 0;
            frameIrqPulseRemaining = 4;
        }
        if (frameSeqCounter == 14915) {
            clockQuarterFrame();
            clockHalfFrame();
            frameIrqFlag = true;
            frameIrqLine = !frameIrqInhibit;
            frameIrqInhibitTransient = frameIrqInhibit ? 2 : 0;
            frameSeqCounter = 0;
        }
    } else {
        if (frameSeqCounter == 3729) clockQuarterFrame();
        if (frameSeqCounter == 7457) { clockQuarterFrame(); clockHalfFrame(); }
        if (frameSeqCounter == 11186) clockQuarterFrame();
        if (frameSeqCounter == 18641) {
            clockQuarterFrame();
            clockHalfFrame();
            frameSeqCounter = 0;
        }
    }
}

float APU2A03::mixOutput() const {
    // Nonlinear NES mixer. channelMask is an output-only debugger mute:
    // it does not stop timers, length counters, register side effects, DMC DMA,
    // frame IRQs, or game logic. It only removes the selected channel from the
    // final audible mix so the frontend can isolate APU problems safely.
    const uint8_t mask = channelMask & 0x1F;
    const float p1 = (mask & 0x01) ? float(pulse1.output()) : 0.0f;
    const float p2 = (mask & 0x02) ? float(pulse2.output()) : 0.0f;
    const float t  = (mask & 0x04) ? float(tri.output())   : 0.0f;
    const float n  = (mask & 0x08) ? float(noise.output()) : 0.0f;
    const float d  = (mask & 0x10) ? float(dmc.output())   : 0.0f;

    const float pulseOut = (p1 + p2) == 0.0f ? 0.0f : (95.88f / ((8128.0f / (p1 + p2)) + 100.0f));
    const float tndDen = (t / 8227.0f) + (n / 12241.0f) + (d / 22638.0f);
    const float tndOut = tndDen == 0.0f ? 0.0f : (159.79f / ((1.0f / tndDen) + 100.0f));
    return pulseOut + tndOut;
}

void APU2A03::clock() {
    // Apply delayed frame sequencer reset (the $4017 write resets after 3 or 4 CPU cycles)
    if (frameSeqResetPending) {
        if (frameSeqResetDelay > 0) {
            frameSeqResetDelay--;
        } else {
            frameSeqResetPending = false;
            frameSeqCounter = 0;
            if (fiveStepMode) {
                clockQuarterFrame();
                clockHalfFrame();
            }
        }
    }

    if (frameIrqInhibitTransient > 0) {
        frameIrqInhibitTransient--;
        if (frameIrqInhibitTransient == 0 && frameIrqInhibit) {
            frameIrqFlag = false;
            frameIrqLine = false;
            frameIrqClearPending = false;
        }
    }

    // ------------------------------------------------------------
    // Channel timers
    // ------------------------------------------------------------
    // On the NES, the pulse channel timers are updated every APU cycle
    // (i.e., every *second* CPU cycle), which is why the pulse frequency is
    // f = fCPU / (16*(t+1)) instead of fCPU / (8*(t+1)).
    // See: https://www.nesdev.org/wiki/APU_Pulse
    const bool apuCycle = ((cpuCycle & 1ULL) == 1ULL); // one APU cycle = 2 CPU cycles

    if (apuCycle) {
        pulse1.clockTimer();
        pulse2.clockTimer();
    }

    // Triangle timer is clocked every CPU cycle.
    // See: https://www.nesdev.org/wiki/Talk:APU (glossary discussion)
    tri.clockTimer();

    // Noise period table is specified in CPU cycles on nesdev.
    // We therefore clock it every CPU cycle using those CPU-cycle periods.
    // See: https://www.nesdev.org/wiki/APU_Noise
    noise.clockTimer();

    // Frame sequencer ticks every 2 CPU cycles (APU cycle)
    if (apuCycle) {
        clockFrameSequencer();
    }

    // Late-phase length-control writes take effect after this cycle's frame
    // clocks, matching the APU's two-phase register latch behavior.
    applyPendingLengthControls();

    // IRQ line combines frame IRQ and DMC IRQ
    irq = frameIrqLine || dmc.irqFlag;

    // Resample output to target sample rate
    sampleTimeAccumulator += timePerCpuTick;
    while (sampleTimeAccumulator >= timePerSample) {
        sampleTimeAccumulator -= timePerSample;

        // Mix output in roughly 0..1 range; keep as float for SDL.
        // Audio generation stays enabled during turbo; the frontend controls
        // fast-forward pitch/backlog by changing the sample rate.
        if (audioEnabled) {
            samples.push_back(filterSample(mixOutput()));
            enforceSampleQueueLimit();
        }
    }

    cpuCycle++;
}

void APU2A03::clockCpuBusAccess(bool getCycle) {
    lastCpuBusGetCycle = getCycle;
    if (getCycle && frameIrqClearPending) {
        frameIrqClearPending = false;
        frameIrqFlag = false;
        frameIrqLine = false;
        frameIrqInhibitTransient = 0;
        irq = frameIrqLine || dmc.irqFlag;
    }
}

uint8_t APU2A03::cpuRead(uint16_t addr, bool readOnly) {
    if (addr == 0x4015) {
        uint8_t status = 0;
        if (pulse1.lengthCounter > 0) status |= 0x01;
        if (pulse2.lengthCounter > 0) status |= 0x02;
        if (tri.lengthCounter > 0)    status |= 0x04;
        if (noise.lengthCounter > 0)  status |= 0x08;
        if (dmc.bytesRemaining > 0 && dmc.pendingEnabled) status |= 0x10;
        if (frameIrqFlag)             status |= 0x40;
        if (dmc.irqFlag)              status |= 0x80;

        // Hardware clears the frame IRQ flag on the next APU get cycle after
        // the status read, not in the read itself. DMC IRQ is not cleared here.
        if (!readOnly) frameIrqClearPending = true;
#ifdef CIPHERNES_DEBUG_4015_PHASE
        static int phaseLogCount = 0;
        if (!readOnly && (status & 0x40) && phaseLogCount < 160) {
            std::fprintf(stderr, "R4015 phase=%d status=%02X pending=%d frame=%d\n",
                         lastCpuBusGetCycle ? 1 : 0, status,
                         frameIrqClearPending ? 1 : 0, frameIrqFlag ? 1 : 0);
            phaseLogCount++;
        }
#endif
        return status;
    }
    return 0x00;
}

bool APU2A03::hasDmcDmaRequest() const {
    return dmc.dmaRequest;
}

bool APU2A03::isDmcBoundaryFetchRequest() const {
    return dmc.dmaRequest && dmc.boundaryFetchRequest;
}

void APU2A03::rejectDmcBoundaryFetchForCooldown() {
    if (!dmc.boundaryFetchRequest) return;
    dmc.dmaRequest = false;
    dmc.boundaryFetchBlocked = true;
}

bool APU2A03::isDmcDmaBusActive() const {
    return dmc.enabled || dmc.implicitAbortActive;
}

bool APU2A03::finishDmcImplicitAbortCycle(bool dmcDmaCycleRan) {
    bool cancelBusTransfer = false;

    if (dmc.implicitAbortActive) {
        dmc.implicitAbortActive = false;
        if (dmcDmaCycleRan && dmc.bytesRemaining == 0) {
            dmc.dmaRequest = false;
            cancelBusTransfer = true;
        }
    }

    return cancelBusTransfer;
}

bool APU2A03::cancelDmcImplicitAbortOnWrite(bool dmaHalted) {
    if (!dmc.implicitAbortActive || !dmaHalted) return false;
    dmc.implicitAbortActive = false;
    dmc.dmaRequest = false;
    return true;
}

#ifdef CIPHERNES_DEBUG_ABORT_TABLES
uint8_t APU2A03::debugDmcImplicitState() const {
    return uint8_t((dmc.implicitAbortPending ? 1 : 0)
        | (dmc.implicitAbortQueued ? 2 : 0)
        | (dmc.implicitAbortActive ? 4 : 0)
        | (dmc.enabled ? 8 : 0)
        | (dmc.pendingEnabled ? 16 : 0));
}

uint16_t APU2A03::debugDmcTimer() const {
    return dmc.timerCounter;
}

uint8_t APU2A03::debugDmcStatusDelay() const {
    return dmc.statusDelay;
}
#endif

uint16_t APU2A03::dmcDmaAddress() const {
    return dmc.currentAddress;
}

void APU2A03::completeDmcDma(uint8_t data) {
    dmc.completeSampleFetch(data);
}

void APU2A03::clockFrameIrqPulse() {
    if (frameIrqPulseRemaining > 0) {
        if (frameIrqPulseRemaining <= 2) {
            frameIrqFlag = !frameIrqInhibit;
        } else {
            frameIrqFlag = true;
        }
        if (frameIrqPulseRemaining <= 3) {
            frameIrqLine = !frameIrqInhibit;
        }
        frameIrqPulseRemaining--;
    }
    irq = frameIrqLine || dmc.irqFlag;
}

void APU2A03::clockDmcOutput() {
    dmc.clockTimer(*this);
    if (dmc.restartFetchDelay > 0) dmc.restartFetchDelay--;
    if (dmc.bytesRemaining > 0) dmc.requestSampleFetch();
    if (dmc.implicitAbortActive) dmc.dmaRequest = true;
}

void APU2A03::clockDmcLoadDma(bool putCycle) {
    if (putCycle && dmc.loadDmaCountdown > 0) {
        dmc.loadDmaCountdown--;
        if (dmc.loadDmaCountdown == 0 && !dmc.dmaRequest) {
            // The verified timing model starts load DMA at the instant the two-PUT countdown
            // expires. Waiting for the next pre-gate APU step shifts all
            // $4015-triggered DMA side effects by one CPU cycle.
            dmc.dmaRequest = true;
            dmc.boundaryFetchRequest = false;
            dmc.boundaryFetchBlocked = false;
        }
    }
    if (dmc.statusDelay > 0 && --dmc.statusDelay == 0) {
        dmc.enabled = dmc.pendingEnabled;
        if (!dmc.enabled) {
            dmc.bytesRemaining = 0;
            dmc.dmaRequest = false;
            dmc.boundaryFetchRequest = false;
            dmc.boundaryFetchBlocked = false;
        }
    }
}

void APU2A03::cpuWrite(uint16_t addr, uint8_t data) {
    switch (addr) {
        // Pulse 1
        case 0x4000: {
            const bool value = (data & 0x20) != 0;
            pulse1.reg0 = data;
            if (cpuCycle & 1ULL) {
                pulse1.lengthHaltPending = true;
                pulse1.lengthHaltPendingValue = value;
            } else {
                pulse1.lengthHaltEffective = value;
                pulse1.lengthHaltPending = false;
            }
        } break;
        case 0x4001: pulse1.reg1 = data; pulse1.sweepReload = true; break;
        case 0x4002: pulse1.reg2 = data; pulse1.timer = (pulse1.timer & 0xFF00) | data; break;
        case 0x4003:
            pulse1.reg3 = data;
            pulse1.timer = (pulse1.timer & 0x00FF) | (uint16_t(data & 0x07) << 8);
            pulse1.dutyStep = 0;
            pulse1.envelopeStart = true;
            if (pulse1.enabled) {
                const bool collision = halfFrameWillClockOnNextCpuCycle();
                if (!(collision && pulse1.lengthCounter > 0)) {
                    pulse1.lengthCounter = lengthTable[(data >> 3) & 0x1F];
                    if (collision) pulse1.suppressLengthClockOnce = true;
                }
            }
            break;

        // Pulse 2
        case 0x4004: {
            const bool value = (data & 0x20) != 0;
            pulse2.reg0 = data;
            if (cpuCycle & 1ULL) {
                pulse2.lengthHaltPending = true;
                pulse2.lengthHaltPendingValue = value;
            } else {
                pulse2.lengthHaltEffective = value;
                pulse2.lengthHaltPending = false;
            }
        } break;
        case 0x4005: pulse2.reg1 = data; pulse2.sweepReload = true; break;
        case 0x4006: pulse2.reg2 = data; pulse2.timer = (pulse2.timer & 0xFF00) | data; break;
        case 0x4007:
            pulse2.reg3 = data;
            pulse2.timer = (pulse2.timer & 0x00FF) | (uint16_t(data & 0x07) << 8);
            pulse2.dutyStep = 0;
            pulse2.envelopeStart = true;
            if (pulse2.enabled) {
                const bool collision = halfFrameWillClockOnNextCpuCycle();
                if (!(collision && pulse2.lengthCounter > 0)) {
                    pulse2.lengthCounter = lengthTable[(data >> 3) & 0x1F];
                    if (collision) pulse2.suppressLengthClockOnce = true;
                }
            }
            break;

        // Triangle
        case 0x4008: {
            const bool value = (data & 0x80) != 0;
            tri.reg0 = data;
            if (cpuCycle & 1ULL) {
                tri.lengthControlPending = true;
                tri.lengthControlPendingValue = value;
            } else {
                tri.lengthControlEffective = value;
                tri.lengthControlPending = false;
            }
        } break;
        case 0x400A: tri.reg2 = data; tri.timer = (tri.timer & 0xFF00) | data; break;
        case 0x400B:
            tri.reg3 = data;
            tri.timer = (tri.timer & 0x00FF) | (uint16_t(data & 0x07) << 8);
            tri.seqStep = 0;
            tri.linearReload = true;
            if (tri.enabled) {
                const bool collision = halfFrameWillClockOnNextCpuCycle();
                if (!(collision && tri.lengthCounter > 0)) {
                    tri.lengthCounter = lengthTable[(data >> 3) & 0x1F];
                    if (collision) tri.suppressLengthClockOnce = true;
                }
            }
            break;

        // Noise
        case 0x400C: {
            const bool value = (data & 0x20) != 0;
            noise.reg0 = data;
            if (cpuCycle & 1ULL) {
                noise.lengthHaltPending = true;
                noise.lengthHaltPendingValue = value;
            } else {
                noise.lengthHaltEffective = value;
                noise.lengthHaltPending = false;
            }
        } break;
        case 0x400E: noise.reg2 = data; noise.updatePeriod(); break;
        case 0x400F:
            noise.reg3 = data;
            noise.envelopeStart = true;
            if (noise.enabled) {
                const bool collision = halfFrameWillClockOnNextCpuCycle();
                if (!(collision && noise.lengthCounter > 0)) {
                    noise.lengthCounter = lengthTable[(data >> 3) & 0x1F];
                    if (collision) noise.suppressLengthClockOnce = true;
                }
            }
            break;

        // DMC
        case 0x4010: dmc.reg0 = data; dmc.updateRate(); if (!dmc.irqEnable()) dmc.irqFlag = false; break;
        case 0x4011: dmc.reg1 = data; dmc.outputLevel = data & 0x7F; break;
        case 0x4012: dmc.reg2 = data; break;
        case 0x4013: dmc.reg3 = data; break;

        // Status
        case 0x4015: {
            // Enable bits
            pulse1.enabled = (data & 0x01) != 0;
            pulse2.enabled = (data & 0x02) != 0;
            tri.enabled    = (data & 0x04) != 0;
            noise.enabled  = (data & 0x08) != 0;
            const bool dmcWasPendingEnabled = dmc.pendingEnabled;
            dmc.pendingEnabled = (data & 0x10) != 0;
            dmc.statusDelay = lastCpuBusGetCycle ? 4 : 3;
            if (!dmc.pendingEnabled) {
                dmc.statusDelay++;
            }
            if (!dmc.pendingEnabled
                && ((!lastCpuBusGetCycle && dmc.timerCounter + 1 == dmc.timerPeriod)
                    || (lastCpuBusGetCycle && dmc.timerCounter == 0))) {
                // The verified timing model extends the delayed disable by two CPU cycles when
                // the write lands on the DMC fire boundary.
                dmc.statusDelay = uint8_t(dmc.statusDelay + 2);
            }
            if (!dmc.pendingEnabled
                && ((lastCpuBusGetCycle && dmc.timerCounter == 2)
                    || (!lastCpuBusGetCycle && dmc.timerCounter == 1))) {
                dmc.statusDelay--;
            }
#ifdef CIPHERNES_DEBUG_ABORT_TABLES
            if (dmc.timerPeriod == 54 || dmc.timerPeriod == 72) {
                std::fprintf(stderr,
                             "DMC_%s timer=%u period=%u bits=%u phase=%d transfer=%d remain=%u silent=%d empty=%d\n",
                             (data & 0x10) ? "ENABLE" : "DISABLE",
                             dmc.timerCounter, dmc.timerPeriod, dmc.bitsRemaining,
                             lastCpuBusGetCycle ? 1 : 0, dmc.dmaRequest ? 1 : 0,
                             dmc.bytesRemaining, dmc.silence ? 1 : 0,
                             dmc.sampleBufferEmpty ? 1 : 0);
            }
#endif

            if (!pulse1.enabled) pulse1.lengthCounter = 0;
            if (!pulse2.enabled) pulse2.lengthCounter = 0;
            if (!tri.enabled)    tri.lengthCounter = 0;
            if (!noise.enabled)  noise.lengthCounter = 0;

            if (!dmc.pendingEnabled) {
                dmc.loadDmaCountdown = 0;
                dmc.restartFetchDelay = 0;
            } else {
                // A disable write takes effect on the delayed DMC enable path,
                // but a following enable write still starts a fresh sample.
                // Without remembering the immediately preceding $4015 disable
                // intent, back-to-back $00/$10 writes can leave the old
                // bytesRemaining value alive for a few CPU phases and fail to
                // reload the programmed sample length. Repeated $10 writes
                // while continuously enabled remain a no-op, as on hardware.
                if (dmc.bytesRemaining == 0 || !dmcWasPendingEnabled) {
                    if (!dmc.silence && dmc.timerCounter >= 69) {
                        dmc.restartFetchDelay = 14;
                    } else {
                        dmc.restartFetchDelay = 0;
                    }
                    dmc.startSample();
                    // Starting the DMC schedules a load DMA whenever its
                    // one-byte sample buffer is empty.  The output-unit silence
                    // flag only describes the shift register and must not
                    // suppress this initial fetch.
                    if (dmc.sampleBufferEmpty) {
                        dmc.loadDmaCountdown = 2;
                    }
                }
                if ((!lastCpuBusGetCycle && dmc.timerCounter == 7)
                    || (lastCpuBusGetCycle && dmc.timerCounter == 8)) {
                    // The verified timing model promotes this at the next output-bit boundary,
                    // creating a one-cycle DMA window that is not delayed by
                    // CPU write cycles.
                    dmc.implicitAbortPending = true;
                }
            }

            // Writing $4015 clears DMC IRQ flag
            dmc.irqFlag = false;
        } break;

        // Frame counter
        case 0x4017: {
            frameCounterReg = data;
            fiveStepMode = (data & 0x80) != 0;
            frameIrqInhibit = (data & 0x40) != 0;
            if (frameIrqInhibit) {
                frameIrqFlag = false;
                frameIrqLine = false;
                frameIrqClearPending = false;
                frameIrqInhibitTransient = 0;
                frameIrqPulseRemaining = 0;
            }

            // Reset frame sequencer after 3 or 4 CPU cycles depending on CPU cycle parity.
            // clock() applies the reset when this countdown reaches zero, so store
            // one less than the externally visible delay.
            frameSeqResetPending = true;
            frameSeqResetDelay = lastCpuBusGetCycle ? 3 : 2;
        } break;

        default:
            break;
    }
}

// ------------------------------
// Save-state support
// ------------------------------
void APU2A03::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    write_bool(out, audioEnabled);
    write_bool(out, turbo);

    write_pod(out, sampleRate);
    write_pod(out, timePerSample);
    write_pod(out, timePerCpuTick);
    write_pod(out, sampleTimeAccumulator);
    write_u64(out, cpuCycle);

    write_pod(out, hp90_a);
    write_pod(out, hp440_a);
    write_pod(out, lp14k_a);
    write_pod(out, hp90_y);
    write_pod(out, hp90_x1);
    write_pod(out, hp440_y);
    write_pod(out, hp440_x1);
    write_pod(out, lp_y);

    write_u8(out, frameCounterReg);
    write_bool(out, frameIrqInhibit);
    write_bool(out, frameIrqFlag);
    write_bool(out, fiveStepMode);
    write_u32(out, frameSeqCounter);
    write_bool(out, frameSeqResetPending);
    write_u8(out, frameSeqResetDelay);

    // Pulse 1/2
    auto savePulse = [&](const Pulse& pu){
        write_u8(out, pu.reg0); write_u8(out, pu.reg1); write_u8(out, pu.reg2); write_u8(out, pu.reg3);
        write_u16(out, pu.timer); write_u16(out, pu.timerCounter); write_u8(out, pu.dutyStep);
        write_bool(out, pu.envelopeStart); write_u8(out, pu.envelopeDivider); write_u8(out, pu.envelopeDecay);
        write_u8(out, pu.lengthCounter);
        write_bool(out, pu.sweepReload); write_u8(out, pu.sweepDivider);
        write_bool(out, pu.enabled);
        write_u8(out, pu.channelIndex);
    };
    savePulse(pulse1);
    savePulse(pulse2);

    // Triangle
    write_u8(out, tri.reg0); write_u8(out, tri.reg1); write_u8(out, tri.reg2); write_u8(out, tri.reg3);
    write_u16(out, tri.timer); write_u16(out, tri.timerCounter); write_u8(out, tri.seqStep);
    write_u8(out, tri.lengthCounter);
    write_u8(out, tri.linearCounter); write_bool(out, tri.linearReload);
    write_bool(out, tri.enabled);

    // Noise
    write_u8(out, noise.reg0); write_u8(out, noise.reg1); write_u8(out, noise.reg2); write_u8(out, noise.reg3);
    write_u16(out, noise.timerPeriod); write_u16(out, noise.timerCounter); write_u16(out, noise.shiftReg);
    write_bool(out, noise.envelopeStart); write_u8(out, noise.envelopeDivider); write_u8(out, noise.envelopeDecay);
    write_u8(out, noise.lengthCounter); write_bool(out, noise.enabled);

    // DMC
    write_u8(out, dmc.reg0); write_u8(out, dmc.reg1); write_u8(out, dmc.reg2); write_u8(out, dmc.reg3);
    write_bool(out, dmc.enabled);
    write_u8(out, dmc.outputLevel);
    write_u16(out, dmc.sampleAddress);
    write_u16(out, dmc.sampleLength);
    write_u16(out, dmc.currentAddress);
    write_u16(out, dmc.bytesRemaining);
    write_u8(out, dmc.sampleBuffer);
    write_bool(out, dmc.sampleBufferEmpty);
    write_u16(out, dmc.timerPeriod);
    write_u16(out, dmc.timerCounter);
    write_u8(out, dmc.shiftReg);
    write_u8(out, dmc.bitsRemaining);
    write_bool(out, dmc.silence);
    write_bool(out, dmc.irqFlag);

    // Don't serialize sample queue; it's fine to clear on load.
}

bool APU2A03::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    if (!read_bool(p,end,audioEnabled)) return false;
    if (!read_bool(p,end,turbo)) return false;

    if (!read_pod(p,end,sampleRate)) return false;
    if (!read_pod(p,end,timePerSample)) return false;
    if (!read_pod(p,end,timePerCpuTick)) return false;
    if (!read_pod(p,end,sampleTimeAccumulator)) return false;
    if (!read_u64(p,end,cpuCycle)) return false;

    if (!read_pod(p,end,hp90_a)) return false;
    if (!read_pod(p,end,hp440_a)) return false;
    if (!read_pod(p,end,lp14k_a)) return false;
    if (!read_pod(p,end,hp90_y)) return false;
    if (!read_pod(p,end,hp90_x1)) return false;
    if (!read_pod(p,end,hp440_y)) return false;
    if (!read_pod(p,end,hp440_x1)) return false;
    if (!read_pod(p,end,lp_y)) return false;

    if (!read_u8(p,end,frameCounterReg)) return false;
    if (!read_bool(p,end,frameIrqInhibit)) return false;
    if (!read_bool(p,end,frameIrqFlag)) return false;
    frameIrqLine = frameIrqFlag && !frameIrqInhibit;
    frameIrqPulseRemaining = 0;
    if (!read_bool(p,end,fiveStepMode)) return false;
    if (!read_u32(p,end,frameSeqCounter)) return false;
    if (!read_bool(p,end,frameSeqResetPending)) return false;
    if (!read_u8(p,end,frameSeqResetDelay)) return false;

    auto loadPulse = [&](Pulse& pu){
        if (!read_u8(p,end,pu.reg0)) return false;
        if (!read_u8(p,end,pu.reg1)) return false;
        if (!read_u8(p,end,pu.reg2)) return false;
        if (!read_u8(p,end,pu.reg3)) return false;
        if (!read_u16(p,end,pu.timer)) return false;
        if (!read_u16(p,end,pu.timerCounter)) return false;
        if (!read_u8(p,end,pu.dutyStep)) return false;
        if (!read_bool(p,end,pu.envelopeStart)) return false;
        if (!read_u8(p,end,pu.envelopeDivider)) return false;
        if (!read_u8(p,end,pu.envelopeDecay)) return false;
        if (!read_u8(p,end,pu.lengthCounter)) return false;
        if (!read_bool(p,end,pu.sweepReload)) return false;
        if (!read_u8(p,end,pu.sweepDivider)) return false;
        if (!read_bool(p,end,pu.enabled)) return false;
        if (!read_u8(p,end,pu.channelIndex)) return false;
        pu.lengthHaltEffective = pu.lengthHalt();
        pu.lengthHaltPending = false;
        pu.lengthHaltPendingValue = pu.lengthHaltEffective;
        pu.suppressLengthClockOnce = false;
        return true;
    };
    if (!loadPulse(pulse1)) return false;
    if (!loadPulse(pulse2)) return false;

    if (!read_u8(p,end,tri.reg0)) return false;
    if (!read_u8(p,end,tri.reg1)) return false;
    if (!read_u8(p,end,tri.reg2)) return false;
    if (!read_u8(p,end,tri.reg3)) return false;
    if (!read_u16(p,end,tri.timer)) return false;
    if (!read_u16(p,end,tri.timerCounter)) return false;
    if (!read_u8(p,end,tri.seqStep)) return false;
    if (!read_u8(p,end,tri.lengthCounter)) return false;
    if (!read_u8(p,end,tri.linearCounter)) return false;
    if (!read_bool(p,end,tri.linearReload)) return false;
    if (!read_bool(p,end,tri.enabled)) return false;
    tri.lengthControlEffective = tri.controlFlag();
    tri.lengthControlPending = false;
    tri.lengthControlPendingValue = tri.lengthControlEffective;
    tri.suppressLengthClockOnce = false;

    if (!read_u8(p,end,noise.reg0)) return false;
    if (!read_u8(p,end,noise.reg1)) return false;
    if (!read_u8(p,end,noise.reg2)) return false;
    if (!read_u8(p,end,noise.reg3)) return false;
    if (!read_u16(p,end,noise.timerPeriod)) return false;
    if (!read_u16(p,end,noise.timerCounter)) return false;
    if (!read_u16(p,end,noise.shiftReg)) return false;
    if (!read_bool(p,end,noise.envelopeStart)) return false;
    if (!read_u8(p,end,noise.envelopeDivider)) return false;
    if (!read_u8(p,end,noise.envelopeDecay)) return false;
    if (!read_u8(p,end,noise.lengthCounter)) return false;
    if (!read_bool(p,end,noise.enabled)) return false;
    noise.lengthHaltEffective = noise.lengthHalt();
    noise.lengthHaltPending = false;
    noise.lengthHaltPendingValue = noise.lengthHaltEffective;
    noise.suppressLengthClockOnce = false;

    if (!read_u8(p,end,dmc.reg0)) return false;
    if (!read_u8(p,end,dmc.reg1)) return false;
    if (!read_u8(p,end,dmc.reg2)) return false;
    if (!read_u8(p,end,dmc.reg3)) return false;
    if (!read_bool(p,end,dmc.enabled)) return false;
    dmc.pendingEnabled = dmc.enabled;
    dmc.statusDelay = 0;
    if (!read_u8(p,end,dmc.outputLevel)) return false;
    if (!read_u16(p,end,dmc.sampleAddress)) return false;
    if (!read_u16(p,end,dmc.sampleLength)) return false;
    if (!read_u16(p,end,dmc.currentAddress)) return false;
    if (!read_u16(p,end,dmc.bytesRemaining)) return false;
    if (!read_u8(p,end,dmc.sampleBuffer)) return false;
    if (!read_bool(p,end,dmc.sampleBufferEmpty)) return false;
    if (!read_u16(p,end,dmc.timerPeriod)) return false;
    if (!read_u16(p,end,dmc.timerCounter)) return false;
    if (!read_u8(p,end,dmc.shiftReg)) return false;
    if (!read_u8(p,end,dmc.bitsRemaining)) return false;
    if (!read_bool(p,end,dmc.silence)) return false;
    if (!read_bool(p,end,dmc.irqFlag)) return false;
    dmc.dmaRequest = false;
    dmc.boundaryFetchRequest = false;
    dmc.boundaryFetchBlocked = false;
    dmc.loadDmaCountdown = 0;
    dmc.restartFetchDelay = 0;
    dmc.implicitAbortPending = false;
    dmc.implicitAbortQueued = false;
    dmc.implicitAbortActive = false;

    // Recompute coeffs in case sample rate changed
    updateFilterCoeffs();

    clearSampleQueue();
    return true;
}
