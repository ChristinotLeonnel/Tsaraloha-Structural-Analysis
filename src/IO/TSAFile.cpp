#include "TSAFile.h"
#include "TSAPreviewGenerator.h"

#include <QByteArray>
#include <QBuffer>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <sstream>

namespace TSA::IO
{

// =============================================================================
// Helper binary read/write functions with safety bounds
// =============================================================================
namespace
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
    uint16_t len = static_cast<uint16_t>(std::min<size_t>(str.size(), MAX_SAFE_STRING_LEN));
    writeU16(buf, len);
    buf.insert(buf.end(), str.begin(), str.begin() + len);
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
    return true;
}

} // anonymous namespace

// =============================================================================
// QtZlibCompressionProvider
// =============================================================================
bool QtZlibCompressionProvider::compress(const uint8_t* inData, size_t inSize, std::vector<uint8_t>& outData)
{
    if (!inData || inSize == 0)
    {
        outData.clear();
        return true;
    }
    QByteArray uncompressed(reinterpret_cast<const char*>(inData), static_cast<qsizetype>(inSize));
    QByteArray compressed = qCompress(uncompressed, 6); // Zlib standard level 6
    if (compressed.isEmpty())
        return false;
    outData.assign(reinterpret_cast<const uint8_t*>(compressed.constData()),
                   reinterpret_cast<const uint8_t*>(compressed.constData()) + compressed.size());
    return true;
}

bool QtZlibCompressionProvider::decompress(const uint8_t* inData, size_t inSize, std::vector<uint8_t>& outData)
{
    if (!inData || inSize == 0)
    {
        outData.clear();
        return true;
    }
    QByteArray compressed(reinterpret_cast<const char*>(inData), static_cast<qsizetype>(inSize));
    QByteArray uncompressed = qUncompress(compressed);
    if (uncompressed.isEmpty() && inSize > 0)
        return false;
    outData.assign(reinterpret_cast<const uint8_t*>(uncompressed.constData()),
                   reinterpret_cast<const uint8_t*>(uncompressed.constData()) + uncompressed.size());
    return true;
}

// =============================================================================
// StandardEncryptionProvider
// =============================================================================
bool StandardEncryptionProvider::encrypt(const uint8_t* inData, size_t inSize, std::vector<uint8_t>& outData, const std::string& key)
{
    if (key.empty())
    {
        outData.assign(inData, inData + inSize);
        return true;
    }
    outData.resize(inSize);
    for (size_t i = 0; i < inSize; ++i)
    {
        uint8_t k = static_cast<uint8_t>(key[i % key.size()]);
        outData[i] = inData[i] ^ k ^ static_cast<uint8_t>((i * 37) & 0xFF);
    }
    return true;
}

bool StandardEncryptionProvider::decrypt(const uint8_t* inData, size_t inSize, std::vector<uint8_t>& outData, const std::string& key)
{
    return encrypt(inData, inSize, outData, key); // Chiffrement symétrique involutif
}

// =============================================================================
// TSAFileWriter
// =============================================================================
TSAFileWriter::TSAFileWriter()
    : m_compressor(std::make_shared<QtZlibCompressionProvider>())
    , m_encryptor(std::make_shared<StandardEncryptionProvider>())
{
}

bool TSAFileWriter::saveToFile(const std::string& filePath,
                              const TSA::Model::Model& model,
                              const TSA::Grid::GridManager* gridManager,
                              const std::string& projectName,
                              const std::string& author,
                              std::string* errorMessage)
{
    // 1. Préparer le payload uncompressed contenant tous les Chunks
    std::vector<uint8_t> payload;

    writeProjectChunk(payload, projectName, author);

    // Miniature 3D embarquée (Thumbnail)
    QByteArray pngBytes;
    if (!m_thumbnail.isNull())
    {
        QBuffer buf(&pngBytes);
        buf.open(QIODevice::WriteOnly);
        m_thumbnail.save(&buf, "PNG");
    }
    else
    {
        pngBytes = TSAPreviewGenerator::generatePngData(model, 512, 512);
    }
    if (!pngBytes.isEmpty())
    {
        writeThumbnailChunk(payload, pngBytes);
    }

    if (model.coordinateSystem())
    {
        writeCoordinateChunk(payload, model.coordinateSystem());
    }
    if (gridManager)
    {
        writeGridChunk(payload, gridManager);
    }
    writeNodeChunk(payload, model.nodes());
    writeBarChunk(payload, model.beams());
    writeColumnChunk(payload, model.columns());
    writeSlabChunk(payload, model.slabs());
    writeWallChunk(payload, model.walls());
    writeFoundationChunk(payload, model.foundations());
    writeTrussChunk(payload, model.trussMembers());

    uint64_t uncompressedSize = payload.size();

    // 2. Traitement Compression
    uint32_t flags = FLAG_NONE;
    if (!pngBytes.isEmpty())
    {
        flags |= FLAG_HAS_THUMBNAIL;
    }
    std::vector<uint8_t> processedData;

    if (m_useCompression && m_compressor)
    {
        if (m_compressor->compress(payload.data(), payload.size(), processedData))
        {
            flags |= FLAG_COMPRESSED;
        }
        else
        {
            processedData = payload;
        }
    }
    else
    {
        processedData = payload;
    }

    // 3. Traitement Protection / Chiffrement
    if (m_useEncryption && !m_password.empty() && m_encryptor)
    {
        std::vector<uint8_t> encryptedData;
        if (m_encryptor->encrypt(processedData.data(), processedData.size(), encryptedData, m_password))
        {
            processedData = std::move(encryptedData);
            flags |= FLAG_ENCRYPTED;
        }
    }

    // 4. Calcul de l'intégrité CRC32 sur le payload final écrit
    uint32_t crc = computeCRC32(processedData.data(), processedData.size());

    // 5. Préparation du Header 256 octets
    TSAFileHeader header;
    header.magic = TSA_FILE_MAGIC;
    header.versionMajor = TSA_FORMAT_VERSION_MAJOR;
    header.versionMinor = TSA_FORMAT_VERSION_MINOR;
    header.appVersionMajor = TSA_APP_VERSION_MAJOR;
    header.appVersionMinor = TSA_APP_VERSION_MINOR;
    header.appVersionPatch = TSA_APP_VERSION_PATCH;
    header.flags = flags;
    header.headerSize = sizeof(TSAFileHeader);
    header.checksumCRC32 = crc;
    header.fileSize = sizeof(TSAFileHeader) + processedData.size();
    header.uncompressedSize = uncompressedSize;
    header.payloadOffset = sizeof(TSAFileHeader);

    auto safeStrCopy = [](char* dst, size_t dstSize, const std::string& src) {
        if (dstSize == 0) return;
        size_t n = (src.size() < dstSize - 1) ? src.size() : (dstSize - 1);
        std::memcpy(dst, src.data(), n);
        dst[n] = '\0';
    };

    safeStrCopy(header.projectName, sizeof(header.projectName), projectName);
    safeStrCopy(header.author, sizeof(header.author), author);

    std::string nowStr = QDateTime::currentDateTime().toString(Qt::ISODate).toStdString();
    safeStrCopy(header.lastModifiedTimestamp, sizeof(header.lastModifiedTimestamp), nowStr);
    if (header.creationTimestamp[0] == '\0')
    {
        safeStrCopy(header.creationTimestamp, sizeof(header.creationTimestamp), nowStr);
    }

    // 6. Écriture atomique dans le fichier
    std::ofstream out(filePath, std::ios::binary);
    if (!out.is_open())
    {
        if (errorMessage) *errorMessage = "Impossible de créer le fichier .tsa : " + filePath;
        return false;
    }

    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    if (!processedData.empty())
    {
        out.write(reinterpret_cast<const char*>(processedData.data()), processedData.size());
    }
    out.close();

    if (!out.good())
    {
        if (errorMessage) *errorMessage = "Erreur d'écriture lors de la finalisation du fichier .tsa.";
        return false;
    }

    return true;
}

