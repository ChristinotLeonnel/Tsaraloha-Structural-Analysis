#include "IfcGuid.h"

#include <QUuid>

#include <cstring>

namespace TSA::BIM::IfcGuid
{

namespace
{
constexpr char kAlphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_$";

int indexOf(char c)
{
    const char* p = std::strchr(kAlphabet, c);
    return (c != '\0' && p) ? static_cast<int>(p - kAlphabet) : -1;
}
} // namespace

std::string compress(const std::array<std::uint8_t, 16>& b)
{
    // Algorithme buildingSMART : 1 octet → 2 caractères, puis 5 × (3 octets → 4 caractères).
    std::string out;
    out.reserve(22);
    out += kAlphabet[b[0] >> 6];
    out += kAlphabet[b[0] & 63];
    for (int i = 1; i < 16; i += 3)
    {
        const std::uint32_t v = (std::uint32_t(b[i]) << 16) | (std::uint32_t(b[i + 1]) << 8) | b[i + 2];
        for (int k = 3; k >= 0; --k) out += kAlphabet[(v >> (6 * k)) & 63];
    }
    return out;
}

bool expand(const std::string& g, std::array<std::uint8_t, 16>& b)
{
    if (!isValid(g)) return false;
    b[0] = static_cast<std::uint8_t>((indexOf(g[0]) << 6) | indexOf(g[1]));
    for (int i = 0; i < 5; ++i)
    {
        std::uint32_t v = 0;
        for (int k = 0; k < 4; ++k) v = (v << 6) | static_cast<std::uint32_t>(indexOf(g[2 + 4 * i + k]));
        b[1 + 3 * i] = static_cast<std::uint8_t>(v >> 16);
        b[2 + 3 * i] = static_cast<std::uint8_t>(v >> 8);
        b[3 + 3 * i] = static_cast<std::uint8_t>(v);
    }
    return true;
}

bool isValid(const std::string& g)
{
    if (g.size() != 22) return false;
    for (char c : g)
        if (indexOf(c) < 0) return false;
    return indexOf(g[0]) <= 3;
}

std::string create()
{
    const QByteArray raw = QUuid::createUuid().toRfc4122();
    std::array<std::uint8_t, 16> b {};
    for (int i = 0; i < 16; ++i) b[i] = static_cast<std::uint8_t>(raw[i]);
    return compress(b);
}

} // namespace TSA::BIM::IfcGuid
