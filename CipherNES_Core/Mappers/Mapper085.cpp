#include "Mapper085.h"
#include "../state_io.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
static const double PI_D = 3.1415926535897932384626433832795;
static const double VRC7_RATE = 49716.0;
static const double NTSC_CPU_RATE = 1789773.0;
static const size_t MAX_AUDIO_QUEUE = 8192u;

// Dumped VRC7 instrument ROM (instrument 0 is the writable custom patch).
static const uint8_t VRC7_PATCHES[16][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x03,0x21,0x05,0x06,0xE8,0x81,0x42,0x27},
    {0x13,0x41,0x14,0x0D,0xD8,0xF6,0x23,0x12},
    {0x11,0x11,0x08,0x08,0xFA,0xB2,0x20,0x12},
    {0x31,0x61,0x0C,0x07,0xA8,0x64,0x61,0x27},
    {0x32,0x21,0x1E,0x06,0xE1,0x76,0x01,0x28},
    {0x02,0x01,0x06,0x00,0xA3,0xE2,0xF4,0xF4},
    {0x21,0x61,0x1D,0x07,0x82,0x81,0x11,0x07},
    {0x23,0x21,0x22,0x17,0xA2,0x72,0x01,0x17},
    {0x35,0x11,0x25,0x00,0x40,0x73,0x72,0x01},
    {0xB5,0x01,0x0F,0x0F,0xA8,0xA5,0x51,0x02},
    {0x17,0xC1,0x24,0x07,0xF8,0xF8,0x22,0x12},
    {0x71,0x23,0x11,0x06,0x65,0x74,0x18,0x16},
    {0x01,0x02,0xD3,0x05,0xC9,0x95,0x03,0x02},
    {0x61,0x63,0x0C,0x00,0x94,0xC0,0x33,0xF6},
    {0x21,0x72,0x0D,0x00,0xC1,0xD5,0x56,0x06}
};

static double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

static void writeDouble(std::vector<uint8_t>& out, double value) {
    uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    ciphernes_state::write_u64(out, bits);
}
static bool readDouble(const uint8_t*& p, const uint8_t* end, double& value) {
    uint64_t bits = 0;
    if (!ciphernes_state::read_u64(p, end, bits)) return false;
    std::memcpy(&value, &bits, sizeof(bits));
    return true;
}
}

Mapper085::Mapper085(uint16_t prgBanks16k, uint16_t chrBanks8k)
    : Mapper(prgBanks16k, chrBanks8k) { reset(); }

uint16_t Mapper085::prgCount8() const { return std::max<uint16_t>(1u, uint16_t(prgBanks * 2u)); }
uint16_t Mapper085::chrCount1() const { return chrBanks ? uint16_t(chrBanks * 8u) : 8u; }

void Mapper085::resetAudio() {
    std::memset(audioRegs, 0, sizeof(audioRegs));
    std::memset(customPatch, 0, sizeof(customPatch));
    audioAddress = 0;
    fmDivider = 0;
    tremoloPhase = vibratoPhase = 0.0;
    audioOut = 0.0f;
    outputAccumulator = 0.0;
    audioQueue.clear();
    for (unsigned i = 0; i < 6; ++i) channels[i] = Channel();
}

void Mapper085::reset() {
    const uint16_t count = prgCount8();
    prg8[0] = 0; prg8[1] = 1 % count; prg8[2] = 2 % count; prg8[3] = uint16_t(count - 1u);
    std::memset(chr1, 0, sizeof(chr1));
    control = 0;
    irqLatch = irqCounter = 0;
    irqPrescaler = 0;
    irqEnabled = irqEnableAfterAck = irqCycleMode = irqActive = false;
    audioMuted = false;
    resetAudio();
}

uint8_t Mapper085::mirroringOverride() const {
    switch (control & 3u) {
        case 0: return 1; // vertical
        case 1: return 0; // horizontal
        case 2: return 3; // one-screen low
        default:return 4; // one-screen high
    }
}

bool Mapper085::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr >= 0x6000u && addr <= 0x7FFFu) {
        if ((control & 0x80u) == 0) return false;
        mappedAddr = PRG_RAM_FLAG | uint32_t(addr & 0x1FFFu);
        return true;
    }
    if (addr < 0x8000u) return false;
    const uint8_t slot = uint8_t((addr - 0x8000u) >> 13);
    mappedAddr = uint32_t(prg8[slot] % prgCount8()) * 0x2000u + uint32_t(addr & 0x1FFFu);
    return true;
}

