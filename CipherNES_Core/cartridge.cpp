#include "cartridge.h"

#include "state_io.h"

#include <fstream>
#include <iostream>
#include <string>
#include <memory>
#include <cstring>
#include <algorithm>
#include <cctype>
#include "platform_compat.h"

#include "Mappers/Mapper.h"
#include "Mappers/Mappers.h"

namespace {
#pragma pack(push, 1)
struct INesHeader {
    char name[4];       // 'NES<EOF>'
    uint8_t prgChunks;  // 16KB units
    uint8_t chrChunks;  // 8KB units
    uint8_t flags6;
    uint8_t flags7;
    uint8_t prgRamSize;
    uint8_t flags9;
    uint8_t flags10;
    uint8_t padding[5];
};
#pragma pack(pop)

static bool isINesMagic(const INesHeader& h) {
    return h.name[0] == 'N' && h.name[1] == 'E' && h.name[2] == 'S' && uint8_t(h.name[3]) == 0x1A;
}

static uint32_t crc32Bytes(const uint8_t* data, size_t size) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320u & uint32_t(0u - (crc & 1u)));
    }
    return crc ^ 0xFFFFFFFFu;
}

static char pathSep() {
#if defined(_WIN32)
    return '\\';
#else
    return '/';
#endif
}

static bool ensureDirExists(const std::string& dirName) {
    if (ciphernes_make_dir(dirName)) return true;
    return true; // Best-effort: an existing directory is acceptable.
}

static std::string romStemFromPath(const std::string& romPath) {
    // Strip directories
    size_t slash = romPath.find_last_of("/\\");
    std::string file = (slash == std::string::npos) ? romPath : romPath.substr(slash + 1);

    // Strip extension
    size_t dot = file.find_last_of('.');
    if (dot != std::string::npos) file = file.substr(0, dot);
    if (file.empty()) file = "save";
    return file;
}

static std::string lowerExtension(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    const size_t dot = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return std::string();
    std::string ext = path.substr(dot);
    for (size_t i=0;i<ext.size();++i) ext[i] = char(std::tolower((unsigned char)ext[i]));
    return ext;
}

static bool readWholeFile(const std::string& path, std::vector<uint8_t>& out) {
    out.clear();
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) return false;
    in.seekg(0, std::ios::end);
    const std::streamoff length = in.tellg();
    if (length < 0) return false;
    in.seekg(0, std::ios::beg);
    out.resize(size_t(length));
    if (!out.empty()) in.read(reinterpret_cast<char*>(out.data()), length);
    return bool(in) || out.empty();
}

static std::string executableDirectory() {
    return ciphernes_executable_directory();
}

static bool loadFdsBios(std::vector<uint8_t>& bios, std::string& loadedPath) {
    // Never pull a BIOS from the executable directory; FDS firmware is
    // user-provided and belongs in the application data directory only.
    const std::string biosPath = ciphernes_runtime_directory() + pathSep() +
                                 "bios" + pathSep() + "disksys.rom";
    std::vector<uint8_t> candidate;
    if (readWholeFile(biosPath, candidate) && candidate.size() == 8192u) {
        bios.swap(candidate);
        loadedPath = biosPath;
        return true;
    }
    bios.clear();
    loadedPath.clear();
    return false;
}

static bool isDirectFdsImage(const std::string& path, const std::vector<uint8_t>& bytes, bool& qdFormat) {
    const std::string ext = lowerExtension(path);
    qdFormat = ext == ".qd";
    const bool header = bytes.size() >= 16u && std::memcmp(bytes.data(), "FDS\x1A", 4) == 0;
    if (header) return true;
    if (ext == ".fds") return !bytes.empty() && (bytes.size() % 65500u) == 0;
    if (ext == ".qd") return !bytes.empty() && (bytes.size() % 65536u) == 0;
    return false;
}
}


Cartridge::Cartridge(const std::string& nesFilePath)
    : Cartridge(nesFilePath, nesFilePath) {}

Cartridge::Cartridge(const std::string& nesFilePath, const std::string& saveIdentityPath) {
    std::ifstream ifs(nesFilePath, std::ios::binary);
    if (!ifs) {
        std::cerr << "Failed to open ROM: " << nesFilePath << "\n";
        valid = false;
        return;
    }

    const std::string identityPath = saveIdentityPath.empty() ? nesFilePath : saveIdentityPath;
    auto initializeFds = [&](const std::vector<uint8_t>& rawDiskImage, bool qdFormat) -> bool {
        std::string biosPath;
        if (!loadFdsBios(prgMemory, biosPath)) {
            std::cerr << "FDS BIOS not found or invalid. Place an 8192-byte disksys.rom in "
                      << ciphernes_runtime_directory() << pathSep() << "bios" << pathSep()
                      << "disksys.rom\n";
            return false;
        }

        mapper = 20;
        prgBankCount = 1;
        chrBankCount = 0;
        mirror = Mirror::VERTICAL;
        batteryBacked = false;
        prgRam.assign(32768u, 0x00);
        chrMemory.assign(8192u, 0x00);
        chrRam.clear();

        ensureDirExists("Saves");
        saveFilePath = std::string("Saves") + pathSep() + romStemFromPath(identityPath) + ".FDS.SAV";
        mapperImpl = std::make_shared<Mapper020>(prgBankCount, chrBankCount, rawDiskImage, saveFilePath, qdFormat);
        if (!mapperImpl || mapperImpl->diskSideCount() == 0) {
            std::cerr << "Invalid or empty FDS disk image: " << nesFilePath << "\n";
            mapperImpl.reset();
            valid = false;
            return false;
        }

        mapperCapabilities = mapperImpl->capabilities();
        mapperUsesCpuClock = mapperImpl->usesCpuClock();
        mapperUsesPpuClock = mapperImpl->usesPpuClock();
        mapperUsesPpuFetchHooks = (mapperCapabilities & MAPPER_CAP_PPU_FETCH_HOOKS) != 0;
        mapperCanAssertIrq = (mapperCapabilities & MAPPER_CAP_IRQ) != 0;
        mapperUsesScanline = (mapperCapabilities & MAPPER_CAP_SCANLINE) != 0;
        mapperUsesPpuAddressBus = (mapperCapabilities & MAPPER_CAP_PPU_ADDRESS_BUS) != 0;
        mapperControlsMirroring = (mapperCapabilities & MAPPER_CAP_MIRRORING) != 0;
        mapperHandlesExternalNametables = false;
        mapperImpl->reset(true);
        mapperImpl->insertDiskSide(0);
        valid = true;
        std::cerr << "Loaded FDS BIOS: " << biosPath << " (" << mapperImpl->diskSideCount() << " side(s))\n";
        return true;
    };

    std::vector<uint8_t> wholeImage;
    if (readWholeFile(nesFilePath, wholeImage)) {
        bool qdFormat = false;
        if (isDirectFdsImage(nesFilePath, wholeImage, qdFormat)) {
            initializeFds(wholeImage, qdFormat);
            return;
        }
    }
    ifs.clear();
    ifs.seekg(0, std::ios::beg);

    INesHeader header{};
    ifs.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!ifs || !isINesMagic(header)) {
        std::cerr << "Not a valid iNES ROM: " << nesFilePath << "\n";
        valid = false;
        return;
    }

    // Base PRG/CHR sizes from header (iNES 1.0 + NES 2.0 linear extensions handled below)
    // Base PRG/CHR sizes from iNES header (16KB PRG units, 8KB CHR units)
