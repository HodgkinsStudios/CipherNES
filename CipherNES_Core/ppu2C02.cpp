#include "ppu2C02.h"
#include "cartridge.h"

#include "state_io.h"

void PPU2C02::connectCartridge(const std::shared_ptr<Cartridge>& cart) {
    cartridge = cart;
    cartridgeRaw = cart.get();
    cartridgeUsesFetchHooks = cart && cart->usesPpuFetchHooks();
    cartridgeUsesScanlineHook = cart && cart->usesScanlineHook();
    cartridgeUsesPpuAddressBus = cart && cart->usesPpuAddressBus();
    cartridgeHandlesExternalNametables = cart && cart->handlesExternalNametables();
    cartridgeUsesDynamicMirroring = cart && cart->hasDynamicMirroring();
    cartridgeFixedMirroring = cart ? static_cast<uint8_t>(cart->mirroring())
                                   : static_cast<uint8_t>(Cartridge::Mirror::VERTICAL);

    // Decode fixed mirroring once at cartridge insertion rather than branching
    // through the mirror mode for every nametable byte fetched by the PPU.
    switch (static_cast<Cartridge::Mirror>(cartridgeFixedMirroring)) {
        case Cartridge::Mirror::HORIZONTAL:
            cartridgeFixedNametableMap = {{0, 0, 1, 1}};
            break;
        case Cartridge::Mirror::VERTICAL:
            cartridgeFixedNametableMap = {{0, 1, 0, 1}};
            break;
        case Cartridge::Mirror::ONE_SCREEN_LO:
            cartridgeFixedNametableMap = {{0, 0, 0, 0}};
            break;
        case Cartridge::Mirror::ONE_SCREEN_HI:
            cartridgeFixedNametableMap = {{1, 1, 1, 1}};
            break;
        case Cartridge::Mirror::FOUR_SCREEN:
        default:
            cartridgeFixedNametableMap = {{0, 1, 2, 3}};
            break;
    }
}

#include <algorithm>

#if defined(CIPHERNES_DEBUG_PPUSTATUS) || defined(CIPHERNES_DEBUG_OAMDATA) || defined(CIPHERNES_DEBUG_SPRITE0)
#include <cstdio>
#endif

namespace {
// NES master palette (approximate). Values are 0xRRGGBB.
static constexpr uint32_t kNesPalette[64] = {
    0x545454, 0x001E74, 0x081090, 0x300088, 0x440064, 0x5C0030, 0x540400, 0x3C1800,
    0x202A00, 0x083A00, 0x004000, 0x003C00, 0x00323C, 0x000000, 0x000000, 0x000000,
    0x989698, 0x084CC4, 0x3032EC, 0x5C1EE4, 0x8814B0, 0xA01464, 0x982220, 0x783C00,
    0x545A00, 0x287200, 0x087C00, 0x007628, 0x006678, 0x000000, 0x000000, 0x000000,
    0xECEEEC, 0x4C9AEC, 0x787CEC, 0xB062EC, 0xE454EC, 0xEC58B4, 0xEC6A64, 0xD48820,
    0xA0AA00, 0x74C400, 0x4CD020, 0x38CC6C, 0x38B4CC, 0x3C3C3C, 0x000000, 0x000000,
    0xECEEEC, 0xA8CCEC, 0xBCBCEC, 0xD4B2EC, 0xECB0EC, 0xECB4CC, 0xECC4B0, 0xE4D4A8,
    0xCCDCA0, 0xB4E4A8, 0xA8E4B4, 0xA0DCC8, 0xA0D0E4, 0xA0A2A0, 0x000000, 0x000000,
};

static inline uint16_t mirrorPaletteAddr(uint16_t addr) {
    addr &= 0x3FFF;
    if (addr >= 0x3F20) addr = 0x3F00 + (addr & 0x001F);
    // Palette special mirrors ($3F10/$3F14/$3F18/$3F1C -> $3F00/$3F04/$3F08/$3F0C)
    if (addr == 0x3F10) addr = 0x3F00;
    if (addr == 0x3F14) addr = 0x3F04;
    if (addr == 0x3F18) addr = 0x3F08;
    if (addr == 0x3F1C) addr = 0x3F0C;
    return addr;
}

}

static const uint32_t PPU_OPEN_BUS_DECAY_CYCLES = 341u * 262u * 30u;

void PPU2C02::driveOpenBus(uint8_t value, uint8_t mask) {
    ppu_open_bus = uint8_t((ppu_open_bus & uint8_t(~mask)) | (value & mask));
    for (unsigned bit = 0; bit < 8; ++bit) {
        const uint8_t bitMask = uint8_t(1u << bit);
        if (mask & bitMask) ppu_open_bus_bit_idle_cycles[bit] = 0;
    }
    ppu_open_bus_decay_active_bits = uint8_t(ppu_open_bus_decay_active_bits | mask);
    ppu_open_bus_idle_cycles = ppu_open_bus_bit_idle_cycles[0];
}

uint8_t PPU2C02::currentOpenBus() const {
    // Expired bits are cleared at their exact decay clock below, so the stored
    // bus value is already the externally visible value.
    return ppu_open_bus;
}

uint8_t PPU2C02::readOAM(uint8_t index) const {
    uint8_t data = oam[index];
    if ((index & 0x03) == 0x02) {
        data &= 0xE3;
    }
    return data;
}

bool PPU2C02::oamWriteBlockedByRendering() const {
    return renderingEnabled() && ((scanline >= 0 && scanline < 240) || scanline == 261);
}

uint8_t PPU2C02::cpuReadOAMData() const {
    if (renderingEnabled() && scanline >= 0 && scanline < 240) {
        // Bus::clock() advances the PPU before the CPU performs its bus
        // operation.  OAMDATA is sampled at the end of the CPU read cycle,
        // which corresponds to the OAM evaluation latch from the preceding
        // PPU dot in this scheduler.
        return oam_read_buffer_prev;
    }
    return readOAM(oam_addr);
}

// ------------------------------------------------------------
// Reset
// ------------------------------------------------------------

void PPU2C02::reset() {
    control.reg = 0x00;
    mask.reg = 0x00;
    pending_mask_reg = 0x00;
    pending_mask_delay = 0x00;
    status.reg = 0x00;

    oam_addr = 0x00;
    address_latch = 0x00;
    ppu_data_buffer = 0x00;
    ppu_address_bus = 0x0000;
    ppu_octal_latch = 0x00;
    ppu_2007_read_sr = false;
    ppu_2007_read_latches = {{0, 1, 0, 1, 0}};
    ppu_2007_read_pending = 0;
    ppu_2007_feedback_hold = false;
    ppu_2007_write_sr = false;
    ppu_2007_write_latches = {{0, 1, 0, 1, 0}};
    ppu_2007_write_pending = 0;
    ppu_2007_write_data = 0x00;
    pending_vram_copy_delay = 0;
    ppu_2007_io_active = false;
    ppu_open_bus = 0x00;
    ppu_open_bus_idle_cycles = PPU_OPEN_BUS_DECAY_CYCLES;
    ppu_open_bus_bit_idle_cycles.fill(PPU_OPEN_BUS_DECAY_CYCLES);
    ppu_open_bus_decay_active_bits = 0;
    rebuildPaletteColorCache();
    fine_x = 0x00;

    vram_addr.reg = 0x0000;
    tram_addr.reg = 0x0000;

    bg_next_tile_id = 0x00;
    bg_next_tile_attrib = 0x00;
    bg_next_tile_lsb = 0x00;
    bg_next_tile_msb = 0x00;
    bg_pattern_fetch_valid = false;

    bg_shifter_pattern_lo = 0x0000;
    bg_shifter_pattern_hi = 0x0000;
    bg_shifter_attrib_lo = 0x0000;
    bg_shifter_attrib_hi = 0x0000;

    sprite_count = 0;
    sprite_zero_hit_possible = false;
    sprite_zero_being_rendered = false;
    sprite_zero_hit_pending = false;
    sprite_zero_hit_pending2 = false;
    oam_corruption_pending = false;
    oam_corruption_row = 0;
    scanline0_sprite_eval_enable = false;
    vblank_nmi_suppressed = false;
    vblank_nmi_start_suppressed = false;
    for (auto& s : sprite_scanline) s = SpriteEntry{};
    secondary_oam.fill(0xFF);
    secondary_oam_addr = 0;
    sprite_eval_tick = 0;
    oam_latch = 0xFF;
    oam_read_buffer = 0xFF;
    oam_read_buffer_prev = 0xFF;
    secondary_oam_full = false;
    secondary_oam_addr_overflowed = false;
    secondary_oam_fetch_frozen = false;
    oam_addr_overflowed = false;
    ninth_sprite_found = false;
    next_scanline_sprite_zero = false;
    sprite_x_counter.fill(0);
    sprite_counter_counting.fill(0);
    sprite_shifter_pattern_lo.fill(0);
    sprite_shifter_pattern_hi.fill(0);
    sprite_size_latched_for_fetch = false;

    scanline = 0;
    cycle = 0;
    odd_frame = false;
    odd_frame_skip_pending = false;

    nmi = false;
    nmi_output = false;
    frame_complete = false;
}

// ------------------------------------------------------------
// CPU register interface
// ------------------------------------------------------------

