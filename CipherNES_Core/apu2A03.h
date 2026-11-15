#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>


struct Bus;

// NES APU (2A03) implementation (NTSC)
//
// This is a register-accurate implementation of the 2A03 APU suitable for
// typical commercial games. It models:
//  - 2x Pulse channels (envelope, sweep, length counter)
//  - Triangle channel (linear counter, length counter)
//  - Noise channel (envelope, length counter, LFSR)
//  - DMC channel (sample fetch, IRQ)
//  - Frame counter (4-step and 5-step modes, frame IRQ)
//
// The APU is clocked at the CPU frequency (one tick per CPU cycle).
// Audio samples are produced by internal resampling to the configured
// output sample rate and stored in an internal buffer (pull with takeSamples()).
struct APU2A03 {
    void connectBus(Bus* b) { bus = b; }

    void reset();
    void softReset();
    void clock();

    // Configure output sample rate (e.g. 44100).
    void setSampleRate(double sampleRateHz);

    // Fetch and clear all queued audio samples generated since the last call.

    // Same as takeSamples(), but returns mono signed 16-bit PCM.

    // Allocation-free copies used by the shared-core exports. Samples that do not fit
    // in the caller's output buffer remain queued for the next take.
    int copyAndClearSamplesI16(int16_t* outSamples, int maxSamples);

    // Enable/disable audio generation (useful during rewind/fast-forward).
    void setAudioEnabled(bool enabled);
    bool getAudioEnabled() const { return audioEnabled; }

    // Output channel mask for audio debugging/isolation. Bits:
    // 0 pulse1, 1 pulse2, 2 triangle, 3 noise, 4 DMC.
    // This mutes channel output only; emulation state/timing/register writes still run.
    void setChannelMask(uint8_t mask) { channelMask = mask & 0x1Fu; }
    uint8_t getChannelMask() const { return channelMask & 0x1Fu; }

    // Turbo hint: when true, the APU may skip buffering samples to run faster.

    // Save-state support
    void saveState(std::vector<uint8_t>& out) const;
    bool loadState(const uint8_t*& p, const uint8_t* end);

    // CPU register interface ($4000-$4017)
    uint8_t cpuRead(uint16_t addr, bool readOnly = false);
    void    cpuWrite(uint16_t addr, uint8_t data);
    bool    hasDmcDmaRequest() const;
    bool    isDmcBoundaryFetchRequest() const;
    void    rejectDmcBoundaryFetchForCooldown();
    bool    isDmcDmaBusActive() const;
    bool    finishDmcImplicitAbortCycle(bool dmcDmaCycleRan);
    bool    cancelDmcImplicitAbortOnWrite(bool dmaHalted);
    uint16_t dmcDmaAddress() const;
    void    completeDmcDma(uint8_t data);
    void    clockFrameIrqPulse();
    void    clockDmcOutput();
    void    clockDmcLoadDma(bool putCycle);
#ifdef CIPHERNES_DEBUG_ABORT_TABLES
    uint8_t debugDmcImplicitState() const;
    uint16_t debugDmcTimer() const;
    uint8_t debugDmcStatusDelay() const;
#endif


    // IRQ line (level). The bus should assert CPU IRQ while this is true.
    bool irq = false;

private:
    Bus* bus = nullptr;

    bool audioEnabled = true;
    bool turbo = false;
    uint8_t channelMask = 0x1F;

    // ------------------------------
    // Timing
    // ------------------------------
    static constexpr double CPU_CLOCK_HZ = 1789773.0; // NTSC CPU clock

    double sampleRate = 44100.0;
    double timePerSample = 1.0 / 44100.0;
    double timePerCpuTick = 1.0 / CPU_CLOCK_HZ;
    double sampleTimeAccumulator = 0.0;

    uint64_t cpuCycle = 0;

    // ------------------------------
    // Output filtering (models the NES' analog chain)
    // ------------------------------
    // Two high-pass filters (~90 Hz and ~440 Hz) and one low-pass (~14 kHz).
    // This removes DC offset and reduces aliasing/crackle, especially with heavy DMC use.
    float hp90_a  = 0.0f;
    float hp440_a = 0.0f;
    float lp14k_a = 0.0f;

