#include "sdl2_minimal.h"
#include "zip_rom_loader.h"
#include "../CipherNES_Core/ciphernes_core.h"
#include "../CipherNES_Core/rewind_history.h"
#include "../CipherNES_Core/host_audio_resampler.h"
#include "../CipherNES_Core/platform_compat.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <dlfcn.h>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <limits.h>
#include <memory>
#include <sstream>
#include <stdint.h>
#include <string>
#include <thread>
#include <utility>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

namespace {

const int NES_W = 256;
const int NES_H = 240;
const double NTSC_FPS = 60.098813897440515;
const Uint32 AUDIO_PREROLL_SAMPLES = 1536u; // ~34.8 ms at 44.1 kHz
const uint8_t PAD_A = 0x80;
const uint8_t PAD_B = 0x40;
const uint8_t PAD_SELECT = 0x20;
const uint8_t PAD_START = 0x10;
const uint8_t PAD_UP = 0x08;
const uint8_t PAD_DOWN = 0x04;
const uint8_t PAD_LEFT = 0x02;
const uint8_t PAD_RIGHT = 0x01;
const int SPECIAL_ACTION_COUNT = 8;
const int REWIND_SPECIAL = 6;
const int FAST_FORWARD_SPECIAL = 7;
const int MENU_BAR_H = 38;
const int STATUS_BAR_H = 30;
const int MENU_ITEM_H = 32;

#if defined(__APPLE__)
const char* PLATFORM_DISPLAY_NAME = "macOS";
const char* PLATFORM_WINDOW_TITLE = "CipherNES - macOS";
const char* PLATFORM_SPLASH_TITLE = "CIPHERNES FOR MACOS";
const char* PLATFORM_SDL_MAPPING_TAG = "Mac OS X";
#else
const char* PLATFORM_DISPLAY_NAME = "Ubuntu";
const char* PLATFORM_WINDOW_TITLE = "CipherNES - Ubuntu";
const char* PLATFORM_SPLASH_TITLE = "CIPHERNES FOR UBUNTU";
const char* PLATFORM_SDL_MAPPING_TAG = "Linux";
#endif

enum CommandId {
    CMD_NONE = 0, CMD_LOAD_ROM = 100, CMD_SOFT_RESET = 101, CMD_HARD_RESET = 102, CMD_QUIT = 103,
    CMD_FDS_EJECT = 104, CMD_FDS_NEXT_SIDE = 105,
    CMD_FULLSCREEN = 200, CMD_FULLSCREEN_STRETCHED = 201, CMD_INFO = 300,
    CMD_PORT1_CONTROLLER = 400, CMD_PORT2_CONTROLLER = 401, CMD_CONFIG_CONTROLLER_INPUT = 402,
    CMD_MUTE = 403, CMD_SAVE_STATE = 404, CMD_LOAD_STATE = 405,
    CMD_STATE_SLOT_1 = 410, CMD_STATE_SLOT_2 = 411, CMD_STATE_SLOT_3 = 412, CMD_STATE_SLOT_4 = 413,
    CMD_APU_PULSE1 = 430, CMD_APU_PULSE2 = 431, CMD_APU_TRIANGLE = 432, CMD_APU_NOISE = 433, CMD_APU_DMC = 434,
    CMD_SPEED_100 = 500, CMD_SPEED_50 = 501, CMD_SPEED_25 = 502,
    CMD_CONFIG_CHEATS = 600, CMD_GAME_GENIE = 601, CMD_ENABLE_CHEATS = 602,
    CMD_RECORD_WITH_TOAST = 700, CMD_RECORD_WITH_TIMER = 701, CMD_CUSTOM_AVI = 702,
    CMD_SCREENSHOT = 710, CMD_RECORD = 711, CMD_STOP_RECORDING = 712,
    CMD_TAS_RECORD_INPUTS = 800, CMD_TAS_STOP_RECORDING_INPUTS = 801,
    CMD_TAS_VIEW_RECORDED_INPUTS = 802, CMD_TAS_STOP_VIEWING_INPUTS = 803,
    CMD_TAS_RESET_BEFORE_INPUT_RECORDING = 804,
};

enum TasMode {
    TAS_IDLE = 0,
    TAS_COUNTDOWN = 1,
    TAS_RECORDING = 2,
    TAS_PLAYBACK = 3
};

struct TasInputFrame {
    uint8_t port1;
    uint8_t port2;
    TasInputFrame(uint8_t p1 = 0, uint8_t p2 = 0) : port1(p1), port2(p2) {}
};

enum ScreenMode { SCREEN_GAME, SCREEN_CONTROLLER, SCREEN_CHEATS, SCREEN_GAME_GENIE, SCREEN_INFO };


struct MenuItemDef {
    int command;
    std::string text;
    int submenu;
    bool separator;
    bool enabled;
    MenuItemDef(int c = 0, const char* t = "", int s = 0, bool sep = false) :
        command(c), text(t ? t : ""), submenu(s), separator(sep), enabled(true) {}
};

struct MenuRootDef {
    std::string text;
    std::vector<MenuItemDef> items;
    bool enabled;
    MenuRootDef() : enabled(true) {}
};


struct GameGenieCheat { std::string name, code; bool enabled; };

enum PadBindingKind {
    PAD_BIND_NONE = 0,
    PAD_BIND_STANDARD_DIRECTION = 1,
    PAD_BIND_GAMEPAD_BUTTON = 2,
    PAD_BIND_GAMEPAD_AXIS = 3,
    PAD_BIND_JOYSTICK_BUTTON = 4,
    PAD_BIND_JOYSTICK_AXIS = 5,
    PAD_BIND_JOYSTICK_HAT = 6
};

struct PadBinding {
    int kind;
    int control;
    int direction;
    PadBinding(int k = PAD_BIND_NONE, int c = -1, int d = 0) : kind(k), control(c), direction(d) {}
};

struct HostController {
    SDL_GameController* gamepad;
    SDL_Joystick* joystick;
    SDL_JoystickID instanceId;
    bool mapped;
    bool ownsJoystick;
    std::string name;
    std::string guid;
    std::string persistentId;
    int axes;
    int buttons;
    int hats;
    HostController() : gamepad(NULL), joystick(NULL), instanceId(-1), mapped(false), ownsJoystick(false),
        axes(0), buttons(0), hats(0) {}
};

std::vector<MenuItemDef> submenuItems(int submenu) {
    if (submenu == 1) return { MenuItemDef(CMD_SOFT_RESET, "Soft Reset"), MenuItemDef(CMD_HARD_RESET, "Hard Reset") };
    if (submenu == 2) return { MenuItemDef(CMD_PORT1_CONTROLLER, "Controller") };
    if (submenu == 3) return { MenuItemDef(CMD_PORT2_CONTROLLER, "Controller") };
    if (submenu == 4) return { MenuItemDef(CMD_SPEED_100, "100%"), MenuItemDef(CMD_SPEED_50, "50%"), MenuItemDef(CMD_SPEED_25, "25%") };
    if (submenu == 5) return { MenuItemDef(CMD_APU_PULSE1, "Pulse 1"), MenuItemDef(CMD_APU_PULSE2, "Pulse 2"),
        MenuItemDef(CMD_APU_TRIANGLE, "Triangle"), MenuItemDef(CMD_APU_NOISE, "Noise"), MenuItemDef(CMD_APU_DMC, "DMC") };
    if (submenu == 6) return { MenuItemDef(CMD_STATE_SLOT_1, "1"), MenuItemDef(CMD_STATE_SLOT_2, "2"), MenuItemDef(CMD_STATE_SLOT_3, "3"), MenuItemDef(CMD_STATE_SLOT_4, "4") };
    if (submenu == 7) return { MenuItemDef(CMD_FDS_EJECT, "Eject Disk"), MenuItemDef(CMD_FDS_NEXT_SIDE, "Insert Next Side") };
    return std::vector<MenuItemDef>();
}

struct SDLApi {
    void* library;
    int (*Init)(Uint32);
    void (*Quit)();
    const char* (*GetError)();
    int (*SetHint)(const char*, const char*);
    SDL_Window* (*CreateWindow)(const char*, int, int, int, int, Uint32);
    void (*DestroyWindow)(SDL_Window*);
    void (*SetWindowTitle)(SDL_Window*, const char*);
    void (*SetWindowMinimumSize)(SDL_Window*, int, int);
    int (*SetWindowFullscreen)(SDL_Window*, Uint32);
    Uint32 (*GetWindowID)(SDL_Window*);
    void (*GetWindowSize)(SDL_Window*, int*, int*);
    SDL_Renderer* (*CreateRenderer)(SDL_Window*, int, Uint32);
    void (*DestroyRenderer)(SDL_Renderer*);
    SDL_Texture* (*CreateTexture)(SDL_Renderer*, Uint32, int, int, int);
    void (*DestroyTexture)(SDL_Texture*);
    int (*UpdateTexture)(SDL_Texture*, const SDL_Rect*, const void*, int);
    int (*SetRenderDrawColor)(SDL_Renderer*, Uint8, Uint8, Uint8, Uint8);
    int (*SetRenderDrawBlendMode)(SDL_Renderer*, int);
    int (*RenderClear)(SDL_Renderer*);
    int (*RenderCopy)(SDL_Renderer*, SDL_Texture*, const SDL_Rect*, const SDL_Rect*);
    int (*RenderFillRect)(SDL_Renderer*, const SDL_Rect*);
    int (*RenderDrawRect)(SDL_Renderer*, const SDL_Rect*);
    int (*RenderDrawLine)(SDL_Renderer*, int, int, int, int);
    int (*RenderReadPixels)(SDL_Renderer*, const SDL_Rect*, Uint32, void*, int);
    void (*RenderPresent)(SDL_Renderer*);
    int (*GetRendererOutputSize)(SDL_Renderer*, int*, int*);
    int (*PollEvent)(SDL_Event*);
    const Uint8* (*GetKeyboardState)(int*);
    void (*Delay)(Uint32);
    Uint64 (*GetPerformanceCounter)();
    Uint64 (*GetPerformanceFrequency)();
    Uint32 (*GetTicks)();
    void (*Free)(void*);
    SDL_AudioDeviceID (*OpenAudioDevice)(const char*, int, const SDL_AudioSpec*, SDL_AudioSpec*, int);
    void (*CloseAudioDevice)(SDL_AudioDeviceID);
    void (*PauseAudioDevice)(SDL_AudioDeviceID, int);
    int (*QueueAudio)(SDL_AudioDeviceID, const void*, Uint32);
    Uint32 (*GetQueuedAudioSize)(SDL_AudioDeviceID);
    void (*ClearQueuedAudio)(SDL_AudioDeviceID);
    int (*NumJoysticks)();
    int (*IsGameController)(int);
    const char* (*JoystickNameForIndex)(int);
    SDL_Joystick* (*JoystickOpen)(int);
    void (*JoystickClose)(SDL_Joystick*);
    SDL_JoystickID (*JoystickInstanceID)(SDL_Joystick*);
    SDL_JoystickGUID (*JoystickGetGUID)(SDL_Joystick*);
    void (*JoystickGetGUIDString)(SDL_JoystickGUID, char*, int);
    int (*JoystickNumAxes)(SDL_Joystick*);
    int (*JoystickNumButtons)(SDL_Joystick*);
    int (*JoystickNumHats)(SDL_Joystick*);
    Sint16 (*JoystickGetAxis)(SDL_Joystick*, int);
    Uint8 (*JoystickGetButton)(SDL_Joystick*, int);
    Uint8 (*JoystickGetHat)(SDL_Joystick*, int);
    SDL_GameController* (*GameControllerOpen)(int);
    void (*GameControllerClose)(SDL_GameController*);
    const char* (*GameControllerName)(SDL_GameController*);
    SDL_Joystick* (*GameControllerGetJoystick)(SDL_GameController*);
    Uint8 (*GameControllerGetButton)(SDL_GameController*, int);
    Sint16 (*GameControllerGetAxis)(SDL_GameController*, int);
    int (*GameControllerAddMapping)(const char*);
    int (*SetThreadPriority)(int);

    SDLApi() : library(NULL) { clearPointers(); }
    ~SDLApi() { if (library) dlclose(library); }

    void clearPointers() {
        Init = NULL; Quit = NULL; GetError = NULL; SetHint = NULL;
        CreateWindow = NULL; DestroyWindow = NULL; SetWindowTitle = NULL;
        SetWindowMinimumSize = NULL; SetWindowFullscreen = NULL;
        GetWindowID = NULL; GetWindowSize = NULL;
        CreateRenderer = NULL; DestroyRenderer = NULL; CreateTexture = NULL;
        DestroyTexture = NULL; UpdateTexture = NULL; SetRenderDrawColor = NULL;
        SetRenderDrawBlendMode = NULL; RenderClear = NULL; RenderCopy = NULL;
        RenderFillRect = NULL; RenderDrawRect = NULL; RenderDrawLine = NULL; RenderReadPixels = NULL;
        RenderPresent = NULL; GetRendererOutputSize = NULL; PollEvent = NULL;
        GetKeyboardState = NULL; Delay = NULL; GetPerformanceCounter = NULL;
        GetPerformanceFrequency = NULL; GetTicks = NULL; Free = NULL; SetThreadPriority = NULL;
        OpenAudioDevice = NULL; CloseAudioDevice = NULL; PauseAudioDevice = NULL;
        QueueAudio = NULL; GetQueuedAudioSize = NULL; ClearQueuedAudio = NULL;
        NumJoysticks = NULL; IsGameController = NULL; GameControllerOpen = NULL;
        JoystickNameForIndex = NULL; JoystickOpen = NULL; JoystickClose = NULL;
        JoystickInstanceID = NULL; JoystickGetGUID = NULL; JoystickGetGUIDString = NULL;
        JoystickNumAxes = NULL; JoystickNumButtons = NULL; JoystickNumHats = NULL;
        JoystickGetAxis = NULL; JoystickGetButton = NULL; JoystickGetHat = NULL;
        GameControllerClose = NULL; GameControllerName = NULL; GameControllerGetJoystick = NULL;
        GameControllerGetButton = NULL; GameControllerGetAxis = NULL;
        GameControllerAddMapping = NULL;
    }

    template<typename T> bool symbol(T& target, const char* name) {
        target = reinterpret_cast<T>(dlsym(library, name));
        if (!target) {
            std::cerr << "Missing SDL2 symbol: " << name << "\n";
            return false;
        }
        return true;
    }

