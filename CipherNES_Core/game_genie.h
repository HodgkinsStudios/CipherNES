#pragma once
#include <cstdint>
#include <string>

struct GameGeniePatch {
    uint16_t addr = 0;
    uint8_t  value = 0;
    bool     hasCompare = false;
    uint8_t  compare = 0;
};

// Parses NES Game Genie code (6 or 8 chars). Returns true on success.
bool parse_nes_game_genie(const std::string& code, GameGeniePatch& out);