    float hp90_y = 0.0f;
    float hp90_x1 = 0.0f;
    float hp440_y = 0.0f;
    float hp440_x1 = 0.0f;
    float lp_y = 0.0f;

    void updateFilterCoeffs();
    float filterSample(float x);

    // Frame counter
    // The APU frame sequencer runs at CPU/2 (ticks every 2 CPU cycles).
    uint8_t frameCounterReg = 0x00; // last written value to $4017
    bool    frameIrqInhibit = false;
    bool    frameIrqFlag    = false;
    bool    frameIrqLine    = false;
    bool    fiveStepMode    = false;
    bool    frameIrqClearPending = false;
    uint8_t frameIrqInhibitTransient = 0;
    uint8_t frameIrqPulseRemaining = 0;
    bool    lastCpuBusGetCycle = false;
    uint32_t frameSeqCounter = 0; // counts APU sequencer ticks (CPU/2 domain)
    bool     frameSeqResetPending = false;
    uint8_t  frameSeqResetDelay = 0;

public:
    void clockCpuBusAccess(bool getCycle);
private:
    void clockFrameSequencer();
    void clockQuarterFrame();
    void clockHalfFrame();
    bool halfFrameWillClockOnNextCpuCycle() const;
    void applyPendingLengthControls();

    // Lookup tables
    // NOTE: These are defined in apu2A03.cpp to avoid ODR/linker issues
    // when building with older C++ standards/toolchains.
    static const uint8_t  lengthTable[32];

    // ------------------------------
    // Pulse
    // ------------------------------
    struct Pulse {
        // Registers
        uint8_t reg0 = 0; // $4000/$4004
        uint8_t reg1 = 0; // $4001/$4005
        uint8_t reg2 = 0; // $4002/$4006
        uint8_t reg3 = 0; // $4003/$4007

        // Sequencer
        uint16_t timer = 0;
        uint16_t timerCounter = 0;
        uint8_t  dutyStep = 0;

        // Envelope
        bool envelopeStart = false;
        uint8_t envelopeDivider = 0;
        uint8_t envelopeDecay = 0;

        // Length
        uint8_t lengthCounter = 0;
        bool lengthHaltEffective = false;
        bool lengthHaltPending = false;
        bool lengthHaltPendingValue = false;
        bool suppressLengthClockOnce = false;

        // Sweep
        bool sweepReload = false;
        uint8_t sweepDivider = 0;

        // Enable
        bool enabled = false;

        // Derived controls
        bool lengthHalt() const { return (reg0 & 0x20) != 0; }
        bool counterHalted() const { return lengthHaltEffective; }
        bool constantVolume() const { return (reg0 & 0x10) != 0; }
        uint8_t volumeOrEnvelope() const { return reg0 & 0x0F; }
        uint8_t duty() const { return (reg0 >> 6) & 0x03; }

        bool sweepEnabled() const { return (reg1 & 0x80) != 0; }
        uint8_t sweepPeriod() const { return (reg1 >> 4) & 0x07; }
        bool sweepNegate() const { return (reg1 & 0x08) != 0; }
        uint8_t sweepShift() const { return reg1 & 0x07; }

        // Channel index (0 for pulse1, 1 for pulse2) affects negate behavior
        uint8_t channelIndex = 0;

        void reset(uint8_t idx);

        void clockTimer();
        void clockEnvelope();
        void clockLength();
        void clockSweep();

        uint16_t targetPeriod() const;
        bool     sweepMute() const;
        uint8_t  output() const;
    } pulse1, pulse2;

    static const uint8_t dutyTable[4][8];

    // ------------------------------
    // Triangle
    // ------------------------------
    struct Triangle {
        uint8_t reg0 = 0; // $4008
        uint8_t reg1 = 0; // unused
        uint8_t reg2 = 0; // $400A
        uint8_t reg3 = 0; // $400B

        uint16_t timer = 0;
        uint16_t timerCounter = 0;
        uint8_t  seqStep = 0;

        uint8_t lengthCounter = 0;
        bool lengthControlEffective = false;
        bool lengthControlPending = false;
        bool lengthControlPendingValue = false;
        bool suppressLengthClockOnce = false;