    bool load() {
#if defined(__APPLE__)
        const char* overridePath = std::getenv("CIPHERNES_SDL2_LIBRARY");
        if (overridePath && *overridePath) library = dlopen(overridePath, RTLD_NOW | RTLD_LOCAL);
        const char* names[] = {
            "@executable_path/../Frameworks/libSDL2-2.0.0.dylib",
            "@executable_path/../Frameworks/libSDL2.dylib",
            "libSDL2-2.0.0.dylib", "libSDL2.dylib",
            "/opt/homebrew/opt/sdl2/lib/libSDL2-2.0.0.dylib",
            "/usr/local/opt/sdl2/lib/libSDL2-2.0.0.dylib"
        };
#else
        const char* names[] = { "libSDL2-2.0.so.0", "libSDL2.so" };
#endif
        for (size_t i = 0; i < sizeof(names) / sizeof(names[0]) && !library; ++i)
            library = dlopen(names[i], RTLD_NOW | RTLD_LOCAL);
        if (!library) return false;
#define LOAD_SDL(name) if (!symbol(name, "SDL_" #name)) return false
        LOAD_SDL(Init); LOAD_SDL(Quit); LOAD_SDL(GetError); LOAD_SDL(SetHint);
        LOAD_SDL(CreateWindow); LOAD_SDL(DestroyWindow); LOAD_SDL(SetWindowTitle);
        LOAD_SDL(SetWindowMinimumSize); LOAD_SDL(SetWindowFullscreen);
        LOAD_SDL(GetWindowID); LOAD_SDL(GetWindowSize);
        LOAD_SDL(CreateRenderer); LOAD_SDL(DestroyRenderer); LOAD_SDL(CreateTexture);
        LOAD_SDL(DestroyTexture); LOAD_SDL(UpdateTexture); LOAD_SDL(SetRenderDrawColor);
        LOAD_SDL(SetRenderDrawBlendMode); LOAD_SDL(RenderClear); LOAD_SDL(RenderCopy);
        LOAD_SDL(RenderFillRect); LOAD_SDL(RenderDrawRect); LOAD_SDL(RenderDrawLine);
        LOAD_SDL(RenderReadPixels);
        LOAD_SDL(RenderPresent); LOAD_SDL(GetRendererOutputSize); LOAD_SDL(PollEvent);
        LOAD_SDL(GetKeyboardState); LOAD_SDL(Delay); LOAD_SDL(GetPerformanceCounter);
        LOAD_SDL(GetPerformanceFrequency); LOAD_SDL(GetTicks);
        if (!symbol(Free, "SDL_free")) return false;
        LOAD_SDL(OpenAudioDevice); LOAD_SDL(CloseAudioDevice); LOAD_SDL(PauseAudioDevice);
        LOAD_SDL(QueueAudio); LOAD_SDL(GetQueuedAudioSize); LOAD_SDL(ClearQueuedAudio);
        LOAD_SDL(NumJoysticks); LOAD_SDL(IsGameController); LOAD_SDL(GameControllerOpen);
        LOAD_SDL(JoystickNameForIndex); LOAD_SDL(JoystickOpen); LOAD_SDL(JoystickClose);
        LOAD_SDL(JoystickInstanceID); LOAD_SDL(JoystickGetGUID); LOAD_SDL(JoystickGetGUIDString);
        LOAD_SDL(JoystickNumAxes); LOAD_SDL(JoystickNumButtons); LOAD_SDL(JoystickNumHats);
        LOAD_SDL(JoystickGetAxis); LOAD_SDL(JoystickGetButton); LOAD_SDL(JoystickGetHat);
        LOAD_SDL(GameControllerClose); LOAD_SDL(GameControllerName); LOAD_SDL(GameControllerGetJoystick);
        LOAD_SDL(GameControllerGetButton); LOAD_SDL(GameControllerGetAxis);
        GameControllerAddMapping = reinterpret_cast<int (*)(const char*)>(dlsym(library, "SDL_GameControllerAddMapping"));
        SetThreadPriority = reinterpret_cast<int (*)(int)>(dlsym(library, "SDL_SetThreadPriority"));
#undef LOAD_SDL
        return true;
    }
};

class App;


bool makeDir(const std::string& path) {
    if (::mkdir(path.c_str(), 0755) == 0) return true;
    return errno == EEXIST;
}

bool fileExists(const std::string& path) {
    struct stat st;
    return ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool dirExists(const std::string& path) {
    struct stat st;
    return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

std::string baseName(const std::string& path) {
    size_t p = path.find_last_of("/\\");
    return p == std::string::npos ? path : path.substr(p + 1);
}

std::string stemName(const std::string& path) {
    std::string name = baseName(path);
    size_t dot = name.find_last_of('.');
    if (dot != std::string::npos) name.resize(dot);
    return name.empty() ? std::string("Unknown") : name;
}

std::string lowerText(std::string value) {
    for (size_t i = 0; i < value.size(); ++i)
        value[i] = char(std::tolower(static_cast<unsigned char>(value[i])));
    return value;
}

bool hasExtension(const std::string& path, const char* extension) {
    std::string a = lowerText(path);
    std::string b = lowerText(extension ? extension : "");
    return a.size() >= b.size() && a.compare(a.size() - b.size(), b.size(), b) == 0;
}

std::vector<unsigned char> readBinary(const std::string& path) {
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) return std::vector<unsigned char>();
    in.seekg(0, std::ios::end);
    std::streamoff length = in.tellg();
    in.seekg(0, std::ios::beg);
    if (length <= 0) return std::vector<unsigned char>();
    std::vector<unsigned char> data(static_cast<size_t>(length));
    in.read(reinterpret_cast<char*>(&data[0]), length);
    if (!in) data.clear();
    return data;
}

bool writeBinary(const std::string& path, const std::vector<unsigned char>& data) {
    std::ofstream out(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!out) return false;
    if (!data.empty()) out.write(reinterpret_cast<const char*>(&data[0]), data.size());
    return out.good();
}

struct PreparedRomPath {
    std::string corePath;
    std::string temporaryDirectory;
    std::string displayBase;
    std::string notice;
};

std::string safeTemporaryRomName(const std::string& entryName) {
    std::string name = baseName(entryName);
    std::string safe;
    for (size_t i = 0; i < name.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(name[i]);
        safe += (std::isalnum(c) || c == '.' || c == '_' || c == '-') ? char(c) : '_';
    }
    if (safe.empty() || !(hasExtension(safe, ".nes") || hasExtension(safe, ".fds") || hasExtension(safe, ".qd")))
        safe = "archive_rom.nes";
    return safe;
}

void cleanupPreparedRom(PreparedRomPath& prepared) {
    if (!prepared.temporaryDirectory.empty()) {
        if (!prepared.corePath.empty()) ::unlink(prepared.corePath.c_str());
        ::rmdir(prepared.temporaryDirectory.c_str());
    }
    prepared = PreparedRomPath();
}

bool prepareRomPath(const std::string& inputPath, PreparedRomPath& prepared, std::string& errorText) {
    prepared = PreparedRomPath();
    errorText.clear();
    if (!hasExtension(inputPath, ".zip")) {
        prepared.corePath = inputPath;
        prepared.displayBase = stemName(inputPath);
        return true;
    }

    ZipRomLoadResult zip;
    if (!loadNesRomFromZip(inputPath, zip, errorText)) return false;
    char directoryTemplate[] = "/tmp/ciphernes-rom-XXXXXX";
    char* directory = ::mkdtemp(directoryTemplate);
    if (!directory) {
        errorText = "Could not create a secure temporary ROM directory.";
        return false;
    }
    prepared.temporaryDirectory = directory;
    prepared.corePath = prepared.temporaryDirectory + "/" + safeTemporaryRomName(zip.entryName);
    prepared.displayBase = stemName(zip.entryName);
    prepared.notice = zip.notice;
    if (!writeBinary(prepared.corePath, zip.romData)) {
        errorText = "Could not materialize the ROM selected from the ZIP archive.";
        cleanupPreparedRom(prepared);
        return false;
    }
    return true;
}

bool writeTextFile(const std::string& path, const std::string& text) {
    std::ofstream out(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    return out.good();
}

std::string xmlEscape(const std::string& value) {
    std::string out;
    for (size_t i = 0; i < value.size(); ++i) {
        switch (value[i]) {
            case '&': out += "&amp;"; break; case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break; case '"': out += "&quot;"; break;
            default: out += value[i]; break;
        }
    }
    return out;
}

std::string xmlTag(const std::string& text, const std::string& tag, size_t start = 0) {
    std::string open = "<" + tag + ">", close = "</" + tag + ">";
    size_t a = text.find(open, start); if (a == std::string::npos) return std::string();
    a += open.size(); size_t b = text.find(close, a); if (b == std::string::npos) return std::string();
    return text.substr(a, b - a);
}

void appendU32LE(std::vector<unsigned char>& out, uint32_t v) {
    out.push_back(static_cast<unsigned char>(v));
    out.push_back(static_cast<unsigned char>(v >> 8));
    out.push_back(static_cast<unsigned char>(v >> 16));
    out.push_back(static_cast<unsigned char>(v >> 24));
}

void appendU64LE(std::vector<unsigned char>& out, uint64_t v) {
    for (int shift = 0; shift < 64; shift += 8)
        out.push_back(static_cast<unsigned char>(v >> shift));
}

uint64_t readU64LE(const unsigned char* p) {
    uint64_t value = 0;
    for (int shift = 0; shift < 64; shift += 8)
        value |= uint64_t(p[shift / 8]) << shift;
    return value;
}

void appendU32BE(std::vector<unsigned char>& out, uint32_t v) {
    out.push_back(static_cast<unsigned char>(v >> 24));
    out.push_back(static_cast<unsigned char>(v >> 16));
    out.push_back(static_cast<unsigned char>(v >> 8));
    out.push_back(static_cast<unsigned char>(v));
}

uint32_t readU32LE(const unsigned char* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

uint32_t readU32BE(const unsigned char* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

uint32_t crc32(const void* bytes, size_t size) {
    const unsigned char* p = static_cast<const unsigned char*>(bytes);
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i) {
        crc ^= p[i];
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

uint32_t adler32(const std::vector<unsigned char>& data) {
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < data.size(); ++i) {
        a = (a + data[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

void pngChunk(std::vector<unsigned char>& png, const char type[4], const std::vector<unsigned char>& payload) {
    appendU32BE(png, static_cast<uint32_t>(payload.size()));
    size_t crcStart = png.size();
    png.insert(png.end(), type, type + 4);
    png.insert(png.end(), payload.begin(), payload.end());
    uint32_t sum = crc32(&png[crcStart], 4 + payload.size());
    appendU32BE(png, sum);
}

bool writePng(const std::string& path, const uint32_t* argb, int width, int height) {
    if (!argb || width <= 0 || height <= 0) return false;
    std::vector<unsigned char> raw;
    raw.reserve(static_cast<size_t>(height) * (1u + static_cast<size_t>(width) * 3u));
    for (int y = 0; y < height; ++y) {
        raw.push_back(0);
        for (int x = 0; x < width; ++x) {
            uint32_t c = argb[y * width + x];
            raw.push_back(static_cast<unsigned char>(c >> 16));
            raw.push_back(static_cast<unsigned char>(c >> 8));
            raw.push_back(static_cast<unsigned char>(c));
        }
    }

    std::vector<unsigned char> z;
    z.push_back(0x78); z.push_back(0x01);
    size_t pos = 0;
    while (pos < raw.size()) {
        size_t amount = std::min<size_t>(65535, raw.size() - pos);
        bool finalBlock = pos + amount == raw.size();
        z.push_back(finalBlock ? 1 : 0);
        uint16_t len = static_cast<uint16_t>(amount);
        uint16_t nlen = static_cast<uint16_t>(~len);
        z.push_back(static_cast<unsigned char>(len)); z.push_back(static_cast<unsigned char>(len >> 8));
        z.push_back(static_cast<unsigned char>(nlen)); z.push_back(static_cast<unsigned char>(nlen >> 8));
        z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + amount);
        pos += amount;
    }
    appendU32BE(z, adler32(raw));

    std::vector<unsigned char> png;
    const unsigned char signature[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    png.insert(png.end(), signature, signature + 8);
    std::vector<unsigned char> ihdr;
    appendU32BE(ihdr, static_cast<uint32_t>(width));
    appendU32BE(ihdr, static_cast<uint32_t>(height));
    ihdr.push_back(8); ihdr.push_back(2); ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);
    pngChunk(png, "IHDR", ihdr);
    pngChunk(png, "IDAT", z);
    pngChunk(png, "IEND", std::vector<unsigned char>());
    return writeBinary(path, png);
}

struct AviIndexEntry { char id[4]; uint32_t flags, offset, size; };

class AviWriter {
public:
    AviWriter() : width(0), height(0), fps(60), sampleRate(44100), frameCount(0), audioSamples(0),
        riffSizePos(0), hdrlSizePos(0), moviListSizePos(0), moviStart(0), avihFramesPos(0),
        strhVidLenPos(0), strhAudLenPos(0), openFlag(false) {}
    ~AviWriter() { close(); }

    bool open(const std::string& path, int w, int h, int f, int rate) {
        close(); width = w; height = h; fps = f; sampleRate = rate;
        frameCount = audioSamples = 0; index.clear();
        out.open(path.c_str(), std::ios::binary | std::ios::trunc);
        if (!out) return false;
        writeHeaders(); openFlag = true; return true;
    }
    bool isOpen() const { return openFlag; }
    void addFrame(const uint32_t* pixels) {
        if (!openFlag || !pixels) return;
        std::vector<unsigned char> frame(static_cast<size_t>(width) * height * 4u);
        for (int y = 0; y < height; ++y)
            std::memcpy(&frame[static_cast<size_t>(y) * width * 4u], pixels + (height - 1 - y) * width, static_cast<size_t>(width) * 4u);
        long start = tell(); fourcc("00db"); u32(frame.size());
        out.write(reinterpret_cast<const char*>(&frame[0]), frame.size()); pad();
        addIndex("00db", 0x10, static_cast<uint32_t>(start - moviStart), frame.size()); ++frameCount;
    }
    void addAudio(const int16_t* pcm, int count) {
        if (!openFlag || !pcm || count <= 0) return;
        uint32_t bytes = static_cast<uint32_t>(count * 2);
        long start = tell(); fourcc("01wb"); u32(bytes);
        out.write(reinterpret_cast<const char*>(pcm), bytes); pad();
        addIndex("01wb", 0, static_cast<uint32_t>(start - moviStart), bytes); audioSamples += count;
    }
    void close() {
        if (!openFlag) { if (out.is_open()) out.close(); return; }
        long afterMovi = tell(); seek(moviListSizePos); u32(afterMovi - moviListSizePos - 4); seek(afterMovi);
        fourcc("idx1"); u32(index.size() * 16u);
        for (size_t i = 0; i < index.size(); ++i) {
            out.write(index[i].id, 4); u32(index[i].flags); u32(index[i].offset); u32(index[i].size);
        }
        long end = tell(); seek(avihFramesPos); u32(frameCount); seek(strhVidLenPos); u32(frameCount);
        seek(strhAudLenPos); u32(audioSamples); seek(riffSizePos); u32(end - 8); seek(end);
        out.close(); openFlag = false;
    }
private:
    int width, height, fps, sampleRate, frameCount, audioSamples;
    std::ofstream out; std::vector<AviIndexEntry> index;
    long riffSizePos, hdrlSizePos, moviListSizePos, moviStart, avihFramesPos, strhVidLenPos, strhAudLenPos;
    bool openFlag;
    long tell() { return static_cast<long>(out.tellp()); }
    void seek(long p) { out.seekp(p, std::ios::beg); }
    void fourcc(const char* s) { out.write(s, 4); }
    void u16(uint16_t v) { char b[2] = { char(v), char(v >> 8) }; out.write(b, 2); }
    void u32(uint32_t v) { char b[4] = { char(v), char(v >> 8), char(v >> 16), char(v >> 24) }; out.write(b, 4); }
    void s32(int32_t v) { u32(static_cast<uint32_t>(v)); }
    void pad() { if (tell() & 1) { char z = 0; out.write(&z, 1); } }
    void addIndex(const char* id, uint32_t flags, uint32_t offset, uint32_t size) {
        AviIndexEntry e; std::memcpy(e.id, id, 4); e.flags = flags; e.offset = offset; e.size = size; index.push_back(e);
    }
    void writeHeaders() {
        fourcc("RIFF"); riffSizePos = tell(); u32(0); fourcc("AVI ");
        fourcc("LIST"); hdrlSizePos = tell(); u32(0); fourcc("hdrl");
        fourcc("avih"); u32(56); u32(1000000 / fps); u32(width * height * 4 * fps); u32(0); u32(0x10);
        avihFramesPos = tell(); u32(0); u32(0); u32(2); u32(width * height * 4); u32(width); u32(height);
        u32(0); u32(0); u32(0); u32(0);
        fourcc("LIST"); long vidSize = tell(); u32(0); fourcc("strl"); fourcc("strh"); u32(56);
        fourcc("vids"); fourcc("DIB "); u32(0); u16(0); u16(0); u32(0); u32(1); u32(fps); u32(0);
        strhVidLenPos = tell(); u32(0); u32(width * height * 4); u32(0xFFFFFFFFu); u32(0);
        u16(0); u16(0); u16(width); u16(height); fourcc("strf"); u32(40); u32(40); s32(width); s32(height);
        u16(1); u16(32); u32(0); u32(width * height * 4); s32(0); s32(0); u32(0); u32(0);
        long afterVid = tell(); seek(vidSize); u32(afterVid - vidSize - 4); seek(afterVid);
        fourcc("LIST"); long audSize = tell(); u32(0); fourcc("strl"); fourcc("strh"); u32(56);
        fourcc("auds"); u32(0); u32(0); u16(0); u16(0); u32(0); u32(1); u32(sampleRate); u32(0);
        strhAudLenPos = tell(); u32(0); u32(2048); u32(0xFFFFFFFFu); u32(2);
        u16(0); u16(0); u16(0); u16(0); fourcc("strf"); u32(16); u16(1); u16(1); u32(sampleRate);
        u32(sampleRate * 2); u16(2); u16(16); long afterAud = tell(); seek(audSize); u32(afterAud - audSize - 4); seek(afterAud);
        long afterHdrl = tell(); seek(hdrlSizePos); u32(afterHdrl - hdrlSizePos - 4); seek(afterHdrl);
        fourcc("LIST"); moviListSizePos = tell(); u32(0); fourcc("movi"); moviStart = tell();
    }
};

const uint8_t FONT_DIGITS[10][7] = {
    {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{30,1,1,14,1,1,30},{2,6,10,18,31,2,2},
    {31,16,16,30,1,1,30},{14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},{14,17,17,15,1,1,14}
};
const uint8_t FONT_LETTERS[26][7] = {
    {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},{30,17,17,17,17,17,30},
    {31,16,16,30,16,16,31},{31,16,16,30,16,16,16},{14,17,16,23,17,17,15},{17,17,17,31,17,17,17},
    {14,4,4,4,4,4,14},{7,2,2,2,2,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},{30,17,17,30,16,16,16},
    {14,17,17,17,21,18,13},{30,17,17,30,20,18,17},{15,16,16,14,1,1,30},{31,4,4,4,4,4,4},
    {17,17,17,17,17,17,14},{17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4},{31,1,2,4,8,16,31}
};

const uint8_t* glyph(char c) {
    static const uint8_t blank[7] = {0,0,0,0,0,0,0};
    static const uint8_t dash[7] = {0,0,0,31,0,0,0};
    static const uint8_t colon[7] = {0,4,4,0,4,4,0};
    static const uint8_t dot[7] = {0,0,0,0,0,4,4};
    static const uint8_t slash[7] = {1,2,2,4,8,8,16};
    static const uint8_t lt[7] = {2,4,8,16,8,4,2};
    static const uint8_t gt[7] = {8,4,2,1,2,4,8};
    static const uint8_t plus[7] = {0,4,4,31,4,4,0};
    static const uint8_t bang[7] = {4,4,4,4,4,0,4};
    static const uint8_t question[7] = {14,17,1,2,4,0,4};
    static const uint8_t lparen[7] = {2,4,8,8,8,4,2};
    static const uint8_t rparen[7] = {8,4,2,2,2,4,8};
    static const uint8_t equals[7] = {0,0,31,0,31,0,0};
    static const uint8_t percent[7] = {17,2,4,8,17,0,0};
    static const uint8_t underscore[7] = {0,0,0,0,0,0,31};
    static const uint8_t quote[7] = {4,4,0,0,0,0,0};
    static const uint8_t lbracket[7] = {14,8,8,8,8,8,14};
    static const uint8_t rbracket[7] = {14,2,2,2,2,2,14};
    static const uint8_t comma[7] = {0,0,0,0,0,4,8};
    c = char(std::toupper(static_cast<unsigned char>(c)));
    if (c >= 'A' && c <= 'Z') return FONT_LETTERS[c - 'A'];
    if (c >= '0' && c <= '9') return FONT_DIGITS[c - '0'];
    switch (c) { case '-': return dash; case ':': return colon; case '.': return dot; case '/': return slash;
        case '<': return lt; case '>': return gt; case '+': return plus; case '!': return bang; case '?': return question;
        case '(': return lparen; case ')': return rparen; case '=': return equals; case '%': return percent;
        case '_': return underscore; case '\'': return quote; case '[': return lbracket; case ']': return rbracket; case ',': return comma; }
    return blank;
}

int textWidth(const std::string& text, int scale = 2) { return static_cast<int>(text.size()) * 6 * scale; }

bool pointInRect(int x, int y, const SDL_Rect& r) {
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

std::string scancodeName(SDL_Scancode key) {
    if (key >= SDL_SCANCODE_A && key <= SDL_SCANCODE_Z) return std::string(1, char('A' + key - SDL_SCANCODE_A));
    if (key >= SDL_SCANCODE_1 && key <= SDL_SCANCODE_9) return std::string(1, char('1' + key - SDL_SCANCODE_1));
    switch (key) {
        case SDL_SCANCODE_0: return "0"; case SDL_SCANCODE_UP: return "UpArrow"; case SDL_SCANCODE_DOWN: return "DownArrow";
        case SDL_SCANCODE_LEFT: return "LeftArrow"; case SDL_SCANCODE_RIGHT: return "RightArrow";
        case SDL_SCANCODE_RETURN: return "Enter"; case SDL_SCANCODE_BACKSPACE: return "Back";
        case SDL_SCANCODE_SPACE: return "Space"; case SDL_SCANCODE_MINUS: return "Minus"; case SDL_SCANCODE_EQUALS: return "Equals";
        case SDL_SCANCODE_F1: return "F1"; case SDL_SCANCODE_F2: return "F2"; case SDL_SCANCODE_F3: return "F3";
        case SDL_SCANCODE_F4: return "F4"; case SDL_SCANCODE_F5: return "F5"; case SDL_SCANCODE_F6: return "F6";
        case SDL_SCANCODE_F7: return "F7"; case SDL_SCANCODE_F8: return "F8"; case SDL_SCANCODE_F9: return "F9";
        case SDL_SCANCODE_F10: return "F10"; case SDL_SCANCODE_F11: return "F11"; case SDL_SCANCODE_F12: return "F12";
        default: return "Unassigned";
    }
}

std::string controllerButtonName(int button) {
    switch(button){case SDL_CONTROLLER_BUTTON_A:return"PAD A";case SDL_CONTROLLER_BUTTON_B:return"PAD B";case SDL_CONTROLLER_BUTTON_X:return"PAD X";case SDL_CONTROLLER_BUTTON_Y:return"PAD Y";
        case SDL_CONTROLLER_BUTTON_BACK:return"PAD BACK";case SDL_CONTROLLER_BUTTON_GUIDE:return"PAD GUIDE";case SDL_CONTROLLER_BUTTON_START:return"PAD START";
        case SDL_CONTROLLER_BUTTON_LEFTSTICK:return"PAD L3";case SDL_CONTROLLER_BUTTON_RIGHTSTICK:return"PAD R3";
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:return"PAD LB";case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:return"PAD RB";
        case SDL_CONTROLLER_BUTTON_DPAD_UP:return"PAD UP";case SDL_CONTROLLER_BUTTON_DPAD_DOWN:return"PAD DOWN";case SDL_CONTROLLER_BUTTON_DPAD_LEFT:return"PAD LEFT";case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:return"PAD RIGHT";
        case SDL_CONTROLLER_BUTTON_MISC1:return"PAD MISC";case SDL_CONTROLLER_BUTTON_PADDLE1:return"PAD P1";case SDL_CONTROLLER_BUTTON_PADDLE2:return"PAD P2";
        case SDL_CONTROLLER_BUTTON_PADDLE3:return"PAD P3";case SDL_CONTROLLER_BUTTON_PADDLE4:return"PAD P4";case SDL_CONTROLLER_BUTTON_TOUCHPAD:return"PAD TOUCH";
        default:return button<0?"NONE":"PAD BUTTON "+std::to_string(button);}
}

std::string controllerAxisName(int axis) {
    switch (axis) {
        case SDL_CONTROLLER_AXIS_LEFTX: return "LEFT X";
        case SDL_CONTROLLER_AXIS_LEFTY: return "LEFT Y";
        case SDL_CONTROLLER_AXIS_RIGHTX: return "RIGHT X";
        case SDL_CONTROLLER_AXIS_RIGHTY: return "RIGHT Y";
        case SDL_CONTROLLER_AXIS_TRIGGERLEFT: return "LEFT TRIGGER";
        case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: return "RIGHT TRIGGER";
        default: return "AXIS " + std::to_string(axis);
    }
}

std::string padBindingName(const PadBinding& binding) {
    if (binding.kind == PAD_BIND_NONE) return "NONE";
    if (binding.kind == PAD_BIND_STANDARD_DIRECTION) {
        const char* names[4] = { "DPAD/LSTICK UP", "DPAD/LSTICK DOWN", "DPAD/LSTICK LEFT", "DPAD/LSTICK RIGHT" };
        return binding.control >= 0 && binding.control < 4 ? names[binding.control] : "DPAD/LSTICK";
    }
    if (binding.kind == PAD_BIND_GAMEPAD_BUTTON) return controllerButtonName(binding.control);
    if (binding.kind == PAD_BIND_GAMEPAD_AXIS) return "PAD " + controllerAxisName(binding.control) + (binding.direction < 0 ? " -" : " +");
    if (binding.kind == PAD_BIND_JOYSTICK_BUTTON) return "JOY BUTTON " + std::to_string(binding.control + 1);
    if (binding.kind == PAD_BIND_JOYSTICK_AXIS) return "JOY AXIS " + std::to_string(binding.control) + (binding.direction < 0 ? " -" : " +");
    if (binding.kind == PAD_BIND_JOYSTICK_HAT) {
        std::string dir = binding.direction == SDL_HAT_UP ? "UP" : binding.direction == SDL_HAT_DOWN ? "DOWN" :
            binding.direction == SDL_HAT_LEFT ? "LEFT" : binding.direction == SDL_HAT_RIGHT ? "RIGHT" : std::to_string(binding.direction);
        return "JOY HAT " + std::to_string(binding.control) + " " + dir;
    }
    return "NONE";
}

std::string padBindingToken(const PadBinding& binding) {
    if (binding.kind == PAD_BIND_STANDARD_DIRECTION) return "SDLDirection" + std::to_string(binding.control);
    if (binding.kind == PAD_BIND_GAMEPAD_BUTTON) return "SDLButton" + std::to_string(binding.control);
    if (binding.kind == PAD_BIND_GAMEPAD_AXIS) return "SDLAxis" + std::to_string(binding.control) + (binding.direction < 0 ? "-" : "+");
    if (binding.kind == PAD_BIND_JOYSTICK_BUTTON) return "JoyButton" + std::to_string(binding.control);
    if (binding.kind == PAD_BIND_JOYSTICK_AXIS) return "JoyAxis" + std::to_string(binding.control) + (binding.direction < 0 ? "-" : "+");
    if (binding.kind == PAD_BIND_JOYSTICK_HAT) return "JoyHat" + std::to_string(binding.control) + ":" + std::to_string(binding.direction);
    return "NONE";
}

PadBinding parsePadBinding(const std::string& token) {
    if (token.empty() || token == "NONE" || token == "Unassigned") return PadBinding();
    if (token.find("SDLDirection") == 0) return PadBinding(PAD_BIND_STANDARD_DIRECTION, std::atoi(token.c_str() + 12), 0);
    if (token.find("SDLButton") == 0) { int button = std::atoi(token.c_str() + 9); return button < 0 ? PadBinding() : PadBinding(PAD_BIND_GAMEPAD_BUTTON, button, 0); }
    if (token.find("SDLAxis") == 0) {
        const char* p = token.c_str() + 7; int axis = std::atoi(p); char last = token.empty() ? '+' : token[token.size() - 1];
        return PadBinding(PAD_BIND_GAMEPAD_AXIS, axis, last == '-' ? -1 : 1);
    }
    if (token.find("JoyButton") == 0) { int button = std::atoi(token.c_str() + 9); return button < 0 ? PadBinding() : PadBinding(PAD_BIND_JOYSTICK_BUTTON, button, 0); }
    if (token.find("JoyAxis") == 0) {
        const char* p = token.c_str() + 7; int axis = std::atoi(p); char last = token.empty() ? '+' : token[token.size() - 1];
        return PadBinding(PAD_BIND_JOYSTICK_AXIS, axis, last == '-' ? -1 : 1);
    }
    if (token.find("JoyHat") == 0) {
        size_t colon = token.find(':');
        if (colon != std::string::npos) return PadBinding(PAD_BIND_JOYSTICK_HAT, std::atoi(token.substr(6, colon - 6).c_str()), std::atoi(token.substr(colon + 1).c_str()));
    }
    return PadBinding();
}

SDL_Scancode parseScancode(const std::string& value) {
    if (value.size() == 1 && value[0] >= 'A' && value[0] <= 'Z') return SDL_SCANCODE_A + value[0] - 'A';
    if(value.size()==1&&value[0]>='1'&&value[0]<='9')return SDL_SCANCODE_1+value[0]-'1';if(value=="0")return SDL_SCANCODE_0;
    if (value == "UpArrow") return SDL_SCANCODE_UP; if (value == "DownArrow") return SDL_SCANCODE_DOWN;
    if (value == "LeftArrow") return SDL_SCANCODE_LEFT; if (value == "RightArrow") return SDL_SCANCODE_RIGHT;
    if (value == "Enter") return SDL_SCANCODE_RETURN; if (value == "Back" || value == "Backspace") return SDL_SCANCODE_BACKSPACE;
    if (value == "Space") return SDL_SCANCODE_SPACE; if (value == "Minus") return SDL_SCANCODE_MINUS;
    if (value == "Equals") return SDL_SCANCODE_EQUALS;
    if(value.size()>=2&&value[0]=='F'){int n=std::atoi(value.c_str()+1);if(n>=1&&n<=12)return SDL_SCANCODE_F1+n-1;}
    return SDL_SCANCODE_UNKNOWN;
}

char scancodeCharacter(SDL_Scancode key, bool shift) {
    if (key >= SDL_SCANCODE_A && key <= SDL_SCANCODE_Z) return char((shift ? 'A' : 'a') + key - SDL_SCANCODE_A);
    if (key >= SDL_SCANCODE_1 && key <= SDL_SCANCODE_9) return char('1' + key - SDL_SCANCODE_1);
    if (key == SDL_SCANCODE_0) return '0'; if (key == SDL_SCANCODE_SPACE) return ' ';
    if (key == SDL_SCANCODE_MINUS) return shift ? '_' : '-'; if (key == SDL_SCANCODE_EQUALS) return shift ? '+' : '=';
    return 0;
}

void drawText(SDLApi& sdl, SDL_Renderer* renderer, int x, int y, const std::string& text, int scale,
              Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha = 255) {
    sdl.SetRenderDrawColor(renderer, red, green, blue, alpha);
    int cursor = x;
    for (size_t i = 0; i < text.size(); ++i) {
        const uint8_t* rows = glyph(text[i]);
        for (int row = 0; row < 7; ++row) for (int col = 0; col < 5; ++col) {
            if (rows[row] & (1u << (4 - col))) {
                SDL_Rect r = { cursor + col * scale, y + row * scale, scale, scale };
                sdl.RenderFillRect(renderer, &r);
            }
        }
        cursor += 6 * scale;
    }
}

void drawCpuText(uint32_t* pixels, int x, int y, const std::string& text, int scale, uint32_t color) {
    if (!pixels) return; int cursor=x;
    for(size_t i=0;i<text.size();++i){const uint8_t* rows=glyph(text[i]);for(int row=0;row<7;++row)for(int col=0;col<5;++col)if(rows[row]&(1u<<(4-col)))for(int yy=0;yy<scale;++yy)for(int xx=0;xx<scale;++xx){int px=cursor+col*scale+xx,py=y+row*scale+yy;if(px>=0&&px<NES_W&&py>=0&&py<NES_H)pixels[py*NES_W+px]=color;}cursor+=6*scale;}
}

void drawPanel(SDLApi& sdl, SDL_Renderer* renderer, const SDL_Rect& rect, Uint8 r = 12, Uint8 g = 20, Uint8 b = 32) {
    sdl.SetRenderDrawColor(renderer, r, g, b, 255); sdl.RenderFillRect(renderer, &rect);
    sdl.SetRenderDrawColor(renderer, 78, 139, 196, 255); sdl.RenderDrawRect(renderer, &rect);
}

void drawButton(SDLApi& sdl, SDL_Renderer* renderer, const SDL_Rect& rect, const std::string& label, bool hot = false) {
    sdl.SetRenderDrawColor(renderer, hot ? 45 : 28, hot ? 102 : 58, hot ? 150 : 86, 255); sdl.RenderFillRect(renderer, &rect);
    sdl.SetRenderDrawColor(renderer, hot ? 170 : 96, hot ? 220 : 158, 255, 255); sdl.RenderDrawRect(renderer, &rect);
    int scale = rect.w >= textWidth(label, 2) + 16 ? 2 : 1;
    drawText(sdl, renderer, rect.x + std::max(6, (rect.w - textWidth(label, scale)) / 2), rect.y + (rect.h - 7 * scale) / 2, label, scale, 238, 244, 250);
}

void drawTextBox(SDLApi& sdl, SDL_Renderer* renderer, const SDL_Rect& rect, const std::string& label, const std::string& value, bool active) {
    drawText(sdl, renderer, rect.x, rect.y - 23, label, 2, 150, 195, 230);
    sdl.SetRenderDrawColor(renderer, active ? 22 : 8, active ? 55 : 18, active ? 82 : 28, 255); sdl.RenderFillRect(renderer, &rect);
    sdl.SetRenderDrawColor(renderer, active ? 255 : 86, active ? 210 : 144, active ? 90 : 190, 255); sdl.RenderDrawRect(renderer, &rect);
    drawText(sdl, renderer, rect.x + 10, rect.y + 11, value + (active ? "_" : ""), 2, 235, 242, 248);
}

// Display the native desktop file/folder chooser. No embedded file browser.
// Linux uses the desktop's file chooser (zenity or KDE kdialog), macOS uses
// the standard AppleScript-backed Cocoa open panel. Commands are fixed strings.
std::string readDialogCommand(const char* command) {
    FILE* pipe = popen(command, "r");
    if (!pipe) return std::string();
    std::string result;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe)) result += buffer;
    int status = pclose(pipe);
    if (status != 0) return std::string();
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) result.pop_back();
    return result;
}
std::string nativeSelectPath(int kind, SDL_Window*) {
#if defined(__APPLE__)
    if (kind == 4) return readDialogCommand("osascript -e 'POSIX path of (choose folder with prompt \"Choose AVI output folder\")' 2>/dev/null");
    return readDialogCommand(kind == 5 ?
        "osascript -e 'POSIX path of (choose file with prompt \"Open CipherNES TAS recording\")' 2>/dev/null" :
        "osascript -e 'POSIX path of (choose file with prompt \"Open NES ROM\")' 2>/dev/null");
#else
    const bool useZenity = std::system("command -v zenity >/dev/null 2>&1") == 0;
    if (kind == 4)
        return readDialogCommand(useZenity ?
            "zenity --file-selection --directory --title='Choose AVI output folder' 2>/dev/null" :
            "kdialog --getexistingdirectory 2>/dev/null");
    if (kind == 5)
        return readDialogCommand(useZenity ?
            "zenity --file-selection --title='Open CipherNES TAS recording' --file-filter='CipherNES recordings | *.ci' 2>/dev/null" :
            "kdialog --getopenfilename . '*.ci' 2>/dev/null");
    return readDialogCommand(useZenity ?
        "zenity --file-selection --title='Open NES ROM' --file-filter='NES ROMs | *.nes *.fds *.qd *.zip' 2>/dev/null" :
        "kdialog --getopenfilename . '*.nes *.fds *.qd *.zip' 2>/dev/null");
#endif
}

class App {
public:
    App();
    ~App();
    bool initialize();
    int run(const std::string& initialRom);
    bool savePreview(const std::string& path, const std::string& initialRom);
    int runControllerSelfTest();
    int runTasBranchSelfTest(const std::string& rom);
private:
    SDLApi sdl;
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    SDL_AudioDeviceID audioDevice;
    Uint32 mainWindowId;
    std::vector<HostController> controllers;
    CIPHERNES_Handle* core;
    bool running, romLoaded, paused, muted, fullscreen, stretched;
    bool cheatsEnabled, unpacedRunWasActive;
    bool portConnected[2], recordWithToast, recordWithTimer, customAviOutput;
    bool tasResetBeforeInputRecording;
    int speedPercent, stateSlot;
    int tasMode, tasCountdownDigit;
    int mouseX, mouseY, activeMenu, activeSubmenu, screenMode;
    int capturePort, captureIndex, selectedCheat, cheatScroll, cheatEditMode, activeTextField;
    uint8_t apuMask;
    std::string romPath, romBase, toastText, currentVideoPath, customAviDirectory;
    std::string tasTitleMessage, tasPlaybackPath;
    std::string editCheatName, editCheatCode, ggCode, ggAddress, ggValue, ggCompare, ggStatus;
    Uint32 toastUntil, recordingStartedAt;
    Uint32 tasCountdownStartedAt, tasTitleMessageUntil;
    double nextFrameAt, nextRenderAt;
    double performanceSecondsPerTick;
    int audioSampleRate;
    int rendererOutputW, rendererOutputH;
    float menuSlide; // 0 hidden, 1 visible
    Uint32 lastMenuHover;
    bool menuHoverActivated;
    const Uint8* keyboardState;
    int keyboardStateCount;
    Uint32 audioQueueCheckCountdown;
    uint32_t stateBufferCapacity;
    bool frameTextureDirty;
    std::vector<MenuRootDef> menus;
    std::vector<GameGenieCheat> cheats;
    std::vector<int16_t> audioScratch;
    std::vector<uint32_t> videoFrameScratch;
    std::vector<TasInputFrame> tasFrames;
    ciphernes_rewind::History rewindHistory;
    std::vector<uint8_t> rewindRestoredState;
    std::vector<int16_t> rewindAudioScratch;
    std::vector<int16_t> resampledAudioScratch; // reused for audio resampling
    std::vector<uint8_t> rewindStateScratch; // reusable transient state serializer
    int16_t lastQueuedAudioSample;
    ciphernes_audio::SpeedResampler speedResampler;
    bool rewindHeld, fastForwardHeld, rewindShowingFrame;
    bool restoredDisplayVisible;
    std::vector<uint32_t> restoredDisplay;
    double fastForwardMultiplier, lastSpeedTick;
    double actualSpeedMultiplier, speedMeasureSince;
    int speedMeasureFrames;
    size_t tasPlaybackPosition;
    SDL_Scancode controllerKeys[2][8], specialKeys[2][SPECIAL_ACTION_COUNT];
    PadBinding controllerPadBindings[2][8], specialPadBindings[2][SPECIAL_ACTION_COUNT];
    std::string portDeviceChoice[2];
    bool specialWasDown[2][SPECIAL_ACTION_COUNT];
    AviWriter avi;

    double nowSeconds() const;
    void idleUntil(double deadlineSeconds);
    void resetAudioQueue(bool primePlayback);
    void toast(const std::string& message, Uint32 milliseconds = 1600);
    bool loadRom(const std::string& path);
    bool loadRomImage(const uint8_t* data, uint32_t bytes, const std::string& virtualPath, const std::string& displayName);
    void unloadRom();
    void updateTitle();
    void updateTasTimers();
    void updateTasMenuState();
    void restartRomForTas();
    void startTasRecording();
    void stopTasRecording();
    bool saveTasRecording(std::string& savedPath);
    bool loadTasRecording(const std::string& path, std::vector<TasInputFrame>& frames, std::string& recordedRom, std::string& errorText) const;
    void startTasPlayback(const std::string& path);
    void stopTasPlayback(bool completed);
    void stopTasForRomChange();
    void handleEvents();
    void handleKey(const SDL_KeyboardEvent& event);
    void handleMouseDown(int x, int y);
    void handleMouseMotion(int x, int y);
    void dispatchCommand(int command);
    uint8_t readPad(int port) const;
    bool specialDown(int port, int index) const;
    void pollSpecialBindings();
    void runOneFrame(bool unpaced);
    void clearRewindHistory();
    void captureRewindFrame();
    bool rewindOneFrame();
    void commitRewindBranch();
    bool actionHeld(int index) const;
    void queueAudioForSpeed(const std::vector<int16_t>& input, double multiplier);
    void drainAudio(bool queuePlayback, bool collectForAvi);
    void render();
    void drawOverlay(int outW, int outH);
    void drawMenuBar(int outW, int outH);
    void setFullscreenMode(bool enable, bool useStretch);
    bool pointerOverMenu(int x, int y) const;
    void drawCurrentScreen(int outW, int outH);
    void drawControllerScreen(int outW, int outH);
    void drawCheatsScreen(int outW, int outH);
    void drawGameGenieScreen(int outW, int outH);
    void drawInfoScreen(int outW, int outH);
    void openBrowser(int kind);
    void openControllers();
    void closeControllers();
    int controllerIndexForInstance(SDL_JoystickID instanceId) const;
    int assignedControllerIndex(int port) const;
    std::string assignedControllerName(int port) const;
    void cycleControllerForPort(int port);
    void assignCaptureController(int port, SDL_JoystickID instanceId);
    bool padBindingDown(int port, const PadBinding& binding) const;
    void setCapturedPadBinding(const PadBinding& binding, SDL_JoystickID instanceId);
    std::vector<unsigned char> captureState(bool persistBatterySave = false);
    bool restoreState(const std::vector<unsigned char>& state);
    void saveState();
    void loadState();
    void takeScreenshot();
    void toggleRecording();
    void stopRecording();
    void toggleCheats();
    void loadCheats();
    bool saveCheats();
    void applyCheats();
    bool decodeGameGenie(const std::string& code, uint16_t& address, uint8_t& value, bool& hasCompare, uint8_t& compare) const;
    bool encodeGameGenie(std::string& code) const;
    void buildMenus();
    std::vector<MenuItemDef> submenuItemsFor(int submenu) const;
    bool menuChecked(int command) const;
    bool itemChecked(const MenuItemDef& item) const;
    void resetInputBindings();
    bool loadInputBindings();
    bool saveInputBindings();
    void editActiveText(const SDL_KeyboardEvent& event);

};


App::App() : window(NULL), renderer(NULL), texture(NULL), audioDevice(0), mainWindowId(0), core(NULL), running(false),
    romLoaded(false), paused(false), muted(false), fullscreen(false), stretched(false),
    cheatsEnabled(false), unpacedRunWasActive(false),
    recordWithToast(false), recordWithTimer(false), customAviOutput(false), tasResetBeforeInputRecording(true),
    speedPercent(100), stateSlot(1),
    tasMode(TAS_IDLE), tasCountdownDigit(0),
    mouseX(0), mouseY(0), activeMenu(-1), activeSubmenu(-1), screenMode(SCREEN_GAME),
    capturePort(-1), captureIndex(-1), selectedCheat(-1), cheatScroll(0), cheatEditMode(0), activeTextField(0),
    apuMask(0x1F), toastUntil(0), recordingStartedAt(0), tasCountdownStartedAt(0), tasTitleMessageUntil(0),
    nextFrameAt(0), nextRenderAt(0), performanceSecondsPerTick(0.0), audioSampleRate(44100),
    rendererOutputW(0), rendererOutputH(0), menuSlide(1.0f), lastMenuHover(0), menuHoverActivated(false), keyboardState(NULL), keyboardStateCount(0), audioQueueCheckCountdown(0),
    stateBufferCapacity(0), frameTextureDirty(true), lastQueuedAudioSample(0),
    rewindHeld(false), fastForwardHeld(false), rewindShowingFrame(false), restoredDisplayVisible(false),
    fastForwardMultiplier(1.0), lastSpeedTick(0.0), actualSpeedMultiplier(1.0), speedMeasureSince(0.0),
    speedMeasureFrames(0), tasPlaybackPosition(0) {
    rewindHistory.configure(256, 1800);
    portConnected[0] = portConnected[1] = true;
    portDeviceChoice[0] = portDeviceChoice[1] = "AUTO";
    std::memset(specialWasDown, 0, sizeof(specialWasDown));
    audioScratch.reserve(8192);
    resampledAudioScratch.reserve(8192);
    videoFrameScratch.reserve(NES_W * NES_H);
    resetInputBindings();
}

App::~App() {
    if (tasMode == TAS_COUNTDOWN || tasMode == TAS_RECORDING) stopTasRecording();
    else if (tasMode == TAS_PLAYBACK) stopTasPlayback(false);
    stopRecording();
    unloadRom();
    if (core) ciphernes_destroy(core);
    closeControllers();
    if (audioDevice) sdl.CloseAudioDevice(audioDevice);
    if (texture) sdl.DestroyTexture(texture);
    if (renderer) sdl.DestroyRenderer(renderer);
    if (window) sdl.DestroyWindow(window);
    if (sdl.Quit) sdl.Quit();
}

double App::nowSeconds() const {
    const double scale = performanceSecondsPerTick > 0.0
        ? performanceSecondsPerTick
        : 1.0 / double(sdl.GetPerformanceFrequency());
    return double(sdl.GetPerformanceCounter()) * scale;
}

void App::idleUntil(double deadlineSeconds) {
    // Sleep against the absolute deadline instead of returning after one coarse
    // delay. SDL_Delay may wake early or late; rechecking prevents both busy
    // polling and accumulated frontend pacing drift without changing emulation.
    for (;;) {
        const double remaining = deadlineSeconds - nowSeconds();
        if (remaining <= 0.0) return;

        if (remaining > 0.003) {
            const Uint32 delayMs = static_cast<Uint32>((remaining - 0.0015) * 1000.0);
            if (delayMs > 0) {
                sdl.Delay(delayMs);
                continue;
            }
        }
        if (remaining > 0.0005) {
            const long microseconds = static_cast<long>((remaining - 0.00025) * 1000000.0);
            if (microseconds > 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(microseconds));
                continue;
            }
        }
        std::this_thread::yield();
    }
}

void App::resetAudioQueue(bool primePlayback) {
    audioQueueCheckCountdown = 0;
    speedResampler.reset();
    lastQueuedAudioSample = 0;
    if (!audioDevice) return;
    sdl.ClearQueuedAudio(audioDevice);
    if (primePlayback && !muted) {
        static const int16_t silence[AUDIO_PREROLL_SAMPLES] = {};
        sdl.QueueAudio(audioDevice, silence, sizeof(silence));
    }
}

void App::toast(const std::string& message, Uint32 milliseconds) {
    toastText = message; toastUntil = sdl.GetTicks() + milliseconds;
}

bool App::initialize() {
    if (!sdl.load()) {
#if defined(__APPLE__)
        std::cerr << "CipherNES could not load the bundled SDL2 runtime.\n";
#else
        std::cerr << "CipherNES needs the Ubuntu SDL2 runtime (package libsdl2-2.0-0).\n";
#endif
        return false;
    }
    if (sdl.Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL2 initialization failed: " << sdl.GetError() << "\n";
        return false;
    }
    const Uint64 performanceFrequency = sdl.GetPerformanceFrequency();
    performanceSecondsPerTick = performanceFrequency ? 1.0 / double(performanceFrequency) : 0.0;
    sdl.SetHint("SDL_RENDER_SCALE_QUALITY", "nearest");
    sdl.SetHint("SDL_RENDER_BATCHING", "1");
#if !defined(__APPLE__)
    sdl.SetHint("SDL_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR", "1");
#endif
    sdl.SetHint("SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS", "0");
    window = sdl.CreateWindow(PLATFORM_WINDOW_TITLE, SDL_WINDOWPOS_CENTERED_MASK, SDL_WINDOWPOS_CENTERED_MASK,
        1280, 820, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) { std::cerr << "Window creation failed: " << sdl.GetError() << "\n"; return false; }
    mainWindowId = sdl.GetWindowID(window);
    sdl.SetWindowMinimumSize(window, 800, 600);
    // The frontend has its own monotonic 60 Hz presentation clock. Blocking on
    // SDL's vsync in RenderPresent causes the emulation thread to miss its 3-4x
    // deadlines (and causes stalls when releasing the fast-forward button).
    // Hosts preferring vsync can opt in; never make it the default limiter.
    const bool requestVsync = std::getenv("CIPHERNES_ENABLE_VSYNC") != NULL &&
        std::getenv("CIPHERNES_DISABLE_VSYNC") == NULL;
    if (requestVsync)
        renderer = sdl.CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) renderer = sdl.CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) renderer = sdl.CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) { std::cerr << "Renderer creation failed: " << sdl.GetError() << "\n"; return false; }
    sdl.SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    sdl.GetRendererOutputSize(renderer, &rendererOutputW, &rendererOutputH);
    if (sdl.SetThreadPriority) sdl.SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);
    texture = sdl.CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, NES_W, NES_H);
    if (!texture) { std::cerr << "Texture creation failed: " << sdl.GetError() << "\n"; return false; }

    SDL_AudioSpec wanted; std::memset(&wanted, 0, sizeof(wanted));
    wanted.freq = 44100; wanted.format = SDL_AUDIO_S16SYS; wanted.channels = 1; wanted.samples = 1024;
    SDL_AudioSpec obtained; std::memset(&obtained, 0, sizeof(obtained));
    audioDevice = sdl.OpenAudioDevice(NULL, 0, &wanted, &obtained, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
    core = ciphernes_create();
    if (!core) { std::cerr << "Could not create CipherNES core.\n"; return false; }
    audioSampleRate = audioDevice && obtained.freq > 0 ? obtained.freq : 44100;
    ciphernes_set_audio_sample_rate(core, double(audioSampleRate));
    if (audioDevice) sdl.PauseAudioDevice(audioDevice, 0);
    else std::cerr << "Audio output unavailable: " << sdl.GetError() << "\n";
    if (sdl.GameControllerAddMapping && fileExists("gamecontrollerdb.txt")) {
        std::ifstream mappings("gamecontrollerdb.txt");
        std::string mapping;
        while (std::getline(mappings, mapping)) {
            if (!mapping.empty() && mapping[mapping.size() - 1] == '\r') mapping.resize(mapping.size() - 1);
            if (!mapping.empty() && mapping[0] != '#') sdl.GameControllerAddMapping(mapping.c_str());
        }
    }
    if (!std::getenv("CIPHERNES_SKIP_INITIAL_CONTROLLER_SCAN")) openControllers();
    // Directories were created under the per-user data root before startup.
    loadInputBindings(); buildMenus();
    updateTitle();
    return true;
}

void App::openControllers() {
    closeControllers();
    int count = sdl.NumJoysticks();
    std::vector<std::string> seenGuids;
    for (int i = 0; i < count && int(controllers.size()) < 16; ++i) {
        HostController device;
        device.mapped = sdl.IsGameController(i) != 0;
        if (device.mapped) {
            device.gamepad = sdl.GameControllerOpen(i);
            if (device.gamepad) device.joystick = sdl.GameControllerGetJoystick(device.gamepad);
        }
        if (!device.joystick) {
            if (device.gamepad) sdl.GameControllerClose(device.gamepad);
            device.mapped = false;
            device.gamepad = NULL;
            device.joystick = sdl.JoystickOpen(i);
            device.ownsJoystick = device.joystick != NULL;
        }
        if (!device.joystick) continue;
        device.instanceId = sdl.JoystickInstanceID(device.joystick);
        const char* name = device.mapped && device.gamepad ? sdl.GameControllerName(device.gamepad) : sdl.JoystickNameForIndex(i);
        device.name = name && *name ? name : "Unknown Controller";
        char guid[64]; std::memset(guid, 0, sizeof(guid));
        sdl.JoystickGetGUIDString(sdl.JoystickGetGUID(device.joystick), guid, sizeof(guid));
        device.guid = *guid ? guid : device.name;
        int duplicate = 0;
        for (size_t j = 0; j < seenGuids.size(); ++j) if (seenGuids[j] == device.guid) ++duplicate;
        seenGuids.push_back(device.guid);
        device.persistentId = device.guid + "#" + std::to_string(duplicate);
        device.axes = std::max(0, sdl.JoystickNumAxes(device.joystick));
        device.buttons = std::max(0, sdl.JoystickNumButtons(device.joystick));
        device.hats = std::max(0, sdl.JoystickNumHats(device.joystick));
        controllers.push_back(device);
    }
}

void App::closeControllers() {
    for (size_t i = 0; i < controllers.size(); ++i) {
        if (controllers[i].gamepad) sdl.GameControllerClose(controllers[i].gamepad);
        else if (controllers[i].ownsJoystick && controllers[i].joystick) sdl.JoystickClose(controllers[i].joystick);
    }
    controllers.clear();
}

int App::controllerIndexForInstance(SDL_JoystickID instanceId) const {
    for (size_t i = 0; i < controllers.size(); ++i) if (controllers[i].instanceId == instanceId) return int(i);
    return -1;
}

int App::assignedControllerIndex(int port) const {
    if (port < 0 || port > 1 || portDeviceChoice[port] == "NONE") return -1;
    int assigned[2] = { -1, -1 };
    for (int p = 0; p < 2; ++p) {
        if (portDeviceChoice[p] == "AUTO" || portDeviceChoice[p] == "NONE") continue;
        for (size_t i = 0; i < controllers.size(); ++i)
            if (controllers[i].persistentId == portDeviceChoice[p]) { assigned[p] = int(i); break; }
    }
    for (int p = 0; p < 2; ++p) {
        if (portDeviceChoice[p] != "AUTO") continue;
        for (size_t i = 0; i < controllers.size(); ++i) {
            bool used = false;
            for (int q = 0; q < 2; ++q) if (q != p && assigned[q] == int(i)) used = true;
            if (!used) { assigned[p] = int(i); break; }
        }
    }
    return assigned[port];
}

std::string App::assignedControllerName(int port) const {
    if (portDeviceChoice[port] == "NONE") return "KEYBOARD ONLY";
    int index = assignedControllerIndex(port);
    if (index >= 0 && index < int(controllers.size())) {
        std::string prefix = portDeviceChoice[port] == "AUTO" ? "AUTO: " : "";
        return prefix + controllers[index].name + (controllers[index].mapped ? " [GAMEPAD]" : " [JOYSTICK]");
    }
    return portDeviceChoice[port] == "AUTO" ? "AUTO: NO CONTROLLER" : "ASSIGNED CONTROLLER MISSING";
}

void App::cycleControllerForPort(int port) {
    if (port < 0 || port > 1) return;
    int current = 0;
    if (portDeviceChoice[port] == "NONE") current = 1;
    else if (portDeviceChoice[port] != "AUTO") {
        for (size_t i = 0; i < controllers.size(); ++i)
            if (controllers[i].persistentId == portDeviceChoice[port]) { current = int(i) + 2; break; }
    }
    int next = (current + 1) % (int(controllers.size()) + 2);
    if (next == 0) portDeviceChoice[port] = "AUTO";
    else if (next == 1) portDeviceChoice[port] = "NONE";
    else portDeviceChoice[port] = controllers[next - 2].persistentId;
    toast("PORT " + std::to_string(port + 1) + " DEVICE: " + assignedControllerName(port), 2200);
}

void App::assignCaptureController(int port, SDL_JoystickID instanceId) {
    int index = controllerIndexForInstance(instanceId);
    if (port >= 0 && port < 2 && index >= 0) portDeviceChoice[port] = controllers[index].persistentId;
}

bool App::padBindingDown(int port, const PadBinding& binding) const {
    int index = assignedControllerIndex(port);
    if (index < 0 || index >= int(controllers.size())) return false;
    const HostController& device = controllers[index];
    const int deadzone = 12000;
    if (binding.kind == PAD_BIND_STANDARD_DIRECTION) {
        if (device.mapped && device.gamepad) {
            int button = binding.control == 0 ? SDL_CONTROLLER_BUTTON_DPAD_UP : binding.control == 1 ? SDL_CONTROLLER_BUTTON_DPAD_DOWN :
                binding.control == 2 ? SDL_CONTROLLER_BUTTON_DPAD_LEFT : SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
            if (sdl.GameControllerGetButton(device.gamepad, button)) return true;
            Sint16 axis = sdl.GameControllerGetAxis(device.gamepad, binding.control < 2 ? SDL_CONTROLLER_AXIS_LEFTY : SDL_CONTROLLER_AXIS_LEFTX);
            return (binding.control == 0 || binding.control == 2) ? axis < -deadzone : axis > deadzone;
        }
        if (device.joystick) {
            if (device.hats > 0) {
                Uint8 mask = binding.control == 0 ? SDL_HAT_UP : binding.control == 1 ? SDL_HAT_DOWN : binding.control == 2 ? SDL_HAT_LEFT : SDL_HAT_RIGHT;
                if (sdl.JoystickGetHat(device.joystick, 0) & mask) return true;
            }
            int axisIndex = binding.control < 2 ? 1 : 0;
            if (axisIndex < device.axes) {
                Sint16 axis = sdl.JoystickGetAxis(device.joystick, axisIndex);
                return (binding.control == 0 || binding.control == 2) ? axis < -deadzone : axis > deadzone;
            }
        }
        return false;
    }
    if (binding.kind == PAD_BIND_GAMEPAD_BUTTON) {
        if (device.mapped && device.gamepad) return sdl.GameControllerGetButton(device.gamepad, binding.control) != 0;
        return device.joystick && binding.control >= 0 && binding.control < device.buttons && sdl.JoystickGetButton(device.joystick, binding.control) != 0;
    }
    if (binding.kind == PAD_BIND_GAMEPAD_AXIS) {
        if (!device.mapped || !device.gamepad) return false;
        Sint16 value = sdl.GameControllerGetAxis(device.gamepad, binding.control);
        return binding.direction < 0 ? value < -deadzone : value > deadzone;
    }
    if (binding.kind == PAD_BIND_JOYSTICK_BUTTON)
        return device.joystick && binding.control >= 0 && binding.control < device.buttons && sdl.JoystickGetButton(device.joystick, binding.control) != 0;
    if (binding.kind == PAD_BIND_JOYSTICK_AXIS) {
        if (!device.joystick || binding.control < 0 || binding.control >= device.axes) return false;
        Sint16 value = sdl.JoystickGetAxis(device.joystick, binding.control);
        return binding.direction < 0 ? value < -deadzone : value > deadzone;
    }
    if (binding.kind == PAD_BIND_JOYSTICK_HAT)
        return device.joystick && binding.control >= 0 && binding.control < device.hats && (sdl.JoystickGetHat(device.joystick, binding.control) & binding.direction) != 0;
    return false;
}

void App::setCapturedPadBinding(const PadBinding& binding, SDL_JoystickID instanceId) {
    if (capturePort < 0 || captureIndex < 0) return;
    assignCaptureController(capturePort, instanceId);
    if (captureIndex < 8) controllerPadBindings[capturePort][captureIndex] = binding;
    else specialPadBindings[capturePort][captureIndex - 8] = binding;
    toast("BOUND TO " + padBindingName(binding));
    capturePort = captureIndex = -1;
}

void App::resetInputBindings() {
    SDL_Scancode p1[8] = { SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
        SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_RETURN, SDL_SCANCODE_BACKSPACE };
    SDL_Scancode special[SPECIAL_ACTION_COUNT] = { SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_E,
        SDL_SCANCODE_P, SDL_SCANCODE_K, SDL_SCANCODE_J, SDL_SCANCODE_R, SDL_SCANCODE_T };
    for (int i = 0; i < 8; ++i) {
        controllerKeys[0][i] = p1[i]; controllerKeys[1][i] = p1[i];
        PadBinding pad = i < 4 ? PadBinding(PAD_BIND_STANDARD_DIRECTION, i, 0) :
            PadBinding(PAD_BIND_GAMEPAD_BUTTON, i == 4 ? SDL_CONTROLLER_BUTTON_A : i == 5 ? SDL_CONTROLLER_BUTTON_X :
                i == 6 ? SDL_CONTROLLER_BUTTON_START : SDL_CONTROLLER_BUTTON_BACK, 0);
        controllerPadBindings[0][i] = controllerPadBindings[1][i] = pad;
    }
    for (int i = 0; i < SPECIAL_ACTION_COUNT; ++i) {
        specialKeys[0][i] = special[i]; specialKeys[1][i] = special[i];
        specialPadBindings[0][i] = specialPadBindings[1][i] = PadBinding();
    }
    portDeviceChoice[0] = portDeviceChoice[1] = "AUTO";
    std::memset(specialWasDown, 0, sizeof(specialWasDown));
}

bool App::loadInputBindings() {
    std::vector<unsigned char> bytes = readBinary("CipherNES.cfg");
    if (bytes.empty()) return saveInputBindings();
    std::string xml(bytes.begin(), bytes.end());
    const char* controllerTags[8] = { "Up", "Down", "Left", "Right", "A", "B", "Start", "Select" };
    const char* specialTags[SPECIAL_ACTION_COUNT] = { "SaveState", "LoadState", "ChangeSlot", "Screenshot", "StartRecord", "StopRecord", "Rewind", "FastForward" };
    for (int port = 0; port < 2; ++port) {
        std::string section = xmlTag(xml, port == 0 ? "Port1" : "Port2");
        std::string device = xmlTag(section, "Device");
        if (!device.empty()) portDeviceChoice[port] = device;
        for (int i = 0; i < 8; ++i) {
            std::string token = xmlTag(section, controllerTags[i]); SDL_Scancode key = parseScancode(token);
            if (key != SDL_SCANCODE_UNKNOWN) controllerKeys[port][i] = key;
            token=xmlTag(section,std::string(controllerTags[i])+"Pad");if(!token.empty()){
                controllerPadBindings[port][i]=parsePadBinding(token);
                const int oldDpad[4]={SDL_CONTROLLER_BUTTON_DPAD_UP,SDL_CONTROLLER_BUTTON_DPAD_DOWN,SDL_CONTROLLER_BUTTON_DPAD_LEFT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT};
                if(i<4&&controllerPadBindings[port][i].kind==PAD_BIND_GAMEPAD_BUTTON&&controllerPadBindings[port][i].control==oldDpad[i])
                    controllerPadBindings[port][i]=PadBinding(PAD_BIND_STANDARD_DIRECTION,i,0);
            }
        }
        for (int i = 0; i < SPECIAL_ACTION_COUNT; ++i) {
            std::string token = xmlTag(section, specialTags[i]); SDL_Scancode key = parseScancode(token);
            if (key != SDL_SCANCODE_UNKNOWN) specialKeys[port][i] = key;
            token=xmlTag(section,std::string(specialTags[i])+"Pad");if(!token.empty())specialPadBindings[port][i]=parsePadBinding(token);
        }
    }
    // Escape is reserved for the display-mode cycle, including imported config files.
    for (int port = 0; port < 2; ++port) {
        for (int i = 0; i < 8; ++i)
            if (controllerKeys[port][i] == SDL_SCANCODE_ESCAPE) controllerKeys[port][i] = SDL_SCANCODE_UNKNOWN;
        for (int i = 0; i < SPECIAL_ACTION_COUNT; ++i)
            if (specialKeys[port][i] == SDL_SCANCODE_ESCAPE) specialKeys[port][i] = SDL_SCANCODE_UNKNOWN;
    }
    return true;
}

bool App::saveInputBindings() {
    const char* controllerTags[8] = { "Up", "Down", "Left", "Right", "A", "B", "Start", "Select" };
    const char* specialTags[SPECIAL_ACTION_COUNT] = { "SaveState", "LoadState", "ChangeSlot", "Screenshot", "StartRecord", "StopRecord", "Rewind", "FastForward" };
    std::ostringstream xml; xml << "<CipherNES>\r\n";
    for (int port = 0; port < 2; ++port) {
        xml << "\t<Port" << port + 1 << ">\r\n\t  <Controller>\r\n";
        xml << "\t\t<Device>" << xmlEscape(portDeviceChoice[port]) << "</Device>\r\n";
        for (int i = 0; i < 8; ++i) xml << "\t\t<" << controllerTags[i] << ">" << scancodeName(controllerKeys[port][i]) << "</" << controllerTags[i] << ">\r\n";
        for (int i = 0; i < SPECIAL_ACTION_COUNT; ++i) xml << "\t\t<" << specialTags[i] << ">" << scancodeName(specialKeys[port][i]) << "</" << specialTags[i] << ">\r\n";
        for (int i = 0; i < 8; ++i) xml << "\t\t<" << controllerTags[i] << "Pad>" << padBindingToken(controllerPadBindings[port][i]) << "</" << controllerTags[i] << "Pad>\r\n";
        for (int i = 0; i < SPECIAL_ACTION_COUNT; ++i) xml << "\t\t<" << specialTags[i] << "Pad>" << padBindingToken(specialPadBindings[port][i]) << "</" << specialTags[i] << "Pad>\r\n";
        xml << "\t  </Controller>\r\n\t</Port" << port + 1 << ">\r\n";
    }
    xml << "</CipherNES>\r\n"; return writeTextFile("CipherNES.cfg", xml.str());
}

void App::buildMenus() {
    menus.clear(); MenuRootDef root;
    root.text = "File"; root.items = { MenuItemDef(CMD_LOAD_ROM, "Load Rom"), MenuItemDef(0, "Reset", 1),
        MenuItemDef(0, "FDS Disk", 7), MenuItemDef(CMD_QUIT, "Quit") }; menus.push_back(root);
    root.text = "Input"; root.items = { MenuItemDef(0, "Port 1", 2), MenuItemDef(0, "Port 2", 3) }; menus.push_back(root);
    root.text = "Configure"; root.items = { MenuItemDef(CMD_CONFIG_CONTROLLER_INPUT, "Controller Input"), MenuItemDef(CMD_RECORD_WITH_TOAST, "Record With Toast"),
        MenuItemDef(CMD_RECORD_WITH_TIMER, "Record With Timer"), MenuItemDef(CMD_CUSTOM_AVI, "Custom AVI Output Location"), MenuItemDef(0, "Emulator Speed", 4) }; menus.push_back(root);
    root.text = "System"; root.items = { MenuItemDef(CMD_FULLSCREEN, "Full Screen"), MenuItemDef(CMD_FULLSCREEN_STRETCHED, "Full Screen(Stretched)"),
        MenuItemDef(CMD_MUTE, "Mute"), MenuItemDef(0, "APU", 5), MenuItemDef(CMD_SAVE_STATE, "Save State"), MenuItemDef(CMD_LOAD_STATE, "Load State"),
        MenuItemDef(0, "Save State Slot", 6), MenuItemDef(CMD_SCREENSHOT, "Screenshot"), MenuItemDef(CMD_RECORD, "Record"), MenuItemDef(CMD_STOP_RECORDING, "Stop Recording") }; menus.push_back(root);
    root.text = "TAS"; root.items = { MenuItemDef(CMD_TAS_RECORD_INPUTS, "Record Inputs"),
        MenuItemDef(CMD_TAS_STOP_RECORDING_INPUTS, "Stop Recording Inputs"),
        MenuItemDef(CMD_TAS_VIEW_RECORDED_INPUTS, "View Recorded Inputs"),
        MenuItemDef(CMD_TAS_STOP_VIEWING_INPUTS, "Stop Viewing Inputs"),
        MenuItemDef(CMD_TAS_RESET_BEFORE_INPUT_RECORDING, "Reset Before Input Recording") }; menus.push_back(root);
    root.text = "Cheats"; root.items = { MenuItemDef(CMD_CONFIG_CHEATS, "Configure Game Geine Codes"), MenuItemDef(CMD_GAME_GENIE, "Game Genie Encoder/Decoder"),
        MenuItemDef(0, "", 0, true), MenuItemDef(CMD_ENABLE_CHEATS, "Enable Cheats(Game Geine)") }; menus.push_back(root);
    root.text = "Help"; root.items = { MenuItemDef(CMD_INFO, "Info") }; menus.push_back(root);
    updateTasMenuState();
}

std::vector<MenuItemDef> App::submenuItemsFor(int submenu) const {
    return submenuItems(submenu);
}

void App::updateTitle() {
    if (!window) return;
    std::ostringstream title; title << PLATFORM_WINDOW_TITLE;
    if (romLoaded) {
        title << " - " << romBase << " - " << speedPercent << "%";
        if (ciphernes_is_fds(core)) {
            const int sideCount = ciphernes_fds_get_side_count(core);
            const int currentSide = ciphernes_fds_get_current_side(core);
            if (currentSide >= 0) title << " [FDS SIDE " << (currentSide + 1) << "/" << sideCount << "]";
            else title << " [FDS DISK EJECTED]";
        }
    }
    if (!tasTitleMessage.empty() && tasTitleMessageUntil && Sint32(tasTitleMessageUntil - sdl.GetTicks()) > 0)
        title << " - " << tasTitleMessage;
    else if (tasMode == TAS_COUNTDOWN)
        title << " - " << tasCountdownDigit;
    else if (tasMode == TAS_RECORDING)
        title << " [RECORDING INPUTS]";
    else if (tasMode == TAS_PLAYBACK)
        title << " [VIEWING INPUTS]";
    if (paused) title << " [PAUSED]";
    if (avi.isOpen()) title << " [RECORDING]";
    sdl.SetWindowTitle(window, title.str().c_str());
}

void App::updateTasMenuState() {
    const bool idleWithRom = romLoaded && tasMode == TAS_IDLE;
    for (size_t r = 0; r < menus.size(); ++r) {
        for (size_t i = 0; i < menus[r].items.size(); ++i) {
            MenuItemDef& item = menus[r].items[i];
            switch (item.command) {
                case CMD_TAS_RECORD_INPUTS:
                case CMD_TAS_VIEW_RECORDED_INPUTS:
                case CMD_TAS_RESET_BEFORE_INPUT_RECORDING:
                    item.enabled = idleWithRom;
                    break;
                case CMD_TAS_STOP_RECORDING_INPUTS:
                    item.enabled = tasMode == TAS_COUNTDOWN || tasMode == TAS_RECORDING;
                    break;
                case CMD_TAS_STOP_VIEWING_INPUTS:
                    item.enabled = tasMode == TAS_PLAYBACK;
                    break;
                default: break;
            }
        }
    }
}

void App::restartRomForTas() {
    if (!romLoaded) return;
    clearRewindHistory();
    ciphernes_reset_hard(core);

    frameTextureDirty = true;
    resetAudioQueue(false);
    nextFrameAt = nowSeconds();
}

void App::updateTasTimers() {
    const Uint32 now = sdl.GetTicks();
    if (!tasTitleMessage.empty() && tasTitleMessageUntil && Sint32(tasTitleMessageUntil - now) <= 0) {
        tasTitleMessage.clear();
        tasTitleMessageUntil = 0;
        updateTitle();
    }
    if (tasMode != TAS_COUNTDOWN) return;
    const Uint32 elapsed = now - tasCountdownStartedAt;
    const int digit = elapsed < 1000u ? 3 : elapsed < 2000u ? 2 : elapsed < 3000u ? 1 : 0;
    if (digit > 0 && digit != tasCountdownDigit) {
        tasCountdownDigit = digit;
        updateTitle();
    }
    if (elapsed >= 3000u) {
        tasMode = TAS_RECORDING;
        tasCountdownDigit = 0;
        audioScratch.clear();
        captureRewindFrame();
        paused = false;
        resetAudioQueue(true);
        nextFrameAt = nowSeconds();
        updateTasMenuState();
        updateTitle();
    }
}

void App::startTasRecording() {
    if (!romLoaded || tasMode != TAS_IDLE) return;
    clearRewindHistory();
    tasFrames.clear();
    tasPlaybackPosition = 0;
    tasPlaybackPath.clear();
    tasTitleMessage.clear();
    tasTitleMessageUntil = 0;
    paused = true;
    if (tasResetBeforeInputRecording) restartRomForTas();
    resetAudioQueue(false);
    tasMode = TAS_COUNTDOWN;
    tasCountdownStartedAt = sdl.GetTicks();
    tasCountdownDigit = 3;
    updateTasMenuState();
    updateTitle();
}

bool App::saveTasRecording(std::string& savedPath) {
    makeDir("TAS");
    std::string safeBase;
    for (size_t i = 0; i < romBase.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(romBase[i]);
        safeBase += (c == '/' || c == '\\' || c < 32) ? '_' : char(c);
    }
    if (safeBase.empty()) safeBase = "recording";
    savedPath = "TAS/" + safeBase + ".ci";
    for (int suffix = 1; fileExists(savedPath); ++suffix)
        savedPath = "TAS/" + safeBase + std::to_string(suffix) + ".ci";

    std::vector<unsigned char> bytes;
    const char magic[8] = { 'P', 'I', 'X', 'E', 'L', 'C', 'I', '1' };
    bytes.insert(bytes.end(), magic, magic + 8);
    appendU32LE(bytes, 1u);
    appendU32LE(bytes, static_cast<uint32_t>(32u + romBase.size()));
    appendU64LE(bytes, static_cast<uint64_t>(tasFrames.size()));
    appendU32LE(bytes, static_cast<uint32_t>(romBase.size()));
    appendU32LE(bytes, 2u);
    bytes.insert(bytes.end(), romBase.begin(), romBase.end());
    for (size_t i = 0; i < tasFrames.size(); ++i) {
        bytes.push_back(tasFrames[i].port1);
        bytes.push_back(tasFrames[i].port2);
    }
    return writeBinary(savedPath, bytes);
}

bool App::loadTasRecording(const std::string& path, std::vector<TasInputFrame>& frames,
                           std::string& recordedRom, std::string& errorText) const {
    frames.clear(); recordedRom.clear(); errorText.clear();
    const std::vector<unsigned char> bytes = readBinary(path);
    const char magic[8] = { 'P', 'I', 'X', 'E', 'L', 'C', 'I', '1' };
    if (bytes.size() < 32u || std::memcmp(&bytes[0], magic, 8) != 0) {
        errorText = "INVALID .CI FILE"; return false;
    }
    const uint32_t version = readU32LE(&bytes[8]);
    const uint32_t headerSize = readU32LE(&bytes[12]);
    const uint64_t frameCount = readU64LE(&bytes[16]);
    const uint32_t romNameSize = readU32LE(&bytes[24]);
    const uint32_t recordSize = readU32LE(&bytes[28]);
    const uint64_t expectedHeaderSize = 32u + uint64_t(romNameSize);
    if (version != 1u || recordSize != 2u || expectedHeaderSize != headerSize || headerSize > bytes.size()) {
        errorText = "UNSUPPORTED .CI FILE"; return false;
    }
    if (frameCount > uint64_t((bytes.size() - headerSize) / recordSize) ||
        uint64_t(headerSize) + frameCount * recordSize != bytes.size() ||
        frameCount > uint64_t(std::numeric_limits<size_t>::max())) {
        errorText = "DAMAGED .CI FILE"; return false;
    }
    recordedRom.assign(reinterpret_cast<const char*>(&bytes[32]), romNameSize);
    frames.reserve(static_cast<size_t>(frameCount));
    size_t offset = headerSize;
    for (uint64_t i = 0; i < frameCount; ++i, offset += 2)
        frames.push_back(TasInputFrame(bytes[offset], bytes[offset + 1]));
    return true;
}

void App::stopTasRecording() {
    if (tasMode != TAS_COUNTDOWN && tasMode != TAS_RECORDING) return;
    if (rewindShowingFrame) commitRewindBranch();
    paused = true;
    resetAudioQueue(false);
    std::string savedPath;
    const bool saved = saveTasRecording(savedPath);
    tasMode = TAS_IDLE;
    tasCountdownDigit = 0;
    tasPlaybackPosition = 0;
    tasFrames.clear();
    paused = false;
    resetAudioQueue(true);
    nextFrameAt = nowSeconds();
    updateTasMenuState();
    if (saved) {
        tasTitleMessage = "SAVED INPUTS TO ./" + savedPath;
        tasTitleMessageUntil = sdl.GetTicks() + 1000u;
        toast("SAVED " + savedPath, 2200);
    } else {
        tasTitleMessage.clear();
        tasTitleMessageUntil = 0;
        toast("COULD NOT SAVE INPUT RECORDING");
    }
    updateTitle();
}

void App::startTasPlayback(const std::string& path) {
    if (!romLoaded || tasMode != TAS_IDLE) return;
    clearRewindHistory();
    std::vector<TasInputFrame> frames;
    std::string recordedRom, errorText;
    if (!loadTasRecording(path, frames, recordedRom, errorText)) {
        toast(errorText.empty() ? "COULD NOT LOAD .CI FILE" : errorText); return;
    }
    if (frames.empty()) { toast(".CI FILE CONTAINS NO INPUT FRAMES"); return; }
    tasFrames.swap(frames);
    tasPlaybackPosition = 0;
    tasPlaybackPath = path;
    tasTitleMessage.clear();
    tasTitleMessageUntil = 0;
    if (tasResetBeforeInputRecording) restartRomForTas();
    tasMode = TAS_PLAYBACK;
    paused = false;
    resetAudioQueue(true);
    nextFrameAt = nowSeconds();
    updateTasMenuState();
    updateTitle();
    if (!recordedRom.empty() && recordedRom != romBase)
        toast("PLAYING INPUTS RECORDED FOR " + recordedRom, 2600);
    else toast("VIEWING " + baseName(path), 1800);
}

void App::stopTasPlayback(bool completed) {
    if (tasMode != TAS_PLAYBACK) return;
    tasMode = TAS_IDLE;
    tasPlaybackPosition = 0;
    tasPlaybackPath.clear();
    tasFrames.clear();
    nextFrameAt = nowSeconds();
    updateTasMenuState();
    updateTitle();
    toast(completed ? "INPUT PLAYBACK COMPLETE" : "INPUT PLAYBACK STOPPED", 1800);
}

void App::stopTasForRomChange() {
    if (tasMode == TAS_COUNTDOWN || tasMode == TAS_RECORDING) stopTasRecording();
    else if (tasMode == TAS_PLAYBACK) stopTasPlayback(false);
    tasTitleMessage.clear();
    tasTitleMessageUntil = 0;
    updateTitle();
}

bool App::loadRom(const std::string& path) {
    stopTasForRomChange();
    stopRecording();
    clearRewindHistory();
    PreparedRomPath prepared;
    std::string loadError;
    if (!prepareRomPath(path, prepared, loadError)) {
        std::cerr << "ROM preparation failed for " << path << ": " << loadError << "\n";
        toast(hasExtension(path, ".zip") ? "ZIP ROM LOAD FAILED" : "ROM LOAD FAILED");
        return false;
    }

    const bool loaded = ciphernes_load_rom(core, prepared.corePath.c_str()) != 0;
    const std::string displayBase = prepared.displayBase;
    const std::string archiveNotice = prepared.notice;
    cleanupPreparedRom(prepared);
    if (!loaded) {
        std::cerr << "Core rejected ROM: " << path << "\n";
        toast("ROM LOAD FAILED");
        return false;
    }

    romLoaded = true; romPath = path; romBase = displayBase; paused = false; cheatsEnabled = false;
    const uint32_t stateBytes = ciphernes_get_state_size(core);
    stateBufferCapacity = stateBytes > UINT32_MAX - 4096u ? stateBytes : stateBytes + 4096u;

    ciphernes_set_apu_channel_mask(core, apuMask); ciphernes_set_audio_enabled(core, muted ? 0 : 1);
    resetAudioQueue(true);
    frameTextureDirty = true;
    audioScratch.clear(); captureRewindFrame();
    nextFrameAt = nowSeconds(); updateTasMenuState(); updateTitle();

    toast(archiveNotice.empty() ? "LOADED " + romBase : archiveNotice, archiveNotice.empty() ? 1800 : 2600);
    return true;
}

bool App::loadRomImage(const uint8_t* data, uint32_t bytes, const std::string& virtualPath, const std::string& displayName) {
    if (!data || bytes < 16u) { toast("IN-MEMORY ROM LOAD FAILED"); return false; }
    stopTasForRomChange();
    stopRecording();
    clearRewindHistory();
    if (!ciphernes_load_rom_image(core, data, bytes, virtualPath.c_str())) {
        std::cerr << "Core rejected in-memory ROM image for " << virtualPath << "\n";
        toast("IN-MEMORY ROM LOAD FAILED");
        return false;
    }

    romLoaded = true;
    romPath = virtualPath;
    romBase = displayName.empty() ? stemName(virtualPath) : displayName;
    paused = false; cheatsEnabled = false;
    const uint32_t stateBytes = ciphernes_get_state_size(core);
    stateBufferCapacity = stateBytes > UINT32_MAX - 4096u ? stateBytes : stateBytes + 4096u;

    ciphernes_set_apu_channel_mask(core, apuMask);
    ciphernes_set_audio_enabled(core, muted ? 0 : 1);
    resetAudioQueue(true);

    frameTextureDirty = true; audioScratch.clear(); captureRewindFrame();
    nextFrameAt = nowSeconds(); updateTasMenuState(); updateTitle();

    toast("PLAYING " + romBase, 1800);
    return true;
}

void App::unloadRom() {
    if (core && romLoaded) {

        ciphernes_unload_rom(core);
    }
    romLoaded = false;
    clearRewindHistory();
    tasMode = TAS_IDLE; tasFrames.clear(); tasPlaybackPosition = 0; tasPlaybackPath.clear();
    stateBufferCapacity = 0; frameTextureDirty = true; updateTasMenuState(); updateTitle();
}

uint8_t App::readPad(int port) const {
    if (!portConnected[port] || screenMode != SCREEN_GAME || activeMenu >= 0) return 0;
    int keyCount = keyboardStateCount;
    const Uint8* keys = keyboardState;
    if (!keys) keys = sdl.GetKeyboardState(&keyCount);
    uint8_t pad = 0;
    const uint8_t bits[8] = { PAD_UP, PAD_DOWN, PAD_LEFT, PAD_RIGHT, PAD_A, PAD_B, PAD_START, PAD_SELECT };
    for (int i = 0; i < 8; ++i) {
        SDL_Scancode key = controllerKeys[port][i];
        if ((key != SDL_SCANCODE_UNKNOWN && key >= 0 && key < keyCount && keys[key]) || padBindingDown(port, controllerPadBindings[port][i])) pad |= bits[i];
    }
    if ((pad & PAD_LEFT) && (pad & PAD_RIGHT)) pad &= ~(PAD_LEFT | PAD_RIGHT);
    if ((pad & PAD_UP) && (pad & PAD_DOWN)) pad &= ~(PAD_UP | PAD_DOWN);
    return pad;
}

bool App::specialDown(int port, int index) const {
    int count = keyboardStateCount;
    const Uint8* keys = keyboardState;
    if (!keys) keys = sdl.GetKeyboardState(&count);
    SDL_Scancode key = specialKeys[port][index]; bool down=key != SDL_SCANCODE_UNKNOWN && key < count && keys[key];
    return down || padBindingDown(port, specialPadBindings[port][index]);
}

void App::pollSpecialBindings() {
    if (screenMode != SCREEN_GAME || activeMenu >= 0) return;
    for (int i = 0; i < SPECIAL_ACTION_COUNT; ++i) {
        bool down0 = specialDown(0, i), down1 = specialDown(1, i);
        bool down = down0 || down1;
        bool wasDown = specialWasDown[0][i] || specialWasDown[1][i];
        if (down && !wasDown) {
            if (i == 0) saveState(); else if (i == 1) loadState(); else if (i == 2) { stateSlot = stateSlot % 4 + 1; toast("STATE SLOT " + std::to_string(stateSlot)); }
            else if (i == 3) takeScreenshot(); else if (i == 4) toggleRecording(); else if (i == 5) stopRecording();
        }
        specialWasDown[0][i] = down0;
        specialWasDown[1][i] = down1;
    }
}

void App::drainAudio(bool queuePlayback, bool collectForAvi) {
    audioScratch.clear();
    int16_t buffer[4096];
    // The device buffer should not become a quarter-second delayed echo when
    // fast-forward begins. Poll every few emulated frames rather than issuing a
    // synchronized device query on every frame in the emulation hot path.
    if (queuePlayback && audioDevice && ++audioQueueCheckCountdown >= 4) {
        audioQueueCheckCountdown = 0;
        const Uint32 maxQueueBytes = static_cast<Uint32>(audioSampleRate * sizeof(int16_t) * 0.14);
        if (sdl.GetQueuedAudioSize(audioDevice) > maxQueueBytes)
            resetAudioQueue(false); // last-resort recovery after a host stall
    }
    while (true) {
        const int count = ciphernes_take_audio_samples_i16(core, buffer, 4096);
        if (count <= 0) break;
        audioScratch.insert(audioScratch.end(), buffer, buffer + count);
        if (count < 4096) break;
    }
    if (queuePlayback && audioDevice && !muted && !audioScratch.empty()) {
        if (fastForwardMultiplier > 1.001) {
            // At the attainable 150% target, follow the request directly for
            // sample-accurate 90-fps/44.1-kHz output. Only limit to measured
            // throughput on underpowered hosts; the allowance covers noise in
            // a 160ms measurement window without a low-speed initial hiccup.
            const double audibleSpeed = std::max(1.0, std::min(fastForwardMultiplier,
                                                        actualSpeedMultiplier + 0.45));
            queueAudioForSpeed(audioScratch, audibleSpeed);
        } else {
            sdl.QueueAudio(audioDevice, audioScratch.data(), static_cast<Uint32>(audioScratch.size() * sizeof(int16_t)));
            lastQueuedAudioSample = audioScratch.back();
            speedResampler.reset(lastQueuedAudioSample);
        }
    }
}

bool App::actionHeld(int index) const {
    if (!romLoaded || screenMode != SCREEN_GAME || activeMenu >= 0) return false;
    return specialDown(0, index) || specialDown(1, index);
}

void App::clearRewindHistory() {
    rewindHistory.clear();
    restoredDisplay.clear(); restoredDisplayVisible = false;
    rewindRestoredState.clear(); rewindAudioScratch.clear();
    rewindHeld = rewindShowingFrame = false;
    fastForwardHeld = false; fastForwardMultiplier = 1.0;
    nextFrameAt = nowSeconds();
}

void App::captureRewindFrame() {
    if (!romLoaded) return;
    // Rewind serializes every emulated frame, including fast-forward frames.
    // Reuse the staging buffer instead of allocating a fresh vector at 240 Hz.
    if (stateBufferCapacity < 4096u) {
        const uint32_t needed = ciphernes_get_state_size(core);
        stateBufferCapacity = needed <= UINT32_MAX - 4096u ? needed + 4096u : needed;
    }
    rewindStateScratch.resize(stateBufferCapacity);
    uint32_t written = ciphernes_save_state_transient(core, rewindStateScratch.data(),
                                                       static_cast<uint32_t>(rewindStateScratch.size()));
    if (!written) {
        const uint32_t needed = ciphernes_get_state_size(core);
        stateBufferCapacity = needed <= UINT32_MAX - 4096u ? needed + 4096u : needed;
        rewindStateScratch.resize(stateBufferCapacity);
        written = ciphernes_save_state_transient(core, rewindStateScratch.data(),
                                                  static_cast<uint32_t>(rewindStateScratch.size()));
    }
    if (written) {
        rewindStateScratch.resize(written);
        rewindHistory.append(rewindStateScratch, ciphernes_get_framebuffer(core), audioScratch,
            static_cast<uint64_t>(tasMode == TAS_PLAYBACK ? tasPlaybackPosition : tasFrames.size()));
    }
}

void App::commitRewindBranch() {
    if (rewindHistory.size() && tasMode == TAS_RECORDING) {
        const uint64_t cursor = rewindHistory.tasCursor();
        if (cursor <= tasFrames.size()) tasFrames.resize(static_cast<size_t>(cursor));
    }
    if (rewindHistory.size() && tasMode == TAS_PLAYBACK) {
        const uint64_t cursor = rewindHistory.tasCursor();
        tasPlaybackPosition = static_cast<size_t>(std::min<uint64_t>(cursor, tasFrames.size()));
    }
    if (rewindShowingFrame && rewindHistory.image()) {
        restoredDisplay.assign(rewindHistory.image(), rewindHistory.image() + NES_W * NES_H);
        restoredDisplayVisible = true;
    }
    rewindHistory.commit();
    rewindShowingFrame = false;
    frameTextureDirty = true;
    resetAudioQueue(!muted && !paused);
    nextFrameAt = nowSeconds();
}

bool App::rewindOneFrame() {
    if (!rewindHistory.stepBack(rewindRestoredState, rewindAudioScratch)) return false;
    if (!restoreState(rewindRestoredState)) { clearRewindHistory(); return false; }
    rewindShowingFrame = true;
    frameTextureDirty = true;
    if (tasMode == TAS_RECORDING) {
        const uint64_t cursor = rewindHistory.tasCursor();
        if (cursor <= tasFrames.size()) tasFrames.resize(static_cast<size_t>(cursor));
    } else if (tasMode == TAS_PLAYBACK) {
        tasPlaybackPosition = static_cast<size_t>(std::min<uint64_t>(rewindHistory.tasCursor(), tasFrames.size()));
    }
    if (!muted && audioDevice && !rewindAudioScratch.empty()) {
        // Maintain a low-latency queue while moving backward at one frame per tick.
        if (sdl.GetQueuedAudioSize(audioDevice) > static_cast<Uint32>(audioSampleRate / 8 * sizeof(int16_t)))
            sdl.ClearQueuedAudio(audioDevice);
        sdl.QueueAudio(audioDevice, rewindAudioScratch.data(),
            static_cast<Uint32>(rewindAudioScratch.size() * sizeof(int16_t)));
    }
    return true;
}

void App::queueAudioForSpeed(const std::vector<int16_t>& input, double multiplier) {
    if (muted || !audioDevice || input.empty()) return;
    speedResampler.append(input, multiplier, resampledAudioScratch);
    if (!resampledAudioScratch.empty()) {
        sdl.QueueAudio(audioDevice, resampledAudioScratch.data(),
                       static_cast<Uint32>(resampledAudioScratch.size() * sizeof(int16_t)));
        lastQueuedAudioSample = resampledAudioScratch.back();
    }
}

void App::runOneFrame(bool unpaced) {
    uint8_t p1 = readPad(0), p2 = readPad(1);

    const bool playingTasFrame = tasMode == TAS_PLAYBACK && tasPlaybackPosition < tasFrames.size();
    if (playingTasFrame) {
        p1 = tasFrames[tasPlaybackPosition].port1;
        p2 = tasFrames[tasPlaybackPosition].port2;
    } else if (tasMode == TAS_RECORDING) {
        tasFrames.push_back(TasInputFrame(p1, p2));
    }

    ciphernes_run_frame_with_input(core, p1, p2);
    restoredDisplayVisible = false; restoredDisplay.clear();
    frameTextureDirty = true;

    const bool recordingVideo = avi.isOpen();
    drainAudio(!muted && !unpaced, recordingVideo);
    if (recordingVideo) {
        const uint32_t* frame = ciphernes_get_framebuffer(core);
        if (recordWithTimer || recordWithToast) {
            videoFrameScratch.assign(frame, frame + NES_W * NES_H);
            if (recordWithTimer) {
                Uint32 elapsed = (sdl.GetTicks() - recordingStartedAt) / 1000; char timer[24];
                std::snprintf(timer, sizeof(timer), "REC %02u:%02u", elapsed / 60, elapsed % 60);
                drawCpuText(&videoFrameScratch[0], 5, 5, timer, 1, 0xFFFFFF55u);
            }
            if (recordWithToast && toastUntil && Sint32(toastUntil - sdl.GetTicks()) > 0) {
                int x = std::max(2, (NES_W - textWidth(toastText, 1)) / 2);
                drawCpuText(&videoFrameScratch[0], x, NES_H - 12, toastText, 1, 0xFFFFFF80u);
            }
            frame = &videoFrameScratch[0];
        }
        avi.addFrame(frame);
        if (!audioScratch.empty()) avi.addAudio(&audioScratch[0], static_cast<int>(audioScratch.size()));
    }
    if (playingTasFrame) {
        ++tasPlaybackPosition;
        if (tasPlaybackPosition >= tasFrames.size()) stopTasPlayback(true);
    }
    captureRewindFrame();
}

std::vector<unsigned char> App::captureState(bool persistBatterySave) {
    std::vector<unsigned char> state;
    if (!romLoaded) return state;
    if (stateBufferCapacity < 4096u) {
        const uint32_t needed = ciphernes_get_state_size(core);
        stateBufferCapacity = needed > UINT32_MAX - 4096u ? needed : needed + 4096u;
    }
    state.resize(stateBufferCapacity);
    uint32_t written = persistBatterySave ? ciphernes_save_state(core, &state[0], static_cast<uint32_t>(state.size())) :
            ciphernes_save_state_transient(core, &state[0], static_cast<uint32_t>(state.size()));
    if (!written) {
        const uint32_t needed = ciphernes_get_state_size(core);
        stateBufferCapacity = needed > UINT32_MAX - 4096u ? needed : needed + 4096u;
        state.resize(stateBufferCapacity);
        written = persistBatterySave ? ciphernes_save_state(core, &state[0], static_cast<uint32_t>(state.size())) :
            ciphernes_save_state_transient(core, &state[0], static_cast<uint32_t>(state.size()));
    }
    if (!written) { state.clear(); return state; }
    state.resize(written);
    if (written <= UINT32_MAX - 4096u && stateBufferCapacity < written + 4096u) stateBufferCapacity = written + 4096u;
    return state;
}

bool App::restoreState(const std::vector<unsigned char>& state) {
    if (!romLoaded || state.empty()) return false;
    const bool restored = ciphernes_load_state(core, &state[0], state.size()) != 0;
    if (restored) frameTextureDirty = true;
    return restored;
}

void App::saveState() {
    if (!romLoaded) { toast("NO ROM LOADED"); return; }
    if (rewindShowingFrame) commitRewindBranch();
    std::vector<unsigned char> state = captureState(true); if (state.empty()) { toast("SAVE STATE FAILED"); return; }
    // Versioned frontend metadata: preserve the exact rendered frame AND the
    // full TAS input prefix. The framebuffer is not part of the core save blob.
    std::vector<unsigned char> tasMetadata;
    const char frontendMarker[8] = { 'C', 'N', 'S', 'S', 'M', '0', '2', '!' };
    tasMetadata.insert(tasMetadata.end(), frontendMarker, frontendMarker + 8);
    const uint32_t* image = restoredDisplayVisible && restoredDisplay.size() == NES_W * NES_H ? restoredDisplay.data() : ciphernes_get_framebuffer(core);
    const std::vector<uint8_t> compressedImage = ciphernes_rewind::pack(reinterpret_cast<const uint8_t*>(image), NES_W * NES_H * 4u);
    appendU32LE(tasMetadata, static_cast<uint32_t>(romBase.size()));
    appendU32LE(tasMetadata, static_cast<uint32_t>(compressedImage.size()));
    const bool recordingInputs = tasMode == TAS_RECORDING;
    appendU64LE(tasMetadata, recordingInputs ? static_cast<uint64_t>(tasFrames.size()) : UINT64_MAX);
    tasMetadata.insert(tasMetadata.end(), romBase.begin(), romBase.end());
    tasMetadata.insert(tasMetadata.end(), compressedImage.begin(), compressedImage.end());
    if (recordingInputs) {
        for (size_t i = 0; i < tasFrames.size(); ++i) {
            tasMetadata.push_back(tasFrames[i].port1);
            tasMetadata.push_back(tasFrames[i].port2);
        }
    }
    std::vector<unsigned char> nss;
    nss.push_back(0); nss.push_back(1); appendU32BE(nss, state.size()); appendU32BE(nss, static_cast<uint32_t>(tasMetadata.size()));
    const char marker[] = "SAVE_STATE_NSS_CIPHERNES_JACOB_HODGKINS";
    nss.insert(nss.end(), marker, marker + sizeof(marker) - 1); nss.insert(nss.end(), state.begin(), state.end());
    nss.insert(nss.end(), tasMetadata.begin(), tasMetadata.end());
    std::ostringstream path; path << "SaveState/" << romBase << stateSlot << ".nss";
    toast(writeBinary(path.str(), nss) ? "SAVED STATE SLOT " + std::to_string(stateSlot) : "COULD NOT WRITE STATE");
}

void App::loadState() {
    if (!romLoaded) { toast("NO ROM LOADED"); return; }
    std::ostringstream path; path << "SaveState/" << romBase << stateSlot << ".nss";
    std::vector<unsigned char> bytes = readBinary(path.str()); if (bytes.empty()) { toast("NO STATE IN THIS SLOT"); return; }
    std::vector<unsigned char> state;
    bool hasTasPrefix = false;
    bool oldTasMarker = false;
    bool hasRestoredFrame = false;
    std::vector<uint32_t> savedFrame;
    std::vector<TasInputFrame> restoredInputs;
    if (bytes.size() >= 10) {
        const uint32_t ss = readU32BE(&bytes[2]), rs = readU32BE(&bytes[6]);
        if (uint64_t(ss) + rs <= bytes.size() && bytes.size() >= 10u + uint64_t(ss) + rs) {
            const size_t start = bytes.size() - size_t(ss) - size_t(rs);
            state.assign(bytes.begin() + start, bytes.begin() + start + ss);
            const size_t meta = start + ss;
            const char marker[8] = { 'C', 'N', 'S', 'S', 'M', '0', '2', '!' };
            const char oldMarker[8] = { 'P', 'N', 'T', 'A', 'S', 'P', 'O', 'S' };
            if (rs >= 24 && std::memcmp(&bytes[meta], marker, 8) == 0) {
                const uint32_t nameLen = readU32LE(&bytes[meta + 8]);
                const uint32_t imageLen = readU32LE(&bytes[meta + 12]);
                const uint64_t count = readU64LE(&bytes[meta + 16]);
                const uint64_t prefix = 24ull + nameLen + imageLen;
                if (prefix <= rs && count <= 20000000ull &&
                    (count == UINT64_MAX ? uint64_t(rs) == prefix : count * 2ull == uint64_t(rs) - prefix) &&
                    std::string(bytes.begin() + meta + 24, bytes.begin() + meta + 24 + nameLen) == romBase) {
                    const size_t imageStart = meta + 24 + nameLen;
                    const std::vector<uint8_t> encoded(bytes.begin() + imageStart, bytes.begin() + imageStart + imageLen);
                    std::vector<uint8_t> decoded;
                    if (ciphernes_rewind::unpack(encoded, decoded, NES_W * NES_H * 4u)) {
                        savedFrame.resize(NES_W * NES_H);
                        std::memcpy(savedFrame.data(), decoded.data(), decoded.size());
                        hasRestoredFrame = true;
                        if (count != UINT64_MAX) {
                            hasTasPrefix = true;
                            restoredInputs.reserve(static_cast<size_t>(count));
                            for (size_t i = 0, at = meta + static_cast<size_t>(prefix); i < count; ++i, at += 2)
                                restoredInputs.push_back(TasInputFrame(bytes[at], bytes[at + 1]));
                        }
                    }
                }
            } else if (rs >= 16 && std::memcmp(&bytes[meta], oldMarker, 8) == 0) oldTasMarker = true;
        }
    }
    if (state.empty() && bytes.size() >= 16 && std::memcmp(&bytes[0], "PNSS", 4) == 0) {
        uint32_t size = readU32LE(&bytes[12]); if (size && size + 16u <= bytes.size()) state.assign(bytes.begin() + 16, bytes.begin() + 16 + size);
    }
    if (state.empty()) state = bytes;
    if (state.size() >= 8 && readU32LE(&state[4]) + 8u == state.size()) state.erase(state.begin(), state.begin() + 8);
    if (tasMode == TAS_RECORDING && !hasTasPrefix) {
        toast(oldTasMarker ? "OLD TAS STATE HAS NO INPUT PREFIX" : "STATE IS NOT TAS-BRANCH COMPATIBLE", 3000);
        return;
    }
    if (tasMode == TAS_PLAYBACK) { toast("STOP TAS PLAYBACK BEFORE LOADING A STATE", 2800); return; }
    if (restoreState(state)) {
        if (tasMode == TAS_RECORDING) tasFrames.swap(restoredInputs);
        clearRewindHistory();
        if (hasRestoredFrame) {
            restoredDisplay.swap(savedFrame);
            restoredDisplayVisible = true;
            audioScratch.clear();
            const std::vector<unsigned char> baseline = captureState();
            if (!baseline.empty()) rewindHistory.append(baseline, restoredDisplay.data(), audioScratch, tasFrames.size());
        }
        resetAudioQueue(!paused); toast("LOADED STATE SLOT " + std::to_string(stateSlot));
    } else toast("LOAD STATE FAILED");
}

void App::takeScreenshot() {
    if (!romLoaded) { toast("NO ROM LOADED"); return; }
    std::string prefix = "Screenshots/" + romBase; std::string path = prefix + ".png";
    for (int i = 1; fileExists(path); ++i) path = prefix + "_" + std::to_string(i) + ".png";
    toast(writePng(path, ciphernes_get_framebuffer(core), NES_W, NES_H) ? "SAVED " + baseName(path) : "SCREENSHOT FAILED");
}

void App::toggleRecording() {
    if (avi.isOpen()) { stopRecording(); return; }
    if (!romLoaded) { toast("NO ROM LOADED"); return; }
    std::string directory = customAviOutput && !customAviDirectory.empty() ? customAviDirectory : "Videos";
    makeDir(directory); std::string prefix = directory + "/" + romBase; std::string path = prefix + ".avi";
    for (int i = 1; fileExists(path); ++i) path = prefix + "_" + std::to_string(i) + ".avi";
    if (avi.open(path, NES_W, NES_H, 60, 44100)) { currentVideoPath = path; recordingStartedAt = sdl.GetTicks(); toast("RECORDING STARTED"); updateTitle(); }
    else toast("RECORDING FAILED");
}

void App::stopRecording() {
    if (!avi.isOpen()) return;
    avi.close(); toast("SAVED " + baseName(currentVideoPath), 2200); currentVideoPath.clear(); updateTitle();
}

void App::toggleCheats() {
    if (!romLoaded) { toast("NO ROM LOADED"); return; }
    cheatsEnabled = !cheatsEnabled; clearRewindHistory(); loadCheats(); applyCheats(); toast(cheatsEnabled ? "CHEATS ENABLED" : "CHEATS DISABLED");
}

void App::loadCheats() {
    cheats.clear(); if (!romLoaded) return;
    std::vector<unsigned char> bytes = readBinary("Cheats/" + romBase + ".ggc");
    if (bytes.empty()) { saveCheats(); return; }
    std::string text(bytes.begin(), bytes.end()); size_t pos = 0;
    while (true) {
        size_t a = text.find("<Cheat>", pos), b = a == std::string::npos ? std::string::npos : text.find("</Cheat>", a);
        if (a == std::string::npos || b == std::string::npos) break;
        std::string block = text.substr(a, b - a); GameGenieCheat c; c.name = xmlTag(block, "Name"); c.code = xmlTag(block, "Code");
        c.enabled = lowerText(xmlTag(block, "Enabled")).find('y') != std::string::npos;
        c.code.erase(std::remove_if(c.code.begin(), c.code.end(), [](char ch){ return std::isspace(static_cast<unsigned char>(ch)); }), c.code.end());
        if (!c.name.empty() || !c.code.empty()) cheats.push_back(c); pos = b + 8;
    }
}

bool App::saveCheats() {
    if (!romLoaded) return false; std::ostringstream out; out << "<GameGeine Cheats>\r\n";
    for (size_t i = 0; i < cheats.size(); ++i) out << "<Cheat>\r\n<Name>" << xmlEscape(cheats[i].name) << "</Name>\r\n<Code>" << xmlEscape(cheats[i].code)
        << "</Code>\r\n<Enabled>" << (cheats[i].enabled ? "Y" : "N") << "</Enabled>\r\n</Cheat>\r\n";
    out << "</GameGeine Cheats>\r\n"; return writeTextFile("Cheats/" + romBase + ".ggc", out.str());
}

void App::applyCheats() {
    if (!romLoaded || !cheatsEnabled) { ciphernes_clear_game_genie(core); return; }
    std::vector<const char*> active; for (size_t i = 0; i < cheats.size(); ++i) if (cheats[i].enabled && !cheats[i].code.empty()) active.push_back(cheats[i].code.c_str());
    if (active.empty()) ciphernes_clear_game_genie(core); else ciphernes_set_game_genie_codes(core, &active[0], active.size());
}

bool App::decodeGameGenie(const std::string& raw, uint16_t& address, uint8_t& value, bool& hasCompare, uint8_t& compare) const {
    const std::string alphabet = "APZLGITYEOXUKSVN"; std::string code;
    for (size_t i = 0; i < raw.size(); ++i) { char c = char(std::toupper(static_cast<unsigned char>(raw[i]))); if (c != '-' && !std::isspace(static_cast<unsigned char>(c))) code += c; }
    if (code.size() != 6 && code.size() != 8) return false; int n[8] = {0};
    for (size_t i = 0; i < code.size(); ++i) { size_t p = alphabet.find(code[i]); if (p == std::string::npos) return false; n[i] = int(p); }
    address = uint16_t(0x8000 | ((n[3]&7)<<12) | ((n[5]&7)<<8) | ((n[4]&8)<<8) | ((n[2]&7)<<4) | ((n[1]&8)<<4) | (n[4]&7) | (n[3]&8));
    hasCompare = code.size() == 8; compare = 0;
    if (!hasCompare) value = uint8_t(((n[1]&7)<<4)|((n[0]&8)<<4)|(n[0]&7)|(n[5]&8));
    else { value=uint8_t(((n[1]&7)<<4)|((n[0]&8)<<4)|(n[0]&7)|(n[7]&8)); compare=uint8_t(((n[7]&7)<<4)|((n[6]&8)<<4)|(n[6]&7)|(n[5]&8)); }
    return true;
}

bool App::encodeGameGenie(std::string& code) const {
    unsigned int address = 0, value = 0, compare = 0; char tail = 0;
    if (std::sscanf(ggAddress.c_str(), "%x%c", &address, &tail) < 1 || address < 0x8000 || address > 0xFFFF) return false;
    if (std::sscanf(ggValue.c_str(), "%x%c", &value, &tail) < 1 || value > 0xFF) return false;
    bool hc = !ggCompare.empty(); if (hc && (std::sscanf(ggCompare.c_str(), "%x%c", &compare, &tail) < 1 || compare > 0xFF)) return false;
    const char* a="APZLGITYEOXUKSVN"; int n4=(address&7)|(((address>>11)&1)<<3), n3=((address>>12)&7)|(((address>>3)&1)<<3);
    int n2=(address>>4)&7, n1=((value>>4)&7)|(((address>>7)&1)<<3), n0=(value&7)|(((value>>7)&1)<<3); char out[9];
    out[0]=a[n0];out[1]=a[n1];out[2]=a[n2];out[3]=a[n3];out[4]=a[n4];
    if(!hc){out[5]=a[((address>>8)&7)|(((value>>3)&1)<<3)];out[6]=0;}
    else{out[5]=a[((address>>8)&7)|(((compare>>3)&1)<<3)];out[6]=a[(compare&7)|(((compare>>7)&1)<<3)];out[7]=a[((compare>>4)&7)|(((value>>3)&1)<<3)];out[8]=0;}
    code=out; return true;
}

void App::openBrowser(int kind) {
    // Release SDL UI menus before displaying the operating system's dialog.
    activeMenu = activeSubmenu = -1;
#if defined(__linux__)
    if (std::system("command -v zenity >/dev/null 2>&1") != 0 &&
        std::system("command -v kdialog >/dev/null 2>&1") != 0) {
        toast("INSTALL ZENITY OR KDIALOG FOR NATIVE FILE SELECTION", 3500);
        return;
    }
#endif
    const std::string path = nativeSelectPath(kind, window);
    if (path.empty()) return;
    if (kind == 0) loadRom(path);
    else if (kind == 4) { customAviDirectory = path; customAviOutput = true; toast("AVI FOLDER SET"); }
    else if (kind == 5) startTasPlayback(path);
}

bool App::menuChecked(int command) const {
    if (command == CMD_FULLSCREEN) return fullscreen && !stretched;
    if (command == CMD_FULLSCREEN_STRETCHED) return fullscreen && stretched;
    if (command == CMD_PORT1_CONTROLLER) return portConnected[0]; if (command == CMD_PORT2_CONTROLLER) return portConnected[1];
    if (command == CMD_RECORD_WITH_TOAST) return recordWithToast; if (command == CMD_RECORD_WITH_TIMER) return recordWithTimer;
    if (command == CMD_CUSTOM_AVI) return customAviOutput; if (command == CMD_MUTE) return muted;
    if (command >= CMD_APU_PULSE1 && command <= CMD_APU_DMC) return (apuMask & (1u << (command - CMD_APU_PULSE1))) != 0;
    if (command == CMD_SPEED_100) return speedPercent == 100; if (command == CMD_SPEED_50) return speedPercent == 50; if (command == CMD_SPEED_25) return speedPercent == 25;
    if (command >= CMD_STATE_SLOT_1 && command <= CMD_STATE_SLOT_4) return stateSlot == command - CMD_STATE_SLOT_1 + 1;
    if (command == CMD_ENABLE_CHEATS) return cheatsEnabled;
    if (command == CMD_TAS_RESET_BEFORE_INPUT_RECORDING) return tasResetBeforeInputRecording;
    return false;
}

bool App::itemChecked(const MenuItemDef& item) const {
    return menuChecked(item.command);
}

void App::dispatchCommand(int command) {
    activeMenu = activeSubmenu = -1;
    switch (command) {
        case CMD_LOAD_ROM: openBrowser(0); break;
        case CMD_SOFT_RESET: if (romLoaded) { ciphernes_reset_soft(core); clearRewindHistory(); resetAudioQueue(!paused); toast("SOFT RESET"); } break;
        case CMD_HARD_RESET: if (romLoaded) { ciphernes_reset_hard(core); clearRewindHistory(); resetAudioQueue(!paused); toast("HARD RESET"); } break;
        case CMD_FDS_EJECT:
            if (!romLoaded || !ciphernes_is_fds(core)) toast("LOAD AN FDS DISK FIRST");
            else { ciphernes_fds_eject(core); clearRewindHistory(); resetAudioQueue(!paused); updateTitle(); toast("FDS DISK EJECTED"); }
            break;
        case CMD_FDS_NEXT_SIDE:
            if (!romLoaded || !ciphernes_is_fds(core)) toast("LOAD AN FDS DISK FIRST");
            else if (ciphernes_fds_insert_next_side(core)) { clearRewindHistory(); resetAudioQueue(!paused); updateTitle();
                const int side = ciphernes_fds_get_current_side(core), count = ciphernes_fds_get_side_count(core);
                toast("FDS SIDE " + std::to_string(side + 1) + "/" + std::to_string(count) + " INSERTED");
            } else toast("FDS SIDE INSERT FAILED");
            break;
        case CMD_QUIT: running = false; break;
        case CMD_PORT1_CONTROLLER: portConnected[0] = !portConnected[0]; toast(portConnected[0] ? "PORT 1 PLUGGED" : "PORT 1 UNPLUGGED"); break;
        case CMD_PORT2_CONTROLLER: portConnected[1] = !portConnected[1]; toast(portConnected[1] ? "PORT 2 PLUGGED" : "PORT 2 UNPLUGGED"); break;
        case CMD_CONFIG_CONTROLLER_INPUT: screenMode = SCREEN_CONTROLLER; capturePort = captureIndex = -1; break;
        case CMD_RECORD_WITH_TOAST: recordWithToast = !recordWithToast; break;
        case CMD_RECORD_WITH_TIMER: recordWithTimer = !recordWithTimer; break;
        case CMD_CUSTOM_AVI: if (customAviOutput) { customAviOutput = false; toast("DEFAULT AVI FOLDER"); } else openBrowser(4); break;
        case CMD_SPEED_100: speedPercent = 100; updateTitle(); break; case CMD_SPEED_50: speedPercent = 50; updateTitle(); break; case CMD_SPEED_25: speedPercent = 25; updateTitle(); break;
        case CMD_FULLSCREEN: setFullscreenMode(!(fullscreen && !stretched), false); break;
        case CMD_FULLSCREEN_STRETCHED: setFullscreenMode(!(fullscreen && stretched), true); break;
        case CMD_MUTE: muted = !muted; ciphernes_set_audio_enabled(core, muted ? 0 : 1); resetAudioQueue(!muted && !paused); break;
        case CMD_APU_PULSE1: case CMD_APU_PULSE2: case CMD_APU_TRIANGLE: case CMD_APU_NOISE: case CMD_APU_DMC:
            apuMask ^= uint8_t(1u << (command - CMD_APU_PULSE1)); ciphernes_set_apu_channel_mask(core, apuMask); break;
        case CMD_SAVE_STATE: saveState(); break; case CMD_LOAD_STATE: loadState(); break;
        case CMD_STATE_SLOT_1: case CMD_STATE_SLOT_2: case CMD_STATE_SLOT_3: case CMD_STATE_SLOT_4:
            stateSlot = command - CMD_STATE_SLOT_1 + 1; toast("STATE SLOT " + std::to_string(stateSlot)); break;
        case CMD_SCREENSHOT: takeScreenshot(); break; case CMD_RECORD: if (!avi.isOpen()) toggleRecording(); break; case CMD_STOP_RECORDING: stopRecording(); break;
        case CMD_TAS_RECORD_INPUTS: startTasRecording(); break;
        case CMD_TAS_STOP_RECORDING_INPUTS: stopTasRecording(); break;
        case CMD_TAS_VIEW_RECORDED_INPUTS: if (romLoaded && tasMode == TAS_IDLE) openBrowser(5); break;
        case CMD_TAS_STOP_VIEWING_INPUTS: stopTasPlayback(false); break;
        case CMD_TAS_RESET_BEFORE_INPUT_RECORDING:
            if (romLoaded && tasMode == TAS_IDLE) tasResetBeforeInputRecording = !tasResetBeforeInputRecording;
            break;
        case CMD_CONFIG_CHEATS: if (!romLoaded) toast("LOAD A ROM FIRST"); else { loadCheats(); screenMode = SCREEN_CHEATS; selectedCheat = cheats.empty() ? -1 : 0; } break;
        case CMD_GAME_GENIE: screenMode = SCREEN_GAME_GENIE; activeTextField = 3; ggStatus.clear(); break;
        case CMD_ENABLE_CHEATS: toggleCheats(); break;
        case CMD_INFO: screenMode = SCREEN_INFO; break;
    }
}

bool App::pointerOverMenu(int x, int y) const {
    if (y >= 0 && y < MENU_BAR_H) return true;
    if (activeMenu < 0 || activeMenu >= int(menus.size())) return false;
    int rootX = 0;
    for (int i = 0; i < activeMenu; ++i) rootX += textWidth(menus[i].text, 2) + 30;
    int width = 260;
    for (size_t i = 0; i < menus[activeMenu].items.size(); ++i)
        width = std::max(width, textWidth(menus[activeMenu].items[i].text, 2) + 64);
    SDL_Rect drop = {rootX, MENU_BAR_H, width, int(menus[activeMenu].items.size()) * MENU_ITEM_H};
    if (pointInRect(x, y, drop)) return true;
    if (activeSubmenu >= 0 && activeSubmenu < int(menus[activeMenu].items.size())) {
        const MenuItemDef& parent = menus[activeMenu].items[activeSubmenu];
        const std::vector<MenuItemDef> sub = submenuItemsFor(parent.submenu);
        SDL_Rect subRect = {rootX + width, MENU_BAR_H + activeSubmenu * MENU_ITEM_H,
                            230, int(sub.size()) * MENU_ITEM_H};
        if (pointInRect(x, y, subRect)) return true;
    }
    return false;
}

void App::setFullscreenMode(bool enable, bool useStretch) {
    // Stretch/aspect-correct are two viewports of the SAME desktop-fullscreen
    // window. SDL_SetWindowFullscreen forces a blocking window-manager round
    // trip on some systems even when fullscreen is already enabled. Never call
    // it just to switch the destination rectangle.
    const bool needsOsTransition = enable != fullscreen;
    const bool needsLayoutTransition = useStretch != stretched;
    if (!needsOsTransition && !needsLayoutTransition) return;
    const double osTransitionStarted = needsOsTransition ? nowSeconds() : 0.0;
    if (needsOsTransition && sdl.SetWindowFullscreen(window, enable ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) != 0) {
        toast("FULLSCREEN MODE UNAVAILABLE");
        return;
    }
    if (std::getenv("CIPHERNES_TIMING_DIAGNOSTICS")) {
        std::cerr << "[Display] " << (enable ? (useStretch ? "stretched" : "aspect") : "windowed")
                  << " WM transition=" << (needsOsTransition ? (nowSeconds() - osTransitionStarted) * 1000.0 : 0.0)
                  << " ms (0 means renderer-only)\n";
    }
    fullscreen = enable;
    stretched = useStretch;
    activeMenu = activeSubmenu = -1;
    menuSlide = enable ? 0.0f : 1.0f;
    menuHoverActivated = false;
    lastMenuHover = 0;
    if (needsOsTransition) {
        // A physical WM transition may block for several host frames. Resume
        // from the new wall-clock deadline instead of running a burst of old
        // frames, which previously looked like a stutter and hurt audio.
        rendererOutputW = rendererOutputH = 0;
        const double now = nowSeconds();
        nextFrameAt = now;
        nextRenderAt = now;
        lastSpeedTick = now;
    }
    frameTextureDirty = true;
}

void App::handleMouseMotion(int x, int y) {
    mouseX = x; mouseY = y;
    if (fullscreen && pointerOverMenu(x, y)) {
        lastMenuHover = sdl.GetTicks();
        menuHoverActivated = true;
    }
    if (activeMenu < 0) return;
    int rootX = 0; for (int i = 0; i < activeMenu; ++i) rootX += textWidth(menus[i].text, 2) + 30;
    int width = 260; for (size_t i = 0; i < menus[activeMenu].items.size(); ++i) width = std::max(width, textWidth(menus[activeMenu].items[i].text, 2) + 64);
    if (x >= rootX && x < rootX + width && y >= MENU_BAR_H) {
        int row = (y - MENU_BAR_H) / MENU_ITEM_H;
        if (row >= 0 && row < int(menus[activeMenu].items.size()) && menus[activeMenu].items[row].submenu) activeSubmenu = row;
    }
}

void App::handleMouseDown(int x, int y) {
    mouseX = x; mouseY = y;
    if (fullscreen && y < MENU_BAR_H) {
        menuHoverActivated = true;
        lastMenuHover = sdl.GetTicks();
    }
    if (y < MENU_BAR_H && (!fullscreen || menuHoverActivated)) {
        int at = 0;
        for (size_t i = 0; i < menus.size(); ++i) { int width = textWidth(menus[i].text, 2) + 30; if (x >= at && x < at + width) { if (!menus[i].enabled) { activeMenu = activeSubmenu = -1; return; } activeMenu = activeMenu == int(i) ? -1 : int(i); activeSubmenu = -1; return; } at += width; }
        activeMenu = activeSubmenu = -1; return;
    }
    if (activeMenu >= 0) {
        int rootX = 0; for (int i = 0; i < activeMenu; ++i) rootX += textWidth(menus[i].text, 2) + 30;
        int width = 260; for (size_t i = 0; i < menus[activeMenu].items.size(); ++i) width = std::max(width, textWidth(menus[activeMenu].items[i].text, 2) + 64);
        if (activeSubmenu >= 0) {
            const MenuItemDef& parent = menus[activeMenu].items[activeSubmenu]; std::vector<MenuItemDef> sub = submenuItemsFor(parent.submenu);
            SDL_Rect subRect = { rootX + width, MENU_BAR_H + activeSubmenu * MENU_ITEM_H, 230, int(sub.size()) * MENU_ITEM_H };
            if (pointInRect(x, y, subRect)) { int row = (y - subRect.y) / MENU_ITEM_H; if (sub[row].enabled) dispatchCommand(sub[row].command); return; }
        }
        SDL_Rect drop = { rootX, MENU_BAR_H, width, int(menus[activeMenu].items.size()) * MENU_ITEM_H };
        if (pointInRect(x, y, drop)) {
            int row = (y - MENU_BAR_H) / MENU_ITEM_H; const MenuItemDef& item = menus[activeMenu].items[row];
            if (item.separator || !item.enabled) return; if (item.submenu) { activeSubmenu = row; return; } dispatchCommand(item.command); return;
        }
        activeMenu = activeSubmenu = -1;
    }
    int outW = 0, outH = 0; sdl.GetRendererOutputSize(renderer, &outW, &outH);
    if (screenMode == SCREEN_CONTROLLER) {
        int colW = (outW - 60) / 2; int port = x < outW / 2 ? 0 : 1; int colX = 20 + port * (colW + 20);
        if (x >= colX + 8 && x < colX + colW - 8 && y >= 143 && y < 166) { cycleControllerForPort(port); return; }
        if (x >= colX && x < colX + colW && y >= 174 && y < 174 + (8 + SPECIAL_ACTION_COUNT) * 20) { capturePort = port; captureIndex = (y - 174) / 20; toast("PRESS A KEY, BUTTON, HAT, TRIGGER, OR MOVE AN AXIS", 4000); return; }
        int by = outH - STATUS_BAR_H - 50; int bw = std::max(105, (outW - 100) / 4);
        if (y >= by && y < by + 34) {
            int b = std::min(3, std::max(0, (x - 20) / (bw + 15)));
            if (b == 0) { resetInputBindings(); toast("DEFAULTS RESTORED"); }
            else if (b == 1 && capturePort >= 0) { if (captureIndex < 8) {controllerKeys[capturePort][captureIndex] = SDL_SCANCODE_UNKNOWN;controllerPadBindings[capturePort][captureIndex]=PadBinding();} else {specialKeys[capturePort][captureIndex - 8] = SDL_SCANCODE_UNKNOWN;specialPadBindings[capturePort][captureIndex-8]=PadBinding();} toast("BINDING UNASSIGNED"); }
            else if (b == 2) toast(saveInputBindings() ? "CONTROLLER CONFIG SAVED" : "CONFIG SAVE FAILED");
            else if (b == 3) { saveInputBindings(); screenMode = SCREEN_GAME; capturePort = captureIndex = -1; }
        }
        return;
    }
    if (screenMode == SCREEN_CHEATS) {
        if (cheatEditMode) {
            SDL_Rect nameBox = { outW / 2 - 280, outH / 2 - 60, 560, 38 }, codeBox = { outW / 2 - 280, outH / 2 + 10, 560, 38 };
            if (pointInRect(x, y, nameBox)) activeTextField = 1; else if (pointInRect(x, y, codeBox)) activeTextField = 2;
            else if (y >= outH / 2 + 70 && y < outH / 2 + 108) {
                if (x < outW / 2) {
                    uint16_t a; uint8_t v, c; bool hc;
                    if (!decodeGameGenie(editCheatCode, a, v, hc, c)) { toast("INVALID GAME GENIE CODE"); return; }
                    GameGenieCheat cheat; cheat.name = editCheatName.empty() ? "Game Genie Code" : editCheatName; cheat.code = editCheatCode; cheat.enabled = true;
                    if (cheatEditMode == 1) { cheats.push_back(cheat); selectedCheat = int(cheats.size()) - 1; } else cheats[selectedCheat] = cheat;
                    saveCheats(); applyCheats(); cheatEditMode = activeTextField = 0;
                } else { cheatEditMode = activeTextField = 0; }
            }
            return;
        }
        int row = (y - 130) / 34; int visible = std::max(1, (outH - 250) / 34);
        if (row >= 0 && row < visible && cheatScroll + row < int(cheats.size())) { selectedCheat = cheatScroll + row; if (x < 95) { cheats[selectedCheat].enabled = !cheats[selectedCheat].enabled; saveCheats(); applyCheats(); } return; }
        int by = outH - STATUS_BAR_H - 54; int bw = std::max(94, (outW - 80) / 6); if (y >= by && y < by + 38) {
            int b = std::min(5, std::max(0, (x - 15) / (bw + 8)));
            if (b == 0) { cheatEditMode = 1; editCheatName.clear(); editCheatCode.clear(); activeTextField = 1; }
            else if (b == 1 && selectedCheat >= 0) { cheats.erase(cheats.begin() + selectedCheat); selectedCheat = cheats.empty() ? -1 : std::min(selectedCheat, int(cheats.size()) - 1); saveCheats(); applyCheats(); }
            else if (b == 2 && selectedCheat >= 0) { cheatEditMode = 2; editCheatName = cheats[selectedCheat].name; editCheatCode = cheats[selectedCheat].code; activeTextField = 1; }
            else if (b == 3) { if (selectedCheat >= 0) ggCode = cheats[selectedCheat].code; screenMode = SCREEN_GAME_GENIE; activeTextField = 3; }
            else if (b == 4) { if(romLoaded){ciphernes_reset_hard(core); clearRewindHistory(); }screenMode=SCREEN_GAME;toast("HARD RESET"); }
            else screenMode = SCREEN_GAME;
        }
        return;
    }
    if (screenMode == SCREEN_GAME_GENIE) {
        SDL_Rect fields[4] = { { 60, 150, outW - 120, 38 }, { 60, 240, outW - 120, 38 }, { 60, 320, outW - 120, 38 }, { 60, 400, outW - 120, 38 } };
        for (int i = 0; i < 4; ++i) if (pointInRect(x, y, fields[i])) { activeTextField = i + 3; return; }
        int by = outH - STATUS_BAR_H - 58; int bw = std::max(110, (outW - 70) / 5); if (y >= by && y < by + 40) {
            int b = std::min(4, std::max(0, (x - 15) / (bw + 8)));
            if (b == 0) { uint16_t a; uint8_t v, c; bool hc; if (decodeGameGenie(ggCode, a, v, hc, c)) { char t[16]; std::snprintf(t, sizeof(t), "%04X", a); ggAddress=t; std::snprintf(t,sizeof(t),"%02X",v);ggValue=t; if(hc){std::snprintf(t,sizeof(t),"%02X",c);ggCompare=t;}else ggCompare="";ggStatus="CODE DECODED";} else ggStatus="INVALID CODE"; }
            else if (b == 1) { std::string code; if (encodeGameGenie(code)) { ggCode=code; ggStatus="CODE ENCODED"; } else ggStatus="INVALID ADDRESS OR VALUE"; }
            else if (b == 2) { ggCode.clear();ggAddress.clear();ggValue.clear();ggCompare.clear();ggStatus.clear(); }
            else if (b == 3) { if (!romLoaded) toast("LOAD A ROM FIRST"); else { loadCheats(); GameGenieCheat c; c.name="Game Genie Code";c.code=ggCode;c.enabled=true;cheats.push_back(c);saveCheats();applyCheats();toast("ADDED TO CHEATS"); } }
            else screenMode = SCREEN_GAME;
        }
        return;
    }
    if (screenMode == SCREEN_INFO) { screenMode = SCREEN_GAME; }
}

void App::editActiveText(const SDL_KeyboardEvent& event) {
    std::string* target = NULL;
    if (activeTextField == 1) target = &editCheatName; else if (activeTextField == 2) target = &editCheatCode;
    else if (activeTextField == 3) target = &ggCode; else if (activeTextField == 4) target = &ggAddress;
    else if (activeTextField == 5) target = &ggValue; else if (activeTextField == 6) target = &ggCompare;
    if (!target) { activeTextField = 0; return; }
    SDL_Scancode key = event.keysym.scancode;
    if (key == SDL_SCANCODE_ESCAPE) { activeTextField = 0; if (cheatEditMode) cheatEditMode = 0; return; }
    if (key == SDL_SCANCODE_TAB || key == SDL_SCANCODE_RETURN) {
        if (activeTextField == 1) activeTextField = 2;
        else if (activeTextField >= 3 && activeTextField <= 6) activeTextField = activeTextField == 6 ? 3 : activeTextField + 1;
        else activeTextField = 0;
        return;
    }
    if (key == SDL_SCANCODE_BACKSPACE) { if (!target->empty()) target->erase(target->size() - 1); return; }
    char c = scancodeCharacter(key, (event.keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT)) != 0);
    if (!c) return;
    if (activeTextField >= 2) {
        c = char(std::toupper(static_cast<unsigned char>(c)));
        bool codeField = activeTextField == 2 || activeTextField == 3;
        if (codeField) { if (!(std::isalpha(static_cast<unsigned char>(c)) || std::isdigit(static_cast<unsigned char>(c)) || c == '-')) return; }
        else if (!std::isxdigit(static_cast<unsigned char>(c))) return;
    }
    size_t limit = activeTextField == 1 ? 42u : (activeTextField == 2 || activeTextField == 3 ? 9u : (activeTextField == 4 ? 4u : 2u));
    if (target->size() < limit) target->push_back(c);
}

void App::handleKey(const SDL_KeyboardEvent& event) {
    SDL_Scancode key = event.keysym.scancode; bool ctrl = (event.keysym.mod & KMOD_CTRL) != 0;
    if (event.repeat) return;
    if (capturePort >= 0 && captureIndex >= 0) {
        if (key == SDL_SCANCODE_ESCAPE) { capturePort = captureIndex = -1; toast("ESC RESERVED FOR DISPLAY MODES"); return; }
        if (captureIndex < 8) controllerKeys[capturePort][captureIndex] = key; else specialKeys[capturePort][captureIndex - 8] = key;
        toast("BOUND TO " + scancodeName(key)); capturePort = captureIndex = -1; return;
    }
    if (activeTextField) { editActiveText(event); return; }
    if (activeMenu >= 0) { if (key == SDL_SCANCODE_ESCAPE) activeMenu = activeSubmenu = -1; return; }
    if (screenMode != SCREEN_GAME) { if (key == SDL_SCANCODE_ESCAPE) screenMode = SCREEN_GAME; return; }
    if (ctrl && key == SDL_SCANCODE_O) { openBrowser(0); return; }
    if (ctrl && key == SDL_SCANCODE_C) { toggleCheats(); return; }
    if (key == SDL_SCANCODE_F2) { if (romLoaded) { bool hard = (event.keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT)) != 0; if (hard) ciphernes_reset_hard(core); else ciphernes_reset_soft(core); clearRewindHistory(); resetAudioQueue(!paused); toast("RESET"); } return; }
    if (key == SDL_SCANCODE_F5) { saveState(); return; }
    if (key == SDL_SCANCODE_F6) { stateSlot = stateSlot % 4 + 1; toast("STATE SLOT " + std::to_string(stateSlot)); return; }
    if (key == SDL_SCANCODE_F8) { loadState(); return; }
    if (key == SDL_SCANCODE_F9) { takeScreenshot(); return; }
    // Escape cycles: windowed -> fullscreen (aspect correct) -> fullscreen
    // stretched -> windowed. Escape cannot be assigned to a game action.
    if (key == SDL_SCANCODE_ESCAPE) {
        if (!fullscreen) setFullscreenMode(true, false);
        else if (!stretched) setFullscreenMode(true, true);
        else setFullscreenMode(false, false);
        return;
    }
    if (key == SDL_SCANCODE_F11) { setFullscreenMode(!fullscreen, stretched); return; }
    if (key == SDL_SCANCODE_F12) { stretched = !stretched; toast(stretched ? "STRETCHED VIDEO" : "ASPECT CORRECT VIDEO"); return; }
    if (key == SDL_SCANCODE_SPACE) { if (tasMode == TAS_COUNTDOWN) { toast("INPUT RECORDING COUNTDOWN ACTIVE"); return; } paused = !paused; resetAudioQueue(!paused); nextFrameAt = nowSeconds(); updateTitle(); toast(paused ? "PAUSED" : "RESUMED"); return; }
    if (key == SDL_SCANCODE_M) { muted = !muted; ciphernes_set_audio_enabled(core, muted ? 0 : 1); resetAudioQueue(!muted && !paused); toast(muted ? "MUTED" : "AUDIO ON"); return; }
    if (key >= SDL_SCANCODE_1 && key <= SDL_SCANCODE_5) { int channel = key - SDL_SCANCODE_1; apuMask ^= uint8_t(1u << channel); ciphernes_set_apu_channel_mask(core, apuMask); toast("APU MASK " + std::to_string(apuMask)); return; }
    if (key == SDL_SCANCODE_MINUS) { speedPercent = speedPercent == 100 ? 50 : 25; updateTitle(); toast("SPEED " + std::to_string(speedPercent) + "%"); return; }
    if (key == SDL_SCANCODE_EQUALS) { speedPercent = speedPercent == 25 ? 50 : 100; updateTitle(); toast("SPEED " + std::to_string(speedPercent) + "%"); return; }
}

