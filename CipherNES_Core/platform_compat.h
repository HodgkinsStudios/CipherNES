#pragma once

// Created by Jacob Hodgkins
// Small host-platform shim shared by the CipherNES core and Windows frontend.

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <sys/stat.h>

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <direct.h>
#include <io.h>
#include <shellapi.h>

#ifdef CreateWindow
#undef CreateWindow
#endif

#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif

#ifndef RTLD_NOW
#define RTLD_NOW 0
#endif
#ifndef RTLD_LOCAL
#define RTLD_LOCAL 0
#endif

inline const char* dlerror() {
    static char message[512];
    const DWORD code = GetLastError();
    if (!code) {
        message[0] = 0;
        return message;
    }
    const DWORD count = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, code, 0, message, static_cast<DWORD>(sizeof(message)), NULL);
    if (!count) std::snprintf(message, sizeof(message), "Windows error %lu", static_cast<unsigned long>(code));
    return message;
}

inline void* dlopen(const char* path, int) {
    SetLastError(0);
    return reinterpret_cast<void*>(LoadLibraryA(path));
}

inline void* dlsym(void* library, const char* name) {
    if (!library || !name) return NULL;
    SetLastError(0);
    return reinterpret_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(library), name));
}

inline int dlclose(void* library) {
    if (!library) return 0;
    return FreeLibrary(reinterpret_cast<HMODULE>(library)) ? 0 : -1;
}

// Minimal dirent compatibility for native MinGW builds. It intentionally
// exposes only d_name because that is all CipherNES uses.
struct dirent {
    char d_name[MAX_PATH];
};

struct DIR {
    HANDLE handle;
    WIN32_FIND_DATAA data;
    bool first;
    dirent current;
    DIR() : handle(INVALID_HANDLE_VALUE), first(true) { current.d_name[0] = 0; }
};

inline DIR* opendir(const char* path) {
    if (!path || !*path) return NULL;
    std::string pattern(path);
    const char last = pattern.empty() ? 0 : pattern[pattern.size() - 1];
    if (last != '/' && last != '\\') pattern += "\\";
    pattern += "*";

    DIR* dir = new DIR();
    dir->handle = FindFirstFileA(pattern.c_str(), &dir->data);
    if (dir->handle == INVALID_HANDLE_VALUE) {
        delete dir;
        return NULL;
    }
    return dir;
}

inline dirent* readdir(DIR* dir) {
    if (!dir || dir->handle == INVALID_HANDLE_VALUE) return NULL;
    if (!dir->first && !FindNextFileA(dir->handle, &dir->data)) return NULL;
    dir->first = false;
    std::strncpy(dir->current.d_name, dir->data.cFileName, sizeof(dir->current.d_name) - 1);
    dir->current.d_name[sizeof(dir->current.d_name) - 1] = 0;
    return &dir->current;
}

inline int closedir(DIR* dir) {
    if (!dir) return -1;
    if (dir->handle != INVALID_HANDLE_VALUE) FindClose(dir->handle);
    delete dir;
    return 0;
}

inline int ciphernes_lstat(const char* path, struct stat* st) {
    return ::stat(path, st);
}

inline bool ciphernes_make_dir(const std::string& path) {
    if (_mkdir(path.c_str()) == 0) return true;
    return errno == EEXIST;
}

inline bool ciphernes_remove_file(const std::string& path) {
    return _unlink(path.c_str()) == 0 || errno == ENOENT;
}

inline bool ciphernes_remove_dir(const std::string& path) {
    return _rmdir(path.c_str()) == 0 || errno == ENOENT;
}

inline char* ciphernes_realpath(const char* path, char* resolved) {
    return _fullpath(resolved, path, PATH_MAX);
}

