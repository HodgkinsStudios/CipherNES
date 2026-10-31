#include "Mapper020.h"
#include "../state_io.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>

namespace {
static uint8_t safeRead(const uint8_t* data, size_t bytes, size_t index) {
    return index < bytes ? data[index] : 0;
}
}

// ---------------- FDS envelope ----------------
void Mapper020::Envelope::reset() {
    speed = gain = 0;
    envelopeOff = false;
    increase = false;
    frequency = 0;
    timer = 0;
    masterSpeed = 0xE8;
}

void Mapper020::Envelope::resetTimer() {
    timer = 8u * (uint32_t(speed) + 1u) * uint32_t(masterSpeed);
}

void Mapper020::Envelope::writeReg(uint16_t addr, uint8_t value) {
    switch (addr & 0x03u) {
        case 0:
            speed = value & 0x3Fu;
            increase = (value & 0x40u) != 0;
            envelopeOff = (value & 0x80u) != 0;
            resetTimer();
            if (envelopeOff) gain = speed;
            break;
        case 2:
            frequency = uint16_t((frequency & 0x0F00u) | value);
            break;
        case 3:
            frequency = uint16_t((frequency & 0x00FFu) | (uint16_t(value & 0x0Fu) << 8));
            break;
        default:
            break;
    }
}

bool Mapper020::Envelope::tick() {
    if (envelopeOff || masterSpeed == 0) return false;
    if (timer > 0) --timer;
    if (timer != 0) return false;
    resetTimer();
    if (increase) {
        if (gain < 32) ++gain;
    } else if (gain > 0) {
        --gain;
    }
    return true;
}

void Mapper020::Envelope::save(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    write_u8(out, speed); write_u8(out, gain); write_bool(out, envelopeOff);
    write_bool(out, increase); write_u16(out, frequency); write_u32(out, timer);
    write_u8(out, masterSpeed);
}

bool Mapper020::Envelope::load(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    return read_u8(p,end,speed) && read_u8(p,end,gain) &&
           read_bool(p,end,envelopeOff) && read_bool(p,end,increase) &&
           read_u16(p,end,frequency) && read_u32(p,end,timer) &&
           read_u8(p,end,masterSpeed);
}

// ---------------- FDS modulation channel ----------------
void Mapper020::Modulator::reset() {
    Envelope::reset();
    counter = 0;
    disabled = false;
    std::memset(table, 0, sizeof(table));
    tablePosition = 0;
    overflowCounter = 0;
    output = 0;
}

void Mapper020::Modulator::updateCounter(int value) {
    value &= 0x7F;
    if (value >= 64) value -= 128;
    counter = int8_t(value);
}

void Mapper020::Modulator::writeReg(uint16_t addr, uint8_t value) {
    switch (addr) {
        case 0x4084:
        case 0x4086:
            Envelope::writeReg(addr, value);
            break;
        case 0x4085:
            updateCounter(value);
            break;
        case 0x4087:
            Envelope::writeReg(addr, value);
            disabled = (value & 0x80u) != 0;
            if (disabled) overflowCounter = 0;
            break;
        default:
            break;
    }
}

void Mapper020::Modulator::writeTable(uint8_t value) {
    if (!disabled) return;
    table[tablePosition & 0x3Fu] = value & 0x07u;
    table[(tablePosition + 1u) & 0x3Fu] = value & 0x07u;
    tablePosition = uint8_t((tablePosition + 2u) & 0x3Fu);
}

bool Mapper020::Modulator::tickModulator() {
    if (disabled || frequency == 0) return false;
    const uint16_t old = overflowCounter;
    overflowCounter = uint16_t(overflowCounter + frequency);
    if (overflowCounter >= old) return false;

    static const int lut[8] = { 0, 1, 2, 4, 0x7FFFFFFF, -4, -2, -1 };
    const int delta = lut[table[tablePosition & 0x3Fu] & 7u];
    updateCounter(delta == 0x7FFFFFFF ? 0 : int(counter) + delta);
    tablePosition = uint8_t((tablePosition + 1u) & 0x3Fu);
    return true;
}