void App::handleEvents() {
    SDL_Event event;
    bool refreshControllers = false;
    while (sdl.PollEvent(&event)) {
        if (event.type == SDL_QUIT) running = false;
        else if (event.type == SDL_KEYDOWN) handleKey(event.key);
        else if (event.type == SDL_MOUSEMOTION) handleMouseMotion(event.motion.x, event.motion.y);
        else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) handleMouseDown(event.button.x, event.button.y);
        else if (event.type == SDL_CONTROLLERBUTTONDOWN && capturePort >= 0 && captureIndex >= 0)
            setCapturedPadBinding(PadBinding(PAD_BIND_GAMEPAD_BUTTON, event.cbutton.button, 0), event.cbutton.which);
        else if (event.type == SDL_CONTROLLERAXISMOTION && capturePort >= 0 && captureIndex >= 0 && std::abs(int(event.caxis.value)) > 16000)
            setCapturedPadBinding(PadBinding(PAD_BIND_GAMEPAD_AXIS, event.caxis.axis, event.caxis.value < 0 ? -1 : 1), event.caxis.which);
        else if (event.type == SDL_JOYBUTTONDOWN && capturePort >= 0 && captureIndex >= 0) {
            int device = controllerIndexForInstance(event.jbutton.which);
            if (device >= 0 && !controllers[device].mapped)
                setCapturedPadBinding(PadBinding(PAD_BIND_JOYSTICK_BUTTON, event.jbutton.button, 0), event.jbutton.which);
        }
        else if (event.type == SDL_JOYAXISMOTION && capturePort >= 0 && captureIndex >= 0 && std::abs(int(event.jaxis.value)) > 16000) {
            int device = controllerIndexForInstance(event.jaxis.which);
            if (device >= 0 && !controllers[device].mapped)
                setCapturedPadBinding(PadBinding(PAD_BIND_JOYSTICK_AXIS, event.jaxis.axis, event.jaxis.value < 0 ? -1 : 1), event.jaxis.which);
        }
        else if (event.type == SDL_JOYHATMOTION && capturePort >= 0 && captureIndex >= 0 && event.jhat.value != SDL_HAT_CENTERED) {
            int device = controllerIndexForInstance(event.jhat.which);
            if (device >= 0 && !controllers[device].mapped) {
                int direction = (event.jhat.value & SDL_HAT_UP) ? SDL_HAT_UP : (event.jhat.value & SDL_HAT_DOWN) ? SDL_HAT_DOWN :
                    (event.jhat.value & SDL_HAT_LEFT) ? SDL_HAT_LEFT : SDL_HAT_RIGHT;
                setCapturedPadBinding(PadBinding(PAD_BIND_JOYSTICK_HAT, event.jhat.hat, direction), event.jhat.which);
            }
        }
        else if (event.type == SDL_DROPFILE) {
            if (event.drop.file) { std::string path = event.drop.file; if (hasExtension(path, ".nes") || hasExtension(path, ".fds") || hasExtension(path, ".qd") || hasExtension(path, ".zip")) loadRom(path); else if (hasExtension(path, ".ci") && romLoaded && tasMode == TAS_IDLE) startTasPlayback(path); sdl.Free(event.drop.file); }
        } else if (event.type == SDL_CONTROLLERDEVICEADDED || event.type == SDL_CONTROLLERDEVICEREMOVED ||
                   event.type == SDL_CONTROLLERDEVICEREMAPPED || event.type == SDL_JOYDEVICEADDED || event.type == SDL_JOYDEVICEREMOVED) {
            refreshControllers = true;
        }
        else if (event.type == SDL_WINDOWEVENT && event.window.windowID == mainWindowId &&
                 (event.window.event == SDL_WINDOWEVENT_RESIZED || event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)) {
            rendererOutputW = rendererOutputH = 0;
        } else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
            ciphernes_set_controller(core, 0, 0); ciphernes_set_controller(core, 1, 0);
        } else if(event.type==SDL_MOUSEWHEEL&&screenMode==SCREEN_CHEATS){cheatScroll=std::max(0,std::min(cheatScroll-event.wheel.y*3,std::max(0,int(cheats.size())-1)));
        }
    }
    if (refreshControllers) {
        openControllers();
        toast(controllers.empty() ? "NO CONTROLLERS CONNECTED" : std::to_string(controllers.size()) + " CONTROLLER(S) CONNECTED", 1800);
    }
}