uint8_t PPU2C02::cpuRead(uint16_t addr, bool readOnly) {
    uint8_t data = currentOpenBus();
    uint8_t drivenMask = 0xFF;
    bool refreshOpenBus = true;
    addr &= 0x0007;

    if (readOnly) {
        // Debug/peek reads; do not emulate side effects.
        switch (addr) {
            case 0x0: data = control.reg; break;
            case 0x1: data = mask.reg; break;
            case 0x2: data = status.reg; break;
            case 0x3: data = oam_addr; break;
            case 0x4: data = oam[oam_addr]; break;
            case 0x5: data = 0x00; break;
            case 0x6: data = 0x00; break;
            case 0x7: data = ppu_data_buffer; break;
        }
        return data;
    }

    switch (addr) {
        case 0x2: { // PPUSTATUS
            // Status read: upper 3 bits are status, lower 5 are stale bus bits.
            const bool vblankSetRace = (scanline == 241 && cycle == 2);
            uint8_t statusSnapshot = status.reg;
            if (vblankSetRace) {
                statusSnapshot &= 0x7F;
                nmi = false;
                nmi_output = false;
            }
            // Sprite overflow reaches the CPU-facing status latch one PPU dot
            // after the evaluator detects the ninth sprite.  Bit 0 of the
            // otherwise-unused status field holds that delayed level.
            const bool overflowForCpu = (scanline == 261 && cycle <= 4)
                ? ((statusSnapshot & 0x20) != 0)
                : ((statusSnapshot & 0x01) != 0);
            data = (statusSnapshot & 0xC0)
                | (overflowForCpu ? 0x20 : 0x00)
                | (currentOpenBus() & 0x1F);
#ifdef CIPHERNES_DEBUG_PPUSTATUS
            std::fprintf(stderr, "R2002 sl=%d cy=%d raw=%02X data=%02X ctrl=%02X mask=%02X\n",
                         int(scanline), int(cycle), int(status.reg), int(data),
                         int(control.reg), int(mask.reg));
#endif
            // Clear vblank and reset address latch. The NMI output line falls
            // immediately; the bus decides whether the edge is still cancelable.
            status.vertical_blank = 0;
            nmi_output = false;
            nmi = false;
            address_latch = 0;
            break;
        }
        case 0x4: { // OAMDATA
            data = cpuReadOAMData();
#ifdef CIPHERNES_DEBUG_OAMDATA
            std::fprintf(stderr, "R2004 sl=%d cy=%d mask=%02X oam=%02X data=%02X\n",
                         int(scanline), int(cycle), int(mask.reg), int(oam_addr), int(data));
#endif
            break;
        }
        case 0x7: { // PPUDATA
            const uint16_t vram = uint16_t(vram_addr.reg & 0x3FFF);
#ifdef CIPHERNES_DEBUG_PPU_BUS
            std::fprintf(stderr, "CPU R2007 sl=%d cy=%d v=%04X bus=%04X oct=%02X rb=%02X\n",
                         int(scanline), int(cycle), unsigned(vram_addr.reg),
                         unsigned(ppu_address_bus), unsigned(ppu_octal_latch),
                         unsigned(ppu_data_buffer));
#endif
            // The CPU sees the old buffer (or palette RAM) now.  Refilling the
            // buffer and incrementing v are delayed by the $2007 state machine.
            data = ppu_data_buffer;
            if (vram >= 0x3F00) {
                // Palette RAM drives only D0-D5. D6-D7 retain the previous
                // CPU-facing PPU bus value and are not refreshed by this read.
                data = uint8_t((ppuRead(vram) & 0x3F) | (currentOpenBus() & 0xC0));
                drivenMask = 0x3F;
            }
            // Consecutive accesses caused by a DMA halt can begin before the
            // previous internal buffer pulse reaches the next full PPU dot.
            // Commit that half-cycle pulse before retriggering the latch.
            if ((ppu_2007_read_pending >> 4) == 1) {
                ppu_data_buffer = fetchExternalPpuBus();
                const bool blank = !renderingEnabled()
                    || (scanline >= 240 && scanline < 261);
                if (blank) {
                    vram_addr.reg = uint16_t(vram_addr.reg
                        + (control.increment_mode ? 32 : 1));
                } else {
                    incrementScrollY();
                }
            }
            // cpuRead() is invoked at the start of the CPU bus phase in this
            // core. /PPUSEL rises at its end, three PPU dots later.
            // High nibble: internal read-buffer sampling path. Low nibble:
            // external /RD/ALE pin-release path. The CPU callback occurs at
            // M2 rise, so the two paths reach their latches at different
            // points inside the bus phase.
            ppu_2007_read_pending = 0x43;
            ppu_2007_io_active = true;
            break;
        }
        default:
            // $2000, $2001, $2003, $2005, and $2006 are write-only. Reads
            // expose the current decay value without electrically refreshing it.
            data = currentOpenBus();
            refreshOpenBus = false;
            break;
    }

    // PPUSTATUS refreshes only D5-D7. Palette reads refresh only D0-D5,
    // and reads of write-only registers do not refresh the decay latch at all.
    if (refreshOpenBus) {
        driveOpenBus(data, addr == 0x2 ? 0xE0 : drivenMask);
    }
    return data;
}

void PPU2C02::cpuWrite(uint16_t addr, uint8_t data) {
    addr &= 0x0007;

    // Writes drive the PPU's CPU data bus; treat as open bus update.
    driveOpenBus(data);

    switch (addr) {
        case 0x0: { // PPUCTRL
            const bool prev = control.enable_nmi;
            control.reg = data;
            // Update nametable select bits in tram addr
            tram_addr.nametable_x = control.nametable_x;
            tram_addr.nametable_y = control.nametable_y;

            // If NMI is enabled during vblank, the NMI line can immediately assert.
            if (!prev && control.enable_nmi && status.vertical_blank
                && !vblank_nmi_suppressed && !vblank_nmi_start_suppressed) {
                nmi = true;
            } else if (!control.enable_nmi) {
                nmi_output = false;
                nmi = false;
            }
            break;
        }
        case 0x1: { // PPUMASK
            pending_mask_reg = data;
            // The CPU write callback occurs at M2 rise in this scheduler. The
            // rendering enables reach the PPU datapath three PPU dots later.
            pending_mask_delay = 3;
            break;
        }
        case 0x3: { // OAMADDR
            oam_addr = data;
            // A CPU-side OAM address write during forced blanking takes control
            // of the OAM address path.  In particular, the conventional
            // $2003=$00 before OAM DMA cancels any corruption transfer that was
            // armed when rendering was disabled earlier in the scanline.
            if (!oamWriteBlockedByRendering()) oam_corruption_pending = false;
            break;
        }
        case 0x4: { // OAMDATA
            if (oamWriteBlockedByRendering()) {
                oam_addr = uint8_t((oam_addr + 4) & 0xFC);
            } else {
                oam[oam_addr] = data;
                oam_addr++;
            }
            break;
        }
        case 0x5: { // PPUSCROLL
            if (address_latch == 0) {
                // First write: X scroll
                fine_x = data & 0x07;
                tram_addr.coarse_x = data >> 3;
                address_latch = 1;
            } else {
                // Second write: Y scroll
                tram_addr.fine_y = data & 0x07;
                tram_addr.coarse_y = data >> 3;
                address_latch = 0;
            }
            break;
        }
        case 0x6: { // PPUADDR
#ifdef CIPHERNES_DEBUG_PPU_BUS
            std::fprintf(stderr, "CPU W2006 sl=%d cy=%d data=%02X latch=%u v=%04X t=%04X bus=%04X oct=%02X\n",
                         int(scanline), int(cycle), unsigned(data), unsigned(address_latch),
                         unsigned(vram_addr.reg), unsigned(tram_addr.reg),
                         unsigned(ppu_address_bus), unsigned(ppu_octal_latch));
#endif
            if (address_latch == 0) {
                tram_addr.reg = uint16_t((uint16_t(data & 0x3F) << 8) | (tram_addr.reg & 0x00FF));
                address_latch = 1;
            } else {
                tram_addr.reg = uint16_t((tram_addr.reg & 0xFF00) | data);
                pending_vram_copy_delay = 3;
                ppu_2007_io_active = true;
                address_latch = 0;
            }
            break;
        }
        case 0x7: { // PPUDATA
            ppu_2007_write_data = data;
            ppu_2007_write_pending = 3;
            ppu_2007_io_active = true;
            break;
        }
        default:
            break;
    }
}

void PPU2C02::dmaWriteOam(uint8_t data) {
    if ((oam_addr & 0x03) == 0x02) {
        data &= 0xE3;
    }
    oam[oam_addr++] = data;
}

// ------------------------------------------------------------
// PPU memory interface
// ------------------------------------------------------------