void Mapper020::Modulator::updateOutput(uint16_t carrierPitch) {
    int32_t temp = int32_t(counter) * int32_t(gain);
    int32_t remainder = temp & 0x0F;
    temp >>= 4;
    if (remainder > 0 && (temp & 0x80) == 0) temp += counter < 0 ? -1 : 2;
    if (temp >= 192) temp -= 256;
    else if (temp < -64) temp += 256;

    temp *= int32_t(carrierPitch);
    remainder = temp & 0x3F;
    temp >>= 6;
    if (remainder >= 32) ++temp;
    output = temp;
}

void Mapper020::Modulator::save(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    Envelope::save(out);
    write_u8(out, uint8_t(counter)); write_bool(out, disabled);
    for (size_t i=0;i<64;i++) write_u8(out, table[i]);
    write_u8(out, tablePosition); write_u16(out, overflowCounter);
    write_u32(out, uint32_t(output));
}

bool Mapper020::Modulator::load(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    uint8_t c = 0; uint32_t o = 0;
    if (!Envelope::load(p,end) || !read_u8(p,end,c) || !read_bool(p,end,disabled)) return false;
    counter = int8_t(c);
    for (size_t i=0;i<64;i++) if (!read_u8(p,end,table[i])) return false;
    if (!read_u8(p,end,tablePosition) || !read_u16(p,end,overflowCounter) || !read_u32(p,end,o)) return false;
    output = int32_t(o);
    return true;
}

// ---------------- Mapper 20 ----------------
Mapper020::Mapper020(uint16_t p, uint16_t c,
                     const std::vector<uint8_t>& rawDiskImage,
                     const std::string& persistentSavePath,
                     bool qdFormat)
    : Mapper(p,c), useQdFormat(qdFormat), savePath(persistentSavePath) {
    std::vector<uint8_t> image = rawDiskImage;

    // A valid sidecar is authoritative, preserving disk writes without
    // modifying the user's original disk image.
    if (!savePath.empty()) {
        std::ifstream in(savePath.c_str(), std::ios::binary);
        if (in) {
            in.seekg(0, std::ios::end);
            std::streamoff n = in.tellg();
            in.seekg(0, std::ios::beg);
            if (n > 0) {
                std::vector<uint8_t> saved(static_cast<size_t>(n), uint8_t(0));
                in.read(reinterpret_cast<char*>(saved.data()), n);
                if (in && ((saved.size() >= 16 && std::memcmp(saved.data(), "FDS\x1A", 4) == 0) ||
                           saved.size() % sideCapacity() == 0)) {
                    image.swap(saved);
                }
            }
        }
    }

    loadImage(image);
    reset(true);
    if (!diskSides.empty()) insertDiskSide(0);
}

Mapper020::~Mapper020() {
    flushPersistentData();
}

void Mapper020::resetAudio() {
    std::memset(waveTable, 0, sizeof(waveTable));
    volume.reset();
    mod.reset();
    waveWriteEnabled = false;
    disableEnvelopes = false;
    haltWaveform = false;
    masterVolume = 0;
    waveOverflowCounter = 0;
    wavePosition = 0;
    currentAudioLevel = 0;
}

void Mapper020::reset() { reset(true); }

void Mapper020::reset(bool hardReset) {
    irqReloadValue = 0;
    irqCounter = 0xFFFFu;
    irqEnabled = irqRepeatEnabled = timerIrqActive = false;
    // The RAM adapter powers up with disk and sound I/O disabled.  The BIOS
    // explicitly enables both through $4023 during its initialization path.
    diskRegistersEnabled = false;
    soundRegistersEnabled = false;
    writeDataRegister = readDataRegister = externalConnector = 0;
    motorOn = resetTransfer = crcControl = diskReady = diskIrqEnabled = false;
    readMode = true;
    transferComplete = badCrc = scanningDisk = diskIrqActive = false;
    previousCrcControl = true;
    endOfHead = true;
    gapEnded = false;
    diskPosition = 0;
    transferDelay = 0;
    crcAccumulator = 0;
    mirrorMode = 1;
    if (hardReset) resetAudio();
}

bool Mapper020::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr >= 0x6000u && addr <= 0xDFFFu) {
        mappedAddr = PRG_RAM_FLAG | uint32_t(addr - 0x6000u);
        return true;
    }
    if (addr >= 0xE000u) {
        mappedAddr = uint32_t(addr - 0xE000u);
        return true;
    }
    return false;
}