prgBankCount = header.prgChunks;
chrBankCount = header.chrChunks;
    // Detect NES 2.0 header (bits 2-3 of flags7 == 0b10)
    const bool isNes2 = (header.flags7 & 0x0C) == 0x08;

// NES 2.0 extends PRG/CHR ROM sizes with MSBs in byte 9 (flags9):
// - PRG ROM size MSB: bits 0-3
// - CHR ROM size MSB: bits 4-7
// This handles the common linear-size case (not the exponent/multiplier encoding used when size == 0xF).
if (isNes2) {
    const uint8_t prgMsb = header.flags9 & 0x0F;
    const uint8_t chrMsb = (header.flags9 >> 4) & 0x0F;

    if (prgBankCount != 0x0F) {
        prgBankCount = uint16_t((uint16_t(prgMsb) << 8) | (prgBankCount & 0x00FF));
    }
    if (chrBankCount != 0x0F) {
        chrBankCount = uint16_t((uint16_t(chrMsb) << 8) | (chrBankCount & 0x00FF));
    }
}


    const bool hasTrainer = (header.flags6 & 0x04) != 0;

    // Mirroring
    // flags6 bit 3: four-screen
    // flags6 bit 0: 0=horizontal, 1=vertical (only if not four-screen)
    if (header.flags6 & 0x08) {
        mirror = Cartridge::Mirror::FOUR_SCREEN;
    } else {
        mirror = (header.flags6 & 0x01) ? Cartridge::Mirror::VERTICAL : Cartridge::Mirror::HORIZONTAL;
    }

    // Detect NES 2.0 header (bits 2-3 of flags7 == 0b10)

