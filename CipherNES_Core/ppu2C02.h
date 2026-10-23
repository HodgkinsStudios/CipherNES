#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

class Cartridge;

// NES PPU (2C02) - cycle-accurate core
class PPU2C02 {
public:
    void connectCartridge(const std::shared_ptr<Cartridge>& cart);

    // CPU register access ($2000-$2007)
    uint8_t cpuRead(uint16_t addr, bool readOnly = false);
    void cpuWrite(uint16_t addr, uint8_t data);
    void dmaWriteOam(uint8_t data);

    // PPU bus access ($0000-$3FFF)
    uint8_t ppuRead(uint16_t addr, bool readOnly = false);
    void ppuWrite(uint16_t addr, uint8_t data);

    void reset();
    void clock();
    int16_t currentScanline() const { return scanline; }
    int16_t currentCycle() const { return cycle; }

    // Frame output (ARGB)
    bool frame_complete = false;
    const std::array<uint32_t, 256 * 240>& getFrame() const { return frame; }


    // Latched high when PPU triggers an NMI (vblank start + enabled)
    bool nmi = false;
    bool nmiLineActive() const { return nmi_output; }

    // Exposed OAM for DMA
    std::array<uint8_t, 256> oam{};

    // Save-state support
    void saveState(std::vector<uint8_t>& out) const;
    bool loadState(const uint8_t*& p, const uint8_t* end);

private:
    std::shared_ptr<Cartridge> cartridge;
    Cartridge* cartridgeRaw = nullptr;
    bool cartridgeUsesFetchHooks = false;
    bool cartridgeUsesScanlineHook = false;
    bool cartridgeUsesPpuAddressBus = false;
    bool cartridgeHandlesExternalNametables = false;
    bool cartridgeUsesDynamicMirroring = false;
    uint8_t cartridgeFixedMirroring = 1;
    // Predecoded CIRAM table mapping for cartridges whose mirroring is fixed.
    // Dynamic-mirroring mappers still take the exact runtime path.
    std::array<uint8_t, 4> cartridgeFixedNametableMap{{0, 1, 0, 1}};

    // -----------------
    // PPU registers
    // -----------------
    union PPUCTRL {
        struct {
            uint8_t nametable_x : 1;
            uint8_t nametable_y : 1;
            uint8_t increment_mode : 1;
            uint8_t pattern_sprite : 1;
            uint8_t pattern_background : 1;
            uint8_t sprite_size : 1;
            uint8_t slave_mode : 1;
            uint8_t enable_nmi : 1;
        };
        uint8_t reg = 0x00;
    } control;

    union PPUMASK {
        struct {
            uint8_t grayscale : 1;
            uint8_t render_background_left : 1;
            uint8_t render_sprites_left : 1;
            uint8_t render_background : 1;
            uint8_t render_sprites : 1;
            uint8_t enhance_red : 1;
            uint8_t enhance_green : 1;
            uint8_t enhance_blue : 1;
        };
        uint8_t reg = 0x00;
    } mask;

    uint8_t pending_mask_reg = 0x00;
    uint8_t pending_mask_delay = 0x00;

    union PPUSTATUS {
        struct {
            uint8_t unused : 5;
            uint8_t sprite_overflow : 1;
            uint8_t sprite_zero_hit : 1;
            uint8_t vertical_blank : 1;
        };
        uint8_t reg = 0x00;
    } status;

    uint8_t oam_addr = 0x00;

    // VRAM address registers
    union LoopyRegister {
        struct {
            uint16_t coarse_x : 5;
            uint16_t coarse_y : 5;
            uint16_t nametable_x : 1;
            uint16_t nametable_y : 1;
            uint16_t fine_y : 3;
            uint16_t unused : 1;
        };
        uint16_t reg = 0x0000;
    } vram_addr, tram_addr;

    uint8_t fine_x = 0x00;
    uint8_t address_latch = 0x00;
    uint8_t ppu_data_buffer = 0x00;