bool Mapper085::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    mappedAddr = 0;
    if (addr >= 0x6000u && addr <= 0x7FFFu) {
        if (control & 0x80u) mappedAddr = PRG_RAM_FLAG | uint32_t(addr & 0x1FFFu);
        return true;
    }
    if (addr < 0x8000u) return false;

    // VRC7 accepts alternate register addresses using A4 instead of A3,
    // except for the audio address port at $9010.
    if ((addr & 0x10u) && ((addr & 0xF010u) != 0x9010u)) {
        addr = uint16_t((addr | 0x08u) & ~0x10u);
    }

    switch (addr & 0xF038u) {
        case 0x8000: prg8[0] = data & 0x3Fu; break;
        case 0x8008: prg8[1] = data & 0x3Fu; break;
        case 0x9000: prg8[2] = data & 0x3Fu; break;
        case 0x9010:
            if (!audioMuted) audioAddress = data;
            break;
        case 0x9030:
            if (!audioMuted) writeAudioRegister(audioAddress, data);
            break;
        case 0xA000: chr1[0] = data; break;
        case 0xA008: chr1[1] = data; break;
        case 0xB000: chr1[2] = data; break;
        case 0xB008: chr1[3] = data; break;
        case 0xC000: chr1[4] = data; break;
        case 0xC008: chr1[5] = data; break;
        case 0xD000: chr1[6] = data; break;
        case 0xD008: chr1[7] = data; break;
        case 0xE000: {
            const bool newMute = (data & 0x40u) != 0;
            control = data;
            if (newMute && !audioMuted) resetAudio();
            audioMuted = newMute;
            break;
        }
        case 0xE008: irqLatch = data; break;
        case 0xF000:
            irqEnableAfterAck = (data & 1u) != 0;
            irqEnabled = (data & 2u) != 0;
            irqCycleMode = (data & 4u) != 0;
            if (irqEnabled) { irqCounter = irqLatch; irqPrescaler = 341; }
            irqActive = false;
            break;
        case 0xF008:
            irqEnabled = irqEnableAfterAck;
            irqActive = false;
            break;
    }
    return true;
}

bool Mapper085::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    addr &= 0x3FFFu;
    if (addr > 0x1FFFu) return false;
    const uint8_t slot = uint8_t(addr >> 10);
    mappedAddr = uint32_t(chr1[slot] % chrCount1()) * 0x400u + uint32_t(addr & 0x3FFu);
    return true;
}

bool Mapper085::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t) {
    addr &= 0x3FFFu;
    if (addr > 0x1FFFu || chrBanks != 0) return false;
    const uint8_t slot = uint8_t(addr >> 10);
    mappedAddr = uint32_t(chr1[slot] % chrCount1()) * 0x400u + uint32_t(addr & 0x3FFu);
    return true;
}

void Mapper085::setAudioSampleRate(double sampleRateHz) {
    if (!(sampleRateHz >= 8000.0 && sampleRateHz <= 384000.0)) sampleRateHz = 44100.0;
    outputSampleRate = sampleRateHz;
    outputAccumulator = 0.0;
    audioQueue.clear();
}

float Mapper085::audioSample() {
    if (audioQueue.empty()) return audioMuted ? 0.0f : audioOut;
    const float sample = audioQueue.front();
    audioQueue.pop_front();
    return audioMuted ? 0.0f : sample;
}

void Mapper085::cpuClock() {
    if (irqEnabled) {
        irqPrescaler = int16_t(irqPrescaler - 3);
        if (irqCycleMode || irqPrescaler <= 0) {
            if (irqCounter == 0xFFu) { irqCounter = irqLatch; irqActive = true; }
            else ++irqCounter;
            irqPrescaler = int16_t(irqPrescaler + 341);
        }
    }

    if (++fmDivider >= 36u) {
        fmDivider = 0;
        if (!audioMuted) stepFm();
        else audioOut = 0.0f;
    }

    // The mapper is advanced on every CPU cycle, while the frontend pulls
    // audio in blocks. Queue one held FM value per configured output sample
    // so a block does not receive a single repeated end-of-block value.
    outputAccumulator += outputSampleRate;
    while (outputAccumulator >= NTSC_CPU_RATE) {
        outputAccumulator -= NTSC_CPU_RATE;
        audioQueue.push_back(audioMuted ? 0.0f : audioOut);
        if (audioQueue.size() > MAX_AUDIO_QUEUE) audioQueue.pop_front();
    }
}