        // Linear counter
        uint8_t linearCounter = 0;
        bool    linearReload = false;

        bool enabled = false;

        bool controlFlag() const { return (reg0 & 0x80) != 0; }
        uint8_t linearReloadValue() const { return reg0 & 0x7F; }

        void reset();

        void clockTimer();
        void clockLength();
        void clockLinear();
        uint8_t output() const;
    } tri;

    static const uint8_t triTable[32];

    // ------------------------------
    // Noise
    // ------------------------------
    struct Noise {
        uint8_t reg0 = 0; // $400C
        uint8_t reg1 = 0; // unused
        uint8_t reg2 = 0; // $400E
        uint8_t reg3 = 0; // $400F

        uint16_t timerPeriod = 0;
        uint16_t timerCounter = 0;
        uint16_t shiftReg = 1; // 15-bit LFSR (bit0 output)

        // Envelope
        bool envelopeStart = false;
        uint8_t envelopeDivider = 0;
        uint8_t envelopeDecay = 0;

        uint8_t lengthCounter = 0;
        bool lengthHaltEffective = false;
        bool lengthHaltPending = false;
        bool lengthHaltPendingValue = false;
        bool suppressLengthClockOnce = false;
        bool enabled = false;

        bool lengthHalt() const { return (reg0 & 0x20) != 0; }
        bool counterHalted() const { return lengthHaltEffective; }
        bool constantVolume() const { return (reg0 & 0x10) != 0; }
        uint8_t volumeOrEnvelope() const { return reg0 & 0x0F; }

        bool mode() const { return (reg2 & 0x80) != 0; }
        uint8_t periodIndex() const { return reg2 & 0x0F; }

        void reset();

        void clockTimer();
        void clockEnvelope();
        void clockLength();
        uint8_t output() const;
        void updatePeriod();
    } noise;

    static const uint16_t noisePeriodTable[16];

    // ------------------------------
    // DMC
    // ------------------------------
    struct DMC {
        uint8_t reg0 = 0; // $4010
        uint8_t reg1 = 0; // $4011
        uint8_t reg2 = 0; // $4012
        uint8_t reg3 = 0; // $4013

        bool enabled = false;
        bool pendingEnabled = false;
        uint8_t statusDelay = 0;

        // IRQ
        bool irqEnable() const { return (reg0 & 0x80) != 0; }
        bool loop() const { return (reg0 & 0x40) != 0; }
        uint8_t rateIndex() const { return reg0 & 0x0F; }

        uint8_t outputLevel = 0; // 7-bit DAC (0-127)

        // Sample state
        uint16_t sampleAddress = 0;
        uint16_t sampleLength = 0;
        uint16_t currentAddress = 0;
        uint16_t bytesRemaining = 0;

        uint8_t sampleBuffer = 0;
        bool    sampleBufferEmpty = true;

        uint16_t timerPeriod = 0;
        uint16_t timerCounter = 0;

        // Output shift register
        uint8_t shiftReg = 0;
        uint8_t bitsRemaining = 8;
        bool    silence = true;

        bool irqFlag = false;
        bool dmaRequest = false;
        bool boundaryFetchRequest = false;
        bool boundaryFetchBlocked = false;
        uint8_t loadDmaCountdown = 0;
        uint8_t restartFetchDelay = 0;
        bool implicitAbortPending = false;
        bool implicitAbortQueued = false;
        bool implicitAbortActive = false;

        void reset();
        void updateRate();

        void startSample();
        void clockTimer(APU2A03& apu);
        void requestSampleFetch();
        void completeSampleFetch(uint8_t data);
        uint8_t output() const { return outputLevel; }
    } dmc;

    static const uint16_t dmcRateTable[16];

    // ------------------------------
    // Mixer
    // ------------------------------
    float mixOutput() const;

    // Internal sample queue. sampleReadOffset lets small frontend pulls consume
    // the queue without losing leftovers or doing an erase/memmove every call.
    std::vector<float> samples;
    std::size_t sampleReadOffset = 0;

    void clearSampleQueue();
    std::size_t queuedSampleCount() const;
    void compactSampleQueueIfNeeded();
    void enforceSampleQueueLimit();
};
