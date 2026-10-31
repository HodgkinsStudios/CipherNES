#include "zip_rom_loader.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include "../CipherNES_Core/platform_compat.h"
#include <fstream>
#include <limits>
#include <sstream>

namespace {

const uint32_t ZIP_LOCAL_SIGNATURE = 0x04034B50u;
const uint32_t ZIP_CENTRAL_SIGNATURE = 0x02014B50u;
const uint32_t ZIP_EOCD_SIGNATURE = 0x06054B50u;
const size_t ZIP_MAX_ROM_BYTES = 512u * 1024u * 1024u;

uint16_t read16(const unsigned char* p) {
    return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
}

uint32_t read32(const unsigned char* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

std::string lowerText(std::string value) {
    for (size_t i = 0; i < value.size(); ++i) value[i] = char(std::tolower(static_cast<unsigned char>(value[i])));
    return value;
}

std::string baseName(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string stemName(const std::string& path) {
    std::string value = baseName(path);
    const size_t dot = value.find_last_of('.');
    if (dot != std::string::npos) value.resize(dot);
    return value;
}

bool hasSuffix(const std::string& value, const char* suffix) {
    const std::string lower = lowerText(value);
    const std::string ending = suffix;
    return lower.size() >= ending.size() && lower.compare(lower.size() - ending.size(), ending.size(), ending) == 0;
}

bool isSupportedRomName(const std::string& name) {
    return hasSuffix(name, ".nes") || hasSuffix(name, ".fds") || hasSuffix(name, ".qd");
}

bool isInesImage(const std::vector<unsigned char>& data) {
    return data.size() >= 16 && data[0] == 'N' && data[1] == 'E' && data[2] == 'S' && data[3] == 0x1A;
}

bool isFdsImage(const std::vector<unsigned char>& data) {
    if (data.size() >= 16 && data[0] == 'F' && data[1] == 'D' && data[2] == 'S' && data[3] == 0x1A) {
        const size_t sides = data[4];
        return sides > 0 && data.size() >= 16u + sides * 65500u;
    }
    return !data.empty() && (data.size() % 65500u) == 0;
}

bool isQdImage(const std::vector<unsigned char>& data) {
    return !data.empty() && (data.size() % 65536u) == 0;
}

bool isSupportedRomImage(const std::string& name, const std::vector<unsigned char>& data) {
    if (hasSuffix(name, ".nes")) return isInesImage(data);
    if (hasSuffix(name, ".fds")) return isFdsImage(data);
    if (hasSuffix(name, ".qd")) return isQdImage(data);
    return false;
}

std::vector<unsigned char> readFile(const std::string& path, std::string& errorText) {
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) {
        errorText = "Could not open ZIP archive.";
        return std::vector<unsigned char>();
    }
    in.seekg(0, std::ios::end);
    const std::streamoff length = in.tellg();
    in.seekg(0, std::ios::beg);
    if (length < 22 || uint64_t(length) > uint64_t(std::numeric_limits<size_t>::max())) {
        errorText = "ZIP archive is empty or too large.";
        return std::vector<unsigned char>();
    }
    std::vector<unsigned char> bytes(static_cast<size_t>(length));
    in.read(reinterpret_cast<char*>(&bytes[0]), length);
    if (!in) {
        errorText = "Could not read the complete ZIP archive.";
        bytes.clear();
    }
    return bytes;
}

uint32_t crc32Bytes(const unsigned char* data, size_t size) {
    static uint32_t table[256];
    static bool initialized = false;
    if (!initialized) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int bit = 0; bit < 8; ++bit) c = (c >> 1) ^ (0xEDB88320u & uint32_t(0u - (c & 1u)));
            table[i] = c;
        }
        initialized = true;
    }
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i) crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

