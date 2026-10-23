#pragma once
#include <cstdint>

#if defined(_WIN32)
  #if defined(CIPHERNES_STATIC)
    #define CIPHERNES_API extern "C"
  #elif defined(CIPHERNES_CORE_BUILD)
    #define CIPHERNES_API extern "C" __declspec(dllexport)
  #else
    #define CIPHERNES_API extern "C" __declspec(dllimport)
  #endif
#else
  #define CIPHERNES_API extern "C" __attribute__((visibility("default")))
#endif

// Opaque handle to the emulator core
typedef struct CIPHERNES_Handle CIPHERNES_Handle;

// API versioning
// Increment CIPHERNES_API_VERSION whenever you change any exported function signature
// or exported struct layout.
#define CIPHERNES_API_VERSION 10
#define CIPHERNES_STATE_VERSION 6

// Buttons bitmask (per controller port)
enum CIPHERNES_Button : uint8_t {
    CIPHERNES_BTN_A      = 0x80,
    CIPHERNES_BTN_B      = 0x40,
    CIPHERNES_BTN_SELECT = 0x20,
    CIPHERNES_BTN_START  = 0x10,
    CIPHERNES_BTN_UP     = 0x08,
    CIPHERNES_BTN_DOWN   = 0x04,
    CIPHERNES_BTN_LEFT   = 0x02,
    CIPHERNES_BTN_RIGHT  = 0x01,
};

// Lifecycle
CIPHERNES_API CIPHERNES_Handle* ciphernes_create();
CIPHERNES_API void          ciphernes_destroy(CIPHERNES_Handle* h);

// Version
CIPHERNES_API int           ciphernes_get_api_version();
CIPHERNES_API const char*   ciphernes_get_core_version_string();

// ROM / reset
CIPHERNES_API int  ciphernes_load_rom(CIPHERNES_Handle* h, const char* nes_file_path); // returns 1 on success
// Loads a complete iNES/NES 2.0 image directly from caller-owned memory. The core
// copies all required bytes before returning. virtual_path is used only as the
// display/save identity (for example, an original .gb path); no persistent .nes file is kept.
CIPHERNES_API int  ciphernes_load_rom_image(CIPHERNES_Handle* h, const uint8_t* image_data, uint32_t image_bytes, const char* virtual_path);
CIPHERNES_API void ciphernes_unload_rom(CIPHERNES_Handle* h);

// Soft reset = like pressing reset button; Hard reset = power-cycle-ish
CIPHERNES_API void ciphernes_reset_soft(CIPHERNES_Handle* h);
CIPHERNES_API void ciphernes_reset_hard(CIPHERNES_Handle* h);

// Famicom Disk System controls. Side indices are zero-based: disk 1 side A=0,
// disk 1 side B=1, disk 2 side A=2, etc. current_side returns -1 if ejected.
CIPHERNES_API int ciphernes_is_fds(CIPHERNES_Handle* h);
CIPHERNES_API int ciphernes_fds_get_side_count(CIPHERNES_Handle* h);
CIPHERNES_API int ciphernes_fds_get_current_side(CIPHERNES_Handle* h);
CIPHERNES_API void ciphernes_fds_eject(CIPHERNES_Handle* h);
CIPHERNES_API int ciphernes_fds_insert_next_side(CIPHERNES_Handle* h);

// Timing
// Run until a full frame is produced (PPU frame_complete). Returns number of master ticks executed.
CIPHERNES_API uint64_t ciphernes_run_frame(CIPHERNES_Handle* h);

// Atomically applies both controller states and executes exactly one frame.
// This is the preferred deterministic stepping primitive for automation and headless hosts.
CIPHERNES_API uint64_t ciphernes_run_frame_with_input(CIPHERNES_Handle* h, uint8_t port1_buttons, uint8_t port2_buttons);

// Input
// controller_port: 0 or 1
// buttons: bitmask using CIPHERNES_Button
CIPHERNES_API void ciphernes_set_controller(CIPHERNES_Handle* h, int controller_port, uint8_t buttons);

// Video
// Returns pointer to 256*240 ARGB32 pixels owned by the core (valid until destroy).
CIPHERNES_API const uint32_t* ciphernes_get_framebuffer(CIPHERNES_Handle* h);

// Audio
// Configure output sample rate (e.g. 44100). Call after create and whenever you change rate.
CIPHERNES_API void ciphernes_set_audio_sample_rate(CIPHERNES_Handle* h, double sample_rate_hz);

// Faster, UI-friendly block pull in signed 16-bit PCM (mono).
// Returns number of samples written.
CIPHERNES_API int ciphernes_take_audio_samples_i16(CIPHERNES_Handle* h, int16_t* out_samples, int max_samples);

// Enable/disable audio generation (useful during rewind/fast-forward).
CIPHERNES_API void ciphernes_set_audio_enabled(CIPHERNES_Handle* h, int enabled);
CIPHERNES_API void ciphernes_set_apu_channel_mask(CIPHERNES_Handle* h, uint8_t mask);

// Save-states (required for rewind, save slots, TAS workflows)
// Query required size (bytes) for the current ROM state.
CIPHERNES_API uint32_t ciphernes_get_state_size(CIPHERNES_Handle* h);
// Writes a full state into out_state (capacity = out_capacity). Returns bytes written, or 0 on failure.
CIPHERNES_API uint32_t ciphernes_save_state_transient(CIPHERNES_Handle* h, uint8_t* out_state, uint32_t out_capacity);
CIPHERNES_API uint32_t ciphernes_save_state(CIPHERNES_Handle* h, uint8_t* out_state, uint32_t out_capacity);
// Loads a full state from state bytes. Returns 1 on success.
CIPHERNES_API int      ciphernes_load_state(CIPHERNES_Handle* h, const uint8_t* state, uint32_t state_size);
// State format version (changes when the binary format changes)
// Cheats / patches
// Provide Game Genie codes (6 or 8 chars). Returns number of codes successfully parsed/applied.
CIPHERNES_API int  ciphernes_set_game_genie_codes(CIPHERNES_Handle* h, const char** codes_utf8, int count);
CIPHERNES_API void ciphernes_clear_game_genie(CIPHERNES_Handle* h);

// Info