    // The 2C02 multiplexes the low eight external address pins with the data
    // bus.  An ALE dot captures the low address in an external octal latch;
    // the following dot reads through that latch.  $2007 has its own
    // five-latch state machines which can overlap the rendering cadence.
    uint16_t ppu_address_bus = 0x0000;
    uint8_t ppu_octal_latch = 0x00;
    bool ppu_2007_read_sr = false;
    std::array<uint8_t, 5> ppu_2007_read_latches{{0, 1, 0, 1, 0}};
    uint8_t ppu_2007_read_pending = 0;
    bool ppu_2007_feedback_hold = false;
    bool ppu_2007_write_sr = false;
    std::array<uint8_t, 5> ppu_2007_write_latches{{0, 1, 0, 1, 0}};
    uint8_t ppu_2007_write_pending = 0;
    uint8_t ppu_2007_write_data = 0x00;
    uint8_t pending_vram_copy_delay = 0;
    // Derived host-side fast-path flag; reconstructed after reset/state load.
    // It is not serialized because the latch state above is authoritative.
    bool ppu_2007_io_active = false;

    // "Open bus" (last value driven on the PPU data bus). Used for
    // PPUSTATUS low bits and other bus-quirk behaviours.
    uint8_t ppu_open_bus = 0x00;
    // Each PPU I/O bus bit has an independent dynamic decay latch. Keep the
    // original scalar as bit 0's serialized compatibility value; the complete
    // per-bit state is reconstructed conservatively when loading old states.
    uint32_t ppu_open_bus_idle_cycles = 0;
    std::array<uint32_t, 8> ppu_open_bus_bit_idle_cycles{};
    // Bits whose independent dynamic decay timers still need clocking. Most
    // PPU dots do not touch the CPU-visible I/O bus, so this mask avoids eight
    // saturated timer checks on every one of the PPU's ~5.37 million dots/sec.
    uint8_t ppu_open_bus_decay_active_bits = 0;

    // Track NMI output line level to generate edge-triggered NMIs.
    bool nmi_output = false;

    // -----------------
    // Memory
    // -----------------
    std::array<std::array<uint8_t, 1024>, 4> tbl_name{};   // 4 nametables (supports four-screen + one-screen)
    // NTSC 2C02 power-up palette observed by the Blargg hardware test.
    // Palette RAM is not cleared by reset, so these values are used only for
    // construction/power-up and then persist across soft resets.
    std::array<uint8_t, 32> tbl_palette{{
        0x09,0x01,0x00,0x01,0x00,0x02,0x02,0x0D,
        0x08,0x10,0x08,0x24,0x00,0x00,0x04,0x2C,
        0x09,0x01,0x34,0x03,0x00,0x04,0x00,0x14,
        0x08,0x3A,0x00,0x02,0x00,0x20,0x2C,0x08
    }};
    // Host-side decoded ARGB values for the 32 palette RAM entries. This is
    // derived from tbl_palette + the grayscale mask and is never serialized.
    std::array<uint32_t, 32> palette_color_cache{};

    // Framebuffer
    std::array<uint32_t, 256 * 240> frame{};

    // -----------------
    // Rendering pipeline
    // -----------------
    uint8_t bg_next_tile_id = 0x00;
    uint8_t bg_next_tile_attrib = 0x00;
    uint8_t bg_next_tile_lsb = 0x00;
    uint8_t bg_next_tile_msb = 0x00;
    bool bg_pattern_fetch_valid = false;

    uint16_t bg_shifter_pattern_lo = 0x0000;
    uint16_t bg_shifter_pattern_hi = 0x0000;
    uint16_t bg_shifter_attrib_lo = 0x0000;
    uint16_t bg_shifter_attrib_hi = 0x0000;

    struct SpriteEntry {
        uint8_t y = 0;
        uint8_t id = 0;
        uint8_t attr = 0;
        uint8_t x = 0;
    };