inline std::string ciphernes_executable_path() {
    char path[MAX_PATH + 1] = {};
    const DWORD n = GetModuleFileNameA(NULL, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return std::string();
    return std::string(path, path + n);
}

inline std::string ciphernes_executable_directory() {
    const std::string path = ciphernes_executable_path();
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? std::string() : path.substr(0, slash);
}

inline std::string ciphernes_home_directory() {
    const char* userProfile = std::getenv("USERPROFILE");
    if (userProfile && *userProfile) return std::string(userProfile);
    const char* drive = std::getenv("HOMEDRIVE");
    const char* homePath = std::getenv("HOMEPATH");
    if (drive && *drive && homePath && *homePath) return std::string(drive) + homePath;
    const char* home = std::getenv("HOME");
    return home && *home ? std::string(home) : std::string(".");
}

inline std::string ciphernes_quote_windows_argument(const char* arg) {
    if (!arg) return std::string();
    std::string input(arg);
    if (input.find_first_of(" \t\"") == std::string::npos) return input;

    std::string out = "\"";
    unsigned backslashes = 0;
    for (size_t i = 0; i < input.size(); ++i) {
        const char c = input[i];
        if (c == '\\') {
            ++backslashes;
            continue;
        }
        if (c == '"') {
            out.append(backslashes * 2 + 1, '\\');
            out += '"';
            backslashes = 0;
            continue;
        }
        out.append(backslashes, '\\');
        backslashes = 0;
        out += c;
    }
    out.append(backslashes * 2, '\\');
    out += '"';
    return out;
}

inline bool ciphernes_launch_process(const char* executable, const char* const* argv, const char* workingDirectory) {
    if (!executable || !*executable) return false;

    std::string commandLine;
    if (argv && argv[0]) {
        for (size_t i = 0; argv[i]; ++i) {
            if (!commandLine.empty()) commandLine += ' ';
            commandLine += ciphernes_quote_windows_argument(argv[i]);
        }
    } else {
        commandLine = ciphernes_quote_windows_argument(executable);
    }

    std::string mutableLine = commandLine;
    mutableLine.push_back('\0');

    STARTUPINFOA startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    const BOOL ok = CreateProcessA(
        executable,
        &mutableLine[0],
        NULL, NULL, FALSE, 0, NULL,
        (workingDirectory && *workingDirectory) ? workingDirectory : NULL,
        &startup, &process);
    if (!ok) return false;

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
}

inline bool ciphernes_open_folder(const char* path) {
    if (!path || !*path) return false;
    const HINSTANCE result = ShellExecuteA(NULL, "open", path, NULL, NULL, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
}

#else

#include <dirent.h>
#include <dlfcn.h>
#include <limits.h>
#include <sys/types.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

inline int ciphernes_lstat(const char* path, struct stat* st) {
    return ::lstat(path, st);
}

inline bool ciphernes_make_dir(const std::string& path) {
    if (::mkdir(path.c_str(), 0755) == 0) return true;
    return errno == EEXIST;
}

inline bool ciphernes_remove_file(const std::string& path) {
    return ::unlink(path.c_str()) == 0 || errno == ENOENT;
}

inline bool ciphernes_remove_dir(const std::string& path) {
    return ::rmdir(path.c_str()) == 0 || errno == ENOENT;
}

inline char* ciphernes_realpath(const char* path, char* resolved) {
    return ::realpath(path, resolved);
}

inline std::string ciphernes_executable_path() {
#if defined(__APPLE__)
    uint32_t size = 0;
    (void)_NSGetExecutablePath(NULL, &size);
    if (size == 0) return std::string();
    std::vector<char> buffer(static_cast<size_t>(size) + 1u, 0);
    if (_NSGetExecutablePath(&buffer[0], &size) != 0) return std::string();
    char resolved[PATH_MAX + 1] = {};
    if (::realpath(&buffer[0], resolved)) return std::string(resolved);
    return std::string(&buffer[0]);
#else
    char path[PATH_MAX + 1] = {};
    const ssize_t n = ::readlink("/proc/self/exe", path, PATH_MAX);
    if (n <= 0 || n > PATH_MAX) return std::string();
    return std::string(path, path + n);
#endif
}

inline std::string ciphernes_executable_directory() {
    const std::string path = ciphernes_executable_path();
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? std::string() : path.substr(0, slash);
}

inline std::string ciphernes_home_directory() {
    const char* home = std::getenv("HOME");
    return home && *home ? std::string(home) : std::string(".");
}

inline bool ciphernes_launch_process(const char* executable, const char* const* argv, const char* workingDirectory) {
    if (!executable || !*executable) return false;
    const pid_t pid = ::fork();
    if (pid < 0) return false;
    if (pid == 0) {
        if (workingDirectory && *workingDirectory) (void)::chdir(workingDirectory);
        if (argv && argv[0]) {
            std::vector<char*> mutableArgv;
            for (size_t i = 0; argv[i]; ++i) mutableArgv.push_back(const_cast<char*>(argv[i]));
            mutableArgv.push_back(NULL);
            ::execvp(executable, &mutableArgv[0]);
        } else {
            char* args[2] = { const_cast<char*>(executable), NULL };
            ::execvp(executable, args);
        }
        ::_exit(127);
    }
    return true;
}

inline bool ciphernes_open_folder(const char* path) {
    if (!path || !*path) return false;
#if defined(__APPLE__)
    const char* command = "open";
#else
    const char* command = "xdg-open";
#endif
    const char* args[3] = { command, path, NULL };
    return ciphernes_launch_process(command, args, NULL);
}

#endif


// CipherNES user-writable runtime directory.  All platforms use the same
// relative layout; only the operating-system application-data base differs.
// CIPHERNES_RUNTIME_DIR deliberately overrides the default for isolated tests.
inline std::string ciphernes_runtime_directory() {
    const char* overridePath = std::getenv("CIPHERNES_RUNTIME_DIR");
    if (overridePath && *overridePath) return std::string(overridePath);
#if defined(_WIN32)
    const char* appdata = std::getenv("APPDATA");
    std::string base = (appdata && *appdata) ? std::string(appdata)
                        : ciphernes_home_directory() + "\\AppData\\Roaming";
    return base + "\\HodgkinsStudios\\CipherNES";
#elif defined(__APPLE__)
    return ciphernes_home_directory() + "/Library/Application Support/HodgkinsStudios/CipherNES";
#else
    // This project intentionally uses ~/.local/share even when XDG_DATA_HOME
    // is set, per the CipherNES project-specific data-location requirement.
    return ciphernes_home_directory() + "/.local/share/HodgkinsStudios/CipherNES";
#endif
}

inline bool ciphernes_ensure_directory_tree(const std::string& path) {
    if (path.empty()) return false;
    std::string component;
    for (size_t i = 0; i < path.size(); ++i) {
        const char c = path[i];
        component += c;
        if (c != '/' && c != '\\' && i + 1 != path.size()) continue;
        if (component.size() == 1 && (c == '/' || c == '\\')) continue;
        if (component.size() == 3 && component[1] == ':') continue;
        // Do not mistake an existing regular file for an existing directory.
        if (!ciphernes_make_dir(component)) return false;
        struct stat st = {};
        if (::stat(component.c_str(), &st) != 0 || !(st.st_mode & S_IFDIR)) return false;
    }
    return true;
}

inline bool ciphernes_initialise_runtime_directory() {
    const std::string base = ciphernes_runtime_directory();
    if (!ciphernes_ensure_directory_tree(base)) return false;
    const char* folders[] = {"SaveState", "Saves", "Screenshots", "Videos", "Cheats", "TAS", "bios", "logs", "roms", "config"};
#if defined(_WIN32)
    const char separator = '\\';
#else
    const char separator = '/';
#endif
    for (size_t i = 0; i < sizeof(folders) / sizeof(folders[0]); ++i) {
        if (!ciphernes_ensure_directory_tree(base + separator + folders[i])) return false;
    }
    return true;
}

#if defined(_WIN32)
#include <fstream>

// Windows release embeds the DLL dependencies as PE RCDATA resources and
// extracts verified bytes on first run. Users distribute only CipherNES.exe.
// The OS itself (kernel32/user32/etc.) is still provided by Windows.
inline bool ciphernes_prepare_embedded_windows_runtime() {
    const std::string folder = ciphernes_runtime_directory() + "\\runtime";
    if (!ciphernes_ensure_directory_tree(folder)) return false;
    struct ResourceFile { int id; const char* filename; };
    const ResourceFile resources[] = {
        {101, "SDL2.dll"}, {102, "zlib1.dll"}, {103, "libwinpthread-1.dll"},
        {104, "libgcc_s_seh-1.dll"}, {105, "libstdc++-6.dll"}
    };
    for (size_t i = 0; i < sizeof(resources)/sizeof(resources[0]); ++i) {
        const HRSRC res = FindResourceA(NULL, MAKEINTRESOURCEA(resources[i].id), RT_RCDATA);
        if (!res) {
            if (resources[i].id <= 102) return false;
            continue; // optional toolchain dependencies
        }
        const DWORD length = SizeofResource(NULL, res);
        const HGLOBAL loaded = LoadResource(NULL, res);
        const void* contents = loaded ? LockResource(loaded) : NULL;
        if (!length || !contents) return false;
        const std::string path = folder + "\\" + resources[i].filename;
        bool matches = false;
        {
            std::ifstream in(path.c_str(), std::ios::binary | std::ios::ate);
            if (in && static_cast<unsigned long long>(in.tellg()) == length) {
                in.seekg(0, std::ios::beg);
                const unsigned char* bytes = static_cast<const unsigned char*>(contents);
                char buffer[4096];
                matches = true;
                for (DWORD offset = 0; offset < length;) {
                    const DWORD n = (length-offset) < sizeof(buffer) ? (length-offset) : sizeof(buffer);
                    in.read(buffer, n);
                    if (!in || std::memcmp(buffer, bytes+offset, n) != 0) { matches = false; break; }
                    offset += n;
                }
            }
        }
        if (!matches) {
            std::ofstream out(path.c_str(), std::ios::binary | std::ios::trunc);
            if (!out) return false;
            out.write(static_cast<const char*>(contents), length);
            if (!out) return false;
        }
    }
    // Required for LoadLibraryA("SDL2.dll") and its transitive dependencies.
    return SetDllDirectoryA(folder.c_str()) != 0;
}
#endif

#if defined(__APPLE__)
#include <mach-o/getsect.h>
#include <mach-o/ldsyms.h>
#include <fstream>

inline const unsigned char* ciphernes_macos_section(const char* section, unsigned long* length) {
    return reinterpret_cast<const unsigned char*>(getsectiondata(
        &_mh_execute_header, "__DATA", section, length));
}

inline bool ciphernes_prepare_embedded_macos_runtime() {
    unsigned long manifestSize = 0;
    const unsigned char* manifest = ciphernes_macos_section("__CNMAP", &manifestSize);
    if (!manifest || !manifestSize) return false;
    std::string folder = ciphernes_runtime_directory() + "/runtime";
    if (!ciphernes_ensure_directory_tree(folder)) return false;
    const std::string names(reinterpret_cast<const char*>(manifest), manifestSize);
    size_t offset = 0;
    unsigned index = 0;
    while (offset < names.size()) {
        size_t end = names.find('\n', offset);
        if (end == std::string::npos) end = names.size();
        std::string name = names.substr(offset, end - offset);
        offset = end + 1;
        if (name.empty()) continue;
        // All names are produced from vetted build dependencies; reject any
        // malformed input rather than writing outside the runtime directory.
        if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos ||
            name == "." || name == ".." || index > 49) return false;
        char section[17];
        std::snprintf(section, sizeof(section), "__CN%02u", index++);
        unsigned long len = 0;
        const unsigned char* bytes = ciphernes_macos_section(section, &len);
        if (!bytes || !len) return false;
        const std::string path = folder + "/" + name;
        bool matches = false;
        {
            std::ifstream in(path.c_str(), std::ios::binary | std::ios::ate);
            if (in && static_cast<unsigned long long>(in.tellg()) == len) {
                in.seekg(0);
                char buf[4096]; matches = true;
                for (unsigned long off=0;off<len;) {
                    const size_t n = static_cast<size_t>(std::min<unsigned long>(sizeof(buf),len-off));
                    in.read(buf,n);
                    if (!in || std::memcmp(buf,bytes+off,n)!=0) { matches = false; break; }
                    off += n;
                }
            }
        }
        if (!matches) {
            std::ofstream out(path.c_str(),std::ios::binary|std::ios::trunc);
            if (!out) return false;
            out.write(reinterpret_cast<const char*>(bytes),len);
            if (!out) return false;
        }
    }
    return setenv("CIPHERNES_SDL2_LIBRARY",(folder+"/libSDL2-2.0.0.dylib").c_str(),1) == 0;
}
#endif