const uint8_t* Mapper085::patchFor(uint8_t instrument) const {
    return instrument == 0 ? customPatch : VRC7_PATCHES[instrument & 0x0Fu];
}

double Mapper085::multiplier(uint8_t value) {
    static const double table[16] = {0.5,1,2,3,4,5,6,7,8,9,10,10,12,12,15,15};
    return table[value & 15u];
}

double Mapper085::feedbackScale(uint8_t value) {
    static const double table[8] = {0.0, PI_D/16.0, PI_D/8.0, PI_D/4.0, PI_D/2.0, PI_D, 2.0*PI_D, 4.0*PI_D};
    return table[value & 7u];
}

double Mapper085::dbToLinear(double db) { return std::pow(10.0, db / 20.0); }

double Mapper085::waveform(double phase, bool halfWave) {
    phase -= std::floor(phase);
    double v = std::sin(phase * 2.0 * PI_D);
    return halfWave && v < 0.0 ? 0.0 : v;
}

void Mapper085::keyOn(Channel& ch) {
    ch.mod.keyOn = ch.car.keyOn = true;
    ch.mod.state = ch.car.state = ENV_ATTACK;
    ch.mod.envelope = ch.car.envelope = 0.0;
    ch.mod.phase = ch.car.phase = 0.0;
    ch.mod.feedback1 = ch.mod.feedback2 = 0.0;
}
void Mapper085::keyOff(Channel& ch) {
    ch.mod.keyOn = ch.car.keyOn = false;
    ch.car.state = ENV_RELEASE;
    // The modulator sustains after key-off when its patch sustain bit is set;
    // stepEnvelope applies that hardware-specific rule.
    if ((patchFor(ch.instrument)[0] & 0x20u) == 0) ch.mod.state = ENV_RELEASE;
}

void Mapper085::writeAudioRegister(uint8_t reg, uint8_t data) {
    reg &= 0x3Fu;
    audioRegs[reg] = data;
    if (reg <= 7u) {
        customPatch[reg] = data;
        return;
    }
    if (reg >= 0x10u && reg <= 0x15u) {
        Channel& ch = channels[reg - 0x10u];
        ch.frequency = uint16_t((ch.frequency & 0x100u) | data);
    } else if (reg >= 0x20u && reg <= 0x25u) {
        Channel& ch = channels[reg - 0x20u];
        const bool oldTrigger = ch.trigger;
        ch.frequency = uint16_t((ch.frequency & 0xFFu) | ((data & 1u) << 8));
        ch.octave = uint8_t((data >> 1) & 7u);
        ch.trigger = (data & 0x10u) != 0;
        ch.sustain = (data & 0x20u) != 0;
        if (!oldTrigger && ch.trigger) keyOn(ch);
        else if (oldTrigger && !ch.trigger) keyOff(ch);
    } else if (reg >= 0x30u && reg <= 0x35u) {
        Channel& ch = channels[reg - 0x30u];
        ch.instrument = uint8_t(data >> 4);
        ch.volume = uint8_t(data & 0x0Fu);
    }
}

void Mapper085::stepEnvelope(Operator& op, uint8_t attack, uint8_t decay,
                             uint8_t sustainLevel, uint8_t release,
                             bool patchSustain, bool channelSustain,
                             bool isModulator, double keyRate) {
    const double target = dbToLinear(-3.0 * double(sustainLevel));
    // OPLL rates are exponential and key-rate scaled. These coefficients are
    // expressed per 49.716 kHz internal sample and preserve the hardware's
    // important edge cases: rate 0 halts and rate 15 attacks nearly instantly.
    const double ks = std::max(0.5, std::min(4.0, keyRate));
    switch (op.state) {
        case ENV_OFF: op.envelope = 0.0; break;
        case ENV_ATTACK:
            if (attack == 0) break;
            if (attack == 15) op.envelope = 1.0;
            else op.envelope += (1.0 - op.envelope) * std::min(1.0, (0.000012 * std::pow(2.0, attack * 0.72)) * ks);
            if (op.envelope >= 0.9995) { op.envelope = 1.0; op.state = ENV_DECAY; }
            break;
        case ENV_DECAY:
            if (decay == 0) break;
            op.envelope *= std::max(0.0, 1.0 - (0.0000027 * std::pow(2.0, decay * 0.70)) * ks);
            if (op.envelope <= target) { op.envelope = target; op.state = ENV_SUSTAIN; }
            break;
        case ENV_SUSTAIN:
            if (!patchSustain && !op.keyOn) op.state = ENV_RELEASE;
            break;
        case ENV_RELEASE: {
            if (isModulator && patchSustain) break;
            uint8_t rr = channelSustain ? 5u : release;
            if (rr == 0) break;
            op.envelope *= std::max(0.0, 1.0 - (0.0000017 * std::pow(2.0, rr * 0.78)) * ks);
            if (op.envelope < 0.00001) { op.envelope = 0.0; op.state = ENV_OFF; }
            break;
        }
    }
    op.envelope = clamp01(op.envelope);
}