// Mapper number:
// iNES 1.0: 8-bit mapper from upper nibble of flags6 + upper nibble of flags7
// NES 2.0: adds upper 4 bits from byte 8 low nibble
uint16_t mapperLo = uint16_t((header.flags7 & 0xF0) | (header.flags6 >> 4));
uint16_t mapperHi = 0;
if (isNes2) {
    mapperHi = uint16_t(header.prgRamSize & 0x0F) << 8; // byte 8 low nibble
}
mapper = mapperLo | mapperHi;
const uint8_t submapperNumber = isNes2 ? uint8_t((header.prgRamSize >> 4) & 0x0F) : 0;


    // Battery-backed PRG-RAM (SRAM)
    batteryBacked = (header.flags6 & 0x02) != 0;

    // Skip trainer if present
    if (hasTrainer) {
        ifs.seekg(512, std::ios::cur);
    }

    // PRG RAM size.
    // iNES 1.0 byte 8 is PRG-RAM in 8KB units, where 0 commonly means 8KB.
    // NES 2.0 byte 8 is mapper/submapper; PRG-RAM size moved to byte 10.
    uint32_t prgRamBytes = 8192u;
    if (isNes2) {
        const uint8_t volatileShift = header.flags10 & 0x0F;
        const uint8_t batteryShift = (header.flags10 >> 4) & 0x0F;
        if (volatileShift != 0) prgRamBytes = 64u << volatileShift;
        else if (batteryShift != 0) prgRamBytes = 64u << batteryShift;
        else prgRamBytes = 8192u; // Preserve this emulator's historical default.
        if (batteryShift != 0) batteryBacked = true;
    } else {
        prgRamBytes = uint32_t(header.prgRamSize) * 8192u;
        if (prgRamBytes == 0) prgRamBytes = 8192u;
    }

    // Legacy iNES mapper-20 dumps contain the disk image after the 16-byte
    // header (and optional trainer), rather than PRG/CHR cartridge data.
    if (mapper == 20) {
        std::vector<uint8_t> diskPayload;
        const std::streampos payloadPos = ifs.tellg();
        ifs.clear();
        ifs.seekg(0, std::ios::end);
        const std::streampos payloadEnd = ifs.tellg();
        if (payloadPos != std::streampos(-1) && payloadEnd > payloadPos) {
            diskPayload.resize(size_t(payloadEnd - payloadPos));
            ifs.seekg(payloadPos, std::ios::beg);
            ifs.read(reinterpret_cast<char*>(diskPayload.data()), std::streamsize(diskPayload.size()));
        }
        bool qdFormat = lowerExtension(nesFilePath) == ".qd";
        initializeFds(diskPayload, qdFormat);
        return;
    }

    // Allocate cartridge work RAM before the mapper begins serving CPU reads.
    // Without this, every mapper's PRG_RAM_FLAG mapping silently reads as 0
    // and discards writes, including the $6000 test result channel used by
    // Blargg's MMC3/APU/CPU validation ROMs. The header-derived size above
    // was previously calculated but never applied to prgRam.
    prgRam.assign(size_t(prgRamBytes), 0x00);

    // Load PRG ROM
    const size_t prgBytes = size_t(prgBankCount) * 16384;
    prgMemory.resize(prgBytes);
    if (prgBytes > 0) {
        ifs.read(reinterpret_cast<char*>(prgMemory.data()), prgBytes);
        if (!ifs) {
            std::cerr << "Failed reading PRG data\n";
            valid = false;
            return;
        }
    }

    // Load CHR ROM (or allocate CHR RAM if chrBanks == 0)
    const size_t chrBytes = size_t(chrBankCount) * 8192;
    if (chrBytes > 0) {
        chrMemory.resize(chrBytes);
        ifs.read(reinterpret_cast<char*>(chrMemory.data()), chrBytes);
        if (!ifs) {
            std::cerr << "Failed reading CHR data\n";
            valid = false;
            return;
        }
    } else {
        // CHR RAM. Action 53 mapper 28 exposes four banked 8KB pages.
        const size_t chrRamBytes =
            (mapper == 6 || mapper == 8 || mapper == 17 || mapper == 28 || mapper == 96)
                ? 32768u
                : ((mapper == 13 || mapper == 111) ? 16384u : 8192u);
        chrMemory.resize(chrRamBytes, 0x00);
    }

    // Mixed CHR-ROM/CHR-RAM boards keep RAM electrically separate from ROM.
    // Sizes are physical board sizes, not inferred aliases into CHR-ROM.
    size_t mixedChrRamBytes = 0;
    switch (mapper) {
        case 74: mixedChrRamBytes = 2u * 1024u; break;
        case 77: mixedChrRamBytes = 6u * 1024u; break;
        case 119: mixedChrRamBytes = 8u * 1024u; break;
        case 165: mixedChrRamBytes = 4u * 1024u; break;
        case 195: mixedChrRamBytes = 4u * 1024u; break;
        case 199: mixedChrRamBytes = 8u * 1024u; break;
        default: break;
    }
    if (mixedChrRamBytes) chrRam.assign(mixedChrRamBytes, 0x00);

    switch (mapper) {
        case 0:
            mapperImpl = std::make_shared<Mapper000>(prgBankCount, chrBankCount);
            break;
        case 1:
            mapperImpl = std::make_shared<Mapper001>(prgBankCount, chrBankCount);
            break;
        case 2:
            mapperImpl = std::make_shared<Mapper002>(prgBankCount, chrBankCount);
            break;
        case 3:
            mapperImpl = std::make_shared<Mapper003>(prgBankCount, chrBankCount);
            break;
        case 4:
            mapperImpl = std::make_shared<Mapper004>(prgBankCount, chrBankCount);
            break;
        case 5:
            mapperImpl = std::make_shared<Mapper005>(prgBankCount, chrBankCount);
            break;
        case 6:
            mapperImpl = std::make_shared<Mapper006>(prgBankCount, chrBankCount, 6);
            break;
        case 7:
            mapperImpl = std::make_shared<Mapper007>(prgBankCount, chrBankCount);
            break;
        case 8:
            mapperImpl = std::make_shared<Mapper006>(prgBankCount, chrBankCount, 8);
            break;
        case 9:
            mapperImpl = std::make_shared<Mapper009>(prgBankCount, chrBankCount);
            break;
        case 10:
            mapperImpl = std::make_shared<Mapper010>(prgBankCount, chrBankCount);
            break;
        case 11:
            mapperImpl = std::make_shared<Mapper011>(prgBankCount, chrBankCount);
            break;
        case 12:
            mapperImpl = std::make_shared<MapperMmc3Variant>(prgBankCount, chrBankCount, 12);
            break;
        case 13:
            mapperImpl = std::make_shared<Mapper013>(prgBankCount, chrBankCount);
            break;
        case 15:
            mapperImpl = std::make_shared<Mapper015>(prgBankCount, chrBankCount);
            break;
        case 16:
            mapperImpl = std::make_shared<Mapper016>(prgBankCount, chrBankCount);
            break;
        case 17:
            mapperImpl = std::make_shared<Mapper017>(prgBankCount, chrBankCount);
            break;
        case 18:
            mapperImpl = std::make_shared<Mapper018>(prgBankCount, chrBankCount);
            break;
        case 19:
            mapperImpl = std::make_shared<Mapper019>(prgBankCount, chrBankCount);
            break;
        case 21:
            mapperImpl = std::make_shared<Mapper023>(prgBankCount, chrBankCount, 21, submapperNumber);
            break;
        case 22:
            mapperImpl = std::make_shared<Mapper022>(prgBankCount, chrBankCount);
            break;
        case 23:
            mapperImpl = std::make_shared<Mapper023>(prgBankCount, chrBankCount, 23, submapperNumber);
            break;
        case 24:
            mapperImpl = std::make_shared<MapperVrc6>(prgBankCount, chrBankCount, 24);
            break;
        case 25:
            mapperImpl = std::make_shared<Mapper023>(prgBankCount, chrBankCount, 25, submapperNumber);
            break;
        case 26:
            mapperImpl = std::make_shared<MapperVrc6>(prgBankCount, chrBankCount, 26);
            break;
        case 28:
            mapperImpl = std::make_shared<Mapper028>(prgBankCount, chrBankCount);
            break;
        case 32:
            mapperImpl = std::make_shared<Mapper032>(prgBankCount, chrBankCount);
            break;
        case 33:
            mapperImpl = std::make_shared<Mapper033>(prgBankCount, chrBankCount);
            break;
        case 34:
            mapperImpl = std::make_shared<Mapper034>(prgBankCount, chrBankCount);
            break;
        case 41:
            mapperImpl = std::make_shared<MapperCommonBoards>(prgBankCount, chrBankCount, 41);
            break;
        case 42:
            mapperImpl = std::make_shared<MapperAdditionalBoards>(prgBankCount, chrBankCount, 42);
            break;
        case 43:
            mapperImpl = std::make_shared<MapperAdditionalBoards>(prgBankCount, chrBankCount, 43);
            break;
        case 44:
            mapperImpl = std::make_shared<MapperMmc3Variant>(prgBankCount, chrBankCount, 44);
            break;
        case 45:
            mapperImpl = std::make_shared<MapperMmc3Variant>(prgBankCount, chrBankCount, 45);
            break;
        case 46:
            mapperImpl = std::make_shared<MapperAdditionalBoards>(prgBankCount, chrBankCount, 46);
            break;
        case 47:
            mapperImpl = std::make_shared<MapperMmc3Variant>(prgBankCount, chrBankCount, 47);
            break;
        case 49:
            mapperImpl = std::make_shared<MapperMmc3Variant>(prgBankCount, chrBankCount, 49);
            break;
        case 50:
            mapperImpl = std::make_shared<MapperAdditionalBoards>(prgBankCount, chrBankCount, 50);
            break;
        case 51:
            mapperImpl = std::make_shared<MapperAdditionalBoards>(prgBankCount, chrBankCount, 51);
            break;
        case 52:
            mapperImpl = std::make_shared<MapperMmc3Variant>(prgBankCount, chrBankCount, 52);
            break;
        case 53: {
            // Historical dumps exist in both concatenation orders.  The menu
            // EPROM's first 32 KiB has a stable CRC used by established emulators.
            const size_t probeSize = std::min<size_t>(0x8000u, prgMemory.size());
            const bool menuFirst = probeSize == 0x8000u &&
                crc32Bytes(prgMemory.data(), probeSize) == 0x63794E25u;
            mapperImpl = std::make_shared<Mapper053>(prgBankCount, chrBankCount, menuFirst);
            break;
        }
        case 56:
            mapperImpl = std::make_shared<MapperKaiser202>(prgBankCount, chrBankCount, 56);
            break;
        case 57:
            mapperImpl = std::make_shared<MapperCommonBoards>(prgBankCount, chrBankCount, 57);
            break;
        case 58:
            mapperImpl = std::make_shared<MapperAdditionalBoards>(prgBankCount, chrBankCount, 58);
            break;
        case 60:
            mapperImpl = std::make_shared<MapperAdditionalBoards>(prgBankCount, chrBankCount, 60);
            break;
        case 61:
            mapperImpl = std::make_shared<MapperAdditionalBoards>(prgBankCount, chrBankCount, 61);
            break;
        case 62:
            mapperImpl = std::make_shared<MapperAdditionalBoards>(prgBankCount, chrBankCount, 62);
            break;
        case 64:
            mapperImpl = std::make_shared<Mapper064>(prgBankCount, chrBankCount);
            break;
        case 65:
            mapperImpl = std::make_shared<Mapper065>(prgBankCount, chrBankCount);
            break;
        case 66:
            mapperImpl = std::make_shared<Mapper066>(prgBankCount, chrBankCount);
            break;
        case 67:
            mapperImpl = std::make_shared<Mapper067>(prgBankCount, chrBankCount);
            break;
        case 68:
            mapperImpl = std::make_shared<Mapper068>(prgBankCount, chrBankCount);
            break;
        case 69:
            mapperImpl = std::make_shared<Mapper069>(prgBankCount, chrBankCount);
            break;
        case 70:
            mapperImpl = std::make_shared<Mapper070>(prgBankCount, chrBankCount);
            break;
        case 71:
            mapperImpl = std::make_shared<Mapper071>(prgBankCount, chrBankCount);
            break;
        case 72:
            mapperImpl = std::make_shared<MapperJalecoSimple>(prgBankCount, chrBankCount, 72);
            break;
        case 73:
            mapperImpl = std::make_shared<Mapper073>(prgBankCount, chrBankCount);
            break;
        case 74:
            mapperImpl = std::make_shared<MapperMmc3Variant>(prgBankCount, chrBankCount, 74);
            break;
        case 75:
            mapperImpl = std::make_shared<Mapper075>(prgBankCount, chrBankCount);
            break;
        case 77:
            mapperImpl = std::make_shared<Mapper077>(prgBankCount, chrBankCount);
            break;
        case 78:
            mapperImpl = std::make_shared<Mapper078>(prgBankCount, chrBankCount);
            break;
        case 76:
            mapperImpl = std::make_shared<MapperNamco108>(prgBankCount, chrBankCount, 76);
            break;
        case 79:
            mapperImpl = std::make_shared<Mapper079>(prgBankCount, chrBankCount);
            break;
        case 80:
            mapperImpl = std::make_shared<Mapper080>(prgBankCount, chrBankCount);
            break;
        case 82:
            mapperImpl = std::make_shared<Mapper082>(prgBankCount, chrBankCount);
            break;
        case 83:
            mapperImpl = std::make_shared<Mapper083>(prgBankCount, chrBankCount);
            break;
        case 85:
            mapperImpl = std::make_shared<Mapper085>(prgBankCount, chrBankCount);
            break;
        case 86:
            mapperImpl = std::make_shared<MapperJalecoSimple>(prgBankCount, chrBankCount, 86);
            break;
        case 87:
            mapperImpl = std::make_shared<Mapper087>(prgBankCount, chrBankCount);
            break;
        case 88:
            mapperImpl = std::make_shared<MapperNamco108>(prgBankCount, chrBankCount, 88);
            break;
        case 89:
            mapperImpl = std::make_shared<MapperCommonBoards>(prgBankCount, chrBankCount, 89);
            break;
        case 90:
            mapperImpl = std::make_shared<MapperJY>(prgBankCount, chrBankCount, 90);
            break;
        case 91:
            mapperImpl = std::make_shared<Mapper091>(prgBankCount, chrBankCount);
            break;
        case 92:
            mapperImpl = std::make_shared<MapperJalecoSimple>(prgBankCount, chrBankCount, 92);
            break;
        case 93:
            mapperImpl = std::make_shared<MapperAdditionalBoards>(prgBankCount, chrBankCount, 93);
            break;
        case 94:
            mapperImpl = std::make_shared<MapperAdditionalBoards>(prgBankCount, chrBankCount, 94);
            break;
        case 95:
            mapperImpl = std::make_shared<MapperNamco108>(prgBankCount, chrBankCount, 95);
            break;
        case 96:
            mapperImpl = std::make_shared<MapperAdditionalBoards>(prgBankCount, chrBankCount, 96);
            break;
        case 97:
            mapperImpl = std::make_shared<Mapper097>(prgBankCount, chrBankCount);
            break;
        case 99:
            mapperImpl = std::make_shared<Mapper099>(prgBankCount, chrBankCount);
            break;
        case 105:
            mapperImpl = std::make_shared<Mapper105>(prgBankCount, chrBankCount);
            break;
        case 107:
            mapperImpl = std::make_shared<MapperAdditionalBoards>(prgBankCount, chrBankCount, 107);
            break;
        case 111:
            mapperImpl = std::make_shared<Mapper111>(prgBankCount, chrBankCount);
            break;
        case 112:
            mapperImpl = std::make_shared<MapperCommonBoards>(prgBankCount, chrBankCount, 112);
            break;
        case 113:
            mapperImpl = std::make_shared<Mapper113>(prgBankCount, chrBankCount);
            break;
        case 114:
        case 115:
            mapperImpl = std::make_shared<MapperMmc3Variant>(prgBankCount, chrBankCount, mapper);
            break;
        case 117:
            mapperImpl = std::make_shared<MapperAdditionalBoards>(prgBankCount, chrBankCount, 117);
            break;
        case 118:
            mapperImpl = std::make_shared<MapperMmc3Variant>(prgBankCount, chrBankCount, 118);
            break;
        case 119:
            mapperImpl = std::make_shared<MapperMmc3Variant>(prgBankCount, chrBankCount, 119);
            break;
        case 120:
            mapperImpl = std::make_shared<MapperAdditionalBoards>(prgBankCount, chrBankCount, 120);
            break;
        case 123:
            mapperImpl = std::make_shared<MapperMmc3Variant>(prgBankCount, chrBankCount, 123);
            break;
        case 128:
            mapperImpl = std::make_shared<MapperMmc3Variant>(prgBankCount, chrBankCount, 128);
            break;
        case 129:
            // Legacy iNES assignment: electrically the same board as mapper 58.
            mapperImpl = std::make_shared<MapperAdditionalBoards>(prgBankCount, chrBankCount, 58);
            break;
        case 130:
            mapperImpl = std::make_shared<Mapper130>(prgBankCount, chrBankCount);
            break;
        case 133:
            mapperImpl = std::make_shared<MapperAdditionalBoards>(prgBankCount, chrBankCount, 133);
            break;
        case 140:
            mapperImpl = std::make_shared<MapperJalecoSimple>(prgBankCount, chrBankCount, 140);
            break;
        case 142:
            mapperImpl = std::make_shared<MapperKaiser202>(prgBankCount, chrBankCount, 142);
            break;
        case 151:
            mapperImpl = std::make_shared<Mapper075>(prgBankCount, chrBankCount);
            break;
        case 160:
            mapperImpl = std::make_shared<Mapper160>(prgBankCount, chrBankCount);
            break;
        case 161:
            // Legacy iNES assignment: duplicate of MMC1 (mapper 1).
            mapperImpl = std::make_shared<Mapper001>(uint8_t(prgBankCount), uint8_t(chrBankCount));
            break;
        case 162:
            mapperImpl = std::make_shared<MapperAdditionalBoards>(prgBankCount, chrBankCount, 162);
            break;
        case 164:
            mapperImpl = std::make_shared<MapperAdditionalBoards>(prgBankCount, chrBankCount, 164);
            break;
        case 165:
            mapperImpl = std::make_shared<MapperMmc3Variant>(prgBankCount, chrBankCount, 165);
            break;
        case 166:
            mapperImpl = std::make_shared<MapperAdditionalBoards>(prgBankCount, chrBankCount, 166);
            break;
        case 180:
            mapperImpl = std::make_shared<Mapper180>(prgBankCount, chrBankCount);
            break;
        case 182:
            mapperImpl = std::make_shared<MapperMmc3Variant>(prgBankCount, chrBankCount, 182);
            break;
        case 184:
            mapperImpl = std::make_shared<Mapper184>(prgBankCount, chrBankCount);
            break;
        case 185:
            mapperImpl = std::make_shared<Mapper185>(prgBankCount, chrBankCount);
            break;
        case 187:
            mapperImpl = std::make_shared<MapperMmc3Variant>(prgBankCount, chrBankCount, 187);
            break;
        case 188:
            mapperImpl = std::make_shared<Mapper188>(prgBankCount, chrBankCount);
            break;
        case 189:
            mapperImpl = std::make_shared<MapperMmc3Variant>(prgBankCount, chrBankCount, 189);
            break;
        case 193:
            mapperImpl = std::make_shared<Mapper193>(prgBankCount, chrBankCount);
            break;
        case 195:
            mapperImpl = std::make_shared<MapperMmc3Variant>(prgBankCount, chrBankCount, 195);
            break;
        case 196:
            mapperImpl = std::make_shared<MapperMmc3Variant>(prgBankCount, chrBankCount, 196);
            break;
        case 199:
            mapperImpl = std::make_shared<MapperMmc3Variant>(prgBankCount, chrBankCount, 199);
            break;
        case 200:
            mapperImpl = std::make_shared<MapperCommonBoards>(prgBankCount, chrBankCount, 200);
            break;
        case 201:
            mapperImpl = std::make_shared<MapperCommonBoards>(prgBankCount, chrBankCount, 201);
            break;
        case 202:
            mapperImpl = std::make_shared<MapperCommonBoards>(prgBankCount, chrBankCount, 202);
            break;
        case 203:
            mapperImpl = std::make_shared<MapperCommonBoards>(prgBankCount, chrBankCount, 203);
            break;
        case 204:
            mapperImpl = std::make_shared<MapperCommonBoards>(prgBankCount, chrBankCount, 204);
            break;
        case 205:
            mapperImpl = std::make_shared<MapperMmc3Variant>(prgBankCount, chrBankCount, 205);
            break;
        case 210:
            mapperImpl = std::make_shared<Mapper210>(prgBankCount, chrBankCount, submapperNumber, batteryBacked);
            break;
        case 211:
            mapperImpl = std::make_shared<MapperJY>(prgBankCount, chrBankCount, 211);
            break;
        case 222:
            mapperImpl = std::make_shared<Mapper222>(prgBankCount, chrBankCount);
            break;
        case 224:
            mapperImpl = std::make_shared<MapperMmc3Variant>(prgBankCount, chrBankCount, 224);
            break;
        case 225:
            mapperImpl = std::make_shared<MapperMulticart>(prgBankCount, chrBankCount, 225);
            break;
        case 226:
            mapperImpl = std::make_shared<MapperMulticart>(prgBankCount, chrBankCount, 226);
            break;
        case 227:
            mapperImpl = std::make_shared<MapperMulticart>(prgBankCount, chrBankCount, 227);
            break;
        case 228:
            mapperImpl = std::make_shared<MapperMulticart>(prgBankCount, chrBankCount, 228);
            break;
        case 229:
            mapperImpl = std::make_shared<MapperMulticart>(prgBankCount, chrBankCount, 229);
            break;
        case 230:
            mapperImpl = std::make_shared<MapperMulticart>(prgBankCount, chrBankCount, 230);
            break;
        case 231:
            mapperImpl = std::make_shared<MapperMulticart>(prgBankCount, chrBankCount, 231);
            break;
        case 233:
            mapperImpl = std::make_shared<MapperMulticart>(prgBankCount, chrBankCount, 233);
            break;
        case 234:
            mapperImpl = std::make_shared<Mapper234>(prgBankCount, chrBankCount);
            break;
        case 240:
            mapperImpl = std::make_shared<MapperMulticart>(prgBankCount, chrBankCount, 240);
            break;
        case 241:
            mapperImpl = std::make_shared<MapperMulticart>(prgBankCount, chrBankCount, 241);
            break;
        case 242:
            mapperImpl = std::make_shared<MapperCommonBoards>(prgBankCount, chrBankCount, 242);
            break;
        case 243:
            mapperImpl = std::make_shared<MapperCommonBoards>(prgBankCount, chrBankCount, 243);
            break;
        case 244:
            mapperImpl = std::make_shared<MapperMulticart>(prgBankCount, chrBankCount, 244);
            break;
        case 245:
            mapperImpl = std::make_shared<MapperMmc3Variant>(prgBankCount, chrBankCount, 245);
            break;
        case 246:
            mapperImpl = std::make_shared<MapperCommonBoards>(prgBankCount, chrBankCount, 246);
            break;
        case 248:
            // Obsolete duplicate assignment; NESdev specifies mapper 115 hardware.
            mapperImpl = std::make_shared<MapperMmc3Variant>(prgBankCount, chrBankCount, 115);
            break;
        case 249:
            mapperImpl = std::make_shared<MapperMmc3Variant>(prgBankCount, chrBankCount, 249);
            break;
        case 250:
            mapperImpl = std::make_shared<MapperMmc3Variant>(prgBankCount, chrBankCount, 250);
            break;
        case 251:
            // Obsolete iNES assignment; same board/register wiring as mapper 45.
            mapperImpl = std::make_shared<MapperMmc3Variant>(prgBankCount, chrBankCount, 45);
            break;
        case 252:
            mapperImpl = std::make_shared<Mapper252>(prgBankCount, chrBankCount);
            break;
        case 254:
            mapperImpl = std::make_shared<MapperMmc3Variant>(prgBankCount, chrBankCount, 254);
            break;
        case 255:
            mapperImpl = std::make_shared<MapperMulticart>(prgBankCount, chrBankCount, 255);
            break;
        case 232:
            mapperImpl = std::make_shared<Mapper232>(prgBankCount, chrBankCount);
            break;
        default:
            std::cerr << "Unsupported mapper: " << int(mapper) << " (0x" << std::hex << int(mapper) << std::dec << ")\n";
            mapperImpl.reset();
            valid = false;
            return;
    }

    mapperCapabilities = mapperImpl->capabilities();
    mapperUsesCpuClock = mapperImpl->usesCpuClock();
    mapperUsesPpuClock = mapperImpl->usesPpuClock();
    mapperUsesPpuFetchHooks = (mapperCapabilities & MAPPER_CAP_PPU_FETCH_HOOKS) != 0;
    mapperCanAssertIrq = (mapperCapabilities & MAPPER_CAP_IRQ) != 0;
    mapperUsesScanline = (mapperCapabilities & MAPPER_CAP_SCANLINE) != 0;
    mapperUsesPpuAddressBus = (mapperCapabilities & MAPPER_CAP_PPU_ADDRESS_BUS) != 0;
    mapperControlsMirroring = (mapperCapabilities & MAPPER_CAP_MIRRORING) != 0;
    mapperHandlesExternalNametables = (mapperCapabilities &
        (MAPPER_CAP_NAMETABLE | MAPPER_CAP_ATTRIBUTE | MAPPER_CAP_PPU_DIRECT |
         MAPPER_CAP_PPU_ADDR_LATCH | MAPPER_CAP_PPU_OBSERVE)) != 0;
    mapperImpl->reset(true);

    // Setup and load/create SRAM save file if cartridge has battery-backed PRG-RAM.
    if (batteryBacked && !prgRam.empty()) {
        initSaveFilePath(saveIdentityPath.empty() ? nesFilePath : saveIdentityPath);
        loadOrCreateSave();
    }

    valid = true;
}