void TSAFileWriter::writeProjectChunk(std::vector<uint8_t>& buffer, const std::string& projectName, const std::string& author)
{
    std::vector<uint8_t> chunkData;
    writeString(chunkData, projectName);
    writeString(chunkData, author);
    writeString(chunkData, "SI_METRIC"); // Unités : mètres, Pa, N, kg
    writeDouble(chunkData, 1.0);         // Échelle des longueurs (1.0 = mètres)

    TSAChunkHeader ch;
    ch.chunkId = CHUNK_PROJ;
    ch.chunkSize = static_cast<uint32_t>(chunkData.size());
    ch.elementCount = 1;

    const uint8_t* chBytes = reinterpret_cast<const uint8_t*>(&ch);
    buffer.insert(buffer.end(), chBytes, chBytes + sizeof(ch));
    buffer.insert(buffer.end(), chunkData.begin(), chunkData.end());
}

void TSAFileWriter::writeThumbnailChunk(std::vector<uint8_t>& buffer, const QByteArray& pngData)
{
    if (pngData.isEmpty()) return;

    std::vector<uint8_t> chunkData;
    uint32_t width = 512, height = 512, formatType = 1; // 1 = PNG
    uint32_t dataLen = static_cast<uint32_t>(pngData.size());
    writeU32(chunkData, width);
    writeU32(chunkData, height);
    writeU32(chunkData, formatType);
    writeU32(chunkData, dataLen);
    chunkData.insert(chunkData.end(), reinterpret_cast<const uint8_t*>(pngData.constData()),
                     reinterpret_cast<const uint8_t*>(pngData.constData()) + pngData.size());

    TSAChunkHeader ch;
    ch.chunkId = CHUNK_THMB;
    ch.chunkSize = static_cast<uint32_t>(chunkData.size());
    ch.elementCount = 1;

    const uint8_t* chBytes = reinterpret_cast<const uint8_t*>(&ch);
    buffer.insert(buffer.end(), chBytes, chBytes + sizeof(ch));
    buffer.insert(buffer.end(), chunkData.begin(), chunkData.end());
}

void TSAFileWriter::writeCoordinateChunk(std::vector<uint8_t>& buffer, const TSA::Coordinate::CoordinateSystem* cs)
{
    if (!cs) return;
    std::string json = cs->serializeToJson();
    std::vector<uint8_t> chunkData;
    writeString(chunkData, json);

    TSAChunkHeader ch;
    ch.chunkId = CHUNK_COOR;
    ch.chunkSize = static_cast<uint32_t>(chunkData.size());
    ch.elementCount = 1;

    const uint8_t* chBytes = reinterpret_cast<const uint8_t*>(&ch);
    buffer.insert(buffer.end(), chBytes, chBytes + sizeof(ch));
    buffer.insert(buffer.end(), chunkData.begin(), chunkData.end());
}

void TSAFileWriter::writeGridChunk(std::vector<uint8_t>& buffer, const TSA::Grid::GridManager* gm)
{
    if (!gm) return;
    std::string json = gm->serializeToJson();
    std::vector<uint8_t> chunkData;
    writeString(chunkData, json);

    TSAChunkHeader ch;
    ch.chunkId = CHUNK_GRID;
    ch.chunkSize = static_cast<uint32_t>(chunkData.size());
    ch.elementCount = static_cast<uint32_t>(gm->grids().size());

    const uint8_t* chBytes = reinterpret_cast<const uint8_t*>(&ch);
    buffer.insert(buffer.end(), chBytes, chBytes + sizeof(ch));
    buffer.insert(buffer.end(), chunkData.begin(), chunkData.end());
}

void TSAFileWriter::writeNodeChunk(std::vector<uint8_t>& buffer, const std::map<int, TSA::Model::Node>& nodes)
{
    std::vector<uint8_t> chunkData;
    for (const auto& [id, n] : nodes)
    {
        writeI32(chunkData, n.id());
        writeString(chunkData, n.name());
        writeDouble(chunkData, n.x());
        writeDouble(chunkData, n.y());
        writeDouble(chunkData, n.z());
        writeString(chunkData, n.levelId());
        writeU8(chunkData, static_cast<uint8_t>(n.supportType()));
        writeString(chunkData, n.color());
    }

    TSAChunkHeader ch;
    ch.chunkId = CHUNK_NODE;
    ch.chunkSize = static_cast<uint32_t>(chunkData.size());
    ch.elementCount = static_cast<uint32_t>(nodes.size());

    const uint8_t* chBytes = reinterpret_cast<const uint8_t*>(&ch);
    buffer.insert(buffer.end(), chBytes, chBytes + sizeof(ch));
    buffer.insert(buffer.end(), chunkData.begin(), chunkData.end());
}

