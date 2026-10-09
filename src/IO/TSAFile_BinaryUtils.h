#pragma once

#include "TSAFileFormat.h"
#include "../Model/Section.h"
#include "../Model/Material.h"
#include "../Model/MaterialLibrary.h"

#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace TSA::IO::Detail
{

inline void writeU8(std::vector<uint8_t>& buf, uint8_t val)
{
    buf.push_back(val);
}

inline void writeU16(std::vector<uint8_t>& buf, uint16_t val)
{
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
}

inline void writeU32(std::vector<uint8_t>& buf, uint32_t val)
{
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
}

inline void writeI32(std::vector<uint8_t>& buf, int32_t val)
{
    writeU32(buf, static_cast<uint32_t>(val));
}

inline void writeU64(std::vector<uint8_t>& buf, uint64_t val)
{
    for (int i = 0; i < 8; ++i)
    {
        buf.push_back(static_cast<uint8_t>((val >> (i * 8)) & 0xFF));
    }
}

inline void writeDouble(std::vector<uint8_t>& buf, double val)
{
    uint64_t bits = 0;
    std::memcpy(&bits, &val, sizeof(double));
    writeU64(buf, bits);
}

inline void writeString(std::vector<uint8_t>& buf, const std::string& str)
{
    // Longueur sur 16 bits : au plus 65 535 octets. Auparavant la borne était MAX_SAFE_STRING_LEN
    // (65 536), convertie en 0 par le cast : toute chaîne ≥ 64 Kio était enregistrée vide.
    size_t len = std::min<size_t>(str.size(), 0xFFFF);
    while (len > 0 && len < str.size() && (static_cast<uint8_t>(str[len]) & 0xC0) == 0x80)
        --len; // ne pas couper un caractère UTF-8
    writeU16(buf, static_cast<uint16_t>(len));
    buf.insert(buf.end(), str.begin(), str.begin() + static_cast<std::ptrdiff_t>(len));
}

/// Chaîne JSON occupant un chunk entier (COOR, GRID). Format d'origine : préfixe u16 + octets.
/// Au-delà de 65 535 octets : préfixe 0 puis le JSON jusqu'à la fin du chunk (une version antérieure
/// y lit une chaîne vide, comme avant cette extension ; aucun fichier existant n'est réinterprété :
/// un préfixe 0 y était toujours suivi d'un chunk de 2 octets).
inline void writeChunkText(std::vector<uint8_t>& buf, const std::string& text)
{
    if (text.size() <= 0xFFFF)
    {
        writeString(buf, text);
        return;
    }
    writeU16(buf, 0);
    buf.insert(buf.end(), text.begin(), text.end());
}


inline bool readU8(const uint8_t* data, size_t size, size_t& offset, uint8_t& val)
{
    if (offset + 1 > size) return false;
    val = data[offset++];
    return true;
}

inline bool readU16(const uint8_t* data, size_t size, size_t& offset, uint16_t& val)
{
    if (offset + 2 > size) return false;
    val = static_cast<uint16_t>(data[offset]) | (static_cast<uint16_t>(data[offset + 1]) << 8);
    offset += 2;
    return true;
}

inline bool readU32(const uint8_t* data, size_t size, size_t& offset, uint32_t& val)
{
    if (offset + 4 > size) return false;
    val = static_cast<uint32_t>(data[offset]) |
          (static_cast<uint32_t>(data[offset + 1]) << 8) |
          (static_cast<uint32_t>(data[offset + 2]) << 16) |
          (static_cast<uint32_t>(data[offset + 3]) << 24);
    offset += 4;
    return true;
}

inline bool readI32(const uint8_t* data, size_t size, size_t& offset, int32_t& val)
{
    uint32_t u = 0;
    if (!readU32(data, size, offset, u)) return false;
    val = static_cast<int32_t>(u);
    return true;
}

inline bool readU64(const uint8_t* data, size_t size, size_t& offset, uint64_t& val)
{
    if (offset + 8 > size) return false;
    val = 0;
    for (int i = 0; i < 8; ++i)
    {
        val |= (static_cast<uint64_t>(data[offset + i]) << (i * 8));
    }
    offset += 8;
    return true;
}

inline bool readDouble(const uint8_t* data, size_t size, size_t& offset, double& val)
{
    uint64_t bits = 0;
    if (!readU64(data, size, offset, bits)) return false;
    std::memcpy(&val, &bits, sizeof(double));
    return true;
}

inline bool readString(const uint8_t* data, size_t size, size_t& offset, std::string& str)
{
    uint16_t len = 0;
    if (!readU16(data, size, offset, len)) return false;
    if (len > MAX_SAFE_STRING_LEN) return false;
    if (offset + len > size) return false;
    str.assign(reinterpret_cast<const char*>(data + offset), len);
    offset += len;
    return true;
}

inline bool readChunkText(const uint8_t* data, size_t size, std::string& text)
{
    size_t offset = 0;
    uint16_t len = 0;
    if (!readU16(data, size, offset, len)) return false;
    if (len == 0 && size > 2)
    {
        text.assign(reinterpret_cast<const char*>(data + 2), size - 2);
        return true;
    }
    offset = 0;
    return readString(data, size, offset, text);
}

// Sérialisation Section
inline void serializeSection(std::vector<uint8_t>& buf, const TSA::Model::Section& s)
{
    writeU32(buf, static_cast<uint32_t>(s.id));
    writeString(buf, s.name);
    writeU8(buf, static_cast<uint8_t>(s.shape));
    writeDouble(buf, s.width);
    writeDouble(buf, s.height);
    writeDouble(buf, s.diameter);
    writeDouble(buf, s.tw);
    writeDouble(buf, s.tf);
}

inline bool deserializeSection(const uint8_t* data, size_t size, size_t& offset, TSA::Model::Section& s)
{
    uint32_t id = 0;
    if (!readU32(data, size, offset, id)) return false;
    s.id = static_cast<int>(id);
    if (!readString(data, size, offset, s.name)) return false;
    uint8_t shape = 0;
    if (!readU8(data, size, offset, shape)) return false;
    s.shape = static_cast<TSA::Model::SectionShape>(shape);
    if (!readDouble(data, size, offset, s.width)) return false;
    if (!readDouble(data, size, offset, s.height)) return false;
    if (!readDouble(data, size, offset, s.diameter)) return false;
    if (!readDouble(data, size, offset, s.tw)) return false;
    if (!readDouble(data, size, offset, s.tf)) return false;
    return true;
}

// Sérialisation Material
inline void serializeMaterial(std::vector<uint8_t>& buf, const TSA::Model::Material& m)
{
    writeU32(buf, static_cast<uint32_t>(m.id));
    writeString(buf, m.name);
    writeU8(buf, static_cast<uint8_t>(m.type));
    writeDouble(buf, m.E);
    writeDouble(buf, m.nu);
    writeDouble(buf, m.density);
    writeDouble(buf, m.fk);
    writeDouble(buf, m.thermalCoeff);
}

inline bool deserializeMaterial(const uint8_t* data, size_t size, size_t& offset, TSA::Model::Material& m)
{
    uint32_t id = 0;
    if (!readU32(data, size, offset, id)) return false;
    m.id = static_cast<int>(id);
    if (!readString(data, size, offset, m.name)) return false;
    uint8_t type = 0;
    if (!readU8(data, size, offset, type)) return false;
    m.type = static_cast<TSA::Model::MaterialType>(type);
    if (!readDouble(data, size, offset, m.E)) return false;
    if (!readDouble(data, size, offset, m.nu)) return false;
    if (!readDouble(data, size, offset, m.density)) return false;
    if (!readDouble(data, size, offset, m.fk)) return false;
    if (!readDouble(data, size, offset, m.thermalCoeff)) return false;
    m.syncMechanical();
    const auto* stdMat = TSA::Model::MaterialLibrary::instance().findById(m.id);
    if (!stdMat) stdMat = TSA::Model::MaterialLibrary::instance().findByName(m.name);
    if (stdMat)
    {
        m.visual = stdMat->visual;
    }
    return true;
}

} // namespace TSA::IO::Detail
