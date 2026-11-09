// g++ -std=c++17 -Wall -Wextra psu_version_test.cpp -o psu_version_test && ./psu_version_test
// Cac vector duoi day lay tu docs/PSU_FRAME_EXAMPLES.md (do firmware sinh, dia chi PSU = 1).
#include <cstdio>
#include <cstring>
#include "psu_version_proto.hpp"

static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { std::printf("FAIL: %s\n", msg); ++fails; } } while (0)

static std::vector<uint8_t> hex(const char *s)
{
    std::vector<uint8_t> v;
    unsigned b;
    while (*s) { if (*s == ' ') { ++s; continue; } std::sscanf(s, "%2x", &b); v.push_back(static_cast<uint8_t>(b)); s += 2; }
    return v;
}

int main()
{
    // CRC16: vector chuan
    CHECK(psu::crc16CcittFalse(reinterpret_cast<const uint8_t *>("123456789"), 9) == 0x29B1, "crc16 vector chuan");

    // BEGIN moi (17 byte)
    CHECK(psu::buildBegin(1, 0x1000, 0xDEADBEEF) == hex("AB CD 90 90 01 00 00 10 00 DE AD BE EF 2D 82 E1 E2"), "BEGIN 17 byte");

    // SET_VERSION / GET_VERSION / tra loi
    auto s = psu::buildSetVersion(1, "PSU_v1.2.3_abc123");
    CHECK(s.size() == 41, "SET_VERSION dai 41 byte");
    CHECK(s[2] == 0x9A && s[3] == 0x9A && s[4] == 1 && s[39] == 0xE1 && s[40] == 0xE2, "SET_VERSION header/addr/tail");
    CHECK(std::memcmp(&s[5], "PSU_v1.2.3_abc123", 17) == 0 && s[22] == 0, "SET_VERSION chuoi + dem 00");

    CHECK(psu::buildGetVersion(1) == hex("AB CD 99 99 01 F1 D1 E1 E2"), "GET_VERSION 9 byte");

    // Tra loi A9 A9: dung buildFrame tao lai, roi phan tich
    std::vector<uint8_t> payload(32, 0);
    std::memcpy(payload.data(), "PSU_v1.2.3_abc123", 17);
    auto reply = psu::buildFrame(psu::CMD_VERSION, 1, payload);
    auto pr = psu::parseVersionReply(reply.data(), reply.size());
    CHECK(pr && pr->addr == 1 && pr->version == "PSU_v1.2.3_abc123", "parse tra loi A9 A9");
    reply[10] ^= 1;
    CHECK(!psu::parseVersionReply(reply.data(), reply.size()), "sai CRC bi tu choi");
    reply[10] ^= 1; reply[2] = 0x95;
    CHECK(!psu::parseVersionReply(reply.data(), reply.size()), "sai CMD bi tu choi");
    std::vector<uint8_t> empty(32, 0);
    auto e = psu::buildFrame(psu::CMD_VERSION, 1, empty);
    auto pe = psu::parseVersionReply(e.data(), e.size());
    CHECK(pe && pe->version.empty(), "chuoi rong (chua dat)");

    // Ten file -> version
    CHECK(psu::versionFromFileName("C:\\fw\\PSU_v1.2.3_abc123.bin").value == "PSU_v1.2.3_abc123", "bo thu muc + .bin (Windows)");
    CHECK(psu::versionFromFileName("/tmp/PSU_v1.2.3_abc123.bin").value == "PSU_v1.2.3_abc123", "bo thu muc + .bin (Unix)");
    auto nn = psu::versionFromFileName("PSU_b\xC3\xA1n.bin");
    CHECK(nn.replaced && nn.value == "PSU_b__n", "ky tu ngoai ASCII doi thanh '_'");
    CHECK(!psu::versionFromFileName("PSU_v1.2.3_phien_ban_rat_dai_hon_32_ky_tu.bin").error.empty(), "qua 32 ky tu bi tu choi");
    CHECK(!psu::versionFromFileName(".bin").error.empty() || psu::versionFromFileName(".bin").value == ".bin", "ten rong/chi co duoi");

    std::printf("psu_version tests done, fails=%d\n", fails);
    return fails ? 1 : 0;
}