void TSAFileWriter::writeBarChunk(std::vector<uint8_t>& buffer, const std::map<int, TSA::Model::Beam>& beams)
{
    std::vector<uint8_t> chunkData;
    for (const auto& [id, b] : beams)
    {
        writeI32(chunkData, b.id());
        writeString(chunkData, b.name());
        writeI32(chunkData, b.startNodeId());
        writeI32(chunkData, b.endNodeId());
        writeU8(chunkData, static_cast<uint8_t>(b.role()));
        serializeSection(chunkData, b.section());
        serializeMaterial(chunkData, b.material());
        writeDouble(chunkData, b.rotation());
        writeU8(chunkData, static_cast<uint8_t>(b.eccentricity()));

        const auto& sr = b.startRelease();
        writeU8(chunkData, sr.fx ? 1 : 0);
        writeU8(chunkData, sr.fy ? 1 : 0);
        writeU8(chunkData, sr.fz ? 1 : 0);
        writeU8(chunkData, sr.mx ? 1 : 0);
        writeU8(chunkData, sr.my ? 1 : 0);
        writeU8(chunkData, sr.mz ? 1 : 0);

        const auto& er = b.endRelease();
        writeU8(chunkData, er.fx ? 1 : 0);
        writeU8(chunkData, er.fy ? 1 : 0);
        writeU8(chunkData, er.fz ? 1 : 0);
        writeU8(chunkData, er.mx ? 1 : 0);
        writeU8(chunkData, er.my ? 1 : 0);
        writeU8(chunkData, er.mz ? 1 : 0);

        writeString(chunkData, b.color());
    }

    TSAChunkHeader ch;
    ch.chunkId = CHUNK_BARS;
    ch.chunkSize = static_cast<uint32_t>(chunkData.size());
    ch.elementCount = static_cast<uint32_t>(beams.size());

    const uint8_t* chBytes = reinterpret_cast<const uint8_t*>(&ch);
    buffer.insert(buffer.end(), chBytes, chBytes + sizeof(ch));
    buffer.insert(buffer.end(), chunkData.begin(), chunkData.end());
}

void TSAFileWriter::writeColumnChunk(std::vector<uint8_t>& buffer, const std::map<int, TSA::Model::Column>& columns)
{
    std::vector<uint8_t> chunkData;
    for (const auto& [id, c] : columns)
    {
        writeI32(chunkData, c.id());
        writeString(chunkData, c.name());
        writeI32(chunkData, c.startNodeId());
        writeI32(chunkData, c.endNodeId());
        serializeSection(chunkData, c.section());
        serializeMaterial(chunkData, c.material());
        writeDouble(chunkData, c.rotation());
        writeString(chunkData, c.color());
    }

    TSAChunkHeader ch;
    ch.chunkId = CHUNK_COLS;
    ch.chunkSize = static_cast<uint32_t>(chunkData.size());
    ch.elementCount = static_cast<uint32_t>(columns.size());

    const uint8_t* chBytes = reinterpret_cast<const uint8_t*>(&ch);
    buffer.insert(buffer.end(), chBytes, chBytes + sizeof(ch));
    buffer.insert(buffer.end(), chunkData.begin(), chunkData.end());
}

void TSAFileWriter::writeSlabChunk(std::vector<uint8_t>& buffer, const std::map<int, TSA::Model::Slab>& slabs)
{
    std::vector<uint8_t> chunkData;
    for (const auto& [id, s] : slabs)
    {
        writeI32(chunkData, s.id());
        writeString(chunkData, s.name());

        const auto& nids = s.nodeIds();
        writeU16(chunkData, static_cast<uint16_t>(nids.size()));
        for (int nid : nids)
        {
            writeI32(chunkData, nid);
        }

        writeDouble(chunkData, s.thickness());
        serializeMaterial(chunkData, s.material());
        writeU8(chunkData, static_cast<uint8_t>(s.slabType()));
        writeString(chunkData, s.color());
    }

    TSAChunkHeader ch;
    ch.chunkId = CHUNK_SLAB;
    ch.chunkSize = static_cast<uint32_t>(chunkData.size());
    ch.elementCount = static_cast<uint32_t>(slabs.size());

    const uint8_t* chBytes = reinterpret_cast<const uint8_t*>(&ch);
    buffer.insert(buffer.end(), chBytes, chBytes + sizeof(ch));
    buffer.insert(buffer.end(), chunkData.begin(), chunkData.end());
}

void TSAFileWriter::writeWallChunk(std::vector<uint8_t>& buffer, const std::map<int, TSA::Model::Wall>& walls)
{
    std::vector<uint8_t> chunkData;
    for (const auto& [id, w] : walls)
    {
        writeI32(chunkData, w.id());
        writeString(chunkData, w.name());
        writeI32(chunkData, w.startNodeId());
        writeI32(chunkData, w.endNodeId());
        writeDouble(chunkData, w.height());
        writeDouble(chunkData, w.thickness());
        writeDouble(chunkData, w.offset());
        serializeMaterial(chunkData, w.material());
        writeString(chunkData, w.color());
    }

    TSAChunkHeader ch;
    ch.chunkId = CHUNK_WALL;
    ch.chunkSize = static_cast<uint32_t>(chunkData.size());
    ch.elementCount = static_cast<uint32_t>(walls.size());

    const uint8_t* chBytes = reinterpret_cast<const uint8_t*>(&ch);
    buffer.insert(buffer.end(), chBytes, chBytes + sizeof(ch));
    buffer.insert(buffer.end(), chunkData.begin(), chunkData.end());
}

void TSAFileWriter::writeFoundationChunk(std::vector<uint8_t>& buffer, const std::map<int, TSA::Model::Foundation>& foundations)
{
    std::vector<uint8_t> chunkData;
    for (const auto& [id, f] : foundations)
    {
        writeI32(chunkData, f.id());
        writeString(chunkData, f.name());
        writeI32(chunkData, f.nodeId());
        writeU8(chunkData, static_cast<uint8_t>(f.foundationType()));
        writeDouble(chunkData, f.widthA());
        writeDouble(chunkData, f.lengthB());
        writeDouble(chunkData, f.heightH());
        serializeMaterial(chunkData, f.material());
        writeDouble(chunkData, f.soilBearingCapacity());
        writeString(chunkData, f.color());
    }

    TSAChunkHeader ch;
    ch.chunkId = CHUNK_FNDN;
    ch.chunkSize = static_cast<uint32_t>(chunkData.size());
    ch.elementCount = static_cast<uint32_t>(foundations.size());

    const uint8_t* chBytes = reinterpret_cast<const uint8_t*>(&ch);
    buffer.insert(buffer.end(), chBytes, chBytes + sizeof(ch));
    buffer.insert(buffer.end(), chunkData.begin(), chunkData.end());
}