void Mapper085::stepFm() {
    tremoloPhase += 3.7 / VRC7_RATE;
    vibratoPhase += 6.1 / VRC7_RATE;
    if (tremoloPhase >= 1.0) tremoloPhase -= 1.0;
    if (vibratoPhase >= 1.0) vibratoPhase -= 1.0;
    const double tremolo = 0.5 + 0.5 * std::sin(tremoloPhase * 2.0 * PI_D);
    const double vibrato = std::sin(vibratoPhase * 2.0 * PI_D);

    double mix = 0.0;
    for (unsigned index = 0; index < 6; ++index) {
        Channel& ch = channels[index];
        const uint8_t* p = patchFor(ch.instrument);
        const uint8_t modFlags = p[0], carFlags = p[1];
        const double keyRate = 1.0 + (double(ch.octave) / 7.0) + (double(ch.frequency) / 511.0) * 0.35;

        stepEnvelope(ch.mod, p[4] >> 4, p[4] & 15u, p[6] >> 4, p[6] & 15u,
                     (modFlags & 0x20u) != 0, ch.sustain, true,
                     (modFlags & 0x10u) ? keyRate : 1.0);
        stepEnvelope(ch.car, p[5] >> 4, p[5] & 15u, p[7] >> 4, p[7] & 15u,
                     (carFlags & 0x20u) != 0, ch.sustain, false,
                     (carFlags & 0x10u) ? keyRate : 1.0);

        if (ch.mod.state == ENV_OFF && ch.car.state == ENV_OFF) continue;

        const double baseInc = double(ch.frequency & 0x1FFu) / std::pow(2.0, 19.0 - double(ch.octave));
        const double modVib = (modFlags & 0x40u) ? (1.0 + vibrato * 0.008) : 1.0;
        const double carVib = (carFlags & 0x40u) ? (1.0 + vibrato * 0.008) : 1.0;
        ch.mod.phase += baseInc * multiplier(modFlags & 15u) * modVib;
        ch.car.phase += baseInc * multiplier(carFlags & 15u) * carVib;
        ch.mod.phase -= std::floor(ch.mod.phase);
        ch.car.phase -= std::floor(ch.car.phase);

        const bool modHalf = (p[3] & 0x08u) != 0;
        const bool carHalf = (p[3] & 0x10u) != 0;
        const uint8_t fb = p[3] & 7u;
        const double fbInput = (ch.mod.feedback1 + ch.mod.feedback2) * 0.5 * feedbackScale(fb);

        double modLevel = dbToLinear(-0.75 * double(p[2] & 0x3Fu));
        if (modFlags & 0x80u) modLevel *= (0.80 + 0.20 * tremolo);
        const double modSample = waveform(ch.mod.phase + fbInput / (2.0 * PI_D), modHalf) * ch.mod.envelope * modLevel;
        ch.mod.feedback2 = ch.mod.feedback1;
        ch.mod.feedback1 = modSample;

        double carLevel = dbToLinear(-3.0 * double(ch.volume));
        if (carFlags & 0x80u) carLevel *= (0.80 + 0.20 * tremolo);
        const double carrier = waveform(ch.car.phase + modSample * 1.75, carHalf) * ch.car.envelope * carLevel;
        mix += carrier;
    }

    // Cartridge mixing is substantially quieter than the internal 2A03 path.
    audioOut = float(std::max(-0.32, std::min(0.32, mix * 0.055)));
}