    std::array<SpriteEntry, 8> sprite_scanline{};
    // The 2C02 does not discover a scanline's sprites all at once.  Dots
    // 1-64 clear secondary OAM and dots 65-256 copy/evaluate primary OAM one
    // byte every two dots.  Keeping that pipeline visible is required for
    // timed $2004 reads and for the sprite flag set races.
    std::array<uint8_t, 32> secondary_oam{};
    uint8_t secondary_oam_addr = 0;
    uint8_t sprite_eval_tick = 0;
    uint8_t oam_latch = 0xFF;
    uint8_t oam_read_buffer = 0xFF;
    uint8_t oam_read_buffer_prev = 0xFF;
    bool secondary_oam_full = false;
    // Hardware OAM2ADDR has a separate overflow latch. Once raised, further
    // OAM2 address increments are inhibited until one of the sprite-engine
    // reset strobes (dots 63, 255, or 339) occurs while rendering is active.
    // AccuracyCoin 144 exercises this independently from the ordinary
    // "secondary OAM is full" evaluation state.
    bool secondary_oam_addr_overflowed = false;
    // Snapshot of the OAM2 overflow/freeze latch at sprite-fetch entry.
    // A wrap that occurs naturally near the end of the current fetch must not
    // retroactively turn that same fetch into a frozen-byte fetch; it affects
    // the following fetch only if the reset strobes are skipped.
    bool secondary_oam_fetch_frozen = false;
    bool oam_addr_overflowed = false;
    bool ninth_sprite_found = false;
    bool next_scanline_sprite_zero = false;
    std::array<uint8_t, 8> sprite_shifter_pattern_lo{};
    std::array<uint8_t, 8> sprite_shifter_pattern_hi{};
    std::array<uint8_t, 8> sprite_x_counter{};
    std::array<uint8_t, 8> sprite_counter_counting{};
    bool sprite_size_latched_for_fetch = false;

    uint8_t sprite_count = 0;
    bool sprite_zero_hit_possible = false;
    bool sprite_zero_being_rendered = false;
    bool sprite_zero_hit_pending = false;
    bool sprite_zero_hit_pending2 = false;
    bool oam_corruption_pending = false;
    uint8_t oam_corruption_row = 0;
    bool scanline0_sprite_eval_enable = false;
    bool vblank_nmi_suppressed = false;
    bool vblank_nmi_start_suppressed = false;

    // -----------------
    // Timing
    // -----------------
    int16_t scanline = 0;
    int16_t cycle = 0;
    bool odd_frame = false;
    bool odd_frame_skip_pending = false;

private:
    // Helpers
    bool renderingEnabled() const { return mask.render_background || mask.render_sprites; }
    bool renderingFetchActive() const {
        return renderingEnabled() && ((scanline >= 0 && scanline < 240) || scanline == 261);
    }

    void incrementScrollX();
    void incrementScrollY();
    void transferAddressX();
    void transferAddressY();

    void loadBackgroundShifters();
    void updateShifters(bool shiftSprites);
    void clockSpriteEvaluation(int16_t sl);
    void clockPpuDataBus(int16_t sl);
    void clockRenderingDataBus(int16_t sl);
    uint8_t fetchExternalPpuBus(uint8_t fetchKind = 0);
    uint16_t spritePatternAddress(uint8_t slot, int16_t sl, bool highPlane) const;

    uint8_t readOAM(uint8_t index) const;
    uint8_t cpuReadOAMData() const;
    bool oamWriteBlockedByRendering() const;

    uint32_t getColorFromPaletteRam(uint8_t palette, uint8_t pixel);
    void rebuildPaletteColorCache();
    void updatePaletteColorCacheEntry(uint8_t addr);
    void driveOpenBus(uint8_t value, uint8_t mask = 0xFF);
    uint8_t currentOpenBus() const;
};