void TSAFileWriter::writeTrussChunk(std::vector<uint8_t>& buffer, const std::map<int, TSA::Model::TrussMember>& trussMembers)
{
    std::vector<uint8_t> chunkData;
    for (const auto& [id, t] : trussMembers)
    {
        writeI32(chunkData, t.id());
        writeString(chunkData, t.name());
        writeI32(chunkData, t.startNodeId());
        writeI32(chunkData, t.endNodeId());
        writeU8(chunkData, static_cast<uint8_t>(t.role()));
        serializeSection(chunkData, t.section());
        serializeMaterial(chunkData, t.material());
        writeString(chunkData, t.color());
    }

    TSAChunkHeader ch;
    ch.chunkId = CHUNK_TRUS;
    ch.chunkSize = static_cast<uint32_t>(chunkData.size());
    ch.elementCount = static_cast<uint32_t>(trussMembers.size());

    const uint8_t* chBytes = reinterpret_cast<const uint8_t*>(&ch);
    buffer.insert(buffer.end(), chBytes, chBytes + sizeof(ch));
    buffer.insert(buffer.end(), chunkData.begin(), chunkData.end());
}

// =============================================================================
// TSAFileReader
// =============================================================================
TSAFileReader::TSAFileReader()
    : m_compressor(std::make_shared<QtZlibCompressionProvider>())
    , m_encryptor(std::make_shared<StandardEncryptionProvider>())
{
}

bool TSAFileReader::readHeader(const std::string& filePath, TSAFileHeader& header, std::string* errorMessage)
{
    std::ifstream in(filePath, std::ios::binary);
    if (!in.is_open())
    {
        if (errorMessage) *errorMessage = "Fichier introuvable ou inaccessible : " + filePath;
        return false;
    }

    in.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (in.gcount() < static_cast<std::streamsize>(sizeof(header)))
    {
        if (errorMessage) *errorMessage = "Le fichier .tsa est corrompu ou incomplet (en-tête tronqué).";
        return false;
    }

    if (header.magic != TSA_FILE_MAGIC)
    {
        if (errorMessage) *errorMessage = "Ce fichier n'est pas un fichier de projet TSA valide (signature magique invalide).";
        return false;
    }

    if (header.versionMajor > TSA_FORMAT_VERSION_MAJOR)
    {
        if (errorMessage)
        {
            *errorMessage = "Ce projet TSA a été créé avec une version plus récente du logiciel (v" +
                            std::to_string(header.versionMajor) + "." + std::to_string(header.versionMinor) +
                            ") et ne peut pas être ouvert par cette version.";
        }
        return false;
    }

    return true;
}

bool TSAFileReader::loadFromFile(const std::string& filePath,
                                TSA::Model::Model& model,
                                TSA::Grid::GridManager* gridManager,
                                const std::string& password,
                                std::string* outProjectName,
                                std::string* outAuthor,
                                QImage* outThumbnail,
                                std::string* errorMessage)
{
    // 1. Ouvrir le fichier
    std::ifstream in(filePath, std::ios::binary | std::ios::ate);
    if (!in.is_open())
    {
        if (errorMessage) *errorMessage = "Fichier introuvable : " + filePath;
        return false;
    }

    std::streamsize totalSize = in.tellg();
    in.seekg(0, std::ios::beg);

    if (totalSize < static_cast<std::streamsize>(sizeof(TSAFileHeader)))
    {
        if (errorMessage) *errorMessage = "Fichier .tsa tronqué ou corrompu (taille inférieure à l'en-tête).";
        return false;
    }

    TSAFileHeader header;
    in.read(reinterpret_cast<char*>(&header), sizeof(header));

    if (header.magic != TSA_FILE_MAGIC)
    {
        if (errorMessage) *errorMessage = "Format de fichier invalide (signature TSAF non reconnue).";
        return false;
    }

    if (header.versionMajor > TSA_FORMAT_VERSION_MAJOR)
    {
        if (errorMessage)
        {
            *errorMessage = "Ce projet TSA a été créé avec une version plus récente (Format v" +
                            std::to_string(header.versionMajor) + "." + std::to_string(header.versionMinor) +
                            "). Veuillez mettre à jour TSA.";
        }
        return false;
    }

    size_t payloadSize = static_cast<size_t>(totalSize - sizeof(TSAFileHeader));
    if (payloadSize > MAX_SAFE_PAYLOAD_SIZE)
    {
        if (errorMessage) *errorMessage = "Taille du payload excessive (dépassement limite de sécurité de 1 Go).";
        return false;
    }

    std::vector<uint8_t> rawPayload(payloadSize);
    if (payloadSize > 0)
    {
        in.read(reinterpret_cast<char*>(rawPayload.data()), payloadSize);
        if (in.gcount() < static_cast<std::streamsize>(payloadSize))
        {
            if (errorMessage) *errorMessage = "Erreur de lecture : données du payload incomplètes.";
            return false;
        }
    }
    in.close();

    // 2. Vérification intégrité CRC32
    uint32_t expectedCrc = computeCRC32(rawPayload.data(), rawPayload.size());
    if (header.checksumCRC32 != 0 && header.checksumCRC32 != expectedCrc)
    {
        if (errorMessage) *errorMessage = "Somme de contrôle CRC32 invalide. Le fichier est altéré ou corrompu.";
        return false;
    }

    // 3. Déchiffrement éventuel
    std::vector<uint8_t> decryptedData;
    if (header.flags & FLAG_ENCRYPTED)
    {
        if (!m_encryptor)
        {
            if (errorMessage) *errorMessage = "Ce fichier est protégé par mot de passe mais aucun déchiffreur n'est disponible.";
            return false;
        }
        if (!m_encryptor->decrypt(rawPayload.data(), rawPayload.size(), decryptedData, password))
        {
            if (errorMessage) *errorMessage = "Échec du déchiffrement (mot de passe incorrect ou corrompu).";
            return false;
        }
    }
    else
    {
        decryptedData = std::move(rawPayload);
    }

    // 4. Décompression éventuelle
    std::vector<uint8_t> uncompressedData;
    if (header.flags & FLAG_COMPRESSED)
    {
        if (!m_compressor)
        {
            if (errorMessage) *errorMessage = "Ce fichier est compressé mais aucun décompresseur n'est configuré.";
            return false;
        }
        if (!m_compressor->decompress(decryptedData.data(), decryptedData.size(), uncompressedData))
        {
            if (errorMessage) *errorMessage = "Échec de décompression du payload .tsa (flux zlib corrompu).";
            return false;
        }
    }
    else
    {
        uncompressedData = std::move(decryptedData);
    }

    // 5. Désérialisation et parsing des Chunks
    return parsePayload(uncompressedData.data(), uncompressedData.size(), model, gridManager, outProjectName, outAuthor, outThumbnail, errorMessage);
}