// Minimal ABI-compatible zlib declarations. CipherNES dynamically loads zlib so
// building the frontend does not require the zlib development headers/package.
typedef unsigned char ZByte;
typedef unsigned int ZUInt;
typedef unsigned long ZULong;
typedef void* ZVoidPtr;
typedef ZVoidPtr (*ZAllocFunc)(ZVoidPtr, ZUInt, ZUInt);
typedef void (*ZFreeFunc)(ZVoidPtr, ZVoidPtr);
struct ZInternalState;
struct ZStream {
    ZByte* next_in;
    ZUInt avail_in;
    ZULong total_in;
    ZByte* next_out;
    ZUInt avail_out;
    ZULong total_out;
    char* msg;
    ZInternalState* state;
    ZAllocFunc zalloc;
    ZFreeFunc zfree;
    ZVoidPtr opaque;
    int data_type;
    ZULong adler;
    ZULong reserved;
};

class ZlibApi {
public:
    ZlibApi() : library(NULL), version(NULL), inflateInit2_(NULL), inflate(NULL), inflateEnd(NULL) {}
    ~ZlibApi() { if (library) dlclose(library); }

    bool load(std::string& errorText) {
#if defined(_WIN32)
        library = dlopen("zlib1.dll", RTLD_NOW | RTLD_LOCAL);
        if (!library) library = dlopen("zlib.dll", RTLD_NOW | RTLD_LOCAL);
#else
        library = dlopen("libz.so.1", RTLD_NOW | RTLD_LOCAL);
        if (!library) library = dlopen("libz.so", RTLD_NOW | RTLD_LOCAL);
#endif
        if (!library) {
#if defined(_WIN32)
            errorText = "This compressed ZIP needs zlib1.dll (or zlib.dll).";
#else
            errorText = "This compressed ZIP needs the zlib runtime (libz.so.1).";
#endif
            return false;
        }
        version = reinterpret_cast<const char* (*)()>(dlsym(library, "zlibVersion"));
        inflateInit2_ = reinterpret_cast<int (*)(ZStream*, int, const char*, int)>(dlsym(library, "inflateInit2_"));
        inflate = reinterpret_cast<int (*)(ZStream*, int)>(dlsym(library, "inflate"));
        inflateEnd = reinterpret_cast<int (*)(ZStream*)>(dlsym(library, "inflateEnd"));
        if (!version || !inflateInit2_ || !inflate || !inflateEnd) {
            errorText = "The installed zlib runtime is missing required inflate functions.";
            return false;
        }
        return true;
    }

    bool rawInflate(const unsigned char* input, size_t inputSize, std::vector<unsigned char>& output, size_t outputSize, std::string& errorText) {
        if (inputSize > std::numeric_limits<ZUInt>::max() || outputSize > std::numeric_limits<ZUInt>::max()) {
            errorText = "Compressed ROM entry exceeds the supported ZIP size.";
            return false;
        }
        if (!library && !load(errorText)) return false;
        output.assign(outputSize, 0);
        if (outputSize == 0) return inputSize == 0;

        ZStream stream;
        std::memset(&stream, 0, sizeof(stream));
        stream.next_in = const_cast<ZByte*>(reinterpret_cast<const ZByte*>(input));
        stream.avail_in = static_cast<ZUInt>(inputSize);
        stream.next_out = reinterpret_cast<ZByte*>(&output[0]);
        stream.avail_out = static_cast<ZUInt>(outputSize);

        const int Z_OK_VALUE = 0;
        const int Z_STREAM_END_VALUE = 1;
        const int Z_FINISH_VALUE = 4;
        int rc = inflateInit2_(&stream, -15, version(), int(sizeof(stream)));
        if (rc != Z_OK_VALUE) {
            errorText = "zlib could not initialize raw DEFLATE decoding.";
            output.clear();
            return false;
        }
        rc = inflate(&stream, Z_FINISH_VALUE);
        inflateEnd(&stream);
        if (rc != Z_STREAM_END_VALUE || stream.total_out != outputSize || stream.total_in != inputSize) {
            errorText = "The ZIP ROM entry has invalid or truncated DEFLATE data.";
            output.clear();
            return false;
        }
        return true;
    }

private:
    void* library;
    const char* (*version)();
    int (*inflateInit2_)(ZStream*, int, const char*, int);
    int (*inflate)(ZStream*, int);
    int (*inflateEnd)(ZStream*);
};