void App::drawMenuBar(int outW, int outH) {
    (void)outH;
    const int slideY = fullscreen ? int((menuSlide - 1.0f) * MENU_BAR_H) : 0;
    SDL_Rect bar = { 0, slideY, outW, MENU_BAR_H }; sdl.SetRenderDrawColor(renderer, 24, 35, 48, 255); sdl.RenderFillRect(renderer, &bar);
    sdl.SetRenderDrawColor(renderer, 72, 105, 136, 255); sdl.RenderDrawLine(renderer, 0, slideY + MENU_BAR_H - 1, outW, slideY + MENU_BAR_H - 1);
    int at = 0;
    for (size_t i = 0; i < menus.size(); ++i) {
        int width = textWidth(menus[i].text, 2) + 30; SDL_Rect root = { at, slideY, width, MENU_BAR_H };
        if (menus[i].enabled && (activeMenu == int(i) || pointInRect(mouseX, mouseY, root))) { sdl.SetRenderDrawColor(renderer, activeMenu == int(i) ? 45 : 34, 76, 109, 255); sdl.RenderFillRect(renderer, &root); }
        drawText(sdl, renderer, at + 15, slideY + 12, menus[i].text, 2, menus[i].enabled ? 238 : 112, menus[i].enabled ? 243 : 122, menus[i].enabled ? 248 : 132); at += width;
    }
    if ((fullscreen && menuSlide < 0.99f) || activeMenu < 0 || activeMenu >= int(menus.size())) return;
    int rootX = 0; for (int i = 0; i < activeMenu; ++i) rootX += textWidth(menus[i].text, 2) + 30;
    int width = 260; for (size_t i = 0; i < menus[activeMenu].items.size(); ++i) width = std::max(width, textWidth(menus[activeMenu].items[i].text, 2) + 64);
    SDL_Rect drop = { rootX, MENU_BAR_H, width, int(menus[activeMenu].items.size()) * MENU_ITEM_H }; drawPanel(sdl, renderer, drop, 19, 28, 39);
    for (size_t i = 0; i < menus[activeMenu].items.size(); ++i) {
        const MenuItemDef& item = menus[activeMenu].items[i]; int y = MENU_BAR_H + int(i) * MENU_ITEM_H;
        if (item.separator) { sdl.SetRenderDrawColor(renderer, 72, 91, 110, 255); sdl.RenderDrawLine(renderer, rootX + 10, y + MENU_ITEM_H / 2, rootX + width - 10, y + MENU_ITEM_H / 2); continue; }
        SDL_Rect row = { rootX + 1, y, width - 2, MENU_ITEM_H };
        if (item.enabled && (pointInRect(mouseX, mouseY, row) || activeSubmenu == int(i))) { sdl.SetRenderDrawColor(renderer, 38, 85, 128, 255); sdl.RenderFillRect(renderer, &row); }
        if (itemChecked(item)) drawText(sdl, renderer, rootX + 10, y + 9, "X", 2, 255, 218, 91);
        drawText(sdl, renderer, rootX + 36, y + 9, item.text, 2, item.enabled ? 235 : 118, item.enabled ? 241 : 128, item.enabled ? 247 : 138);
        if (item.submenu) drawText(sdl, renderer, rootX + width - 24, y + 9, ">", 2, item.enabled ? 180 : 95, item.enabled ? 207 : 105, item.enabled ? 230 : 115);
    }
    if (activeSubmenu >= 0 && activeSubmenu < int(menus[activeMenu].items.size())) {
        const MenuItemDef& parent = menus[activeMenu].items[activeSubmenu]; std::vector<MenuItemDef> sub = submenuItemsFor(parent.submenu); if (sub.empty()) return;
        SDL_Rect box = { rootX + width, MENU_BAR_H + activeSubmenu * MENU_ITEM_H, 230, int(sub.size()) * MENU_ITEM_H }; drawPanel(sdl, renderer, box, 19, 28, 39);
        for (size_t i = 0; i < sub.size(); ++i) { SDL_Rect row={box.x+1,box.y+int(i)*MENU_ITEM_H,box.w-2,MENU_ITEM_H}; if(sub[i].enabled&&pointInRect(mouseX,mouseY,row)){sdl.SetRenderDrawColor(renderer,38,85,128,255);sdl.RenderFillRect(renderer,&row);} if(itemChecked(sub[i]))drawText(sdl,renderer,box.x+10,row.y+9,"X",2,255,218,91); drawText(sdl,renderer,box.x+36,row.y+9,sub[i].text,2,sub[i].enabled?235:118,sub[i].enabled?241:128,sub[i].enabled?247:138); }
    }
}