uint8_t PPU2C02::ppuRead(uint16_t addr, bool readOnly) {
    uint8_t data = 0x00;
    addr &= 0x3FFF;

    if (addr <= 0x1FFF) {
        // Cartridge CHR/pattern. Nametable/palette reads are handled locally;
        // avoiding a mapper virtual call for those hot-path fetches is a
        // noticeable speed win during normal rendering.
        if (cartridgeRaw && cartridgeRaw->ppuRead(addr, data)) {
            return data;
        }
        // If cartridge didn't handle, treat as CHR RAM not present (0)
        return 0x00;
    }

    if (addr >= 0x2000 && addr <= 0x3EFF) {
        // Some custom cartridges provide external/generated nametable data.
        // Normal mappers return false here, so the standard internal CIRAM path
        // below remains unchanged.
        if (cartridgeHandlesExternalNametables && cartridgeRaw->ppuRead(addr, data)) {
            return data;
        }

        addr &= 0x0FFF;
        uint16_t table = uint16_t((addr >> 10) & 0x03);
        const uint16_t offset = uint16_t(addr & 0x03FF);

        // Fixed mirroring is predecoded when the cartridge is connected. Only
        // mapper-controlled mirroring needs the runtime mode branch.
        if (!cartridgeUsesDynamicMirroring) {
            table = cartridgeFixedNametableMap[table];
        } else {
            const auto m = cartridgeRaw->mirroring();
            if (m == Cartridge::Mirror::VERTICAL) table = uint16_t(table & 0x01);
            else if (m == Cartridge::Mirror::HORIZONTAL) table = uint16_t(table >> 1);
            else if (m == Cartridge::Mirror::ONE_SCREEN_LO) table = 0;
            else if (m == Cartridge::Mirror::ONE_SCREEN_HI) table = 1;
        }

        return tbl_name[table][offset];
    }

    if (addr >= 0x3F00 && addr <= 0x3FFF) {
        addr = mirrorPaletteAddr(addr);
        uint8_t v = uint8_t((currentOpenBus() & 0xC0) | (tbl_palette[addr & 0x001F] & 0x3F));
        // Greyscale
        if (mask.grayscale) v = uint8_t((v & 0xC0) | (v & 0x30));
        return v;
    }

    return 0x00;
}

void PPU2C02::ppuWrite(uint16_t addr, uint8_t data) {
    addr &= 0x3FFF;

    if (addr <= 0x1FFF) {
        // Cartridge CHR/pattern (CHR RAM writes)
        if (cartridgeRaw && cartridgeRaw->ppuWrite(addr, data)) {
            return;
        }
        // If cartridge didn't handle, nothing to write
        return;
    }

    if (addr >= 0x2000 && addr <= 0x3EFF) {
        // Custom cartridges may expose generated/external nametable RAM.
        // Normal mappers ignore this and fall through to internal CIRAM.
        if (cartridgeHandlesExternalNametables && cartridgeRaw->ppuWrite(addr, data)) {
            return;
        }

        addr &= 0x0FFF;
        uint16_t table = uint16_t((addr >> 10) & 0x03);
        const uint16_t offset = uint16_t(addr & 0x03FF);

        if (!cartridgeUsesDynamicMirroring) {
            table = cartridgeFixedNametableMap[table];
        } else {
            const auto m = cartridgeRaw->mirroring();
            if (m == Cartridge::Mirror::VERTICAL) table = uint16_t(table & 0x01);
            else if (m == Cartridge::Mirror::HORIZONTAL) table = uint16_t(table >> 1);
            else if (m == Cartridge::Mirror::ONE_SCREEN_LO) table = 0;
            else if (m == Cartridge::Mirror::ONE_SCREEN_HI) table = 1;
        }

        tbl_name[table][offset] = data;
        return;
    }

    if (addr >= 0x3F00 && addr <= 0x3FFF) {
        addr = mirrorPaletteAddr(addr);
        const uint8_t paletteAddr = uint8_t(addr & 0x001F);
        tbl_palette[paletteAddr] = uint8_t(data & 0x3F);
        updatePaletteColorCacheEntry(paletteAddr);
        return;
    }
}

// ------------------------------------------------------------
// Internal helpers (scrolling & shifters)
// ------------------------------------------------------------

void PPU2C02::incrementScrollX() {
    if (mask.render_background || mask.render_sprites) {
        if (vram_addr.coarse_x == 31) {
            vram_addr.coarse_x = 0;
            vram_addr.nametable_x = ~vram_addr.nametable_x;
        } else {
            vram_addr.coarse_x++;
        }
    }
}

void PPU2C02::incrementScrollY() {
    if (mask.render_background || mask.render_sprites) {
        if (vram_addr.fine_y < 7) {
            vram_addr.fine_y++;
        } else {
            vram_addr.fine_y = 0;
            if (vram_addr.coarse_y == 29) {
                vram_addr.coarse_y = 0;
                vram_addr.nametable_y = ~vram_addr.nametable_y;
            } else if (vram_addr.coarse_y == 31) {
                vram_addr.coarse_y = 0;
            } else {
                vram_addr.coarse_y++;
            }
        }
    }
}

void PPU2C02::transferAddressX() {
    if (mask.render_background || mask.render_sprites) {
        vram_addr.nametable_x = tram_addr.nametable_x;
        vram_addr.coarse_x = tram_addr.coarse_x;
    }
}

void PPU2C02::transferAddressY() {
    if (mask.render_background || mask.render_sprites) {
        vram_addr.fine_y = tram_addr.fine_y;
        vram_addr.nametable_y = tram_addr.nametable_y;
        vram_addr.coarse_y = tram_addr.coarse_y;
    }
}

void PPU2C02::loadBackgroundShifters() {
    bg_shifter_pattern_lo = (bg_shifter_pattern_lo & 0xFF00) | bg_next_tile_lsb;
    bg_shifter_pattern_hi = (bg_shifter_pattern_hi & 0xFF00) | bg_next_tile_msb;

    bg_shifter_attrib_lo = (bg_shifter_attrib_lo & 0xFF00) | ((bg_next_tile_attrib & 0x01) ? 0xFF : 0x00);
    bg_shifter_attrib_hi = (bg_shifter_attrib_hi & 0xFF00) | ((bg_next_tile_attrib & 0x02) ? 0xFF : 0x00);
}

void PPU2C02::updateShifters(bool shiftSprites) {
    // The caller invokes this only while either background or sprite rendering
    // is enabled, so the background shifter edge is unconditional here.
    bg_shifter_pattern_lo <<= 1;
    bg_shifter_pattern_hi = uint16_t((bg_shifter_pattern_hi << 1) | 0x0001);
    bg_shifter_attrib_lo <<= 1;
    bg_shifter_attrib_hi <<= 1;

    // Sprite shifters/counters run only across visible fetch dots 2-256. The
    // caller already knows the dot range, avoiding two repeated range tests and
    // a redundant renderingEnabled() check in this per-dot hot path.
    if (!shiftSprites) return;
    const uint8_t activeSprites = std::min<uint8_t>(sprite_count, 8);
    for (uint8_t i = 0; i < activeSprites; i++) {
        if (sprite_counter_counting[i]) {
            if (sprite_x_counter[i] > 0) sprite_x_counter[i]--;
            if (sprite_x_counter[i] == 0) sprite_counter_counting[i] = 0;
        } else {
            sprite_shifter_pattern_lo[i] <<= 1;
            sprite_shifter_pattern_hi[i] <<= 1;
        }
    }
}

