#pragma once

#include <cstdint>
#include <vector>
#include <deque>
#include "Mapper.h"

// Konami VRC7 (iNES mapper 85).
// Implements the cartridge banking/IRQ hardware and the six-channel VRC7
// two-operator FM unit with the dumped VRC7 instrument ROM.
class Mapper085 final : public Mapper {
public:
    Mapper085(uint16_t prgBanks16k, uint16_t chrBanks8k);

    uint32_t capabilities() const override {
        return MAPPER_CAP_IRQ | MAPPER_CAP_MIRRORING | MAPPER_CAP_EXPANSION_AUDIO;
    }

    void reset() override;
    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    uint8_t mirroringOverride() const override;

    bool usesCpuClock() const override { return true; }
    void cpuClock() override;
    bool irqState() const override { return irqActive; }
    void irqClear() override { irqActive = false; }

    bool usesExpansionAudio() const override { return true; }
    void setAudioSampleRate(double sampleRateHz) override;
    float audioSample() override;
    uint32_t saveStateVersion() const override { return 2u; }

    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;

private:
    enum EnvelopeState : uint8_t { ENV_OFF=0, ENV_ATTACK, ENV_DECAY, ENV_SUSTAIN, ENV_RELEASE };

    struct Operator {
        double phase = 0.0;
        double envelope = 0.0; // linear amplitude, 0=silent, 1=full
        double feedback1 = 0.0;
        double feedback2 = 0.0;
        EnvelopeState state = ENV_OFF;
        bool keyOn = false;
    };

    struct Channel {
        uint16_t frequency = 0;
        uint8_t octave = 0;
        uint8_t instrument = 0;
        uint8_t volume = 15;
        bool sustain = false;
        bool trigger = false;
        Operator mod;
        Operator car;
    } channels[6];

    uint16_t mapperNumber = 85;
    uint16_t prg8[4] = {};
    uint16_t chr1[8] = {};
    uint8_t control = 0;

    uint8_t irqLatch = 0;
    uint8_t irqCounter = 0;
    int16_t irqPrescaler = 0;
    bool irqEnabled = false;
    bool irqEnableAfterAck = false;
    bool irqCycleMode = false;
    bool irqActive = false;

    uint8_t audioAddress = 0;
    uint8_t audioRegs[0x40] = {};
    uint8_t customPatch[8] = {};
    bool audioMuted = false;
    uint8_t fmDivider = 0;
    double tremoloPhase = 0.0;
    double vibratoPhase = 0.0;
    float audioOut = 0.0f;
    double outputSampleRate = 44100.0;
    double outputAccumulator = 0.0;
    std::deque<float> audioQueue;

    uint16_t prgCount8() const;
    uint16_t chrCount1() const;
    void writeAudioRegister(uint8_t reg, uint8_t data);
    void resetAudio();
    void stepFm();
    void keyOn(Channel& ch);
    void keyOff(Channel& ch);

    static double multiplier(uint8_t value);
    static double feedbackScale(uint8_t value);
    static double dbToLinear(double db);
    static double waveform(double phase, bool halfWave);
    static void stepEnvelope(Operator& op, uint8_t attack, uint8_t decay,
                             uint8_t sustainLevel, uint8_t release,
                             bool patchSustain, bool channelSustain,
                             bool isModulator, double keyRate);
    const uint8_t* patchFor(uint8_t instrument) const;
};