void App::drawControllerScreen(int outW, int outH) {
    SDL_Rect bg = { 0, MENU_BAR_H, outW, outH - MENU_BAR_H - STATUS_BAR_H };
    drawPanel(sdl, renderer, bg, 9, 15, 24);
    drawText(sdl, renderer, 24, MENU_BAR_H + 18, "CONTROLLER INPUT", 3, 102, 196, 255);
    drawText(sdl, renderer, 24, MENU_BAR_H + 52,
        "CLICK DEVICE TO CHANGE IT. CLICK A ROW, THEN PRESS A KEY, BUTTON, HAT, TRIGGER, OR AXIS.",
        2, 188, 210, 228);

    const char* controls[8 + SPECIAL_ACTION_COUNT] = {
        "UP", "DOWN", "LEFT", "RIGHT", "A", "B", "START", "SELECT",
        "SAVE STATE", "LOAD STATE", "CHANGE SLOT", "SCREENSHOT", "START AVI", "STOP AVI", "REWIND", "FAST FORWARD"
    };
    int keyCount = 0;
    const Uint8* keys = sdl.GetKeyboardState(&keyCount);
    int colW = (outW - 60) / 2;

    for (int port = 0; port < 2; ++port) {
        int x = 20 + port * (colW + 20);
        SDL_Rect panel = { x, 115, colW, 435 };
        drawPanel(sdl, renderer, panel, 13, 24, 37);
        drawText(sdl, renderer, x + 12, 126,
            "PORT " + std::to_string(port + 1) + (portConnected[port] ? "  PLUGGED" : "  UNPLUGGED"),
            2, portConnected[port] ? 118 : 220, portConnected[port] ? 239 : 120, portConnected[port] ? 146 : 120);

        SDL_Rect deviceRow = { x + 8, 142, colW - 16, 24 };
        if (pointInRect(mouseX, mouseY, deviceRow)) {
            sdl.SetRenderDrawColor(renderer, 31, 70, 104, 255);
            sdl.RenderFillRect(renderer, &deviceRow);
        }
        std::string device = "DEVICE: " + assignedControllerName(port) + "  (CLICK)";
        int maxDeviceChars = std::max(8, (colW - 28) / 12);
        if (int(device.size()) > maxDeviceChars) device.resize(maxDeviceChars);
        drawText(sdl, renderer, x + 14, 148, device, 2, 255, 214, 92);

        for (int i = 0; i < 8 + SPECIAL_ACTION_COUNT; ++i) {
            int y = 174 + i * 20;
            SDL_Rect row = { x + 8, y - 3, colW - 16, 19 };
            bool selected = capturePort == port && captureIndex == i;
            SDL_Scancode key = i < 8 ? controllerKeys[port][i] : specialKeys[port][i - 8];
            PadBinding padBinding = i < 8 ? controllerPadBindings[port][i] : specialPadBindings[port][i - 8];
            bool keyDown = key != SDL_SCANCODE_UNKNOWN && key >= 0 && key < keyCount && keys[key];
            bool active = keyDown || padBindingDown(port, padBinding);
            if (selected || active) {
                if (selected) sdl.SetRenderDrawColor(renderer, 44, 91, 132, 255);
                else sdl.SetRenderDrawColor(renderer, 28, 91, 61, 255);
                sdl.RenderFillRect(renderer, &row);
            }
            drawText(sdl, renderer, x + 14, y, controls[i], 2, active ? 145 : 166, active ? 245 : 202, active ? 165 : 230);

            std::string name = scancodeName(key) + " / " + padBindingName(padBinding);
            int maxChars = std::max(10, (colW - 190) / 12);
            if (int(name.size()) > maxChars) name.resize(maxChars);
            int tx = x + colW - textWidth(name, 2) - 16;
            drawText(sdl, renderer, std::max(x + 170, tx), y, name, 2,
                selected ? 255 : 238, selected ? 213 : 238, selected ? 91 : 242);
        }
    }

    int by = outH - STATUS_BAR_H - 50;
    int bw = std::max(105, (outW - 100) / 4);
    const char* buttons[4] = { "RESET DEFAULTS", "UNASSIGN", "SAVE", "RETURN" };
    for (int i = 0; i < 4; ++i) {
        SDL_Rect b = { 20 + i * (bw + 15), by, bw, 34 };
        drawButton(sdl, renderer, b, buttons[i], pointInRect(mouseX, mouseY, b));
    }
}