void PPU2C02::clockSpriteEvaluation(int16_t sl) {
    if (sl < -1 || sl >= 240) return;

    bool rendering = renderingEnabled();
    // The PPUMASK select path reaches sprite fetch on the final delay dot.
    // In particular, disabling at dot 257/258 must prevent the X counter
    // reload later in that sprite's fetch slot.
    if (pending_mask_delay == 1 && (pending_mask_reg & 0x18) == 0) {
        rendering = false;
    }

    // RP2C02G sprite-engine reset strobes. These clear the OAM2 address
    // overflow/freeze latch only if rendering is active at the strobe.
    // Keeping this latch alive across forced blanking is what allows the
    // advanced Frozen OAM2 Increment behavior.
    if (rendering && (cycle == 63 || cycle == 255 || cycle == 339)) {
        secondary_oam_addr_overflowed = false;
        secondary_oam_fetch_frozen = false;
        // The dot-255 strobe clears the overflow latch, but the existing
        // evaluation cadence remains externally visible through $2004 until
        // sprite fetch begins. Dot 339 is safely in the idle/read phase.
        if (cycle == 339) secondary_oam_addr = 0;
    }

    const bool preRender = sl == -1;
    const uint8_t height = control.sprite_size ? 16 : 8;
    const uint8_t evalLine = uint8_t(sl);

    if (cycle >= 1 && cycle <= 64) {
        if (!rendering) return;
        if (cycle == 1) {
            secondary_oam_addr = 0;
            secondary_oam_full = false;
            sprite_eval_tick = 0;
            oam_addr_overflowed = false;
            ninth_sprite_found = false;
        }
        if (cycle & 1) {
            oam_latch = preRender ? secondary_oam[secondary_oam_addr] : 0xFF;
        } else {
            if (!preRender) secondary_oam[secondary_oam_addr] = oam_latch;
            secondary_oam_addr = uint8_t((secondary_oam_addr + 1) & 0x1F);
        }
        oam_read_buffer = 0xFF;
        return;
    }

    if (cycle >= 65 && cycle <= 256) {
        if (cycle == 65) {
            secondary_oam_addr = 0;
            ninth_sprite_found = false;
        }
        if (!rendering) return;

        if (cycle & 1) {
            oam_latch = oam[oam_addr];
            // Attribute bits 2-4 are not present in the OAM evaluation path on
            // the revision-G PPU targeted by AccuracyCoin.
            if ((oam_addr & 3) == 2) oam_latch &= 0xE7;
            oam_read_buffer = oam_latch;
            return;
        }

        if (oam_addr_overflowed) {
            oam_addr = uint8_t((oam_addr + 4) & 0xFC);
            oam_latch = secondary_oam[secondary_oam_addr];
            oam_read_buffer = oam_latch;
            return;
        }

        const uint8_t previousOamAddr = oam_addr;
        if (!secondary_oam_full && !preRender) secondary_oam[secondary_oam_addr] = oam_latch;
        const uint8_t secondaryRead = secondary_oam[secondary_oam_addr];

        if (sprite_eval_tick == 0) {
            const uint16_t distance = uint16_t(evalLine) - uint16_t(oam_latch);
            const bool inRange = !preRender && distance < height;
            if (!ninth_sprite_found && inRange) {
                if (!secondary_oam_full) {
                    oam_addr++;
                    secondary_oam_addr = uint8_t((secondary_oam_addr + 1) & 0x1F);
                    if (secondary_oam_addr == 0) {
                        secondary_oam_full = true;
                        secondary_oam_addr_overflowed = true;
                    }
                    if (cycle == 66) next_scanline_sprite_zero = true;
                } else {
                    ninth_sprite_found = true;
                    oam_addr++;
                    status.sprite_overflow = 1;
                }
                sprite_eval_tick = uint8_t((sprite_eval_tick + 1) & 3);
            } else {
                if (cycle == 66) next_scanline_sprite_zero = false;
                if (secondary_oam_full && !ninth_sprite_found) {
                    if ((oam_addr & 3) == 3) oam_addr++;
                    else oam_addr = uint8_t(oam_addr + 5);
                } else {
                    oam_addr = uint8_t((oam_addr + 4) & 0xFC);
                }
            }
        } else {
            if (sprite_eval_tick == 3) {
                const uint16_t distance = uint16_t(evalLine) - uint16_t(oam_latch);
                if (distance < height) {
                    oam_addr = uint8_t(oam_addr + (secondary_oam_full ? 4 : 1));
                } else {
                    oam_addr = uint8_t((oam_addr + 1) & 0xFC);
                }
            } else {
                oam_addr++;
            }
            sprite_eval_tick = uint8_t((sprite_eval_tick + 1) & 3);
            if (!secondary_oam_full && !preRender) {
                secondary_oam_addr = uint8_t((secondary_oam_addr + 1) & 0x1F);
                if (secondary_oam_addr == 0) {
                    secondary_oam_full = true;
                    secondary_oam_addr_overflowed = true;
                }
            }
        }

        if (oam_addr < previousOamAddr && oam_addr < 4) oam_addr_overflowed = true;
        oam_latch = secondaryRead;
        oam_read_buffer = oam_latch;
        return;
    }

    if (cycle >= 257 && cycle <= 320) {
        if (rendering) oam_addr = 0;
        if (cycle == 257) {
            // Snapshot whether this fetch *starts* with OAM2ADDR frozen. A
            // normal fetch also wraps OAM2ADDR near its end; that wrap raises
            // the persistent latch for a later fetch, but must not alter the
            // byte-selection cadence of the fetch already in progress.
            secondary_oam_fetch_frozen = secondary_oam_addr_overflowed;
            // Under normal rendering, dot 255 reset OAM2ADDR to zero. If that
            // reset was skipped because rendering was disabled, preserve both
            // the address and the overflow latch into sprite fetch.
            if (!secondary_oam_addr_overflowed) secondary_oam_addr = 0;
            sprite_eval_tick = 0;
            sprite_count = 8;
            sprite_zero_hit_possible = next_scanline_sprite_zero;
        }
        if (rendering) {
            const uint8_t slot = uint8_t((cycle - 257) >> 3);
            const uint8_t phase = uint8_t((cycle - 257) & 7);
            const uint8_t base = uint8_t(slot << 2);
            if (slot < 8) {
                const bool frozenAtStartOfDot = secondary_oam_fetch_frozen;

                // Preserve the already-certified normal $2004 sprite-fetch
                // cadence. Only the OAM2-overflow path substitutes the live,
                // frozen OAM2 address for all four sprite fields.
                if (frozenAtStartOfDot) {
                    // An overflowed OAM2ADDR inhibits every sprite-fetch
                    // increment, so every OAM read exposes the same byte.
                    // If rendering is re-enabled part-way through the first
                    // four fetch phases, reconstruct the field latches whose
                    // strobes have already elapsed from that same frozen byte.
                    // This mirrors the hardware result (all four object bytes
                    // become the frozen OAM2 value) without disturbing the
                    // normal, already-certified fetch cadence.
                    oam_latch = secondary_oam[secondary_oam_addr];
                    if (phase <= 3) {
                        sprite_scanline[slot].y = oam_latch;
                        if (phase >= 1) sprite_scanline[slot].id = oam_latch;
                        if (phase >= 2) sprite_scanline[slot].attr = oam_latch;
                        if (phase >= 3) {
                            sprite_scanline[slot].x = oam_latch;
                            sprite_x_counter[slot] = oam_latch;
                        }
                    }
                } else {
                    if (phase == 0) sprite_scanline[slot].y = secondary_oam[base];
                    else if (phase == 1) sprite_scanline[slot].id = secondary_oam[uint8_t(base + 1)];
                    else if (phase == 2) sprite_scanline[slot].attr = secondary_oam[uint8_t(base + 2)];
                    else if (phase >= 3) {
                        sprite_scanline[slot].x = secondary_oam[uint8_t(base + 3)];
                        if (phase == 3) sprite_x_counter[slot] = sprite_scanline[slot].x;
                    }
                    oam_latch = secondary_oam[uint8_t(base + std::min<uint8_t>(phase, 3))];
                }

                // OAM2ADDR advances across the first four fetch dots while
                // rendering is active. Forced blanking pauses this counter.
                if (phase <= 3 && !secondary_oam_addr_overflowed) {
                    const uint8_t previous = secondary_oam_addr;
                    secondary_oam_addr = uint8_t((secondary_oam_addr + 1) & 0x1F);
                    if (previous == 0x1F) secondary_oam_addr_overflowed = true;
                }
            }
        }
        oam_read_buffer = oam_latch;
        return;
    }

    if (cycle == 339 && rendering) {
        for (uint8_t i = 0; i < 8; ++i) {
            sprite_counter_counting[i] = sprite_x_counter[i] != 0;
        }
    }
#ifdef CIPHERNES_DEBUG_PPU_BUS
    if (cycle == 339 && scanline < 10) {
        std::fprintf(stderr, "DOT339 sl=%d rend=%d x=%u count=%u lo=%02X hi=%02X spr0=%d\n",
                     int(scanline), rendering ? 1 : 0, unsigned(sprite_x_counter[0]),
                     unsigned(sprite_counter_counting[0]),
                     unsigned(sprite_shifter_pattern_lo[0]),
                     unsigned(sprite_shifter_pattern_hi[0]),
                     sprite_zero_hit_possible ? 1 : 0);
    }
#endif
    if ((cycle == 0 || cycle > 320) && rendering)
        oam_read_buffer = secondary_oam[secondary_oam_addr];
}

uint8_t PPU2C02::fetchExternalPpuBus(uint8_t fetchKind) {
    // The memory sees the high six PPU address pins plus the low byte held by
    // the external 74LS373 latch.  The returned byte then appears on those
    // same low pins, which matters when ALE and /RD overlap.
    uint16_t address = uint16_t((ppu_address_bus & 0x3F00) | ppu_octal_latch);
    // Palette RAM is internal. An external read while v points at $3Fxx is
    // serviced by the underlying nametable address ($2Fxx).
    if (address >= 0x3F00) address = uint16_t(address - 0x1000);
    if (cartridgeUsesFetchHooks && cartridge) {
        cartridge->beforePpuFetch(address, fetchKind, scanline, cycle, 0);
    }
    const uint8_t data = ppuRead(address);
    if (cartridgeUsesFetchHooks && cartridge) {
        cartridge->afterPpuFetch(address, fetchKind, scanline, cycle, 0, data);
    }
    ppu_address_bus = uint16_t((ppu_address_bus & 0x3F00) | data);
    return data;
}

uint16_t PPU2C02::spritePatternAddress(uint8_t slot, int16_t sl, bool highPlane) const {
    const SpriteEntry& sprite = sprite_scanline[slot];
    // During the pre-render fetch the 8-bit scanline arithmetic observes
    // 261 as $05, which is what permits retained secondary-OAM sprites to be
    // fetched for scanline 0.
    const int16_t spriteLine = (sl == -1) ? 5 : sl;
    uint8_t row = uint8_t(spriteLine - int16_t(sprite.y));
    uint16_t address = 0;

    if (!control.sprite_size) {
        row &= 0x07;
        if (sprite.attr & 0x80) row = uint8_t(7 - row);
        address = uint16_t((control.pattern_sprite ? 0x1000 : 0x0000)
            | (uint16_t(sprite.id) << 4) | row);
    } else {
        if (sprite.attr & 0x80) row = uint8_t(15 - (row & 0x0F));
        else row &= 0x0F;
        uint16_t tile = uint16_t(sprite.id & 0xFE);
        if (row >= 8) {
            tile++;
            row = uint8_t(row - 8);
        }
        address = uint16_t(((sprite.id & 1) ? 0x1000 : 0x0000)
            | (tile << 4) | row);
    }
    if (highPlane) address = uint16_t(address + 8);
    return address;
}