bool Mapper020::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t) {
    if (addr >= 0x6000u && addr <= 0xDFFFu) {
        mappedAddr = PRG_RAM_FLAG | uint32_t(addr - 0x6000u);
        return true;
    }
    return false;
}

bool Mapper020::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    addr &= 0x3FFFu;
    if (addr <= 0x1FFFu) { mappedAddr = addr; return true; }
    return false;
}

bool Mapper020::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t) {
    addr &= 0x3FFFu;
    if (addr <= 0x1FFFu) { mappedAddr = addr; return true; }
    return false;
}

void Mapper020::clockTimerIrq() {
    if (!irqEnabled) return;
    if (irqCounter == 0) {
        timerIrqActive = true;
        irqCounter = irqReloadValue;
        if (!irqRepeatEnabled) irqEnabled = false;
    } else {
        --irqCounter;
    }
}

uint8_t Mapper020::readDiskByte() const {
    if (diskNumber == NoDiskInserted || diskNumber >= diskSides.size()) return 0;
    const std::vector<uint8_t>& side = diskSides[diskNumber];
    return diskPosition < side.size() ? side[diskPosition] : 0;
}

void Mapper020::writeDiskByte(uint8_t value) {
    if (diskNumber == NoDiskInserted || diskNumber >= diskSides.size()) return;
    std::vector<uint8_t>& side = diskSides[diskNumber];
    if (diskPosition >= side.size()) return;
    if (side[diskPosition] != value) {
        side[diskPosition] = value;
        diskDirty = true;
    }
}

void Mapper020::updateCrc(uint8_t value) {
    crcAccumulator ^= value;
    for (int i=0;i<8;i++) {
        const bool carry = (crcAccumulator & 1u) != 0;
        crcAccumulator >>= 1;
        if (carry) crcAccumulator ^= 0x8408u;
    }
}

void Mapper020::clockDiskDrive() {
    if (diskNumber == NoDiskInserted || !motorOn || diskNumber >= diskSides.size()) {
        endOfHead = true;
        scanningDisk = false;
        return;
    }
    if (resetTransfer && !scanningDisk) return;

    if (endOfHead) {
        transferDelay = 50000u;
        endOfHead = false;
        diskPosition = 0;
        gapEnded = false;
        return;
    }

    if (transferDelay > 0) { --transferDelay; return; }
    scanningDisk = true;

    uint8_t diskData = 0;
    bool needIrq = diskIrqEnabled;
    if (readMode) {
        diskData = readDiskByte();
        if (!previousCrcControl) updateCrc(diskData);

        if (!diskReady) {
            gapEnded = false;
            crcAccumulator = 0;
            badCrc = false;
        } else if (diskData != 0 && !gapEnded) {
            gapEnded = true;
            needIrq = false;
        }

        if (gapEnded) {
            transferComplete = true;
            readDataRegister = diskData;
            if (needIrq) diskIrqActive = true;
        }
        if (!previousCrcControl && crcControl) badCrc = crcAccumulator != 0;
    } else {
        if (!crcControl) {
            transferComplete = true;
            diskData = writeDataRegister;
            if (needIrq) diskIrqActive = true;
        }
        if (!diskReady) {
            diskData = 0;
            crcAccumulator = 0;
        }
        if (!crcControl) updateCrc(diskData);
        else { diskData = uint8_t(crcAccumulator & 0xFFu); crcAccumulator >>= 8; }
        writeDiskByte(diskData);
        gapEnded = false;
        badCrc = false;
    }

    previousCrcControl = crcControl;
    ++diskPosition;
    if (diskPosition >= diskSides[diskNumber].size()) {
        motorOn = false;
        if (diskIrqEnabled) diskIrqActive = true;
    } else {
        // One byte is transferred every 150 CPU clocks (149 idle clocks plus
        // this transfer clock), matching the drive bit rate.
        transferDelay = 149u;
    }
}