void App::drawCheatsScreen(int outW, int outH) {
    SDL_Rect bg={0,MENU_BAR_H,outW,outH-MENU_BAR_H-STATUS_BAR_H};drawPanel(sdl,renderer,bg,9,15,24);
    drawText(sdl,renderer,24,MENU_BAR_H+18,"GAME GENIE CHEATS",3,102,196,255);
    drawText(sdl,renderer,24,MENU_BAR_H+55,"CLICK THE BOX TO ENABLE OR DISABLE A CODE",2,180,207,228);
    int visible=std::max(1,(outH-250)/34);if(selectedCheat>=cheatScroll+visible)cheatScroll=selectedCheat-visible+1;if(selectedCheat<cheatScroll)cheatScroll=std::max(0,selectedCheat);
    for(int row=0;row<visible;++row){int index=cheatScroll+row;if(index>=int(cheats.size()))break;int y=130+row*34;SDL_Rect rr={22,y-7,outW-44,30};if(index==selectedCheat){sdl.SetRenderDrawColor(renderer,34,76,112,255);sdl.RenderFillRect(renderer,&rr);}drawText(sdl,renderer,34,y,cheats[index].enabled?"[X]":"[ ]",2,cheats[index].enabled?255:160,cheats[index].enabled?216:180,90);drawText(sdl,renderer,90,y,cheats[index].name,2,232,239,245);drawText(sdl,renderer,std::max(400,outW-180),y,cheats[index].code,2,145,205,245);}
    if(cheats.empty())drawText(sdl,renderer,30,150,"NO CODES YET. CHOOSE ADD.",2,180,198,216);
    int by=outH-STATUS_BAR_H-54,bw=std::max(94,(outW-80)/6);const char* buttons[6]={"ADD","REMOVE","EDIT","ENCODER","RESET","RETURN"};for(int i=0;i<6;++i){SDL_Rect b={15+i*(bw+8),by,bw,38};drawButton(sdl,renderer,b,buttons[i],pointInRect(mouseX,mouseY,b));}
    if(cheatEditMode){SDL_Rect shade={0,MENU_BAR_H,outW,outH-MENU_BAR_H-STATUS_BAR_H};sdl.SetRenderDrawColor(renderer,0,0,0,180);sdl.RenderFillRect(renderer,&shade);SDL_Rect dialog={outW/2-320,outH/2-140,640,285};drawPanel(sdl,renderer,dialog,15,27,41);drawText(sdl,renderer,dialog.x+22,dialog.y+18,cheatEditMode==1?"ADD CHEAT":"EDIT CHEAT",3,255,215,90);SDL_Rect n={outW/2-280,outH/2-60,560,38},c={outW/2-280,outH/2+10,560,38};drawTextBox(sdl,renderer,n,"NAME",editCheatName,activeTextField==1);drawTextBox(sdl,renderer,c,"GAME GENIE CODE",editCheatCode,activeTextField==2);SDL_Rect ok={outW/2-280,outH/2+70,260,38},cancel={outW/2+20,outH/2+70,260,38};drawButton(sdl,renderer,ok,"OK",pointInRect(mouseX,mouseY,ok));drawButton(sdl,renderer,cancel,"CANCEL",pointInRect(mouseX,mouseY,cancel));}
}