bool TSAFileReader::parsePayload(const uint8_t* data, size_t size,
                                 TSA::Model::Model& model,
                                 TSA::Grid::GridManager* gridManager,
                                 std::string* outProjectName,
                                 std::string* outAuthor,
                                 QImage* outThumbnail,
                                 std::string* errorMessage)
{
    size_t offset = 0;

    std::map<int, TSA::Model::Node> loadedNodes;
    std::map<int, TSA::Model::Beam> loadedBeams;
    std::map<int, TSA::Model::Column> loadedColumns;
    std::map<int, TSA::Model::Slab> loadedSlabs;
    std::map<int, TSA::Model::Wall> loadedWalls;
    std::map<int, TSA::Model::Foundation> loadedFoundations;
    std::map<int, TSA::Model::TrussMember> loadedTrussMembers;

    while (offset + sizeof(TSAChunkHeader) <= size)
    {
        TSAChunkHeader ch;
        std::memcpy(&ch, data + offset, sizeof(ch));
        offset += sizeof(ch);

        if (offset + ch.chunkSize > size)
        {
            if (errorMessage) *errorMessage = "Chunk tronqué ou corrompu dans le fichier .tsa.";
            return false;
        }

        const uint8_t* chunkBytes = data + offset;
        size_t chunkLen = ch.chunkSize;

        switch (ch.chunkId)
        {
        case CHUNK_PROJ:
            if (!readProjectChunk(chunkBytes, chunkLen, outProjectName, outAuthor, errorMessage)) return false;
            break;
        case CHUNK_THMB:
            readThumbnailChunk(chunkBytes, chunkLen, outThumbnail, errorMessage);
            break;
        case CHUNK_COOR:
            if (model.coordinateSystem())
            {
                if (!readCoordinateChunk(chunkBytes, chunkLen, model.coordinateSystem(), errorMessage)) return false;
            }
            break;
        case CHUNK_GRID:
            if (gridManager)
            {
                if (!readGridChunk(chunkBytes, chunkLen, gridManager, errorMessage)) return false;
            }
            break;
        case CHUNK_NODE:
            if (!readNodeChunk(chunkBytes, chunkLen, ch.elementCount, loadedNodes, errorMessage)) return false;
            break;
        case CHUNK_BARS:
            if (!readBarChunk(chunkBytes, chunkLen, ch.elementCount, loadedBeams, errorMessage)) return false;
            break;
        case CHUNK_COLS:
            if (!readColumnChunk(chunkBytes, chunkLen, ch.elementCount, loadedColumns, errorMessage)) return false;
            break;
        case CHUNK_SLAB:
            if (!readSlabChunk(chunkBytes, chunkLen, ch.elementCount, loadedSlabs, errorMessage)) return false;
            break;
        case CHUNK_WALL:
            if (!readWallChunk(chunkBytes, chunkLen, ch.elementCount, loadedWalls, errorMessage)) return false;
            break;
        case CHUNK_FNDN:
            if (!readFoundationChunk(chunkBytes, chunkLen, ch.elementCount, loadedFoundations, errorMessage)) return false;
            break;
        case CHUNK_TRUS:
            if (!readTrussChunk(chunkBytes, chunkLen, ch.elementCount, loadedTrussMembers, errorMessage)) return false;
            break;
        default:
            // Chunk inconnu (version future) : ignoré en toute sécurité grâce à chunkSize
            break;
        }

        offset += ch.chunkSize;
    }

    // 6. Injection atomique dans le modèle structural existant
    TSA::Model::Model::ModelStateSnapshot snapshot;
    snapshot.nodes = std::move(loadedNodes);
    snapshot.beams = std::move(loadedBeams);
    snapshot.columns = std::move(loadedColumns);
    snapshot.slabs = std::move(loadedSlabs);
    snapshot.walls = std::move(loadedWalls);
    snapshot.foundations = std::move(loadedFoundations);
    snapshot.trussMembers = std::move(loadedTrussMembers);

    // Calcul des identifiants suivants
    int maxN = 0, maxB = 0, maxC = 0, maxS = 0, maxW = 0, maxF = 0, maxT = 0;
    for (const auto& [id, _] : snapshot.nodes) maxN = std::max(maxN, id);
    for (const auto& [id, _] : snapshot.beams) maxB = std::max(maxB, id);
    for (const auto& [id, _] : snapshot.columns) maxC = std::max(maxC, id);
    for (const auto& [id, _] : snapshot.slabs) maxS = std::max(maxS, id);
    for (const auto& [id, _] : snapshot.walls) maxW = std::max(maxW, id);
    for (const auto& [id, _] : snapshot.foundations) maxF = std::max(maxF, id);
    for (const auto& [id, _] : snapshot.trussMembers) maxT = std::max(maxT, id);

    snapshot.nextNodeId = maxN + 1;
    snapshot.nextBeamId = maxB + 1;
    snapshot.nextColumnId = maxC + 1;
    snapshot.nextSlabId = maxS + 1;
    snapshot.nextWallId = maxW + 1;
    snapshot.nextFoundationId = maxF + 1;
    snapshot.nextTrussMemberId = maxT + 1;
    snapshot.actionName = "Chargement Projet .tsa";

    // Application dans le modèle -> déclenche automatiquement onModelCleared() chez tous les observateurs (OccView, ModelTree)
    model.restoreSnapshot(snapshot);

    return true;
}

