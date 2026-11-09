#pragma once

#include <string>
#include <vector>

struct ZipRomLoadResult {
    std::vector<unsigned char> romData;
    std::string entryName;
    std::string notice;
};

// Loads one valid iNES/NES 2.0 ROM from a ZIP archive. The loader parses the
// central directory, supports stored and DEFLATE entries, rejects encrypted or
// ZIP64 archives, validates CRC-32, and never writes archive paths to disk.
// Loads the first valid NES/FDS/QD image from a ZIP archive.
bool loadNesRomFromZip(const std::string& archivePath, ZipRomLoadResult& result, std::string& errorText);