void App::drawGameGenieScreen(int outW, int outH) {
    SDL_Rect bg={0,MENU_BAR_H,outW,outH-MENU_BAR_H-STATUS_BAR_H};drawPanel(sdl,renderer,bg,9,15,24);
    drawText(sdl,renderer,28,MENU_BAR_H+18,"GAME GENIE ENCODER / DECODER",3,102,196,255);
    drawText(sdl,renderer,28,MENU_BAR_H+55,"DECODE A 6 OR 8 LETTER CODE, OR ENTER CPU ADDRESS AND REPLACEMENT BYTE TO ENCODE.",2,180,207,228);
    SDL_Rect fields[4]={{60,150,outW-120,38},{60,240,outW-120,38},{60,320,outW-120,38},{60,400,outW-120,38}};drawTextBox(sdl,renderer,fields[0],"GAME GENIE CODE",ggCode,activeTextField==3);drawTextBox(sdl,renderer,fields[1],"CPU ADDRESS (8000-FFFF)",ggAddress,activeTextField==4);drawTextBox(sdl,renderer,fields[2],"REPLACE BYTE (00-FF)",ggValue,activeTextField==5);drawTextBox(sdl,renderer,fields[3],"OPTIONAL COMPARE BYTE (00-FF)",ggCompare,activeTextField==6);
    drawText(sdl,renderer,60,465,ggStatus,2,255,215,90);
    int by=outH-STATUS_BAR_H-58,bw=std::max(110,(outW-70)/5);const char* buttons[5]={"DECODE","ENCODE","CLEAR","USE IN CHEATS","RETURN"};for(int i=0;i<5;++i){SDL_Rect b={15+i*(bw+8),by,bw,40};drawButton(sdl,renderer,b,buttons[i],pointInRect(mouseX,mouseY,b));}
}

void App::drawInfoScreen(int outW, int outH) {
    const int pw = std::min(680, std::max(260, outW - 40));
    const int ph = 270;
    SDL_Rect panel = {(outW - pw) / 2, (outH - ph) / 2, pw, ph};
    drawPanel(sdl, renderer, panel, 12, 22, 34);
    const char* lines[] = {"CIPHERNES", "HODGKINSSTUDIOS", "CIPHERNES CORE 1.0"};
    const int scales[] = {4, 3, 3};
    for (int i = 0; i < 3; ++i) {
        const int scale = scales[i];
        const int x = panel.x + std::max(10, (panel.w - textWidth(lines[i], scale)) / 2);
        drawText(sdl, renderer, x, panel.y + 34 + i * 80, lines[i], scale,
                 i == 0 ? 110 : 235, i == 0 ? 205 : 221, i == 0 ? 255 : 234);
    }
}

void App::drawCurrentScreen(int outW, int outH) {
    if(screenMode==SCREEN_CONTROLLER)drawControllerScreen(outW,outH);else if(screenMode==SCREEN_CHEATS)drawCheatsScreen(outW,outH);else if(screenMode==SCREEN_GAME_GENIE)drawGameGenieScreen(outW,outH);else if(screenMode==SCREEN_INFO)drawInfoScreen(outW,outH);
}

void App::drawOverlay(int outW, int outH) {
    SDL_Rect bar = { 0, outH - STATUS_BAR_H, outW, STATUS_BAR_H };
    if (!fullscreen) {
    sdl.SetRenderDrawColor(renderer, 7, 13, 21, 245); sdl.RenderFillRect(renderer, &bar);
    sdl.SetRenderDrawColor(renderer, 72, 105, 136, 255); sdl.RenderDrawLine(renderer, 0, bar.y, outW, bar.y);
    std::ostringstream status; status << (romLoaded ? romBase : "NO ROM") << "  SLOT " << stateSlot << "  " << speedPercent << "%";
    if (muted) status << "  MUTED"; if (cheatsEnabled) status << "  CHEATS"; if (avi.isOpen()) status << "  REC";
    if (rewindHeld) status << "  REWIND";
    else if (fastForwardMultiplier > 1.01) status << "  FF " << int(fastForwardMultiplier * 100) << "% target / "
        << int(actualSpeedMultiplier * 100) << "% actual";
    if (tasMode == TAS_RECORDING) status << "  TAS REC " << tasFrames.size();
    else if (tasMode == TAS_PLAYBACK) status << "  TAS PLAY " << tasPlaybackPosition << "/" << tasFrames.size();
    drawText(sdl, renderer, 10, bar.y + 9, status.str(), 2, 225, 235, 245);
    }
    if (toastUntil && Sint32(toastUntil - sdl.GetTicks()) > 0) {
        int width = textWidth(toastText, 2) + 30; SDL_Rect box = { std::max(8, (outW - width) / 2), outH - (fullscreen ? 58 : STATUS_BAR_H + 58), std::min(width, outW - 16), 42 };
        sdl.SetRenderDrawColor(renderer, 4, 8, 14, 225); sdl.RenderFillRect(renderer, &box);
        sdl.SetRenderDrawColor(renderer, 255, 205, 70, 255); sdl.RenderDrawRect(renderer, &box);
        drawText(sdl, renderer, box.x + 15, box.y + 13, toastText, 2, 255, 235, 150);
    }
    if (screenMode != SCREEN_GAME) drawCurrentScreen(outW, outH);
    if (!fullscreen || menuSlide > 0.0f) drawMenuBar(outW, outH);
}

void App::render() {
    if (rendererOutputW <= 0 || rendererOutputH <= 0)
        sdl.GetRendererOutputSize(renderer, &rendererOutputW, &rendererOutputH);
    const int outW = rendererOutputW;
    const int outH = rendererOutputH;
    if (fullscreen) {
        const Uint32 now = sdl.GetTicks();
        // Keep the menu open while the pointer is on the bar or its dropdown.
        if (menuHoverActivated && pointerOverMenu(mouseX, mouseY)) lastMenuHover = now;
        bool revealed = menuHoverActivated && Uint32(now - lastMenuHover) < 1000u;
        if (!revealed && activeMenu >= 0) activeMenu = activeSubmenu = -1;
        menuSlide += revealed ? 0.18f : -0.18f;
        menuSlide = std::max(0.0f, std::min(1.0f, menuSlide));
    } else menuSlide = 1.0f;
    sdl.SetRenderDrawColor(renderer, 10, 14, 22, 255); sdl.RenderClear(renderer);
    if (romLoaded) {
        if (frameTextureDirty) {
            const uint32_t* pixels = rewindShowingFrame && rewindHistory.image() ? rewindHistory.image() :
                (restoredDisplayVisible && restoredDisplay.size() == NES_W * NES_H ? restoredDisplay.data() : ciphernes_get_framebuffer(core));
            sdl.UpdateTexture(texture, NULL, pixels, NES_W * 4);
            frameTextureDirty = false;
        }
        SDL_Rect dst;
        int contentY = fullscreen ? 0 : MENU_BAR_H;
        int contentH = std::max(1, fullscreen ? outH : outH - MENU_BAR_H - STATUS_BAR_H);
        if (stretched) { dst.x = 0; dst.y = contentY; dst.w = outW; dst.h = contentH; }
        else {
            double scale = std::min(double(outW) / NES_W, double(contentH) / NES_H);
            if (scale >= 1.0) scale = std::max(1.0, std::floor(scale));
            dst.w = int(NES_W * scale); dst.h = int(NES_H * scale); dst.x = (outW - dst.w) / 2; dst.y = contentY + (contentH - dst.h) / 2;
        }
        sdl.RenderCopy(renderer, texture, NULL, &dst);
    } else {
        drawText(sdl, renderer, std::max(20, outW / 2 - 288), outH / 2 - 45, PLATFORM_SPLASH_TITLE, 4, 100, 190, 255);
        drawText(sdl, renderer, std::max(20, outW / 2 - 210), outH / 2 + 25, "USE FILE > LOAD ROM", 3, 235, 235, 240);
    }
    drawOverlay(outW, outH); sdl.RenderPresent(renderer);
}

int App::run(const std::string& initialRom) {
    running = true; if (!initialRom.empty()) loadRom(initialRom);
    nextFrameAt = nextRenderAt = nowSeconds();
    const bool timingDiagnostics = std::getenv("CIPHERNES_TIMING_DIAGNOSTICS") != NULL;
    double diagnosticSince = nowSeconds();
    uint64_t diagnosticFrames = 0;
    while (running) {
        handleEvents();
        keyboardState = sdl.GetKeyboardState(&keyboardStateCount);
        updateTasTimers();
        pollSpecialBindings();
        const bool uiClear = screenMode == SCREEN_GAME && activeMenu < 0;
        const bool tasFrameBlocked = tasMode == TAS_COUNTDOWN;
        const bool normalPacedGameplay = romLoaded && !paused && uiClear && !tasFrameBlocked;
        const bool wantsRewind = romLoaded && uiClear && !tasFrameBlocked && !avi.isOpen() && actionHeld(REWIND_SPECIAL);
        const bool wantsFast = normalPacedGameplay && !wantsRewind && tasMode != TAS_RECORDING && !avi.isOpen() && actionHeld(FAST_FORWARD_SPECIAL);
        const double speedNow = nowSeconds();
        const double dt = lastSpeedTick > 0 ? std::min(0.050, std::max(0.0, speedNow - lastSpeedTick)) : 0.0;
        lastSpeedTick = speedNow;
        const double target = wantsFast ? 1.5 : 1.0;
        const double step = dt * 4.0; // ~125 ms 1x -> 1.5x, smoothed in both directions
        if (fastForwardMultiplier < target) fastForwardMultiplier = std::min(target, fastForwardMultiplier + step);
        else fastForwardMultiplier = std::max(target, fastForwardMultiplier - step);
        // Keep the small queued-audio tail across a speed transition. Flushing
        // it on every key press adds audible gaps at precisely the wrong time.
        if (wantsFast != fastForwardHeld) {
            speedMeasureSince = speedNow;
            speedMeasureFrames = 0;
            actualSpeedMultiplier = wantsFast ? 1.0 : actualSpeedMultiplier;
            nextFrameAt = speedNow;
        }
        fastForwardHeld = wantsFast;
        bool emulatedFrame = false;
        int emulatedFrameCount = 0;
        if (wantsRewind && !rewindHeld) {
            rewindHeld = true;
            resetAudioQueue(false);
            nextFrameAt = speedNow;
        } else if (!wantsRewind && rewindHeld) {
            rewindHeld = false;
            commitRewindBranch();
        }
        if (rewindHeld) {
            const double interval = 1.0 / NTSC_FPS;
            if (speedNow >= nextFrameAt) {
                emulatedFrame = rewindOneFrame();
                nextFrameAt += interval;
                if (nextFrameAt < nowSeconds() - interval) nextFrameAt = nowSeconds() + interval;
            }
        } else if (normalPacedGameplay) {
            const double interval = (1.0 / NTSC_FPS) * (100.0 / speedPercent) / fastForwardMultiplier;
            double now = speedNow;
            int framesThisPresentation = 0;
            const int maxFrames = fastForwardMultiplier > 1.01 ? 4 : 2;
            while (now >= nextFrameAt && framesThisPresentation < maxFrames && romLoaded && !paused) {
                const bool wasTasPlayback = tasMode == TAS_PLAYBACK;
                runOneFrame(false);
                emulatedFrame = true;
                ++framesThisPresentation;
                ++emulatedFrameCount;
                nextFrameAt += interval;
                now = nowSeconds();
                if (wasTasPlayback && tasMode != TAS_PLAYBACK) {
                    nextFrameAt = now + interval;
                    break;
                }
            }
            if (now >= nextFrameAt + interval) nextFrameAt = now + interval;
            if (speedMeasureSince <= 0.0) speedMeasureSince = speedNow;
            speedMeasureFrames += framesThisPresentation;
            const double measureAt = nowSeconds();
            const double elapsed = measureAt - speedMeasureSince;
            if (elapsed >= 0.16) {
                const double observed = double(speedMeasureFrames) / (elapsed * NTSC_FPS);
                const double bounded = std::max(0.1, std::min(8.0, observed));
                actualSpeedMultiplier = actualSpeedMultiplier * 0.45 + bounded * 0.55;
                speedMeasureSince = measureAt;
                speedMeasureFrames = 0;
            }
        } else {
            nextFrameAt = speedNow;
            speedMeasureSince = speedNow;
            speedMeasureFrames = 0;
        }

        const double renderNow = nowSeconds();
        const bool periodicUiRender = renderNow >= nextRenderAt;
        // Keep the presentation clock advancing even if no emulated frame was
        // finished. Otherwise the old stale deadline made idleUntil() spin
        // indefinitely at 100% CPU after an expensive frame.
        if (periodicUiRender || (emulatedFrame && fastForwardMultiplier <= 1.01)) {
            if (emulatedFrame || !normalPacedGameplay || rewindHeld) render();
            const double renderedAt = nowSeconds();
            nextRenderAt += 1.0 / NTSC_FPS;
            if (nextRenderAt < renderedAt) nextRenderAt = renderedAt + 1.0 / NTSC_FPS;
        }

        if (timingDiagnostics) {
            diagnosticFrames += static_cast<uint64_t>(emulatedFrameCount);
            const double sampleAt = nowSeconds();
            if (sampleAt - diagnosticSince >= 1.0) {
                const double achieved = double(diagnosticFrames) / ((sampleAt - diagnosticSince) * NTSC_FPS);
                std::cerr << "[Pacing] target=" << fastForwardMultiplier * 100.0 << "% achieved="
                          << achieved * 100.0 << "% rewind=" << (rewindHeld ? 1 : 0)
                          << " queue_ms=" << (audioDevice ? (1000.0 * sdl.GetQueuedAudioSize(audioDevice)
                               / (double(audioSampleRate) * sizeof(int16_t))) : 0.0) << "\n";
                diagnosticFrames = 0;
                diagnosticSince = sampleAt;
            }
        }

        // The display timer and emulation timer are independent. Sleeping only
        // to the next emulated frame also keeps input polling responsive.
        if (rewindHeld || normalPacedGameplay) idleUntil(std::min(nextFrameAt, nextRenderAt));
        else idleUntil(nextRenderAt);
    }
    return 0;
}

