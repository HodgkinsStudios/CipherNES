#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

namespace ciphernes_state {

inline void write_bytes(std::vector<uint8_t>& out, const void* data, size_t n) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(data);
    out.insert(out.end(), p, p + n);
}

template <typename T>
inline void write_pod(std::vector<uint8_t>& out, const T& v) {
    static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");
    write_bytes(out, &v, sizeof(T));
}

inline void write_u32(std::vector<uint8_t>& out, uint32_t v) { write_pod(out, v); }
inline void write_u64(std::vector<uint8_t>& out, uint64_t v) { write_pod(out, v); }
inline void write_u16(std::vector<uint8_t>& out, uint16_t v) { write_pod(out, v); }
inline void write_u8 (std::vector<uint8_t>& out, uint8_t v)  { write_pod(out, v); }
inline void write_bool(std::vector<uint8_t>& out, bool v) { uint8_t b = v ? 1 : 0; write_u8(out, b); }

inline void write_vec(std::vector<uint8_t>& out, const std::vector<uint8_t>& v) {
    write_u32(out, (uint32_t)v.size());
    if (!v.empty()) write_bytes(out, v.data(), v.size());
}

inline void write_vector_u8(std::vector<uint8_t>& out, const std::vector<uint8_t>& v) { write_vec(out, v); }
inline void write_vector_u16(std::vector<uint8_t>& out, const std::vector<uint16_t>& v) {
    write_u32(out, (uint32_t)v.size());
    if (!v.empty()) write_bytes(out, v.data(), v.size() * sizeof(uint16_t));
}

inline void write_string(std::vector<uint8_t>& out, const std::string& s) {
    write_u32(out, (uint32_t)s.size());
    if (!s.empty()) write_bytes(out, s.data(), s.size());
}

// -------------------- read --------------------

inline bool read_bytes(const uint8_t*& p, const uint8_t* end, void* out, size_t n) {
    if ((size_t)(end - p) < n) return false;
    std::memcpy(out, p, n);
    p += n;
    return true;
}

template <typename T>
inline bool read_pod(const uint8_t*& p, const uint8_t* end, T& outv) {
    static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");
    return read_bytes(p, end, &outv, sizeof(T));
}

inline bool read_u32(const uint8_t*& p, const uint8_t* end, uint32_t& v) { return read_pod(p,end,v); }
inline bool read_u64(const uint8_t*& p, const uint8_t* end, uint64_t& v) { return read_pod(p,end,v); }
inline bool read_u16(const uint8_t*& p, const uint8_t* end, uint16_t& v) { return read_pod(p,end,v); }
inline bool read_u8 (const uint8_t*& p, const uint8_t* end, uint8_t& v)  { return read_pod(p,end,v); }
inline bool read_bool(const uint8_t*& p, const uint8_t* end, bool& v) {
    uint8_t b=0; if(!read_u8(p,end,b)) return false; v = (b!=0); return true;
}

inline bool read_vec(const uint8_t*& p, const uint8_t* end, std::vector<uint8_t>& v) {
    uint32_t n=0; if(!read_u32(p,end,n)) return false;
    if ((size_t)(end - p) < n) return false;
    v.assign(p, p + n);
    p += n;
    return true;
}

inline bool read_vector_u8(const uint8_t*& p, const uint8_t* end, std::vector<uint8_t>& v) { return read_vec(p, end, v); }
inline bool read_vector_u16(const uint8_t*& p, const uint8_t* end, std::vector<uint16_t>& v) {
    uint32_t n=0; if(!read_u32(p,end,n)) return false;
    size_t bytes = size_t(n) * sizeof(uint16_t);
    if ((size_t)(end - p) < bytes) return false;
    v.resize(n);
    if (bytes) std::memcpy(v.data(), p, bytes);
    p += bytes;
    return true;
}

inline bool read_string(const uint8_t*& p, const uint8_t* end, std::string& s) {
    uint32_t n=0; if(!read_u32(p,end,n)) return false;
    if ((size_t)(end - p) < n) return false;
    s.assign(reinterpret_cast<const char*>(p), reinterpret_cast<const char*>(p + n));
    p += n;
    return true;
}

} // namespace ciphernes_state