void Mapper020::clockAudio() {
    const uint16_t carrier = volume.frequency;
    if (!haltWaveform && !disableEnvelopes) {
        volume.tick();
        if (mod.tick()) mod.updateOutput(carrier);
    }
    if (mod.tickModulator()) mod.updateOutput(carrier);

    if (!haltWaveform) {
        const int pitch = int(carrier) + mod.output;
        if (pitch > 0) {
            const uint16_t add = uint16_t(pitch);
            const uint16_t old = waveOverflowCounter;
            waveOverflowCounter = uint16_t(waveOverflowCounter + add);
            if (waveOverflowCounter < old) wavePosition = uint8_t((wavePosition + 1u) & 0x3Fu);
        }
    }

    if (waveWriteEnabled) {
        currentAudioLevel = 0;
    } else {
        static const uint32_t volumeTable[4] = { 36u, 24u, 17u, 14u };
        const uint32_t level = uint32_t(std::min<int>(volume.gain, 32)) * volumeTable[masterVolume & 3u];
        currentAudioLevel = uint8_t((uint32_t(waveTable[wavePosition] & 0x3Fu) * level) / 1152u);
    }
}

void Mapper020::cpuClock() {
    clockTimerIrq();
    clockDiskDrive();
    clockAudio();
}

float Mapper020::audioSample() {
    // Match the existing expansion-audio path: silence is exactly zero and the
    // hardware-relative master volume table is already applied above.
    return float(currentAudioLevel) * (0.30f / 63.0f);
}

uint8_t Mapper020::readAudioRegister(uint16_t addr) const {
    if (addr <= 0x407Fu) return waveWriteEnabled ? waveTable[addr & 0x3Fu] : waveTable[wavePosition];
    if (addr == 0x4090u) return volume.gain & 0x3Fu;
    if (addr == 0x4092u) return mod.gain & 0x3Fu;
    return 0;
}

void Mapper020::writeAudioRegister(uint16_t addr, uint8_t value) {
    if (addr <= 0x407Fu) {
        if (waveWriteEnabled) waveTable[addr & 0x3Fu] = value & 0x3Fu;
        return;
    }
    switch (addr) {
        case 0x4080:
        case 0x4082:
            volume.writeReg(addr, value);
            mod.updateOutput(volume.frequency);
            break;
        case 0x4083:
            disableEnvelopes = (value & 0x40u) != 0;
            haltWaveform = (value & 0x80u) != 0;
            if (haltWaveform) wavePosition = 0;
            if (disableEnvelopes) { volume.resetTimer(); mod.resetTimer(); }
            volume.writeReg(addr, value);
            mod.updateOutput(volume.frequency);
            break;
        case 0x4084:
        case 0x4085:
            mod.writeReg(addr, value);
            mod.updateOutput(volume.frequency);
            break;
        case 0x4086:
        case 0x4087:
            mod.writeReg(addr, value);
            break;
        case 0x4088:
            mod.writeTable(value);
            break;
        case 0x4089:
            masterVolume = value & 0x03u;
            waveWriteEnabled = (value & 0x80u) != 0;
            break;
        case 0x408A:
            volume.masterSpeed = value;
            mod.masterSpeed = value;
            break;
        default:
            break;
    }
}

bool Mapper020::cpuWriteDirect(uint16_t addr, uint8_t value) {
    if (addr < 0x4020u || addr > 0x4092u) return false;
    if ((!diskRegistersEnabled && addr >= 0x4024u && addr <= 0x4026u) ||
        (!soundRegistersEnabled && addr >= 0x4040u)) return true;

    switch (addr) {
        case 0x4020: irqReloadValue = uint16_t((irqReloadValue & 0xFF00u) | value); break;
        case 0x4021: irqReloadValue = uint16_t((irqReloadValue & 0x00FFu) | (uint16_t(value) << 8)); break;
        case 0x4022:
            irqRepeatEnabled = (value & 0x01u) != 0;
            irqEnabled = (value & 0x02u) != 0 && diskRegistersEnabled;
            if (irqEnabled) irqCounter = irqReloadValue;
            else timerIrqActive = false;
            break;
        case 0x4023:
            diskRegistersEnabled = (value & 0x01u) != 0;
            soundRegistersEnabled = (value & 0x02u) != 0;
            if (!diskRegistersEnabled) {
                irqEnabled = false;
                timerIrqActive = false;
                diskIrqActive = false;
            }
            break;
        case 0x4024:
            writeDataRegister = value;
            transferComplete = false;
            diskIrqActive = false;
            break;
        case 0x4025:
            motorOn = (value & 0x01u) != 0;
            if (!motorOn) {
                endOfHead = true;
                scanningDisk = false;
            }
            resetTransfer = (value & 0x02u) != 0;
            readMode = (value & 0x04u) != 0;
            mirrorMode = (value & 0x08u) ? 0u : 1u;
            crcControl = (value & 0x10u) != 0;
            diskReady = (value & 0x40u) != 0;
            diskIrqEnabled = (value & 0x80u) != 0;
            diskIrqActive = false;
            break;
        case 0x4026: externalConnector = value; break;
        default:
            if (addr >= 0x4040u) writeAudioRegister(addr, value);
            break;
    }
    return true;
}