void PPU2C02::clockRenderingDataBus(int16_t sl) {
    const auto reverseByte = [](uint8_t b) {
        b = uint8_t((b >> 4) | (b << 4));
        b = uint8_t(((b & 0xCC) >> 2) | ((b & 0x33) << 2));
        return uint8_t(((b & 0xAA) >> 1) | ((b & 0x55) << 1));
    };

    if ((cycle >= 1 && cycle <= 256) || (cycle >= 321 && cycle <= 336)) {
        const uint8_t phase = uint8_t((cycle - 1) & 7);
        switch (phase) {
            case 0:
                if (bg_pattern_fetch_valid) {
                    loadBackgroundShifters();
                    bg_pattern_fetch_valid = false;
                }
                ppu_address_bus = uint16_t(0x2000 | (vram_addr.reg & 0x0FFF));
                break;
            case 1:
                bg_next_tile_id = fetchExternalPpuBus(1);
#ifdef CIPHERNES_DEBUG_PPU_BUS
                if (scanline < 10 && cycle >= 180 && cycle <= 200) {
                    std::fprintf(stderr, "BGNT sl=%d cy=%d id=%02X v=%04X bus=%04X oct=%02X\n",
                                 int(scanline), int(cycle), unsigned(bg_next_tile_id),
                                 unsigned(vram_addr.reg), unsigned(ppu_address_bus), unsigned(ppu_octal_latch));
                }
#endif
                break;
            case 2:
                ppu_address_bus = uint16_t(0x23C0
                    | (vram_addr.nametable_y << 11)
                    | (vram_addr.nametable_x << 10)
                    | ((vram_addr.coarse_y >> 2) << 3)
                    | (vram_addr.coarse_x >> 2));
                break;
            case 3: {
                uint8_t attr = fetchExternalPpuBus(2);
                if (vram_addr.coarse_y & 0x02) attr >>= 4;
                if (vram_addr.coarse_x & 0x02) attr >>= 2;
                bg_next_tile_attrib = uint8_t(attr & 0x03);
                break;
            }
            case 4:
                ppu_address_bus = uint16_t((control.pattern_background ? 0x1000 : 0x0000)
                    + (uint16_t(bg_next_tile_id) << 4) + vram_addr.fine_y);
                break;
            case 5:
                bg_next_tile_lsb = fetchExternalPpuBus(3);
                break;
            case 6:
                ppu_address_bus = uint16_t((control.pattern_background ? 0x1000 : 0x0000)
                    + (uint16_t(bg_next_tile_id) << 4) + vram_addr.fine_y + 8);
                break;
            case 7:
                bg_next_tile_msb = fetchExternalPpuBus(4);
                bg_pattern_fetch_valid = true;
                incrementScrollX();
                break;
        }
    } else if (cycle >= 257 && cycle <= 320) {
        const uint8_t slot = uint8_t((cycle - 257) >> 3);
        const uint8_t phase = uint8_t((cycle - 257) & 7);
        if (cycle == 257) sprite_size_latched_for_fetch = control.sprite_size;
        switch (phase) {
            case 0:
            case 2:
                ppu_address_bus = uint16_t(0x2000 | (vram_addr.reg & 0x0FFF));
                break;
            case 1:
            case 3:
                (void)fetchExternalPpuBus(1);
                break;
            case 4:
                ppu_address_bus = spritePatternAddress(slot, sl, false);
                break;
            case 5:
                sprite_shifter_pattern_lo[slot] = fetchExternalPpuBus(5);
                break;
            case 6:
                ppu_address_bus = spritePatternAddress(slot, sl, true);
                break;
            case 7:
                sprite_shifter_pattern_hi[slot] = fetchExternalPpuBus(6);
                if (sprite_scanline[slot].attr & 0x40) {
                    sprite_shifter_pattern_lo[slot] = reverseByte(sprite_shifter_pattern_lo[slot]);
                    sprite_shifter_pattern_hi[slot] = reverseByte(sprite_shifter_pattern_hi[slot]);
                }
                break;
        }
    } else if (cycle >= 337 && cycle <= 340) {
        if (cycle == 337 || cycle == 339) {
            ppu_address_bus = uint16_t(0x2000 | (vram_addr.reg & 0x0FFF));
        } else {
            const uint8_t ignored = fetchExternalPpuBus(1);
            if (cycle == 338) bg_next_tile_id = ignored;
        }
    } else if (cycle == 0 && sl != -1) {
        ppu_address_bus = uint16_t((control.pattern_background ? 0x1000 : 0x0000)
            + (uint16_t(bg_next_tile_id) << 4) + vram_addr.fine_y);
    }
}

void PPU2C02::clockPpuDataBus(int16_t sl) {
    const bool rendering = renderingEnabled();
    const bool renderLine = (sl >= -1 && sl < 240);
    const bool blank = !rendering || (scanline >= 240 && scanline < 261);

    // The five-latch $2007 read/write machines spend virtually every PPU dot
    // in this stable idle state.  Bypass their propagation work until a CPU
    // register access actually starts one of the machines.  Rendering bus
    // activity is still performed on every original dot, including the exact
    // ALE latch timing; only the provably inert CPU-I/O machinery is skipped.
    const bool ioMachineIdle = !ppu_2007_io_active;
    if (ioMachineIdle) {
        if (!rendering) {
            ppu_address_bus = uint16_t(vram_addr.reg & 0x3FFF);
            return;
        }
        if (!renderLine) return;

        clockRenderingDataBus(sl);
        // Rendering asserts ALE on odd dots after selecting the new PAR
        // address.  Capture AD0-AD7 at the same point as the full path.
        if (cycle & 1) ppu_octal_latch = uint8_t(ppu_address_bus);
        return;
    }
    // A simultaneous ALE + /RD pulse feeds the external data byte back into
    // the 74LS373 low-address latch.  The latch can retain that feedback for
    // the following rendering ALE dot instead of immediately accepting the
    // PAR's new low byte.  This is the phase-sensitive behavior exercised by
    // AccuracyCoin's "ALE + Read" test.
    const bool preserveFeedbackLatch = ppu_2007_feedback_hold;
    ppu_2007_feedback_hold = false;

    uint8_t earlyReadPhase = uint8_t(ppu_2007_read_pending >> 4);
    uint8_t externalReadDelay = uint8_t(ppu_2007_read_pending & 0x0F);
    const bool earlyReadAle = earlyReadPhase == 3;
    const bool earlyReadBufferClock = earlyReadPhase == 1;
    if (earlyReadPhase > 0) earlyReadPhase--;
    if (externalReadDelay > 0 && --externalReadDelay == 0) ppu_2007_read_sr = true;
    ppu_2007_read_pending = uint8_t((earlyReadPhase << 4) | externalReadDelay);
    if (ppu_2007_write_pending > 0 && --ppu_2007_write_pending == 0) {
        ppu_2007_write_sr = true;
    }

    // Full-clock half of the two five-latch $2007 state machines.
    ppu_2007_read_latches[0] = ppu_2007_read_sr ? 1 : 0;
    ppu_2007_read_latches[2] = ppu_2007_read_latches[1] ? 0 : 1;
    ppu_2007_read_latches[4] = ppu_2007_read_latches[3] ? 0 : 1;
    const bool readBufferClock = ppu_2007_read_latches[4] && !ppu_2007_read_latches[2];
    const bool readAle = !ppu_2007_read_latches[4] && ppu_2007_read_latches[2];

    ppu_2007_write_latches[0] = ppu_2007_write_sr ? 1 : 0;
    ppu_2007_write_latches[2] = ppu_2007_write_latches[1] ? 0 : 1;
    ppu_2007_write_latches[4] = ppu_2007_write_latches[3] ? 0 : 1;
    const bool writeAle = !ppu_2007_write_latches[4] && ppu_2007_write_latches[2];
    const bool previousWriteDataPhase = ppu_2007_write_latches[1]
        && !ppu_2007_write_latches[3];

    const bool normalAle = rendering && renderLine && (cycle & 1);
    const bool normalRead = rendering && renderLine && !(cycle & 1);
    const bool readLine = earlyReadBufferClock || readBufferClock || normalRead;
    const bool ale = earlyReadAle || readAle || writeAle || normalAle;
#ifdef CIPHERNES_DEBUG_PPU_BUS
    if (readAle || readBufferClock || writeAle) {
        std::fprintf(stderr, "SM sl=%d cy=%d rALE=%d rBUF=%d wALE=%d nALE=%d nRD=%d v=%04X bus=%04X oct=%02X\n",
                     int(scanline), int(cycle), readAle ? 1 : 0, readBufferClock ? 1 : 0,
                     writeAle ? 1 : 0, normalAle ? 1 : 0, normalRead ? 1 : 0,
                     unsigned(vram_addr.reg), unsigned(ppu_address_bus), unsigned(ppu_octal_latch));
    }
#endif

    if ((earlyReadAle || readAle || writeAle) && !readLine) {
        ppu_address_bus = uint16_t(vram_addr.reg & 0x3FFF);
        ppu_octal_latch = uint8_t(ppu_address_bus);
    }

    // A second $2006 write does not copy t to v at the CPU register boundary.
    // It reaches the video address path a few PPU clocks later.
    if (pending_vram_copy_delay > 0) {
        pending_vram_copy_delay--;
        if (pending_vram_copy_delay == 0) {
#ifdef CIPHERNES_DEBUG_PPU_BUS
            std::fprintf(stderr, "COPYV sl=%d cy=%d old=%04X new=%04X bus=%04X oct=%02X RD=%d ALE=%d\n",
                         int(scanline), int(cycle), unsigned(vram_addr.reg), unsigned(tram_addr.reg),
                         unsigned(ppu_address_bus), unsigned(ppu_octal_latch), readLine ? 1 : 0, ale ? 1 : 0);
#endif
            vram_addr.reg = tram_addr.reg;
            ppu_address_bus = uint16_t(vram_addr.reg & 0x3FFF);
        }
    }

    if (!rendering) ppu_address_bus = uint16_t(vram_addr.reg & 0x3FFF);

    // When ALE and /RD are both active, the value already on the shared pins
    // feeds back into the octal latch before the rendering PAR is selected.
    if (ale && readLine) ppu_octal_latch = uint8_t(ppu_address_bus);

    if (rendering && renderLine) clockRenderingDataBus(sl);

    if (ale && !readLine && !preserveFeedbackLatch) ppu_octal_latch = uint8_t(ppu_address_bus);

    // The delayed $2007 buffer read happens after the rendering address mux.
    if (earlyReadBufferClock) {
        ppu_data_buffer = fetchExternalPpuBus();
        if (ale) {
            // fetchExternalPpuBus() leaves the returned data on AD0-AD7. With
            // ALE still asserted, that byte is immediately captured by the
            // external address latch and remains there through the next dot.
            ppu_octal_latch = uint8_t(ppu_address_bus);
            ppu_2007_feedback_hold = true;
        }
    }
    if (readBufferClock) (void)fetchExternalPpuBus();

    // Half-clock propagation.
    ppu_2007_read_latches[1] = ppu_2007_read_latches[0] ? 0 : 1;
    ppu_2007_read_latches[3] = ppu_2007_read_latches[2] ? 0 : 1;
    if (!ppu_2007_read_latches[3]) ppu_2007_read_sr = false;

    ppu_2007_write_latches[1] = ppu_2007_write_latches[0] ? 0 : 1;
    ppu_2007_write_latches[3] = ppu_2007_write_latches[2] ? 0 : 1;
    if (!ppu_2007_write_latches[3]) ppu_2007_write_sr = false;
    const bool writeDataPhase = ppu_2007_write_latches[1]
        && !ppu_2007_write_latches[3];
    if (writeDataPhase) ppuWrite(ppu_address_bus, ppu_2007_write_data);

    if (previousWriteDataPhase || earlyReadBufferClock) {
        if (blank) vram_addr.reg = uint16_t(vram_addr.reg + (control.increment_mode ? 32 : 1));
        else incrementScrollY();
    }

    // Re-arm the common idle fast path only after both latch halves and every
    // delayed operation have returned to their exact stable state.
    ppu_2007_io_active =
        ppu_2007_feedback_hold
        || ppu_2007_read_pending != 0
        || ppu_2007_read_sr
        || ppu_2007_write_pending != 0
        || ppu_2007_write_sr
        || pending_vram_copy_delay != 0
        || ppu_2007_read_latches[0] != 0
        || ppu_2007_read_latches[1] != 1
        || ppu_2007_read_latches[2] != 0
        || ppu_2007_read_latches[3] != 1
        || ppu_2007_read_latches[4] != 0
        || ppu_2007_write_latches[0] != 0
        || ppu_2007_write_latches[1] != 1
        || ppu_2007_write_latches[2] != 0
        || ppu_2007_write_latches[3] != 1
        || ppu_2007_write_latches[4] != 0;
}

