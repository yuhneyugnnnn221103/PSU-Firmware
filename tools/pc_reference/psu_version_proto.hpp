// Tham chieu cho phia PC (C++17, khong phu thuoc Qt): khung PSU + SET/GET_VERSION + BEGIN moi (17 byte).
// Chep vao proto/ cua ACTL va noi vao cac ham dung khung hien co. Test: psu_version_test.cpp.
//
// Khung: AB CD | CMD(2) | ADDR | payload | CRC16 (BE) | E1 E2. CRC16-CCITT-FALSE (0x1021, init 0xFFFF)
// tinh tu byte ADDR den het payload (bo AB CD, CMD, CRC, E1 E2). So nhieu byte la big-endian.
#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace psu {

constexpr uint16_t CMD_BEGIN       = 0x9090;
constexpr uint16_t CMD_SET_VERSION = 0x9A9A;   // PC -> PSU, chi recovery, giua ERASE va END
constexpr uint16_t CMD_GET_VERSION = 0x9999;   // PC -> PSU, app va recovery
constexpr uint16_t CMD_VERSION     = 0xA9A9;   // PSU -> PC, tra loi GET_VERSION
constexpr size_t   VERSION_LEN     = 32;       // chuoi ASCII, dem 0x00

inline uint16_t crc16CcittFalse(const uint8_t *d, size_t n)
{
    uint16_t c = 0xFFFF;
    for (size_t i = 0; i < n; ++i) {
        c ^= static_cast<uint16_t>(d[i]) << 8;
        for (int b = 0; b < 8; ++b) c = (c & 0x8000) ? static_cast<uint16_t>((c << 1) ^ 0x1021) : static_cast<uint16_t>(c << 1);
    }
    return c;
}

inline std::vector<uint8_t> buildFrame(uint16_t cmd, uint8_t addr, const std::vector<uint8_t> &payload)
{
    std::vector<uint8_t> f = {0xAB, 0xCD, static_cast<uint8_t>(cmd >> 8), static_cast<uint8_t>(cmd), addr};
    f.insert(f.end(), payload.begin(), payload.end());
    const uint16_t crc = crc16CcittFalse(&f[4], f.size() - 4);   // tu ADDR den het payload
    f.push_back(static_cast<uint8_t>(crc >> 8));
    f.push_back(static_cast<uint8_t>(crc));
    f.push_back(0xE1);
    f.push_back(0xE2);
    return f;
}

// BEGIN moi: 17 byte = size (4) + CRC32 cua file (4). KHONG con truong version u32 (14-17 cu).
inline std::vector<uint8_t> buildBegin(uint8_t addr, uint32_t size, uint32_t crc32)
{
    std::vector<uint8_t> p = {
        static_cast<uint8_t>(size >> 24), static_cast<uint8_t>(size >> 16), static_cast<uint8_t>(size >> 8), static_cast<uint8_t>(size),
        static_cast<uint8_t>(crc32 >> 24), static_cast<uint8_t>(crc32 >> 16), static_cast<uint8_t>(crc32 >> 8), static_cast<uint8_t>(crc32)};
    return buildFrame(CMD_BEGIN, addr, p);
}

struct VersionName {
    std::string value;   // chuoi se gui (<= 32 ky tu in duoc); rong neu loi
    std::string error;   // rong neu hop le
    bool replaced = false;   // co ky tu ngoai 0x20..0x7E bi doi thanh '_'
};

// Lay chuoi version tu duong dan file .bin: bo thu muc, bo phan mo rong. Ky tu ngoai 0x20..0x7E -> '_'.
// Dai hon 32 -> loi (de nguoi dung doi ten, khong tu cat vi de mat phan hash o cuoi).
inline VersionName versionFromFileName(const std::string &path)
{
    VersionName r;
    size_t s = path.find_last_of("/\\");
    std::string name = (s == std::string::npos) ? path : path.substr(s + 1);
    const size_t dot = name.find_last_of('.');
    if (dot != std::string::npos && dot > 0) name.erase(dot);
    for (char &ch : name) {
        const unsigned char u = static_cast<unsigned char>(ch);
        if (u < 0x20 || u > 0x7E) { ch = '_'; r.replaced = true; }
    }
    if (name.empty())              { r.error = "ten file rong"; return r; }
    if (name.size() > VERSION_LEN) { r.error = "ten file dai hon 32 ky tu: doi ten file"; return r; }
    r.value = name;
    return r;
}

// SET_VERSION: 41 byte, payload = chuoi 32 byte (dem 0x00).
inline std::vector<uint8_t> buildSetVersion(uint8_t addr, const std::string &version)
{
    std::vector<uint8_t> p(VERSION_LEN, 0x00);
    for (size_t i = 0; i < version.size() && i < VERSION_LEN; ++i) p[i] = static_cast<uint8_t>(version[i]);
    return buildFrame(CMD_SET_VERSION, addr, p);
}

// GET_VERSION: 9 byte, khong payload.
inline std::vector<uint8_t> buildGetVersion(uint8_t addr) { return buildFrame(CMD_GET_VERSION, addr, {}); }

struct VersionReply {
    uint8_t addr = 0;
    std::string version;   // rong = chua dat / khong co anh hop le
};

// Phan tich khung A9 A9 (41 byte). nullopt neu sai do dai/header/tail/CMD/CRC.
inline std::optional<VersionReply> parseVersionReply(const uint8_t *f, size_t n)
{
    if (n != 41) return std::nullopt;
    if (f[0] != 0xAB || f[1] != 0xCD || f[39] != 0xE1 || f[40] != 0xE2) return std::nullopt;
    if ((static_cast<uint16_t>(f[2]) << 8 | f[3]) != CMD_VERSION) return std::nullopt;
    const uint16_t crc = crc16CcittFalse(&f[4], 1 + VERSION_LEN);   // ADDR + 32 byte chuoi
    if (crc != (static_cast<uint16_t>(f[37]) << 8 | f[38])) return std::nullopt;
    VersionReply r;
    r.addr = f[4];
    for (size_t i = 0; i < VERSION_LEN && f[5 + i] >= 0x20 && f[5 + i] <= 0x7E; ++i) r.version.push_back(static_cast<char>(f[5 + i]));
    return r;
}

}  // namespace psu