void Cartridge::resetMapper(bool hardReset) {
    if (mapperImpl) mapperImpl->reset(hardReset);
}

bool Cartridge::isDiskSystem() const { return mapperImpl && mapperImpl->isDiskSystem(); }
uint32_t Cartridge::diskSideCount() const { return mapperImpl ? mapperImpl->diskSideCount() : 0u; }
uint32_t Cartridge::currentDiskSide() const { return mapperImpl ? mapperImpl->currentDiskSide() : 0xFFFFFFFFu; }
bool Cartridge::insertDiskSide(uint32_t side) { return mapperImpl && mapperImpl->insertDiskSide(side); }
void Cartridge::ejectDisk() { if (mapperImpl) mapperImpl->ejectDisk(); }

Cartridge::~Cartridge() {
    // Persist SRAM on shutdown
    flushSave();
}

void Cartridge::initSaveFilePath(const std::string& nesFilePath) {
    // The frontend selects the platform-specific user-data directory.
    const std::string savesDir = "Saves";
    ensureDirExists(savesDir);

    const std::string stem = romStemFromPath(nesFilePath);
    // Save files are relative to the user-data directory, never the program.
    saveFilePath = savesDir + std::string(1, pathSep()) + stem + ".SAV";
}

void Cartridge::loadOrCreateSave() {
    if (!batteryBacked || prgRam.empty() || saveFilePath.empty()) return;

    std::ifstream probe(saveFilePath, std::ios::binary);
    const bool exists = probe.good();
    probe.close();

    if (exists) {
        std::ifstream ifs(saveFilePath.c_str(), std::ios::binary);
        if (ifs) {
            ifs.read(reinterpret_cast<char*>(prgRam.data()), std::streamsize(prgRam.size()));
            // If file shorter than PRG RAM, remainder stays 0x00.
        }
    } else {
        // Create an initial save file filled with the current PRG RAM contents (typically 0x00)
        std::ofstream ofs(saveFilePath.c_str(), std::ios::binary);
        if (ofs) {
            ofs.write(reinterpret_cast<const char*>(prgRam.data()), std::streamsize(prgRam.size()));
        }
    }

    prgRamDirty = false;
}