struct Entry {
    std::string name;
    uint16_t flags;
    uint16_t method;
    uint32_t crc;
    uint32_t compressedSize;
    uint32_t uncompressedSize;
    uint32_t localOffset;
};

bool parseEntries(const std::vector<unsigned char>& archive, std::vector<Entry>& entries, std::string& errorText) {
    if (archive.size() < 22) {
        errorText = "ZIP archive is too short.";
        return false;
    }
    const size_t searchFloor = archive.size() > 65557u ? archive.size() - 65557u : 0u;
    size_t eocd = std::string::npos;
    for (size_t pos = archive.size() - 22u;; --pos) {
        if (read32(&archive[pos]) == ZIP_EOCD_SIGNATURE) {
            const uint16_t commentLength = read16(&archive[pos + 20]);
            if (pos + 22u + commentLength == archive.size()) { eocd = pos; break; }
        }
        if (pos == searchFloor) break;
    }
    if (eocd == std::string::npos) {
        errorText = "ZIP end-of-central-directory record was not found.";
        return false;
    }

    const uint16_t diskNumber = read16(&archive[eocd + 4]);
    const uint16_t centralDisk = read16(&archive[eocd + 6]);
    const uint16_t entriesOnDisk = read16(&archive[eocd + 8]);
    const uint16_t entryCount = read16(&archive[eocd + 10]);
    const uint32_t centralSize = read32(&archive[eocd + 12]);
    const uint32_t centralOffset = read32(&archive[eocd + 16]);
    if (diskNumber != 0 || centralDisk != 0 || entriesOnDisk != entryCount) {
        errorText = "Multi-disk ZIP archives are not supported.";
        return false;
    }
    if (entryCount == 0xFFFFu || centralSize == 0xFFFFFFFFu || centralOffset == 0xFFFFFFFFu) {
        errorText = "ZIP64 archives are not supported for ROM loading.";
        return false;
    }
    if (uint64_t(centralOffset) + centralSize > archive.size()) {
        errorText = "ZIP central directory points outside the archive.";
        return false;
    }

    size_t pos = centralOffset;
    for (uint32_t index = 0; index < entryCount; ++index) {
        if (pos + 46u > archive.size() || read32(&archive[pos]) != ZIP_CENTRAL_SIGNATURE) {
            errorText = "ZIP central directory is malformed.";
            return false;
        }
        const uint16_t nameLength = read16(&archive[pos + 28]);
        const uint16_t extraLength = read16(&archive[pos + 30]);
        const uint16_t commentLength = read16(&archive[pos + 32]);
        const uint16_t startDisk = read16(&archive[pos + 34]);
        const size_t next = pos + 46u + nameLength + extraLength + commentLength;
        if (next > archive.size() || startDisk != 0) {
            errorText = "ZIP central-directory entry is invalid.";
            return false;
        }
        Entry entry;
        entry.flags = read16(&archive[pos + 8]);
        entry.method = read16(&archive[pos + 10]);
        entry.crc = read32(&archive[pos + 16]);
        entry.compressedSize = read32(&archive[pos + 20]);
        entry.uncompressedSize = read32(&archive[pos + 24]);
        entry.localOffset = read32(&archive[pos + 42]);
        entry.name.assign(reinterpret_cast<const char*>(&archive[pos + 46]), nameLength);
        if (isSupportedRomName(entry.name) && !entry.name.empty() && entry.name[entry.name.size() - 1] != '/') entries.push_back(entry);
        pos = next;
    }
    if (entries.empty()) {
        errorText = "The ZIP archive does not contain a supported .nes, .fds, or .qd image.";
        return false;
    }
    return true;
}