void PPU2C02::updatePaletteColorCacheEntry(uint8_t addr) {
    addr &= 0x1F;
    uint8_t index = uint8_t(tbl_palette[addr] & 0x3F);
    if (mask.grayscale) index = uint8_t(index & 0x30);
    palette_color_cache[addr] = 0xFF000000u | kNesPalette[index];
}

void PPU2C02::rebuildPaletteColorCache() {
    for (uint8_t addr = 0; addr < 32; ++addr) {
        updatePaletteColorCacheEntry(addr);
    }
}

uint32_t PPU2C02::getColorFromPaletteRam(uint8_t palette, uint8_t pixel) {
    // Hot path: palette bytes and grayscale conversion are decoded only when
    // either input changes, leaving one canonical-address lookup per pixel.
    uint8_t addr = uint8_t(((uint16_t(palette) << 2) + uint16_t(pixel)) & 0x1F);
    if ((addr & 0x13) == 0x10) addr = uint8_t(addr & 0x0F);
    return palette_color_cache[addr];
}


// ------------------------------------------------------------
// Main PPU clock
// ------------------------------------------------------------

void PPU2C02::clock() {
    // Sample the external address held from the previous dot before this dot's
    // internal address generator updates it. This aligns mapper observation
    // with the PPU's external address/data bus phase.
    if (cartridgeUsesPpuAddressBus) {
        cartridgeRaw->ppuAddressBus(uint16_t(ppu_address_bus & 0x3FFF));
    }

    oam_read_buffer_prev = oam_read_buffer;
    status.unused = uint8_t((status.unused & 0x1E) | status.sprite_overflow);

    if (ppu_open_bus_decay_active_bits != 0) {
        uint8_t active = ppu_open_bus_decay_active_bits;
        for (unsigned bit = 0; bit < 8; ++bit) {
            const uint8_t bitMask = uint8_t(1u << bit);
            if (!(active & bitMask)) continue;
            uint32_t& idle = ppu_open_bus_bit_idle_cycles[bit];
            if (++idle >= PPU_OPEN_BUS_DECAY_CYCLES) {
                ppu_open_bus = uint8_t(ppu_open_bus & uint8_t(~bitMask));
                active = uint8_t(active & uint8_t(~bitMask));
            }
        }
        ppu_open_bus_decay_active_bits = active;
        ppu_open_bus_idle_cycles = ppu_open_bus_bit_idle_cycles[0];
    }

    auto applyPendingMask = [&]() {
        if (pending_mask_delay > 0) {
            pending_mask_delay--;
            if (pending_mask_delay == 0) {
                const bool oldRendering = (mask.render_background || mask.render_sprites);
                const bool newRendering = (pending_mask_reg & 0x18) != 0;
                const bool inEvalRange = (scanline >= 0 && scanline < 240) || scanline == 261;
                if (oldRendering && !newRendering && inEvalRange) {
                    bg_pattern_fetch_valid = false;
                    if (cycle >= 1 && cycle <= 64) {
                        oam_corruption_row = uint8_t((cycle >> 1) & 0x1F);
                    } else if (cycle >= 65 && cycle <= 256) {
                        oam_corruption_row = uint8_t(((sprite_count + 3) & ~0x03) & 0x1F);
                    } else if (cycle >= 257 && cycle <= 320) {
                        oam_corruption_row = uint8_t((((cycle - 257) / 8) * 4) & 0x1F);
                    } else {
                        oam_corruption_row = 0;
                    }
                    oam_corruption_pending = true;
                }
                if (!oldRendering && newRendering && inEvalRange) {
                    if (scanline == 261 && cycle > 66) {
                        scanline0_sprite_eval_enable = true;
                    }
                }
                const bool grayscaleChanged = mask.grayscale != ((pending_mask_reg & 0x01) != 0);
                mask.reg = pending_mask_reg;
                if (grayscaleChanged) rebuildPaletteColorCache();
#ifdef CIPHERNES_DEBUG_PPU_BUS
                if (scanline < 10) {
                    std::fprintf(stderr, "MASK sl=%d cy=%d old=%d new=%d x=%u count=%u lo=%02X hi=%02X spr0=%d\n",
                                 int(scanline), int(cycle), oldRendering ? 1 : 0,
                                 newRendering ? 1 : 0, unsigned(sprite_x_counter[0]),
                                 unsigned(sprite_counter_counting[0]),
                                 unsigned(sprite_shifter_pattern_lo[0]),
                                 unsigned(sprite_shifter_pattern_hi[0]),
                                 sprite_zero_hit_possible ? 1 : 0);
                }
#endif
            }
        }
    };

    // Visible scanlines: 0..239
    // Post-render: 240
    // VBlank: 241..260
    // Pre-render: 261

    // Convert internal scanline to OLC-style pre-render (-1)
    int16_t sl = scanline;
    if (sl == 261) sl = -1;

    // Sample the odd-frame shortening decision late enough for precisely
    // timed PPUMASK writes, but preserve the final pre-render bus phase. The
    // shortened clock is consumed as the next visible scanline's idle dot.
    const bool oddFrameRenderEnabled = pending_mask_delay > 0
        ? ((pending_mask_reg & 0x18) != 0)
        : (mask.render_background || mask.render_sprites);
    if (sl == -1 && cycle == 339) {
        odd_frame_skip_pending = odd_frame && oddFrameRenderEnabled;
    }
    if (sl == 0 && cycle == 0 && odd_frame_skip_pending) {
        cycle = 1;
        odd_frame_skip_pending = false;
    }

    // Clear vblank at pre-render
    if (sl == -1 && cycle == 1) {
        vblank_nmi_suppressed = true;
        nmi_output = false;
        status.sprite_zero_hit = 0;
        status.sprite_overflow = 0;
        sprite_zero_hit_pending = false;
        sprite_zero_hit_pending2 = false;
        scanline0_sprite_eval_enable = false;

        // Clear sprite shifters
        sprite_shifter_pattern_lo.fill(0);
        sprite_shifter_pattern_hi.fill(0);
    }
    if (sl == -1 && cycle == 2) {
        status.vertical_blank = 0;
        vblank_nmi_suppressed = false;
    }

    clockSpriteEvaluation(sl);

    // Render pixel (visible area)
    if (sl >= 0 && sl < 240 && cycle >= 1 && cycle <= 256) {
        uint8_t bg_pixel = 0;
        uint8_t bg_palette = 0;

        if (mask.render_background) {
            const uint16_t bit_mux = 0x8000 >> fine_x;
            uint8_t p0 = (bg_shifter_pattern_lo & bit_mux) ? 1 : 0;
            uint8_t p1 = (bg_shifter_pattern_hi & bit_mux) ? 1 : 0;
            bg_pixel = (p1 << 1) | p0;

            uint8_t a0 = (bg_shifter_attrib_lo & bit_mux) ? 1 : 0;
            uint8_t a1 = (bg_shifter_attrib_hi & bit_mux) ? 1 : 0;
            bg_palette = (a1 << 1) | a0;

            // Left-side masking
            if (cycle <= 8 && !mask.render_background_left) {
                bg_pixel = 0;
                bg_palette = 0;
            }
        }

        uint8_t spr_pixel = 0;
        uint8_t spr_palette = 0;
        uint8_t spr_priority = 0;

        if (mask.render_sprites) {
            if (cycle <= 8 && !mask.render_sprites_left) {
                // masked
            } else {
                sprite_zero_being_rendered = false;
                const uint8_t activeSprites = std::min<uint8_t>(sprite_count, 8);
                for (uint8_t i = 0; i < activeSprites; i++) {
                    if (!sprite_counter_counting[i]) {
                        uint8_t p0 = (sprite_shifter_pattern_lo[i] & 0x80) ? 1 : 0;
                        uint8_t p1 = (sprite_shifter_pattern_hi[i] & 0x80) ? 1 : 0;
                        uint8_t p = (p1 << 1) | p0;

                        if (p != 0) {
                            spr_pixel = p;
                            spr_palette = (sprite_scanline[i].attr & 0x03) + 0x04;
                            spr_priority = (sprite_scanline[i].attr & 0x20) ? 1 : 0;
                            if (i == 0) sprite_zero_being_rendered = true;
                            break;
                        }
                    }
                }
            }
        }

        uint8_t final_pixel = 0;
        uint8_t final_palette = 0;
#ifdef CIPHERNES_DEBUG_PPU_BUS
        if (((sl == 4 || sl == 5) && cycle >= 190 && cycle <= 210)
            || (sl == 4 && cycle >= 145 && cycle <= 185)) {
            std::fprintf(stderr, "PIX sl=%d cy=%d bg=%u spr=%u spr0poss=%d spr0rend=%d sx=%u sc=%u lo=%02X hi=%02X\n",
                         int(sl), int(cycle), unsigned(bg_pixel), unsigned(spr_pixel),
                         sprite_zero_hit_possible ? 1 : 0, sprite_zero_being_rendered ? 1 : 0,
                         unsigned(sprite_scanline[0].x), unsigned(sprite_counter_counting[0]),
                         unsigned(sprite_shifter_pattern_lo[0]), unsigned(sprite_shifter_pattern_hi[0]));
        }
#endif

        if (bg_pixel == 0 && spr_pixel == 0) {
            final_pixel = 0;
            final_palette = 0;
        } else if (bg_pixel == 0 && spr_pixel > 0) {
            final_pixel = spr_pixel;
            final_palette = spr_palette;
        } else if (bg_pixel > 0 && spr_pixel == 0) {
            final_pixel = bg_pixel;
            final_palette = bg_palette;
        } else {
            // Both non-zero
            if (spr_priority == 0) {
                final_pixel = spr_pixel;
                final_palette = spr_palette;
            } else {
                final_pixel = bg_pixel;
                final_palette = bg_palette;
            }

            // Sprite 0 hit
            if (sprite_zero_hit_possible && sprite_zero_being_rendered
                && !status.sprite_zero_hit
                && !sprite_zero_hit_pending
                && !sprite_zero_hit_pending2) {
                if (mask.render_background && mask.render_sprites) {
                    // More accurate: honour left masking
                    // Sprite pattern output in this scheduler reaches the pixel mixer one
                    // dot after its external X coordinate. Treat dot 9 as part of the
                    // hardware's left-eight clipping window for hit detection as well.
                    const bool leftMaskedPixel = cycle <= 9;
                    if (!(leftMaskedPixel && (!mask.render_background_left || !mask.render_sprites_left))) {
#ifdef CIPHERNES_DEBUG_SPRITE0
                        if (cycle >= 248) {
                            std::fprintf(stderr, "SPR0 sl=%d cy=%d sprx=%u bg=%u spr=%u status=%02X\n",
                                         int(sl), int(cycle),
                                         unsigned(sprite_count ? sprite_scanline[0].x : 0),
                                         unsigned(bg_pixel), unsigned(spr_pixel), unsigned(status.reg));
                        }
#endif
                        // Dot 256 cannot set sprite-zero hit. An X=$FF sprite
                        // normally becomes active only there, but it may be
                        // visible earlier when dot 339 left its counter halted.
                        if (cycle < 256 || sprite_scanline[0].x != 0xFF) {
                            sprite_zero_hit_pending = true;
                        }
                    }
                }
            }
        }

        // All frames retain their pixels for smooth reversible playback.
        frame[size_t(sl) * 256 + size_t(cycle - 1)] = getColorFromPaletteRam(final_palette, final_pixel);
    }
    applyPendingMask();

    if ((mask.render_background || mask.render_sprites) && sl >= -1 && sl < 240) {
        if ((cycle >= 2 && cycle < 258) || (cycle >= 321 && cycle < 338)) {
            updateShifters(cycle < 257);
        }
    }

    // Fetch commits follow the shifter edge on a dot. Keeping that ordering
    // is observable when a hybrid nametable fetch supplies a one-pixel tile.
    clockPpuDataBus(sl);

    if ((mask.render_background || mask.render_sprites) && sl >= -1 && sl < 240) {

        if (cycle == 256) {
            incrementScrollY();
        }

        if (cycle == 257) {
            loadBackgroundShifters();
            transferAddressX();
        }

        if (sl == -1 && cycle >= 280 && cycle < 305) {
            transferAddressY();
        }
    }

    if (!renderingEnabled() && sl >= 0 && sl < 240 && cycle >= 1 && cycle < 257) {
        const uint8_t activeSprites = std::min<uint8_t>(sprite_count, 8);
        for (uint8_t i = 0; i < activeSprites; i++) {
            if (!sprite_counter_counting[i]) continue;
            if (sprite_x_counter[i] > 0) sprite_x_counter[i]--;
            if (sprite_x_counter[i] == 0) sprite_counter_counting[i] = 0;
        }
    }

    // Sprite counters continue counting through forced blanking, but shifters
    // stop.  updateShifters() models both paths; dot 339 controls whether a
    // freshly fetched counter enters counting mode.

    // MMC3-style scanline IRQ tick (pragmatic approach)
    // Tick near the end of each visible scanline when rendering is enabled
    if (sl >= 0 && sl < 240 && cycle == 260 && (mask.render_background || mask.render_sprites)) {
        if (cartridgeUsesScanlineHook) cartridgeRaw->scanline();
    }

    // VBlank set
    if (sl == 241 && cycle == 1) {
        status.vertical_blank = 1;
        vblank_nmi_start_suppressed = true;
    }
    if (sl == 241 && cycle == 3) {
        vblank_nmi_start_suppressed = false;
    }

    if (oam_corruption_pending && renderingEnabled()
        && (((scanline >= 0 && scanline < 240) || scanline == 261) && cycle == 1)) {
        const uint8_t dst = uint8_t(oam_corruption_row << 3);
        for (uint8_t i = 0; i < 8; i++) {
            oam[uint8_t(dst + i)] = oam[i];
        }
        oam_corruption_pending = false;
    }

    if (sprite_zero_hit_pending2) {
        sprite_zero_hit_pending2 = false;
        status.sprite_zero_hit = 1;
    }
    if (sprite_zero_hit_pending) {
        sprite_zero_hit_pending = false;
        sprite_zero_hit_pending2 = true;
    }

    // Model the PPU's NMI output line (edge triggered at the CPU).
    // The line is high whenever both vblank is set and NMI is enabled.
    const bool new_nmi_output = !vblank_nmi_suppressed && !vblank_nmi_start_suppressed
        && (status.vertical_blank != 0) && (control.enable_nmi != 0);
    if (!nmi_output && new_nmi_output) {
        // Rising edge
        nmi = true;
    }
    nmi_output = new_nmi_output;

    // Advance timing
    cycle++;
    if (cycle >= 341) {
        cycle = 0;
        scanline++;
        if (scanline >= 262) {
            scanline = 0;
            odd_frame = !odd_frame;
            frame_complete = true;
        }
    }

    // Clear frame_complete once the host has had a chance to sample it
    if (frame_complete && scanline == 0 && cycle == 1) {
        frame_complete = false;
    }
}