void Cartridge::flushSave() {
    if (mapperImpl) mapperImpl->flushPersistentData();
    if (!batteryBacked || prgRam.empty() || saveFilePath.empty()) return;

    // Even if we didn't detect changes, it is harmless to rewrite the file; but
    // we keep a dirty flag to reduce disk writes.
    if (!prgRamDirty) return;

    std::ofstream ofs(saveFilePath.c_str(), std::ios::binary | std::ios::trunc);
    if (ofs) {
        ofs.write(reinterpret_cast<const char*>(prgRam.data()), std::streamsize(prgRam.size()));
        prgRamDirty = false;
    }
}

Cartridge::Mirror Cartridge::mirroring() const {
    if (mapperImpl && (mapperCapabilities & MAPPER_CAP_MIRRORING)) {
        uint8_t ovr = mapperImpl->mirroringOverride();
        if (ovr != 0xFF) {
            if (ovr == 0) return Mirror::HORIZONTAL;
            if (ovr == 1) return Mirror::VERTICAL;
            if (ovr == 2) return Mirror::FOUR_SCREEN;
            if (ovr == 3) return Mirror::ONE_SCREEN_LO;
            if (ovr == 4) return Mirror::ONE_SCREEN_HI;
        }
    }
    return mirror;
}

bool Cartridge::irqState() const {
    return (mapperCapabilities & MAPPER_CAP_IRQ) && mapperImpl && mapperImpl->irqState();
}