bool Mapper020::cpuReadDirect(uint16_t addr, uint8_t& data) {
    if (addr < 0x4020u || addr > 0x4092u) return false;
    if (soundRegistersEnabled && addr >= 0x4040u) {
        data = readAudioRegister(addr);
        return true;
    }
    if (!diskRegistersEnabled || addr > 0x4033u) { data = 0; return true; }

    switch (addr) {
        case 0x4030:
            data = uint8_t((timerIrqActive ? 0x01u : 0u) |
                           (transferComplete ? 0x02u : 0u) |
                           ((useQdFormat && badCrc) ? 0x10u : 0u) |
                           (endOfHead ? 0x40u : 0u));
            transferComplete = false;
            timerIrqActive = false;
            diskIrqActive = false;
            return true;
        case 0x4031:
            data = readDataRegister;
            transferComplete = false;
            diskIrqActive = false;
            return true;
        case 0x4032: {
            const bool absent = diskNumber == NoDiskInserted || diskNumber >= diskSides.size();
            data = uint8_t((absent ? 0x01u : 0u) |
                           ((absent || !scanningDisk) ? 0x02u : 0u) |
                           (absent ? 0x04u : 0u));
            return true;
        }
        case 0x4033:
            // Bit 7 is the RAM adapter's battery-good signal.
            data = uint8_t(externalConnector | 0x80u);
            return true;
        default:
            data = 0;
            return true;
    }
}

bool Mapper020::insertDiskSide(uint32_t side) {
    if (side >= diskSides.size()) return false;
    diskNumber = side;
    diskPosition = 0;
    transferDelay = 0;
    endOfHead = true;
    scanningDisk = false;
    transferComplete = false;
    badCrc = false;
    crcAccumulator = 0;
    previousCrcControl = true;
    gapEnded = false;
    diskIrqActive = false;
    return true;
}

void Mapper020::ejectDisk() {
    diskNumber = NoDiskInserted;
    motorOn = false;
    scanningDisk = false;
    endOfHead = true;
    transferComplete = false;
    diskIrqActive = false;
}

void Mapper020::addGaps(std::vector<uint8_t>& side, const uint8_t* source, size_t bytes) const {
    side.insert(side.end(), 28300u / 8u, 0);
    for (size_t j=0; j<sideCapacity();) {
        const uint8_t blockType = safeRead(source, bytes, j);
        size_t blockLength = 1;
        switch (blockType) {
            case 1: blockLength = 56; break;
            case 2: blockLength = 2; break;
            case 3: blockLength = 16; break;
            case 4:
                blockLength = 1u + (useQdFormat
                    ? size_t(safeRead(source,bytes,j>=5?j-5:0) | (uint16_t(safeRead(source,bytes,j>=4?j-4:0)) << 8))
                    : size_t(safeRead(source,bytes,j>=3?j-3:0) | (uint16_t(safeRead(source,bytes,j>=2?j-2:0)) << 8)));
                break;
            default:
                side.push_back(0x80);
                if (j < bytes) side.insert(side.end(), source + j, source + bytes);
                if (side.size() < sideCapacity()) side.resize(sideCapacity(), 0);
                return;
        }
        if (useQdFormat) blockLength += 2;
        if (j + blockLength > bytes) break;
        side.push_back(0x80);
        side.insert(side.end(), source + j, source + j + blockLength);
        if (!useQdFormat) { side.push_back(0x4D); side.push_back(0x62); }
        side.insert(side.end(), 976u / 8u, 0);
        j += blockLength;
    }
    if (side.size() < sideCapacity()) side.resize(sideCapacity(), 0);
}

