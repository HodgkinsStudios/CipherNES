#pragma once

#include <cstdint>
#include "Mapper.h"

// Mapper 2 (UxROM)
class Mapper002 final : public Mapper {
public:
    Mapper002(uint8_t prgBanks, uint8_t chrBanks, uint8_t submapper = 0) : Mapper(prgBanks, chrBanks), submapper(submapper) { reset(); }

    uint32_t capabilities() const override { return (submapper == 2) ? MAPPER_CAP_BUS_CONFLICT : 0u; }
    void reset() override;

    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;

private:
    const uint8_t submapper;
    uint8_t prgBankSelect = 0;
};