void Mapper085::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    for (unsigned i=0;i<4;i++) write_u16(out,prg8[i]);
    for (unsigned i=0;i<8;i++) write_u16(out,chr1[i]);
    write_u8(out,control); write_u8(out,irqLatch); write_u8(out,irqCounter);
    write_u16(out,uint16_t(irqPrescaler)); write_bool(out,irqEnabled); write_bool(out,irqEnableAfterAck);
    write_bool(out,irqCycleMode); write_bool(out,irqActive);
    write_u8(out,audioAddress); for(unsigned i=0;i<0x40;i++)write_u8(out,audioRegs[i]);
    for(unsigned i=0;i<8;i++) write_u8(out,customPatch[i]);
    write_bool(out,audioMuted);
    write_u8(out,fmDivider);
    writeDouble(out,tremoloPhase); writeDouble(out,vibratoPhase);
    uint32_t audioBits=0; std::memcpy(&audioBits,&audioOut,sizeof(audioBits)); write_u32(out,audioBits);
    writeDouble(out,outputSampleRate); writeDouble(out,outputAccumulator);
    write_u32(out,uint32_t(audioQueue.size()));
    for (std::deque<float>::const_iterator it=audioQueue.begin(); it!=audioQueue.end(); ++it) {
        uint32_t bits=0; std::memcpy(&bits,&*it,sizeof(bits)); write_u32(out,bits);
    }
    for(unsigned i=0;i<6;i++){
        const Channel& c=channels[i]; write_u16(out,c.frequency);write_u8(out,c.octave);write_u8(out,c.instrument);write_u8(out,c.volume);write_bool(out,c.sustain);write_bool(out,c.trigger);
        const Operator* ops[2]={&c.mod,&c.car};
        for(unsigned k=0;k<2;k++){const Operator&o=*ops[k];writeDouble(out,o.phase);writeDouble(out,o.envelope);writeDouble(out,o.feedback1);writeDouble(out,o.feedback2);write_u8(out,uint8_t(o.state));write_bool(out,o.keyOn);}
    }
}

bool Mapper085::loadState(const uint8_t*& p,const uint8_t* end) {
    using namespace ciphernes_state;
    for(unsigned i=0;i<4;i++)if(!read_u16(p,end,prg8[i]))return false;
    for(unsigned i=0;i<8;i++)if(!read_u16(p,end,chr1[i]))return false;
    uint16_t pre=0;if(!read_u8(p,end,control)||!read_u8(p,end,irqLatch)||!read_u8(p,end,irqCounter)||!read_u16(p,end,pre)||!read_bool(p,end,irqEnabled)||!read_bool(p,end,irqEnableAfterAck)||!read_bool(p,end,irqCycleMode)||!read_bool(p,end,irqActive))return false;irqPrescaler=int16_t(pre);
    if(!read_u8(p,end,audioAddress)) return false;
    for(unsigned i=0;i<0x40;i++) if(!read_u8(p,end,audioRegs[i])) return false;
    for(unsigned i=0;i<8;i++) if(!read_u8(p,end,customPatch[i])) return false;
    if(!read_bool(p,end,audioMuted) || !read_u8(p,end,fmDivider) ||
       !readDouble(p,end,tremoloPhase) || !readDouble(p,end,vibratoPhase)) return false;
    uint32_t audioBits=0;if(!read_u32(p,end,audioBits))return false;std::memcpy(&audioOut,&audioBits,sizeof(audioBits));
    if(!readDouble(p,end,outputSampleRate)||!readDouble(p,end,outputAccumulator))return false;
    uint32_t queued=0;if(!read_u32(p,end,queued)||queued>MAX_AUDIO_QUEUE)return false;audioQueue.clear();
    for(uint32_t i=0;i<queued;i++){uint32_t bits=0;float sample=0.0f;if(!read_u32(p,end,bits))return false;std::memcpy(&sample,&bits,sizeof(bits));audioQueue.push_back(sample);}
    for(unsigned i=0;i<6;i++){Channel&c=channels[i];if(!read_u16(p,end,c.frequency)||!read_u8(p,end,c.octave)||!read_u8(p,end,c.instrument)||!read_u8(p,end,c.volume)||!read_bool(p,end,c.sustain)||!read_bool(p,end,c.trigger))return false;Operator*ops[2]={&c.mod,&c.car};for(unsigned k=0;k<2;k++){Operator&o=*ops[k];uint8_t st=0;if(!readDouble(p,end,o.phase)||!readDouble(p,end,o.envelope)||!readDouble(p,end,o.feedback1)||!readDouble(p,end,o.feedback2)||!read_u8(p,end,st)||!read_bool(p,end,o.keyOn))return false;o.state=EnvelopeState(st);}}
    return true;
}