bool TSAFileReader::readProjectChunk(const uint8_t* data, size_t size, std::string* outProjectName, std::string* outAuthor, std::string* /*errorMessage*/)
{
    size_t off = 0;
    std::string projName, authorName, units;
    double scale = 1.0;
    readString(data, size, off, projName);
    readString(data, size, off, authorName);
    readString(data, size, off, units);
    readDouble(data, size, off, scale);

    if (outProjectName) *outProjectName = projName;
    if (outAuthor) *outAuthor = authorName;
    return true;
}

bool TSAFileReader::readThumbnailChunk(const uint8_t* data, size_t size, QImage* outThumbnail, std::string* /*errorMessage*/)
{
    if (!outThumbnail) return true;
    size_t off = 0;
    uint32_t w = 0, h = 0, formatType = 0, dataLen = 0;
    if (!readU32(data, size, off, w)) return false;
    if (!readU32(data, size, off, h)) return false;
    if (!readU32(data, size, off, formatType)) return false;
    if (!readU32(data, size, off, dataLen)) return false;
    if (off + dataLen > size) return false;

    QImage img;
    if (img.loadFromData(reinterpret_cast<const uchar*>(data + off), static_cast<int>(dataLen), "PNG"))
    {
        *outThumbnail = img;
        return true;
    }
    return false;
}

bool TSAFileReader::extractThumbnail(const std::string& filePath, QImage& outThumbnail, std::string* errorMessage)
{
    TSAFileHeader header;
    if (!readHeader(filePath, header, errorMessage))
    {
        return false;
    }

    std::ifstream in(filePath, std::ios::binary);
    if (!in.is_open())
    {
        if (errorMessage) *errorMessage = "Fichier introuvable : " + filePath;
        return false;
    }

    in.seekg(header.payloadOffset, std::ios::beg);
    size_t payloadDiskSize = static_cast<size_t>(header.fileSize - header.payloadOffset);
    if (payloadDiskSize == 0 || payloadDiskSize > MAX_SAFE_PAYLOAD_SIZE)
    {
        if (errorMessage) *errorMessage = "Payload invalide dans le fichier .tsa.";
        return false;
    }

    std::vector<uint8_t> diskPayload(payloadDiskSize);
    in.read(reinterpret_cast<char*>(diskPayload.data()), payloadDiskSize);

    std::vector<uint8_t> uncompressed;
    if (header.flags & FLAG_COMPRESSED)
    {
        QtZlibCompressionProvider comp;
        if (!comp.decompress(diskPayload.data(), diskPayload.size(), uncompressed))
        {
            if (errorMessage) *errorMessage = "Échec de décompression pour miniature.";
            return false;
        }
    }
    else
    {
        uncompressed = std::move(diskPayload);
    }

    size_t offset = 0;
    while (offset + sizeof(TSAChunkHeader) <= uncompressed.size())
    {
        TSAChunkHeader ch;
        std::memcpy(&ch, uncompressed.data() + offset, sizeof(ch));
        offset += sizeof(ch);
        if (offset + ch.chunkSize > uncompressed.size()) break;

        if (ch.chunkId == CHUNK_THMB)
        {
            TSAFileReader reader;
            return reader.readThumbnailChunk(uncompressed.data() + offset, ch.chunkSize, &outThumbnail, errorMessage);
        }
        offset += ch.chunkSize;
    }

    if (errorMessage) *errorMessage = "Aucune miniature trouvée.";
    return false;
}

bool TSAFileReader::readCoordinateChunk(const uint8_t* data, size_t size, TSA::Coordinate::CoordinateSystem* cs, std::string* /*errorMessage*/)
{
    if (!cs) return true;
    size_t off = 0;
    std::string json;
    if (!readString(data, size, off, json)) return false;
    cs->deserializeFromJson(json);
    return true;
}

bool TSAFileReader::readGridChunk(const uint8_t* data, size_t size, TSA::Grid::GridManager* gm, std::string* /*errorMessage*/)
{
    if (!gm) return true;
    size_t off = 0;
    std::string json;
    if (!readString(data, size, off, json)) return false;
    gm->deserializeFromJson(json);
    return true;
}

bool TSAFileReader::readNodeChunk(const uint8_t* data, size_t size, uint32_t count, std::map<int, TSA::Model::Node>& nodes, std::string* errorMessage)
{
    if (count > MAX_SAFE_NODES)
    {
        if (errorMessage) *errorMessage = "Nombre de nœuds anormalement élevé dans le fichier.";
        return false;
    }
    size_t off = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        int32_t id = 0;
        std::string name, levelId, color;
        double x = 0, y = 0, z = 0;
        uint8_t supp = 0;

        if (!readI32(data, size, off, id)) return false;
        if (!readString(data, size, off, name)) return false;
        if (!readDouble(data, size, off, x)) return false;
        if (!readDouble(data, size, off, y)) return false;
        if (!readDouble(data, size, off, z)) return false;
        if (!readString(data, size, off, levelId)) return false;
        if (!readU8(data, size, off, supp)) return false;
        if (!readString(data, size, off, color)) return false;

        TSA::Model::Node n(id, x, y, z, levelId, name);
        n.setSupportType(static_cast<TSA::Model::SupportType>(supp));
        n.setColor(color);
        nodes[id] = n;
    }
    return true;
}