void Cartridge::irqClear() {
    if (mapperImpl) mapperImpl->irqClear();
}

void Cartridge::scanline() {
    if ((mapperCapabilities & MAPPER_CAP_SCANLINE) && mapperImpl) mapperImpl->scanline();
}

bool Cartridge::usesCpuClock() const {
    return mapperUsesCpuClock;
}

void Cartridge::cpuClock() {
    if (mapperImpl) mapperImpl->cpuClock();
}

void Cartridge::cpuClockEx(uint64_t masterClock, uint16_t cpuAddressBus, bool cpuIsWriteCycle, bool dmaActive) {
    if (mapperImpl) mapperImpl->cpuClockEx(masterClock, cpuAddressBus, cpuIsWriteCycle, dmaActive);
}

bool Cartridge::usesPpuClock() const {
    return mapperUsesPpuClock;
}

void Cartridge::ppuClock(int16_t scanline, int16_t cycle, uint64_t masterClock) {
    if (mapperImpl) mapperImpl->ppuClock(scanline, cycle, masterClock);
}

void Cartridge::beforePpuFetch(uint16_t addr, uint8_t kind, int16_t scanline, int16_t cycle, uint64_t masterClock) {
    if (mapperImpl) mapperImpl->beforePpuFetch(addr, kind, scanline, cycle, masterClock);
}