bool extractEntry(const std::vector<unsigned char>& archive, const Entry& entry, ZlibApi& zlib,
                  std::vector<unsigned char>& output, std::string& errorText) {
    if (entry.flags & 0x0001u) {
        errorText = "Encrypted ZIP ROM entries are not supported.";
        return false;
    }
    if (entry.method != 0 && entry.method != 8) {
        std::ostringstream message;
        message << "ZIP compression method " << entry.method << " is not supported.";
        errorText = message.str();
        return false;
    }
    if (entry.uncompressedSize > ZIP_MAX_ROM_BYTES) {
        errorText = "The ROM inside this ZIP exceeds the 512 MB safety limit.";
        return false;
    }
    const size_t local = entry.localOffset;
    if (local + 30u > archive.size() || read32(&archive[local]) != ZIP_LOCAL_SIGNATURE) {
        errorText = "ZIP local file header is missing or invalid.";
        return false;
    }
    const uint16_t localFlags = read16(&archive[local + 6]);
    const uint16_t localMethod = read16(&archive[local + 8]);
    const uint16_t nameLength = read16(&archive[local + 26]);
    const uint16_t extraLength = read16(&archive[local + 28]);
    if ((localFlags & 0x0001u) || localMethod != entry.method) {
        errorText = "ZIP local and central entry metadata do not agree.";
        return false;
    }
    const uint64_t dataOffset64 = uint64_t(local) + 30u + nameLength + extraLength;
    const uint64_t dataEnd64 = dataOffset64 + entry.compressedSize;
    if (dataEnd64 > archive.size()) {
        errorText = "ZIP ROM data points outside the archive.";
        return false;
    }
    const unsigned char* compressed = &archive[static_cast<size_t>(dataOffset64)];
    if (entry.method == 0) {
        if (entry.compressedSize != entry.uncompressedSize) {
            errorText = "Stored ZIP entry has inconsistent sizes.";
            return false;
        }
        output.assign(compressed, compressed + entry.uncompressedSize);
    } else if (!zlib.rawInflate(compressed, entry.compressedSize, output, entry.uncompressedSize, errorText)) {
        return false;
    }
    if (crc32Bytes(output.empty() ? NULL : &output[0], output.size()) != entry.crc) {
        output.clear();
        errorText = "ZIP ROM entry failed CRC-32 validation.";
        return false;
    }
    return true;
}

} // namespace

bool loadNesRomFromZip(const std::string& archivePath, ZipRomLoadResult& result, std::string& errorText) {
    result = ZipRomLoadResult();
    errorText.clear();
    const std::vector<unsigned char> archive = readFile(archivePath, errorText);
    if (archive.empty()) return false;

    std::vector<Entry> entries;
    if (!parseEntries(archive, entries, errorText)) return false;

    const std::string archiveStem = lowerText(stemName(archivePath));
    std::stable_sort(entries.begin(), entries.end(), [&archiveStem](const Entry& a, const Entry& b) {
        const bool aMatch = lowerText(stemName(a.name)) == archiveStem;
        const bool bMatch = lowerText(stemName(b.name)) == archiveStem;
        if (aMatch != bMatch) return aMatch > bMatch;
        return lowerText(a.name) < lowerText(b.name);
    });

    ZlibApi zlib;
    std::string lastError;
    for (size_t i = 0; i < entries.size(); ++i) {
        std::vector<unsigned char> rom;
        std::string entryError;
        if (!extractEntry(archive, entries[i], zlib, rom, entryError)) {
            lastError = entryError;
            continue;
        }
        if (!isSupportedRomImage(entries[i].name, rom)) {
            lastError = "A ROM entry did not match its expected NES/FDS/QD format.";
            continue;
        }
        result.romData.swap(rom);
        result.entryName = entries[i].name;
        if (entries.size() > 1) {
            std::ostringstream notice;
            notice << "ZIP HAS " << entries.size() << " ROMS - LOADED " << baseName(result.entryName);
            result.notice = notice.str();
        }
        return true;
    }
    errorText = lastError.empty() ? "No valid NES/FDS/QD image could be extracted from the ZIP archive." : lastError;
    return false;
}