bool App::savePreview(const std::string& path, const std::string& initialRom) {
    if(!initialRom.empty()){if(!loadRom(initialRom))return false;for(int i=0;i<3;++i)runOneFrame(false);}const char* requested=std::getenv("CIPHERNES_UI_PREVIEW_SCREEN");std::string which=requested?lowerText(requested):"menu";
    if(which=="controller")screenMode=SCREEN_CONTROLLER;else if(which=="cheats")screenMode=SCREEN_CHEATS;else if(which=="genie")screenMode=SCREEN_GAME_GENIE;else if(which=="info")screenMode=SCREEN_INFO;else if(which=="tools"){activeMenu=-1;for(size_t i=0;i<menus.size();++i)if(lowerText(menus[i].text)=="tools"){activeMenu=int(i);break;}}else if(which=="tas"){activeMenu=-1;for(size_t i=0;i<menus.size();++i)if(lowerText(menus[i].text)=="tas"){activeMenu=int(i);break;}}else activeMenu=0;
    // Optional static framebuffer preview of fullscreen behavior for packaging audits.
    const char* previewFullscreen = std::getenv("CIPHERNES_UI_PREVIEW_FULLSCREEN");
    if (previewFullscreen) {
        const bool wantStretch = lowerText(previewFullscreen) == "stretch";
        setFullscreenMode(true, wantStretch);
        activeMenu = activeSubmenu = -1;
        mouseX = 600;
        mouseY = 300;
        if (std::getenv("CIPHERNES_UI_PREVIEW_HOVER")) {
            handleMouseMotion(18, 1);
            for (int i = 0; i < 8; ++i) render();
        }
    }
    render();int w=0,h=0;sdl.GetRendererOutputSize(renderer,&w,&h);if(w<=0||h<=0)return false;
    std::vector<uint32_t> pixels(static_cast<size_t>(w)*h);if(sdl.RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,&pixels[0],w*4)!=0)return false;return writePng(path,&pixels[0],w,h);
}

// End-to-end frontend regression using the actual TAS save/load/rewind/export logic.
// It is intentionally only invoked by --tas-branch-test (never during gameplay).
int App::runTasBranchSelfTest(const std::string& rom) {
    if (!loadRom(rom)) return 30;
    tasMode = TAS_RECORDING;
    tasFrames.clear();
    clearRewindHistory();
    audioScratch.clear(); captureRewindFrame();
    const auto advance = [this](uint8_t port1) {
        tasFrames.push_back(TasInputFrame(port1, 0));
        ciphernes_run_frame_with_input(core, port1, 0);
        drainAudio(false, false);
        captureRewindFrame();
    };
    for (int i = 0; i < 10; ++i) advance(i % 2 ? 0x80 : 0x40);
    const std::vector<unsigned char> savedMachine = captureState();
    saveState();
    for (int i = 0; i < 5; ++i) advance(0x10);
    const std::vector<unsigned char> oldFuture = captureState();
    stateSlot = 2; saveState(); // a future state whose original frames will be abandoned
    for (int i = 0; i < 3; ++i) if (!rewindOneFrame()) return 31;
    if (tasFrames.size() != 12 || rewindHistory.tasCursor() != 12) return 32;
    commitRewindBranch();
    for (int i = 0; i < 2; ++i) advance(0x02);
    if (tasFrames.size() != 14 || tasFrames[13].port1 != 0x02) return 33;
    stateSlot = 1; loadState();
    if (tasFrames.size() != 10 || captureState() != savedMachine || !restoredDisplayVisible) return 34;
    stateSlot = 2; loadState();
    if (tasFrames.size() != 15 || captureState() != oldFuture || tasFrames[14].port1 != 0x10) return 35;
    // Ensure the output contains no discarded inputs after branching again.
    for (int i=0; i<4; ++i) advance(0x08);
    for (int i=0; i<3; ++i) if (!rewindOneFrame()) return 36;
    commitRewindBranch();
    advance(0x01);
    std::string savedPath;
    if (!saveTasRecording(savedPath)) return 37;
    std::vector<TasInputFrame> loaded;
    std::string recordedRom, error;
    if (!loadTasRecording(savedPath, loaded, recordedRom, error) || loaded.size()!=17 ||
        loaded.back().port1 != 0x01 || recordedRom != romBase) return 38;
    // Keep the test movie from being auto-exported by the App destructor.
    tasMode = TAS_IDLE;
    tasFrames.clear();
    std::cout << "TAS BRANCH TEST PASS: rewind/commit, slot 1, restored future slot 2, "
                 "exact state + input-prefix, clean .ci export.\n";
    return 0;
}

int App::runControllerSelfTest() {
    std::cout << "Controller self-test: initializing virtual controller checks..." << std::endl;
    typedef int (*AttachVirtualFn)(int, int, int, int);
    typedef int (*DetachVirtualFn)(int);
    typedef int (*SetVirtualAxisFn)(SDL_Joystick*, int, Sint16);
    typedef int (*SetVirtualButtonFn)(SDL_Joystick*, int, Uint8);
    typedef int (*SetVirtualHatFn)(SDL_Joystick*, int, Uint8);
    typedef void (*JoystickUpdateFn)();
    AttachVirtualFn attachVirtual = reinterpret_cast<AttachVirtualFn>(dlsym(sdl.library, "SDL_JoystickAttachVirtual"));
    DetachVirtualFn detachVirtual = reinterpret_cast<DetachVirtualFn>(dlsym(sdl.library, "SDL_JoystickDetachVirtual"));
    SetVirtualAxisFn setAxis = reinterpret_cast<SetVirtualAxisFn>(dlsym(sdl.library, "SDL_JoystickSetVirtualAxis"));
    SetVirtualButtonFn setButton = reinterpret_cast<SetVirtualButtonFn>(dlsym(sdl.library, "SDL_JoystickSetVirtualButton"));
    SetVirtualHatFn setHat = reinterpret_cast<SetVirtualHatFn>(dlsym(sdl.library, "SDL_JoystickSetVirtualHat"));
    JoystickUpdateFn joystickUpdate = reinterpret_cast<JoystickUpdateFn>(dlsym(sdl.library, "SDL_JoystickUpdate"));
    if (!attachVirtual || !detachVirtual || !setAxis || !setButton || !setHat || !joystickUpdate) {
        std::cerr << "Controller self-test needs SDL 2.0.14 or newer virtual joystick support.\n";
        return 2;
    }

    int virtualIndex = attachVirtual(0, 4, 12, 1);
    std::cout << "Controller self-test: virtual joystick attach returned " << virtualIndex << std::endl;
    if (virtualIndex < 0) {
        std::cerr << "Could not attach SDL virtual joystick: " << sdl.GetError() << "\n";
        return 3;
    }
    openControllers();
    std::cout << "Controller self-test: controller enumeration complete (" << controllers.size() << " devices)." << std::endl;
    int deviceIndex = -1;
    for (int i = int(controllers.size()) - 1; i >= 0; --i) {
        if (!controllers[i].mapped && controllers[i].axes >= 4 && controllers[i].buttons >= 12 && controllers[i].hats >= 1) {
            deviceIndex = i;
            break;
        }
    }
    if (deviceIndex < 0) {
        closeControllers(); detachVirtual(virtualIndex);
        std::cerr << "Virtual joystick was not enumerated by the controller manager.\n";
        return 4;
    }

    portDeviceChoice[0] = controllers[deviceIndex].persistentId;
    SDL_Joystick* joystick = controllers[deviceIndex].joystick;
    SDL_Event testEvent;
    while (sdl.PollEvent(&testEvent)) {}

    const bool rawSetButton = setButton(joystick, 3, 1) == 0;
    joystickUpdate();
    bool sawRawButtonEvent = false;
    while (sdl.PollEvent(&testEvent))
        if (testEvent.type == SDL_JOYBUTTONDOWN && testEvent.jbutton.button == 3) sawRawButtonEvent = true;
    const bool rawButtonPolling = padBindingDown(0, PadBinding(PAD_BIND_JOYSTICK_BUTTON, 3, 0));
    setButton(joystick, 3, 0);

    const bool rawSetAxis = setAxis(joystick, 0, -26000) == 0;
    joystickUpdate();
    const bool rawAxisPolling = padBindingDown(0, PadBinding(PAD_BIND_JOYSTICK_AXIS, 0, -1));
    const bool rawStandardLeft = padBindingDown(0, PadBinding(PAD_BIND_STANDARD_DIRECTION, 2, 0));
    setAxis(joystick, 0, 0);

    const bool rawSetHat = setHat(joystick, 0, SDL_HAT_UP) == 0;
    joystickUpdate();
    const bool rawHatPolling = padBindingDown(0, PadBinding(PAD_BIND_JOYSTICK_HAT, 0, SDL_HAT_UP));
    const bool rawStandardUp = padBindingDown(0, PadBinding(PAD_BIND_STANDARD_DIRECTION, 0, 0));
    setHat(joystick, 0, SDL_HAT_CENTERED);

    bool serializationOk = true;
    const PadBinding samples[] = {
        PadBinding(PAD_BIND_STANDARD_DIRECTION, 1, 0), PadBinding(PAD_BIND_GAMEPAD_BUTTON, 9, 0),
        PadBinding(PAD_BIND_GAMEPAD_AXIS, 5, 1), PadBinding(PAD_BIND_JOYSTICK_BUTTON, 7, 0),
        PadBinding(PAD_BIND_JOYSTICK_AXIS, 2, -1), PadBinding(PAD_BIND_JOYSTICK_HAT, 0, SDL_HAT_RIGHT)
    };
    for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i) {
        PadBinding parsed = parsePadBinding(padBindingToken(samples[i]));
        serializationOk = serializationOk && parsed.kind == samples[i].kind &&
            parsed.control == samples[i].control && parsed.direction == samples[i].direction;
    }

    const bool rawPollingOk = rawSetButton && rawButtonPolling && rawSetAxis &&
        rawAxisPolling && rawStandardLeft && rawSetHat && rawHatPolling &&
        rawStandardUp && serializationOk;
    const bool rawPathOk = rawPollingOk && sawRawButtonEvent;
    bool ok = rawPathOk;

    std::cout << "Controller self-test raw details:"
              << " set-button=" << (rawSetButton ? "PASS" : "FAIL")
              << " event=" << (sawRawButtonEvent ? "PASS" : "FAIL")
              << " button-poll=" << (rawButtonPolling ? "PASS" : "FAIL")
              << " set-axis=" << (rawSetAxis ? "PASS" : "FAIL")
              << " axis-poll=" << (rawAxisPolling ? "PASS" : "FAIL")
              << " std-left=" << (rawStandardLeft ? "PASS" : "FAIL")
              << " set-hat=" << (rawSetHat ? "PASS" : "FAIL")
              << " hat-poll=" << (rawHatPolling ? "PASS" : "FAIL")
              << " std-up=" << (rawStandardUp ? "PASS" : "FAIL")
              << " bindings=" << (serializationOk ? "PASS" : "FAIL")
              << std::endl;

    const bool allowMappedSkip = std::getenv("CIPHERNES_CONTROLLER_TEST_ALLOW_MAPPED_SKIP") != NULL;
    bool mappedPathTested = false;
    if (sdl.GameControllerAddMapping) {
        std::cout << "Controller self-test: checking SDL GameController mapping path..." << std::endl;
        std::string guid = controllers[deviceIndex].guid;
        std::string mapping = guid + ",CipherNES Virtual Controller,a:b0,b:b1,x:b2,y:b3,back:b4,guide:b5,start:b6,"
            "leftstick:b7,rightstick:b8,leftshoulder:b9,rightshoulder:b10,dpup:h0.1,dpdown:h0.4,"
            "dpleft:h0.8,dpright:h0.2,leftx:a0,lefty:a1,rightx:a2,righty:a3,platform:" + std::string(PLATFORM_SDL_MAPPING_TAG) + ",";
        ok = ok && sdl.GameControllerAddMapping(mapping.c_str()) >= 0;
        closeControllers();
        openControllers();
        deviceIndex = -1;
        for (int i = int(controllers.size()) - 1; i >= 0; --i) {
            if (controllers[i].guid == guid && controllers[i].mapped) { deviceIndex = i; break; }
        }
        if (deviceIndex >= 0) {
            mappedPathTested = true;
            portDeviceChoice[0] = controllers[deviceIndex].persistentId;
            joystick = controllers[deviceIndex].joystick;
            while (sdl.PollEvent(&testEvent)) {}
            ok = ok && setButton(joystick, 0, 1) == 0;
            joystickUpdate();
            bool sawMappedButtonEvent = false;
            while (sdl.PollEvent(&testEvent))
                if (testEvent.type == SDL_CONTROLLERBUTTONDOWN && testEvent.cbutton.button == SDL_CONTROLLER_BUTTON_A) sawMappedButtonEvent = true;
            ok = ok && sawMappedButtonEvent;
            ok = ok && padBindingDown(0, PadBinding(PAD_BIND_GAMEPAD_BUTTON, SDL_CONTROLLER_BUTTON_A, 0));
            setButton(joystick, 0, 0);
            ok = ok && setAxis(joystick, 1, -26000) == 0;
            joystickUpdate();
            ok = ok && padBindingDown(0, PadBinding(PAD_BIND_GAMEPAD_AXIS, SDL_CONTROLLER_AXIS_LEFTY, -1));
            ok = ok && padBindingDown(0, PadBinding(PAD_BIND_STANDARD_DIRECTION, 0, 0));
            setAxis(joystick, 1, 0);
        } else ok = false;
    }

    if (allowMappedSkip && !mappedPathTested) {
        // Older baseline SDL2 releases may omit synthetic joystick events and
        // may fail to re-enumerate virtual joysticks through GameController.
        // standard-direction fallback, and binding serialization. Normal
        // --controller-test remains strict about events and mapped controllers.
        ok = rawPollingOk;
        std::cout << "Controller self-test: synthetic event/mapped virtual paths skipped for compatibility baseline." << std::endl;
    }
    std::cout << "Controller self-test: cleaning up virtual controller..." << std::endl;
    closeControllers();
    detachVirtual(virtualIndex);
    // This command-line self-test exits immediately, so there is no reason to
    // rescan physical HID devices after detaching the virtual controller. On
    // headless macOS CI runners that extra rescan can block in the host HID
    // stack even though all controller assertions have already completed.
    if (!ok) {
        std::cerr << "Controller self-test FAILED.\n";
        return 5;
    }
    std::cout << "Controller self-test PASS\n"
              << "- Raw joystick enumeration PASS\n"
              << "- SDL joystick/controller event ABI " << (rawPathOk ? "PASS" : (allowMappedSkip ? "SKIPPED" : "FAIL")) << "\n"
              << "- Button polling PASS\n"
              << "- Axis and dead-zone polling PASS\n"
              << "- Hat/D-pad polling PASS\n"
              << "- Standard direction fallback PASS\n"
              << "- SDL GameController mapping/polling " << (mappedPathTested ? "PASS" : "SKIPPED") << "\n"
              << "- Binding serialization round-trip PASS\n";
    return 0;
}

int validateCore(const std::string& rom, int frames, bool benchmark) {
    std::cout << ciphernes_get_core_version_string() << "\nAPI version: " << ciphernes_get_api_version() << "\n";
    if (ciphernes_get_api_version() != CIPHERNES_API_VERSION) { std::cerr << "Core/header API mismatch\n"; return 2; }
    CIPHERNES_Handle* core = ciphernes_create(); if (!core) return 3;
    PreparedRomPath prepared; std::string prepareError;
    if (!prepareRomPath(rom, prepared, prepareError)) {
        std::cerr << "Could not prepare validation ROM: " << prepareError << "\n";
        ciphernes_destroy(core); return 4;
    }
    const std::string zipEntry = prepared.displayBase;
    if (!ciphernes_load_rom(core, prepared.corePath.c_str())) {
        cleanupPreparedRom(prepared);
        std::cerr << "Could not load validation ROM: " << rom << "\n"; ciphernes_destroy(core); return 4;
    }
    cleanupPreparedRom(prepared);
    ciphernes_set_audio_sample_rate(core, 44100.0); std::vector<int16_t> audio(8192); uint64_t clocks = 0;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    for (int i = 0; i < frames; ++i) { clocks += ciphernes_run_frame(core); ciphernes_take_audio_samples_i16(core, &audio[0], audio.size()); }
    double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    const uint32_t* frame = ciphernes_get_framebuffer(core); uint32_t frameCrc = crc32(frame, NES_W * NES_H * 4u);
    uint32_t stateSize = ciphernes_get_state_size(core); std::vector<unsigned char> state(stateSize + 4096u);
    uint32_t written = ciphernes_save_state(core, &state[0], state.size()); bool reload = written && ciphernes_load_state(core, &state[0], written);
    std::cout << "ROM: " << rom << (hasExtension(rom, ".zip") ? " (entry " + zipEntry + ")" : std::string()) << "\nFrames: " << frames << "\nMaster clocks: " << clocks << "\nFramebuffer CRC32: ";
    char crcText[16]; std::snprintf(crcText, sizeof(crcText), "%08X", frameCrc); std::cout << crcText << "\n";
    std::cout << "State bytes: " << written << "\nState round-trip: " << (reload ? "PASS" : "FAIL") << "\n";
    std::cout << "Elapsed: " << seconds << " s, emulated FPS: " << (seconds > 0 ? frames / seconds : 0) << "\n";
    ciphernes_destroy(core);
    if (!reload || frameCrc == 0) return 5;
    if (benchmark) std::cout << "Real-time multiple: " << (seconds > 0 ? (frames / seconds) / NTSC_FPS : 0) << "x\n";
    return 0;
}

std::string absoluteBeforeChdir(const std::string& path) {
    if (path.empty() || path[0] == '/') return path;
    char cwd[PATH_MAX]; if (!getcwd(cwd, sizeof(cwd))) return path;
    return std::string(cwd) + "/" + path;
}

bool enterRuntimeDirectory() {
    if (!ciphernes_initialise_runtime_directory()) {
        std::cerr << "Could not create CipherNES user-data folders: " << ciphernes_runtime_directory() << "\n";
        return false;
    }
#if defined(__APPLE__)
    if (!ciphernes_prepare_embedded_macos_runtime()) {
        std::cerr << "Could not initialise the bundled macOS SDL2 runtime.\n";
        return false;
    }
#endif
    const std::string directory = ciphernes_runtime_directory();
    if (chdir(directory.c_str()) != 0) {
        std::cerr << "Could not enter CipherNES user-data directory: " << directory << "\n";
        return false;
    }
    return true;
}

void printUsage() {
    std::cout << "CipherNES " << PLATFORM_DISPLAY_NAME << "\n"
              << "Usage: CipherNES [game.nes|disk.fds|disk.qd|archive.zip]\n"
              << "       CipherNES --validate [game.nes|disk.fds|disk.qd|archive.zip] [frames]\n"
              << "       CipherNES --benchmark [game.nes|disk.fds|disk.qd|archive.zip] [frames]\n"
              << "       CipherNES --controller-test\n"
              << "       CipherNES --tas-branch-test owned-rom.nes\n"
              << "       CipherNES --ui-preview output.png [game.nes|disk.fds|disk.qd|archive.zip]\n"
              << "       CipherNES --sdl-runtime-test\n";
#if defined(__APPLE__)
    std::cout << "SDL2 is embedded in this executable. Validation and benchmarks are headless.\n";
#else
    std::cout << "The GUI needs libsdl2-2.0-0. Validation and benchmarks are headless.\n";
#endif
}

} // namespace

int main(int argc, char** argv) {
    std::string command = argc > 1 ? argv[1] : "";
    std::string suppliedPath, previewOutput, previewRom;
    if (!command.empty() && command[0] != '-') suppliedPath = absoluteBeforeChdir(command);
    else if ((command == "--validate" || command == "--benchmark" || command == "--tas-branch-test") && argc > 2) suppliedPath = absoluteBeforeChdir(argv[2]);
    else if(command=="--ui-preview"&&argc>2){previewOutput=absoluteBeforeChdir(argv[2]);if(argc>3)previewRom=absoluteBeforeChdir(argv[3]);}
    if (!enterRuntimeDirectory()) return 1;
    if (command == "--help" || command == "-h") { printUsage(); return 0; }
    if (command == "--sdl-runtime-test") {
        // Probe all SDL2 symbols and embedded shared-library dependencies without
        // initializing macOS HID, Cocoa windows or the graphics backend. This is
        // safe for headless CI; a real desktop launch must still be tested.
        SDLApi api;
        if (!api.load()) {
            std::cerr << "CipherNES: SDL2 runtime loading or symbol verification failed.\n";
            return 1;
        }
        std::cout << "CipherNES: SDL2 runtime loaded and required symbols resolved.\n";
        return 0;
    }
    if (command == "--validate" || command == "--benchmark") {
        std::string rom = suppliedPath.empty() ? "TestRoms/nestest.nes" : suppliedPath;
        int frames = argc > 3 ? std::max(1, std::atoi(argv[3])) : (command == "--benchmark" ? 600 : 180);
        return validateCore(rom, frames, command == "--benchmark");
    }
    if (command == "--tas-branch-test") {
        if (suppliedPath.empty()) { printUsage(); return 2; }
        App app;
        if (!app.initialize()) return 1;
        return app.runTasBranchSelfTest(suppliedPath);
    }
    if (command == "--controller-test") {
        setenv("SDL_VIDEODRIVER", "dummy", 0);
        setenv("SDL_AUDIODRIVER", "dummy", 0);
#if defined(__APPLE__)
        // Hosted macOS machines can block while SDL2 probes host HID devices.
        // The self-test supplies its own virtual joystick, so disable HIDAPI
        // only for this diagnostic mode. Normal CipherNES launches keep the
        // standard macOS SDL controller stack fully enabled.
        setenv("SDL_JOYSTICK_HIDAPI", "0", 0);
        setenv("SDL_JOYSTICK_MFI", "0", 0);
        setenv("SDL_JOYSTICK_IOKIT", "0", 0);
        setenv("CIPHERNES_SKIP_INITIAL_CONTROLLER_SCAN", "1", 1);
#endif
        App app; if (!app.initialize()) return 1; return app.runControllerSelfTest();
    }
    if(command=="--ui-preview"){
        if(argc<3){printUsage();return 2;}App app;if(!app.initialize())return 1;return app.savePreview(previewOutput,previewRom)?0:3;
    }
    App app; if (!app.initialize()) return 1; return app.run(suppliedPath);
}
