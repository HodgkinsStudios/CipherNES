#include "game_genie.h"

#include <algorithm>
#include <cctype>
#include <cstring>

static int gg_char_to_nibble(char c) {
    // NES Game Genie alphabet
    // A P Z L G I T Y E O X U K S V N
    static const char* alpha = "APZLGITYEOXUKSVN";
    c = (char)std::toupper((unsigned char)c);
    const char* p = std::strchr(alpha, c);
    if (!p) return -1;
    return (int)(p - alpha);
}

bool parse_nes_game_genie(const std::string& code_in, GameGeniePatch& out) {
    std::string code;
    code.reserve(code_in.size());
    for (char c : code_in) {
        if (c == '-' || c == ' ') continue;
        code.push_back((char)std::toupper((unsigned char)c));
    }

    if (!(code.size() == 6 || code.size() == 8)) return false;

    int n[8] = {0};
    for (size_t i = 0; i < code.size(); i++) {
        int v = gg_char_to_nibble(code[i]);
        if (v < 0) return false;
        n[i] = v;
    }

    // Address bit scrambling per NES Game Genie spec
    uint16_t addr = 0x8000;
    addr |= (uint16_t)(n[3] & 0x07) << 12;
    addr |= (uint16_t)(n[5] & 0x07) << 8;
    addr |= (uint16_t)(n[4] & 0x08) << 8;
    addr |= (uint16_t)(n[2] & 0x07) << 4;
    addr |= (uint16_t)(n[1] & 0x08) << 4;
    addr |= (uint16_t)(n[4] & 0x07);
    addr |= (uint16_t)(n[3] & 0x08);

    uint8_t value = 0;
    value |= (uint8_t)(n[1] & 0x07) << 4;
    value |= (uint8_t)(n[0] & 0x08) << 4;
    value |= (uint8_t)(n[0] & 0x07);

    bool hasCompare = (code.size() == 8);
    uint8_t compare = 0;

    if (!hasCompare) {
        value |= (uint8_t)(n[5] & 0x08);
    } else {
        // 8-char codes supply a compare byte; the data's bit 3 comes from n[7]
        value |= (uint8_t)(n[7] & 0x08);

        compare |= (uint8_t)(n[7] & 0x07) << 4;
        compare |= (uint8_t)(n[6] & 0x08) << 4;
        compare |= (uint8_t)(n[6] & 0x07);
        compare |= (uint8_t)(n[5] & 0x08);
    }

    out.addr = addr;
    out.value = value;
    out.hasCompare = hasCompare;
    out.compare = compare;
    return true;
}