bool TSAFileReader::readBarChunk(const uint8_t* data, size_t size, uint32_t count, std::map<int, TSA::Model::Beam>& beams, std::string* errorMessage)
{
    if (count > MAX_SAFE_ELEMENTS)
    {
        if (errorMessage) *errorMessage = "Nombre de barres anormalement élevé dans le fichier.";
        return false;
    }
    size_t off = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        int32_t id = 0, startId = 0, endId = 0;
        std::string name, color;
        uint8_t role = 0, ecc = 0;
        TSA::Model::Section sec;
        TSA::Model::Material mat;
        double rot = 0;

        if (!readI32(data, size, off, id)) return false;
        if (!readString(data, size, off, name)) return false;
        if (!readI32(data, size, off, startId)) return false;
        if (!readI32(data, size, off, endId)) return false;
        if (!readU8(data, size, off, role)) return false;
        if (!deserializeSection(data, size, off, sec)) return false;
        if (!deserializeMaterial(data, size, off, mat)) return false;
        if (!readDouble(data, size, off, rot)) return false;
        if (!readU8(data, size, off, ecc)) return false;

        uint8_t sfx = 0, sfy = 0, sfz = 0, smx = 0, smy = 0, smz = 0;
        if (!readU8(data, size, off, sfx)) return false;
        if (!readU8(data, size, off, sfy)) return false;
        if (!readU8(data, size, off, sfz)) return false;
        if (!readU8(data, size, off, smx)) return false;
        if (!readU8(data, size, off, smy)) return false;
        if (!readU8(data, size, off, smz)) return false;

        uint8_t efx = 0, efy = 0, efz = 0, emx = 0, emy = 0, emz = 0;
        if (!readU8(data, size, off, efx)) return false;
        if (!readU8(data, size, off, efy)) return false;
        if (!readU8(data, size, off, efz)) return false;
        if (!readU8(data, size, off, emx)) return false;
        if (!readU8(data, size, off, emy)) return false;
        if (!readU8(data, size, off, emz)) return false;

        if (!readString(data, size, off, color)) return false;

        TSA::Model::Beam b(id, startId, endId, sec, mat, static_cast<TSA::Model::BarRole>(role), rot, name);
        b.setEccentricity(static_cast<TSA::Model::BarEccentricity>(ecc));

        TSA::Model::EndRelease sr;
        sr.fx = (sfx != 0); sr.fy = (sfy != 0); sr.fz = (sfz != 0);
        sr.mx = (smx != 0); sr.my = (smy != 0); sr.mz = (smz != 0);
        b.setStartRelease(sr);

        TSA::Model::EndRelease er;
        er.fx = (efx != 0); er.fy = (efy != 0); er.fz = (efz != 0);
        er.mx = (emx != 0); er.my = (emy != 0); er.mz = (emz != 0);
        b.setEndRelease(er);

        b.setColor(color);
        beams[id] = b;
    }
    return true;
}

bool TSAFileReader::readColumnChunk(const uint8_t* data, size_t size, uint32_t count, std::map<int, TSA::Model::Column>& columns, std::string* errorMessage)
{
    if (count > MAX_SAFE_ELEMENTS)
    {
        if (errorMessage) *errorMessage = "Nombre de poteaux anormalement élevé dans le fichier.";
        return false;
    }
    size_t off = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        int32_t id = 0, startId = 0, endId = 0;
        std::string name, color;
        TSA::Model::Section sec;
        TSA::Model::Material mat;
        double rot = 0;

        if (!readI32(data, size, off, id)) return false;
        if (!readString(data, size, off, name)) return false;
        if (!readI32(data, size, off, startId)) return false;
        if (!readI32(data, size, off, endId)) return false;
        if (!deserializeSection(data, size, off, sec)) return false;
        if (!deserializeMaterial(data, size, off, mat)) return false;
        if (!readDouble(data, size, off, rot)) return false;
        if (!readString(data, size, off, color)) return false;

        TSA::Model::Column col(id, startId, endId, sec, mat, rot, name);
        col.setColor(color);
        columns[id] = col;
    }
    return true;
}

bool TSAFileReader::readSlabChunk(const uint8_t* data, size_t size, uint32_t count, std::map<int, TSA::Model::Slab>& slabs, std::string* errorMessage)
{
    if (count > MAX_SAFE_ELEMENTS)
    {
        if (errorMessage) *errorMessage = "Nombre de dalles anormalement élevé dans le fichier.";
        return false;
    }
    size_t off = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        int32_t id = 0;
        std::string name, color;
        uint16_t numNodes = 0;
        double thickness = 0.20;
        TSA::Model::Material mat;
        uint8_t slabType = 0;

        if (!readI32(data, size, off, id)) return false;
        if (!readString(data, size, off, name)) return false;
        if (!readU16(data, size, off, numNodes)) return false;

        std::vector<int> nodeIds;
        nodeIds.reserve(numNodes);
        for (uint16_t k = 0; k < numNodes; ++k)
        {
            int32_t nid = 0;
            if (!readI32(data, size, off, nid)) return false;
            nodeIds.push_back(nid);
        }

        if (!readDouble(data, size, off, thickness)) return false;
        if (!deserializeMaterial(data, size, off, mat)) return false;
        if (!readU8(data, size, off, slabType)) return false;
        if (!readString(data, size, off, color)) return false;

        TSA::Model::Slab s(id, nodeIds, thickness, name, static_cast<TSA::Model::SlabType>(slabType));
        s.setMaterial(mat);
        s.setColor(color);
        slabs[id] = s;
    }
    return true;
}

bool TSAFileReader::readWallChunk(const uint8_t* data, size_t size, uint32_t count, std::map<int, TSA::Model::Wall>& walls, std::string* errorMessage)
{
    if (count > MAX_SAFE_ELEMENTS)
    {
        if (errorMessage) *errorMessage = "Nombre de voiles anormalement élevé dans le fichier.";
        return false;
    }
    size_t off = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        int32_t id = 0, startId = 0, endId = 0;
        std::string name, color;
        double h = 3.0, t = 0.20, offVal = 0.0;
        TSA::Model::Material mat;

        if (!readI32(data, size, off, id)) return false;
        if (!readString(data, size, off, name)) return false;
        if (!readI32(data, size, off, startId)) return false;
        if (!readI32(data, size, off, endId)) return false;
        if (!readDouble(data, size, off, h)) return false;
        if (!readDouble(data, size, off, t)) return false;
        if (!readDouble(data, size, off, offVal)) return false;
        if (!deserializeMaterial(data, size, off, mat)) return false;
        if (!readString(data, size, off, color)) return false;

        TSA::Model::Wall w(id, startId, endId, h, t, name);
        w.setOffset(offVal);
        w.setMaterial(mat);
        w.setColor(color);
        walls[id] = w;
    }
    return true;
}