void Cartridge::afterPpuFetch(uint16_t addr, uint8_t kind, int16_t scanline, int16_t cycle, uint64_t masterClock, uint8_t data) {
    if (mapperImpl) mapperImpl->afterPpuFetch(addr, kind, scanline, cycle, masterClock, data);
}

void Cartridge::ppuAddressBus(uint16_t addr) {
    if (mapperUsesPpuAddressBus && mapperImpl) mapperImpl->ppuAddressBus(addr);
}

bool Cartridge::setMmc3AlternateIrqRevision(bool alternate) {
    if (mapper != 4 || !mapperImpl) return false;
    mapperImpl->setMmc3AlternateIrqRevision(alternate);
    return true;
}

bool Cartridge::cpuRead(uint16_t addr, uint8_t& data) {
    if (!mapperImpl) return false;
    const bool observe = (mapperCapabilities & MAPPER_CAP_CPU_OBSERVE) != 0;
    if (observe) mapperImpl->beforeCpuRead(addr);

    if ((mapperCapabilities & MAPPER_CAP_CPU_DIRECT) && mapperImpl->cpuReadDirect(addr, data)) {
        if (mapperCapabilities & MAPPER_CAP_CPU_READ_FILTER) mapperImpl->cpuFilterRead(addr, data);
        if (observe) mapperImpl->afterCpuRead(addr, data);
        return true;
    }

    uint32_t mapped = 0;
    if (mapperImpl->cpuMapRead(addr, mapped)) {
        if (mapped & 0x80000000u) {
            uint32_t idx = mapped & 0x7FFFFFFFu;
            data = (idx < prgRam.size()) ? prgRam[idx] : 0x00;
        } else {
            data = (mapped < prgMemory.size()) ? prgMemory[mapped] : 0x00;
        }
        if (mapperCapabilities & MAPPER_CAP_CPU_READ_FILTER) mapperImpl->cpuFilterRead(addr, data);
        if (observe) mapperImpl->afterCpuRead(addr, data);
        return true;
    }
    return false;
}

bool Cartridge::cpuWrite(uint16_t addr, uint8_t data) {
    if (!mapperImpl) return false;
    const bool observe = (mapperCapabilities & MAPPER_CAP_CPU_OBSERVE) != 0;
    if (observe) mapperImpl->beforeCpuWrite(addr, data);

    if ((mapperCapabilities & MAPPER_CAP_CPU_DIRECT) && mapperImpl->cpuWriteDirect(addr, data)) {
        if (observe) mapperImpl->afterCpuWrite(addr, data);
        return true;
    }

    uint8_t mapperData = data;
    if ((mapperCapabilities & MAPPER_CAP_BUS_CONFLICT) && addr >= 0x8000u) {
        uint32_t conflictMapped = 0;
        if (mapperImpl->cpuMapRead(addr, conflictMapped) &&
            (conflictMapped & Mapper::PRG_RAM_FLAG) == 0 && conflictMapped < prgMemory.size()) {
            mapperData = uint8_t(mapperData & prgMemory[conflictMapped]);
        }
    }

    uint32_t mapped = 0;
    if (mapperImpl->cpuMapWrite(addr, mapped, mapperData)) {
        if (mapped & 0x80000000u) {
            uint32_t idx = mapped & 0x7FFFFFFFu;
            if (idx < prgRam.size()) {
                prgRam[idx] = data;
                if (batteryBacked) prgRamDirty = true;
            }
        } else {
            // Writes to PRG ROM are ignored.
            (void)data;
        }
        if (observe) mapperImpl->afterCpuWrite(addr, data);
        return true;
    }
    return false;
}