bool Mapper020::loadImage(const std::vector<uint8_t>& rawImage) {
    diskSides.clear();
    if (rawImage.empty()) return false;
    imageHadHeader = rawImage.size() >= 16u && std::memcmp(rawImage.data(), "FDS\x1A", 4) == 0;
    size_t offset = imageHadHeader ? 16u : 0u;
    size_t count = imageHadHeader ? size_t(rawImage[4]) : (rawImage.size() - offset) / sideCapacity();
    if (count == 0) return false;
    const size_t available = rawImage.size() > offset ? rawImage.size() - offset : 0;
    count = std::min(count, available / sideCapacity());
    if (count == 0) return false;

    for (size_t i=0;i<count;i++) {
        diskSides.push_back(std::vector<uint8_t>());
        addGaps(diskSides.back(), rawImage.data() + offset + i * sideCapacity(), sideCapacity());
    }
    return !diskSides.empty();
}

std::vector<uint8_t> Mapper020::rebuildImage() const {
    std::vector<uint8_t> out;
    if (imageHadHeader) {
        const uint8_t header[16] = { 'F','D','S',0x1A,uint8_t(diskSides.size()),0,0,0,0,0,0,0,0,0,0,0 };
        out.insert(out.end(), header, header + 16);
    }

    size_t sideIndex = 0;
    for (size_t s=0;s<diskSides.size();s++) {
        const std::vector<uint8_t>& side = diskSides[s];
        bool inGap = true;
        size_t i=0;
        uint32_t fileSize=0;
        while (i < side.size()) {
            if (inGap) {
                if (side[i] == 0x80) inGap = false;
                ++i;
                continue;
            }
            size_t blockLength=1;
            switch (side[i]) {
                case 1: blockLength=56; break;
                case 2: blockLength=2; break;
                case 3:
                    blockLength=16;
                    if (i+14 < side.size()) fileSize=uint32_t(side[i+13]) | (uint32_t(side[i+14])<<8);
                    break;
                case 4: blockLength=1u+fileSize; break;
                default:
                    out.insert(out.end(), side.begin()+i, side.end());
                    i=side.size();
                    blockLength=0;
                    break;
            }
            if (!blockLength || i+blockLength > side.size()) break;
            if (useQdFormat) blockLength += 2;
            if (i+blockLength > side.size()) break;
            out.insert(out.end(), side.begin()+i, side.begin()+i+blockLength);
            i += blockLength;
            if (!useQdFormat) i = std::min(side.size(), i+2u);
            inGap=true;
        }
        ++sideIndex;
        out.resize((imageHadHeader ? 16u : 0u) + sideIndex * sideCapacity(), 0);
    }
    return out;
}

void Mapper020::flushPersistentData() {
    if (!diskDirty || savePath.empty() || diskSides.empty()) return;
    const std::vector<uint8_t> data = rebuildImage();
    std::ofstream out(savePath.c_str(), std::ios::binary | std::ios::trunc);
    if (!out || data.empty()) return;
    out.write(reinterpret_cast<const char*>(data.data()), std::streamsize(data.size()));
    if (out) diskDirty = false;
}

std::string Mapper020::debugText() const {
    std::ostringstream ss;
    ss << "Mapper 20 / FDS\n";
    ss << "Disk: ";
    if (diskNumber == NoDiskInserted) ss << "ejected";
    else ss << (diskNumber + 1u) << '/' << diskSides.size() << " pos=" << diskPosition;
    ss << "\nMotor=" << (motorOn?"on":"off")
       << " scanning=" << (scanningDisk?"yes":"no")
       << " mode=" << (readMode?"read":"write")
       << " timerIRQ=" << (timerIrqActive?"1":"0")
       << " diskIRQ=" << (diskIrqActive?"1":"0");
    return ss.str();
}