// ------------------------------
// Save-state support
// ------------------------------
void PPU2C02::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    write_u8(out, control.reg);
    write_u8(out, mask.reg);
    write_u8(out, pending_mask_reg);
    write_u8(out, pending_mask_delay);
    write_u8(out, status.reg);
    write_u8(out, oam_addr);

    write_u16(out, vram_addr.reg);
    write_u16(out, tram_addr.reg);
    write_u8(out, fine_x);
    write_u8(out, address_latch);
    write_u8(out, ppu_data_buffer);
    write_u8(out, ppu_open_bus);
    write_u32(out, ppu_open_bus_idle_cycles);
    write_u16(out, ppu_address_bus);
    write_u8(out, ppu_octal_latch);
    write_bool(out, ppu_2007_read_sr);
    write_bytes(out, ppu_2007_read_latches.data(), ppu_2007_read_latches.size());
    write_u8(out, ppu_2007_read_pending);
    write_bool(out, ppu_2007_write_sr);
    write_bytes(out, ppu_2007_write_latches.data(), ppu_2007_write_latches.size());
    write_u8(out, ppu_2007_write_pending);
    write_u8(out, ppu_2007_write_data);
    write_u8(out, pending_vram_copy_delay);

    write_bool(out, frame_complete);
    write_bool(out, nmi);
    write_bool(out, nmi_output);

    write_bytes(out, oam.data(), oam.size());
    write_bytes(out, tbl_name.data(), sizeof(tbl_name));
    write_bytes(out, tbl_palette.data(), tbl_palette.size());

    // Pipeline
    write_u8(out, bg_next_tile_id);
    write_u8(out, bg_next_tile_attrib);
    write_u8(out, bg_next_tile_lsb);
    write_u8(out, bg_next_tile_msb);
    write_bool(out, bg_pattern_fetch_valid);

    write_u16(out, bg_shifter_pattern_lo);
    write_u16(out, bg_shifter_pattern_hi);
    write_u16(out, bg_shifter_attrib_lo);
    write_u16(out, bg_shifter_attrib_hi);

    write_bytes(out, sprite_scanline.data(), sizeof(sprite_scanline));
    write_bytes(out, secondary_oam.data(), secondary_oam.size());
    write_u8(out, secondary_oam_addr);
    write_u8(out, sprite_eval_tick);
    write_u8(out, oam_latch);
    write_u8(out, oam_read_buffer);
    write_bool(out, secondary_oam_full);
    write_bool(out, secondary_oam_addr_overflowed);
    write_bool(out, secondary_oam_fetch_frozen);
    write_bool(out, oam_addr_overflowed);
    write_bool(out, ninth_sprite_found);
    write_bool(out, next_scanline_sprite_zero);
    write_bytes(out, sprite_shifter_pattern_lo.data(), sprite_shifter_pattern_lo.size());
    write_bytes(out, sprite_shifter_pattern_hi.data(), sprite_shifter_pattern_hi.size());
    write_bytes(out, sprite_x_counter.data(), sprite_x_counter.size());
    write_bytes(out, sprite_counter_counting.data(), sprite_counter_counting.size());
    write_bool(out, sprite_size_latched_for_fetch);

    write_u8(out, sprite_count);
    write_bool(out, sprite_zero_hit_possible);
    write_bool(out, sprite_zero_being_rendered);
    write_bool(out, sprite_zero_hit_pending);
    write_bool(out, sprite_zero_hit_pending2);
    write_bool(out, oam_corruption_pending);
    write_u8(out, oam_corruption_row);
    write_bool(out, scanline0_sprite_eval_enable);
    write_bool(out, vblank_nmi_suppressed);
    write_bool(out, vblank_nmi_start_suppressed);

    // Timing
    write_pod(out, scanline);
    write_pod(out, cycle);
    write_bool(out, odd_frame);

    // NOTE: framebuffer is derived; we don't store it to keep states smaller.
}