bool Cartridge::ppuRead(uint16_t addr, uint8_t& data) {
    if (!mapperImpl) return false;
    const bool observe = (mapperCapabilities & MAPPER_CAP_PPU_OBSERVE) != 0;
    if (observe) mapperImpl->beforePpuRead(addr);

    if ((mapperCapabilities & MAPPER_CAP_ATTRIBUTE) && (addr & 0x3FFFu) >= 0x2000u &&
        (addr & 0x03FFu) >= 0x03C0u && mapperImpl->attributeRead(addr, data)) {
        if (observe) mapperImpl->afterPpuRead(addr, data);
        return true;
    }
    if ((mapperCapabilities & MAPPER_CAP_NAMETABLE) && (addr & 0x3FFFu) >= 0x2000u &&
        mapperImpl->nametableRead(addr, data)) {
        if (observe) mapperImpl->afterPpuRead(addr, data);
        return true;
    }

    if ((mapperCapabilities & MAPPER_CAP_PPU_DIRECT) && mapperImpl->ppuReadDirect(addr, data)) {
        if (observe) mapperImpl->afterPpuRead(addr, data);
        return true;
    }

    uint32_t mapped = 0;
    if (mapperCapabilities & MAPPER_CAP_PPU_ADDR_LATCH) mapperImpl->ppuAddressLatch(addr);
    if (mapperImpl->ppuMapRead(addr, mapped)) {
        if (mapped & Mapper::CHR_RAM_FLAG) {
            const uint32_t idx = mapped & 0x3FFFFFFFu;
            data = (idx < chrRam.size()) ? chrRam[idx] : 0x00;
        } else {
            data = (mapped < chrMemory.size()) ? chrMemory[mapped] : 0x00;
        }
        if (observe) mapperImpl->afterPpuRead(addr, data);
        return true;
    }
    return false;
}

bool Cartridge::ppuWrite(uint16_t addr, uint8_t data) {
    if (!mapperImpl) return false;
    const bool observe = (mapperCapabilities & MAPPER_CAP_PPU_OBSERVE) != 0;
    if (observe) mapperImpl->beforePpuWrite(addr, data);

    if ((mapperCapabilities & MAPPER_CAP_ATTRIBUTE) && (addr & 0x3FFFu) >= 0x2000u &&
        (addr & 0x03FFu) >= 0x03C0u && mapperImpl->attributeWrite(addr, data)) {
        if (observe) mapperImpl->afterPpuWrite(addr, data);
        return true;
    }
    if ((mapperCapabilities & MAPPER_CAP_NAMETABLE) && (addr & 0x3FFFu) >= 0x2000u &&
        mapperImpl->nametableWrite(addr, data)) {
        if (observe) mapperImpl->afterPpuWrite(addr, data);
        return true;
    }

    if ((mapperCapabilities & MAPPER_CAP_PPU_DIRECT) && mapperImpl->ppuWriteDirect(addr, data)) {
        if (observe) mapperImpl->afterPpuWrite(addr, data);
        return true;
    }

    uint32_t mapped = 0;
    if (mapperCapabilities & MAPPER_CAP_PPU_ADDR_LATCH) mapperImpl->ppuAddressLatch(addr);
    if (mapperImpl->ppuMapWrite(addr, mapped, data)) {
        if (mapped & Mapper::CHR_RAM_FLAG) {
            const uint32_t idx = mapped & 0x3FFFFFFFu;
            if (idx < chrRam.size()) chrRam[idx] = data;
        } else if (mapped < chrMemory.size()) {
            chrMemory[mapped] = data;
        }
        if (observe) mapperImpl->afterPpuWrite(addr, data);
        return true;
    }
    return false;
}

void Cartridge::setExpansionAudioSampleRate(double sampleRateHz) {
    if (mapperImpl) mapperImpl->setAudioSampleRate(sampleRateHz);
}

void Cartridge::processCartridgeAudioPath(float* samples, int count) {
    if (!samples || count <= 0 || !mapperImpl || !(mapperCapabilities & MAPPER_CAP_EXPANSION_AUDIO) || !mapperImpl->usesExpansionAudio()) return;
    for (int i = 0; i < count; ++i) {
        samples[i] += mapperImpl->audioSample();
        if (samples[i] > 1.0f) samples[i] = 1.0f;
        else if (samples[i] < -1.0f) samples[i] = -1.0f;
    }
}

void Cartridge::processCartridgeAudioPathI16(int16_t* samples, int count) {
    if (!samples || count <= 0 || !mapperImpl || !(mapperCapabilities & MAPPER_CAP_EXPANSION_AUDIO) || !mapperImpl->usesExpansionAudio()) return;
    for (int i = 0; i < count; ++i) {
        int v = (int)samples[i] + (int)(mapperImpl->audioSample() * 32767.0f);
        if (v > 32767) v = 32767;
        else if (v < -32768) v = -32768;
        samples[i] = (int16_t)v;
    }
}

std::string Cartridge::mapperDebugText() const {
    return mapperImpl ? mapperImpl->debugText() : std::string();
}

bool Cartridge::mapperNametableRead(uint16_t addr, uint8_t& data) { return mapperImpl ? mapperImpl->nametableRead(addr, data) : false; }
bool Cartridge::mapperNametableWrite(uint16_t addr, uint8_t data) { return mapperImpl ? mapperImpl->nametableWrite(addr, data) : false; }

// ------------------------------
// Save-state support
// ------------------------------
void Cartridge::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    write_bool(out, valid);
    write_u16(out, mapper);
    write_u16(out, prgBankCount);
    write_u16(out, chrBankCount);
    write_u8(out, (uint8_t)mirror);

    // Mutable cartridge memory
    write_vec(out, prgRam);
    write_vec(out, chrMemory);
    write_vec(out, chrRam);

    write_bool(out, batteryBacked);
    write_bool(out, prgRamDirty);

    // Mapper internal state
    if (mapperImpl) {
        write_bool(out, true);
        mapperImpl->saveState(out);
    } else {
        write_bool(out, false);
    }
}

bool Cartridge::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    if (!read_bool(p,end,valid)) return false;
    if (!read_u16(p,end,mapper)) return false;
    if (!read_u16(p,end,prgBankCount)) return false;
    if (!read_u16(p,end,chrBankCount)) return false;
    uint8_t mir=0; if (!read_u8(p,end,mir)) return false; mirror = (Mirror)mir;

    if (!read_vec(p,end,prgRam)) return false;
    if (!read_vec(p,end,chrMemory)) return false;
    if (!read_vec(p,end,chrRam)) return false;

    if (!read_bool(p,end,batteryBacked)) return false;
    if (!read_bool(p,end,prgRamDirty)) return false;

    bool hasMapper=false; if(!read_bool(p,end,hasMapper)) return false;
    if (hasMapper) {
        if (!mapperImpl) return false;
        if (!mapperImpl->loadState(p,end)) return false;
    }
    return true;
}