bool TSAFileReader::readFoundationChunk(const uint8_t* data, size_t size, uint32_t count, std::map<int, TSA::Model::Foundation>& foundations, std::string* errorMessage)
{
    if (count > MAX_SAFE_ELEMENTS)
    {
        if (errorMessage) *errorMessage = "Nombre de fondations anormalement élevé dans le fichier.";
        return false;
    }
    size_t off = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        int32_t id = 0, nodeId = 0;
        std::string name, color;
        uint8_t ftype = 0;
        double wa = 1.5, lb = 1.5, hh = 0.5, cap = 250.0;
        TSA::Model::Material mat;

        if (!readI32(data, size, off, id)) return false;
        if (!readString(data, size, off, name)) return false;
        if (!readI32(data, size, off, nodeId)) return false;
        if (!readU8(data, size, off, ftype)) return false;
        if (!readDouble(data, size, off, wa)) return false;
        if (!readDouble(data, size, off, lb)) return false;
        if (!readDouble(data, size, off, hh)) return false;
        if (!deserializeMaterial(data, size, off, mat)) return false;
        if (!readDouble(data, size, off, cap)) return false;
        if (!readString(data, size, off, color)) return false;

        TSA::Model::Foundation f(id, nodeId, wa, lb, hh, name, static_cast<TSA::Model::FoundationType>(ftype));
        f.setMaterial(mat);
        f.setSoilBearingCapacity(cap);
        f.setColor(color);
        foundations[id] = f;
    }
    return true;
}

bool TSAFileReader::readTrussChunk(const uint8_t* data, size_t size, uint32_t count, std::map<int, TSA::Model::TrussMember>& trussMembers, std::string* errorMessage)
{
    if (count > MAX_SAFE_ELEMENTS)
    {
        if (errorMessage) *errorMessage = "Nombre d'éléments de treillis anormalement élevé dans le fichier.";
        return false;
    }
    size_t off = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        int32_t id = 0, startId = 0, endId = 0;
        std::string name, color;
        uint8_t role = 0;
        TSA::Model::Section sec;
        TSA::Model::Material mat;

        if (!readI32(data, size, off, id)) return false;
        if (!readString(data, size, off, name)) return false;
        if (!readI32(data, size, off, startId)) return false;
        if (!readI32(data, size, off, endId)) return false;
        if (!readU8(data, size, off, role)) return false;
        if (!deserializeSection(data, size, off, sec)) return false;
        if (!deserializeMaterial(data, size, off, mat)) return false;
        if (!readString(data, size, off, color)) return false;

        TSA::Model::TrussMember t(id, startId, endId, sec.diameter > 0.0 ? sec.diameter : sec.width, name, static_cast<TSA::Model::TrussMemberRole>(role));
        t.setSection(sec);
        t.setMaterial(mat);
        t.setColor(color);
        trussMembers[id] = t;
    }
    return true;
}

// =============================================================================
// TSAProjectIO
// =============================================================================
bool TSAProjectIO::saveProject(const QString& filePath,
                              const TSA::Model::Model& model,
                              const TSA::Grid::GridManager* gridManager,
                              const QString& projectName,
                              const QString& author,
                              bool compress,
                              const QImage& thumbnail,
                              QString* errorMessage)
{
    TSAFileWriter writer;
    writer.setCompressionEnabled(compress);
    if (!thumbnail.isNull())
    {
        writer.setThumbnail(thumbnail);
    }

    std::string err;
    bool ok = writer.saveToFile(filePath.toStdString(),
                                model,
                                gridManager,
                                projectName.toStdString(),
                                author.toStdString(),
                                &err);
    if (!ok && errorMessage)
    {
        *errorMessage = QString::fromStdString(err);
    }
    return ok;
}

bool TSAProjectIO::loadProject(const QString& filePath,
                              TSA::Model::Model& model,
                              TSA::Grid::GridManager* gridManager,
                              QString* outProjectName,
                              QString* outAuthor,
                              QImage* outThumbnail,
                              QString* errorMessage)
{
    TSAFileReader reader;
    std::string pName, auth, err;

    bool ok = reader.loadFromFile(filePath.toStdString(),
                                  model,
                                  gridManager,
                                  "",
                                  &pName,
                                  &auth,
                                  outThumbnail,
                                  &err);
    if (ok)
    {
        if (outProjectName) *outProjectName = QString::fromStdString(pName);
        if (outAuthor) *outAuthor = QString::fromStdString(auth);
    }
    else
    {
        if (errorMessage) *errorMessage = QString::fromStdString(err);
    }
    return ok;
}

bool TSAProjectIO::saveToFile(const QString& filePath,
                             const TSA::Model::Model& model,
                             const TSA::Grid::GridManager* gridManager,
                             std::string* errorMessage)
{
    return saveToFile(filePath, model, gridManager, QImage(), errorMessage);
}

bool TSAProjectIO::saveToFile(const QString& filePath,
                             const TSA::Model::Model& model,
                             const TSA::Grid::GridManager* gridManager,
                             const QImage& thumbnail,
                             std::string* errorMessage)
{
    QString qErr;
    bool ok = saveProject(filePath, model, gridManager, "", "", true, thumbnail, &qErr);
    if (!ok && errorMessage)
    {
        *errorMessage = qErr.toStdString();
    }
    return ok;
}

bool TSAProjectIO::loadFromFile(const QString& filePath,
                               TSA::Model::Model& model,
                               TSA::Grid::GridManager* gridManager,
                               std::string* errorMessage)
{
    return loadFromFile(filePath, model, gridManager, nullptr, errorMessage);
}

bool TSAProjectIO::loadFromFile(const QString& filePath,
                               TSA::Model::Model& model,
                               TSA::Grid::GridManager* gridManager,
                               QImage* outThumbnail,
                               std::string* errorMessage)
{
    QString qErr;
    bool ok = loadProject(filePath, model, gridManager, nullptr, nullptr, outThumbnail, &qErr);
    if (!ok && errorMessage)
    {
        *errorMessage = qErr.toStdString();
    }
    return ok;
}

bool TSAProjectIO::extractThumbnail(const QString& filePath,
                                   QImage& outThumbnail,
                                   QString* errorMessage)
{
    std::string err;
    bool ok = TSAFileReader::extractThumbnail(filePath.toStdString(), outThumbnail, &err);
    if (!ok && errorMessage)
    {
        *errorMessage = QString::fromStdString(err);
    }
    return ok;
}

bool TSAProjectIO::isTSAFile(const QString& filePath)
{
    TSAFileHeader h;
    return TSAFileReader::readHeader(filePath.toStdString(), h, nullptr);
}

} // namespace TSA::IO