bool PPU2C02::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    if (!read_u8(p,end,control.reg)) return false;
    if (!read_u8(p,end,mask.reg)) return false;
    if (p < end) {
        if (!read_u8(p,end,pending_mask_reg)) return false;
        if (!read_u8(p,end,pending_mask_delay)) return false;
    } else {
        pending_mask_reg = mask.reg;
        pending_mask_delay = 0;
    }
    if (!read_u8(p,end,status.reg)) return false;
    if (!read_u8(p,end,oam_addr)) return false;

    if (!read_u16(p,end,vram_addr.reg)) return false;
    if (!read_u16(p,end,tram_addr.reg)) return false;
    if (!read_u8(p,end,fine_x)) return false;
    if (!read_u8(p,end,address_latch)) return false;
    if (!read_u8(p,end,ppu_data_buffer)) return false;
    if (!read_u8(p,end,ppu_open_bus)) return false;
    if (!read_u32(p,end,ppu_open_bus_idle_cycles)) return false;
    // Existing save states serialize one decay timer. Reconstruct all eight
    // independent latches from that conservative compatibility value.
    ppu_open_bus_bit_idle_cycles.fill(ppu_open_bus_idle_cycles);
    if (ppu_open_bus_idle_cycles >= PPU_OPEN_BUS_DECAY_CYCLES) {
        ppu_open_bus = 0;
        ppu_open_bus_decay_active_bits = 0;
    } else {
        ppu_open_bus_decay_active_bits = 0xFF;
    }
    if (!read_u16(p,end,ppu_address_bus)) return false;
    if (!read_u8(p,end,ppu_octal_latch)) return false;
    if (!read_bool(p,end,ppu_2007_read_sr)) return false;
    if (!read_bytes(p,end,ppu_2007_read_latches.data(),ppu_2007_read_latches.size())) return false;
    if (!read_u8(p,end,ppu_2007_read_pending)) return false;
    // This is a sub-dot transient and is intentionally not part of the state
    // file format. Save states are taken between host frames, where no active
    // ALE feedback pulse can be pending.
    ppu_2007_feedback_hold = false;
    if (!read_bool(p,end,ppu_2007_write_sr)) return false;
    if (!read_bytes(p,end,ppu_2007_write_latches.data(),ppu_2007_write_latches.size())) return false;
    if (!read_u8(p,end,ppu_2007_write_pending)) return false;
    if (!read_u8(p,end,ppu_2007_write_data)) return false;
    if (!read_u8(p,end,pending_vram_copy_delay)) return false;
    ppu_2007_io_active =
        ppu_2007_feedback_hold
        || ppu_2007_read_pending != 0
        || ppu_2007_read_sr
        || ppu_2007_write_pending != 0
        || ppu_2007_write_sr
        || pending_vram_copy_delay != 0
        || ppu_2007_read_latches[0] != 0
        || ppu_2007_read_latches[1] != 1
        || ppu_2007_read_latches[2] != 0
        || ppu_2007_read_latches[3] != 1
        || ppu_2007_read_latches[4] != 0
        || ppu_2007_write_latches[0] != 0
        || ppu_2007_write_latches[1] != 1
        || ppu_2007_write_latches[2] != 0
        || ppu_2007_write_latches[3] != 1
        || ppu_2007_write_latches[4] != 0;

    if (!read_bool(p,end,frame_complete)) return false;
    if (!read_bool(p,end,nmi)) return false;
    if (!read_bool(p,end,nmi_output)) return false;

    if (!read_bytes(p,end,oam.data(),oam.size())) return false;
    if (!read_bytes(p,end,tbl_name.data(),sizeof(tbl_name))) return false;
    if (!read_bytes(p,end,tbl_palette.data(),tbl_palette.size())) return false;
    rebuildPaletteColorCache();

    if (!read_u8(p,end,bg_next_tile_id)) return false;
    if (!read_u8(p,end,bg_next_tile_attrib)) return false;
    if (!read_u8(p,end,bg_next_tile_lsb)) return false;
    if (!read_u8(p,end,bg_next_tile_msb)) return false;
    if (!read_bool(p,end,bg_pattern_fetch_valid)) return false;

    if (!read_u16(p,end,bg_shifter_pattern_lo)) return false;
    if (!read_u16(p,end,bg_shifter_pattern_hi)) return false;
    if (!read_u16(p,end,bg_shifter_attrib_lo)) return false;
    if (!read_u16(p,end,bg_shifter_attrib_hi)) return false;

    if (!read_bytes(p,end,sprite_scanline.data(),sizeof(sprite_scanline))) return false;
    if (!read_bytes(p,end,secondary_oam.data(),secondary_oam.size())) return false;
    if (!read_u8(p,end,secondary_oam_addr)) return false;
    if (!read_u8(p,end,sprite_eval_tick)) return false;
    if (!read_u8(p,end,oam_latch)) return false;
    if (!read_u8(p,end,oam_read_buffer)) return false;
    // Reconstruct the preceding-dot sample; it is derived from the serialized
    // current OAM read buffer and does not need its own state field.
    oam_read_buffer_prev = oam_read_buffer;
    if (!read_bool(p,end,secondary_oam_full)) return false;
    if (!read_bool(p,end,secondary_oam_addr_overflowed)) return false;
    if (!read_bool(p,end,secondary_oam_fetch_frozen)) return false;
    if (!read_bool(p,end,oam_addr_overflowed)) return false;
    if (!read_bool(p,end,ninth_sprite_found)) return false;
    if (!read_bool(p,end,next_scanline_sprite_zero)) return false;
    if (!read_bytes(p,end,sprite_shifter_pattern_lo.data(),sprite_shifter_pattern_lo.size())) return false;
    if (!read_bytes(p,end,sprite_shifter_pattern_hi.data(),sprite_shifter_pattern_hi.size())) return false;
    if (!read_bytes(p,end,sprite_x_counter.data(),sprite_x_counter.size())) return false;
    if (!read_bytes(p,end,sprite_counter_counting.data(),sprite_counter_counting.size())) return false;
    if (!read_bool(p,end,sprite_size_latched_for_fetch)) return false;

    if (!read_u8(p,end,sprite_count)) return false;
    if (!read_bool(p,end,sprite_zero_hit_possible)) return false;
    if (!read_bool(p,end,sprite_zero_being_rendered)) return false;
    if (!read_bool(p,end,sprite_zero_hit_pending)) return false;
    if (!read_bool(p,end,sprite_zero_hit_pending2)) return false;
    if (!read_bool(p,end,oam_corruption_pending)) return false;
    if (!read_u8(p,end,oam_corruption_row)) return false;
    if (!read_bool(p,end,scanline0_sprite_eval_enable)) return false;
    if (!read_bool(p,end,vblank_nmi_suppressed)) return false;
    if (!read_bool(p,end,vblank_nmi_start_suppressed)) return false;

    if (!read_pod(p,end,scanline)) return false;
    if (!read_pod(p,end,cycle)) return false;
    if (!read_bool(p,end,odd_frame)) return false;
    // This sub-frame latch is consumed before the host-visible frame boundary,
    // so it is intentionally reconstructed as inactive after loading a state.
    odd_frame_skip_pending = false;

    return true;
}