void Mapper020::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    write_u32(out, saveStateVersion());
    write_bool(out,useQdFormat); write_bool(out,imageHadHeader); write_bool(out,diskDirty);
    write_u32(out,uint32_t(diskSides.size()));
    for (size_t i=0;i<diskSides.size();i++) write_vec(out,diskSides[i]);

    write_u16(out,irqReloadValue); write_u16(out,irqCounter); write_bool(out,irqEnabled);
    write_bool(out,irqRepeatEnabled); write_bool(out,timerIrqActive);
    write_bool(out,diskRegistersEnabled); write_bool(out,soundRegistersEnabled);
    write_u8(out,writeDataRegister); write_bool(out,motorOn); write_bool(out,resetTransfer);
    write_bool(out,readMode); write_bool(out,crcControl); write_bool(out,diskReady);
    write_bool(out,diskIrqEnabled); write_u8(out,externalConnector);
    write_u8(out,readDataRegister); write_bool(out,transferComplete); write_bool(out,badCrc);
    write_bool(out,endOfHead); write_bool(out,scanningDisk); write_bool(out,previousCrcControl);
    write_bool(out,gapEnded); write_bool(out,diskIrqActive); write_u32(out,diskNumber);
    write_u32(out,diskPosition); write_u32(out,transferDelay); write_u16(out,crcAccumulator);
    write_u8(out,mirrorMode);

    for(size_t i=0;i<64;i++) write_u8(out,waveTable[i]);
    write_bool(out,waveWriteEnabled); write_bool(out,disableEnvelopes); write_bool(out,haltWaveform);
    write_u8(out,masterVolume); write_u16(out,waveOverflowCounter); write_u8(out,wavePosition);
    write_u8(out,currentAudioLevel); volume.save(out); mod.save(out);
}

bool Mapper020::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    uint32_t version=0,count=0;
    if(!read_u32(p,end,version) || version<1u || version>saveStateVersion()) return false;
    if(!read_bool(p,end,useQdFormat)||!read_bool(p,end,imageHadHeader)||!read_bool(p,end,diskDirty)||!read_u32(p,end,count)) return false;
    if(count>256u) return false;
    diskSides.resize(count);
    for(uint32_t i=0;i<count;i++) if(!read_vec(p,end,diskSides[i])) return false;

    if(!read_u16(p,end,irqReloadValue)||!read_u16(p,end,irqCounter)||!read_bool(p,end,irqEnabled)||
       !read_bool(p,end,irqRepeatEnabled)||!read_bool(p,end,timerIrqActive)||
       !read_bool(p,end,diskRegistersEnabled)||!read_bool(p,end,soundRegistersEnabled)||
       !read_u8(p,end,writeDataRegister)||!read_bool(p,end,motorOn)||!read_bool(p,end,resetTransfer)||
       !read_bool(p,end,readMode)||!read_bool(p,end,crcControl)||!read_bool(p,end,diskReady)||
       !read_bool(p,end,diskIrqEnabled)||!read_u8(p,end,externalConnector)||
       !read_u8(p,end,readDataRegister)||!read_bool(p,end,transferComplete)||!read_bool(p,end,badCrc)||
       !read_bool(p,end,endOfHead)||!read_bool(p,end,scanningDisk)||!read_bool(p,end,previousCrcControl)||
       !read_bool(p,end,gapEnded)||!read_bool(p,end,diskIrqActive)||!read_u32(p,end,diskNumber)||
       !read_u32(p,end,diskPosition)||!read_u32(p,end,transferDelay)||!read_u16(p,end,crcAccumulator)||
       !read_u8(p,end,mirrorMode)) return false;

    for(size_t i=0;i<64;i++) if(!read_u8(p,end,waveTable[i])) return false;
    if(!read_bool(p,end,waveWriteEnabled)||!read_bool(p,end,disableEnvelopes)||!read_bool(p,end,haltWaveform)||
       !read_u8(p,end,masterVolume)||!read_u16(p,end,waveOverflowCounter)||!read_u8(p,end,wavePosition)||
       !read_u8(p,end,currentAudioLevel)||!volume.load(p,end)||!mod.load(p,end)) return false;

    if(diskNumber!=NoDiskInserted && diskNumber>=diskSides.size()) diskNumber=NoDiskInserted;
    if(diskNumber!=NoDiskInserted && diskPosition>=diskSides[diskNumber].size()) diskPosition=0;
    return true;
}
