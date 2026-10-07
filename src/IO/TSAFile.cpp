#include "TSAFile.h"
#include "TSAFile_BinaryUtils.h"
#include "TSAPreviewGenerator.h"
#include "TSAPreviewBlock.h"
#include "../Model/MaterialLibrary.h"
#include "../Standards/ModelValidator.h"
#include "../Diagnostics/Logger.h"

#include <QSaveFile>
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
    writeSupportChunk(payload, model.nodes());
    writeBarChunk(payload, model.beams());
    writeColumnChunk(payload, model.columns());
    writeSlabChunk(payload, model.slabs());
    writeWallChunk(payload, model.walls());
    writeFoundationChunk(payload, model.foundations());
    writeTrussChunk(payload, model.trussMembers());
    writeCableChunk(payload, model.cables());
    writeLoadChunk(payload, model.loadManager().createSnapshot());
    writeBimChunk(payload, model.bim());
    if (!model.analysisSettingsJson().empty())
    {
        writeSettingsChunk(payload, model.analysisSettingsJson());
    }

    // Snapshots mécaniques de calcul & références d'extensions (Phase 8)
    std::map<std::string, TSA::ExtensionSystem::MechanicalSnapshot> snapshotsToSave = model.calculationSnapshots();
    std::map<std::string, TSA::ExtensionSystem::DefinitionReference> referencesToSave = model.definitionReferences();

    auto captureMat = [&](const TSA::Model::Material& mat) {
        std::string key = "material:" + mat.name;
        if (snapshotsToSave.find(key) == snapshotsToSave.end())
        {
            TSA::ExtensionSystem::MechanicalSnapshot snap;
            snap.youngModulus = mat.E;
            snap.poissonRatio = mat.nu;
            snap.density = mat.density;
            snap.characteristicStrength = mat.fk;
            snap.yieldStrength = mat.fk;
            snap.thermalCoeff = mat.thermalCoeff;
            snapshotsToSave[key] = snap;

            TSA::ExtensionSystem::DefinitionReference ref;
            ref.libraryId = "org.tsaraloha.tsalib";
            ref.libraryVersion = TSA::ExtensionSystem::SemanticVersion(1, 0, 0);
            ref.definitionId = mat.name;
            ref.definitionVersion = TSA::ExtensionSystem::SemanticVersion(1, 0, 0);
            referencesToSave[key] = ref;
        }
    };

    for (const auto& [id, b] : model.beams()) captureMat(b.material());
    for (const auto& [id, c] : model.columns()) captureMat(c.material());
    for (const auto& [id, s] : model.slabs()) captureMat(s.material());
    for (const auto& [id, w] : model.walls()) captureMat(w.material());
    for (const auto& [id, f] : model.foundations()) captureMat(f.material());
    for (const auto& [id, t] : model.trussMembers()) captureMat(t.material());

    for (const auto& [id, cable] : model.cables())
    {
        const auto& def = cable.definition();
        std::string key = "cable:" + (def.id().empty() ? def.name() : def.id());
        if (snapshotsToSave.find(key) == snapshotsToSave.end())
        {
            TSA::ExtensionSystem::MechanicalSnapshot snap;
            snap.youngModulus = def.elasticModulus();
            snap.poissonRatio = 0.30;
            snap.density = def.density();
            snap.characteristicStrength = def.characteristicStrength();
            snap.yieldStrength = def.ultimateStrength();
            snap.thermalCoeff = 1.2e-5;
            snapshotsToSave[key] = snap;

            TSA::ExtensionSystem::DefinitionReference ref;
            ref.libraryId = "org.tsaraloha.tsalib";
            ref.libraryVersion = TSA::ExtensionSystem::SemanticVersion(1, 0, 0);
            ref.definitionId = def.id().empty() ? def.name() : def.id();
            ref.definitionVersion = TSA::ExtensionSystem::SemanticVersion(1, 0, 0);
            referencesToSave[key] = ref;
        }
    }

    if (!snapshotsToSave.empty())
    {
        writeSnapshotChunk(payload, snapshotsToSave, referencesToSave);
    }

    uint64_t uncompressedSize = payload.size();

    // 2. Traitement Compression
    uint32_t flags = FLAG_NONE;
    if (!pngBytes.isEmpty())
    {
        flags |= FLAG_HAS_THUMBNAIL;
        // Aperçu lisible par l'Explorateur sans décompression — jamais pour un fichier protégé par
        // mot de passe (l'image du modèle ne doit pas être lisible en clair).
        const bool willEncrypt = m_useEncryption && !m_password.empty() && m_encryptor;
        if (!willEncrypt && static_cast<uint64_t>(pngBytes.size()) <= TSA_PREVIEW_MAX_BYTES)
            flags |= FLAG_HAS_PREVIEW_BLOCK;
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

    // 5. Préparation du Header (272 octets, packé)
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

    // 6. Écriture atomique : QSaveFile écrit dans un fichier temporaire du même dossier puis le
    //    renomme sur la cible à commit(). Auparavant le fichier existant était tronqué puis réécrit
    //    en place : un crash, un disque plein ou une coupure pendant la sauvegarde détruisait le
    //    projet de l'utilisateur.
    QSaveFile out(QString::fromStdString(filePath));
    if (!out.open(QIODevice::WriteOnly))
    {
        if (errorMessage) *errorMessage = "Impossible de créer le fichier .tsa : " + filePath + " (" + out.errorString().toStdString() + ")";
        return false;
    }

    bool written = out.write(reinterpret_cast<const char*>(&header), sizeof(header)) == static_cast<qint64>(sizeof(header));
    if (written && !processedData.empty())
    {
        written = out.write(reinterpret_cast<const char*>(processedData.data()), static_cast<qint64>(processedData.size())) ==
                  static_cast<qint64>(processedData.size());
    }
    // Bloc d'aperçu (format 1.2) après le payload : header.fileSize marque la fin du payload.
    if (written && (flags & FLAG_HAS_PREVIEW_BLOCK))
    {
        QImage previewImage;
        previewImage.loadFromData(pngBytes, "PNG");
        TSAPreviewBlockHeader block;
        block.width = static_cast<uint32_t>(previewImage.width());
        block.height = static_cast<uint32_t>(previewImage.height());
        block.dataSize = static_cast<uint32_t>(pngBytes.size());
        written = out.write(reinterpret_cast<const char*>(&block), sizeof(block)) == static_cast<qint64>(sizeof(block))
               && out.write(pngBytes) == pngBytes.size();
    }
    if (!written)
    {
        out.cancelWriting();
        if (errorMessage) *errorMessage = "Erreur d'écriture du fichier .tsa (" + out.errorString().toStdString() + "). Le fichier existant est conservé.";
        return false;
    }
    if (!out.commit())
    {
        if (errorMessage) *errorMessage = "Erreur lors de la finalisation du fichier .tsa (" + out.errorString().toStdString() + "). Le fichier existant est conservé.";
        return false;
    }

    return true;
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

    // Fichiers ≥ 1.2 : un bloc d'aperçu suit le payload, dont la fin est header.fileSize.
    // Fichiers antérieurs : header.fileSize vaut la taille totale (comportement inchangé).
    const uint64_t payloadEnd = (header.fileSize >= sizeof(TSAFileHeader) && header.fileSize <= static_cast<uint64_t>(totalSize))
                                    ? header.fileSize
                                    : static_cast<uint64_t>(totalSize);
    size_t payloadSize = static_cast<size_t>(payloadEnd - sizeof(TSAFileHeader));
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

bool TSAFileReader::extractPreviewBlock(const std::string& filePath, QImage& outImage)
{
    std::ifstream in(filePath, std::ios::binary | std::ios::ate);
    if (!in.is_open()) return false;
    const uint64_t length = static_cast<uint64_t>(in.tellg());
    if (length < sizeof(TSAFileHeader)) return false;
    in.seekg(0, std::ios::beg);
    TSAFileHeader header;
    if (!in.read(reinterpret_cast<char*>(&header), sizeof(header))) return false;
    uint64_t blockOffset = 0;
    if (!locatePreviewBlock(header, length, &blockOffset)) return false;
    in.seekg(static_cast<std::streamoff>(blockOffset), std::ios::beg);
    TSAPreviewBlockHeader block;
    if (!in.read(reinterpret_cast<char*>(&block), sizeof(block))) return false;
    TSAPreviewLocation where;
    if (!validatePreviewBlock(block, blockOffset, length, &where)) return false;
    QByteArray png(static_cast<qsizetype>(where.imageSize), Qt::Uninitialized);
    in.seekg(static_cast<std::streamoff>(where.imageOffset), std::ios::beg);
    if (!in.read(png.data(), png.size())) return false;
    return outImage.loadFromData(png, "PNG");
}

bool TSAFileReader::extractThumbnail(const std::string& filePath, QImage& outThumbnail, std::string* errorMessage)
{
    // Format ≥ 1.2 : bloc d'aperçu direct (pas de décompression).
    if (extractPreviewBlock(filePath, outThumbnail)) return true;

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
    std::map<int, TSA::Model::Cable> loadedCables;
    std::map<std::string, TSA::ExtensionSystem::MechanicalSnapshot> loadedSnapshots;
    std::map<std::string, TSA::ExtensionSystem::DefinitionReference> loadedReferences;
    TSA::Model::LoadManager::LoadSnapshot loadedLoads;
    bool hasLoadChunk = false;
    TSA::BIM::BimModel loadedBim;
    std::string loadedSettings;

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
        case CHUNK_SUPP:
            if (!readSupportChunk(chunkBytes, chunkLen, ch.elementCount, loadedNodes, errorMessage)) return false;
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
        case CHUNK_CABL:
            if (!readCableChunk(chunkBytes, chunkLen, ch.elementCount, loadedCables, errorMessage)) return false;
            break;
        case CHUNK_SNAP:
            if (!readSnapshotChunk(chunkBytes, chunkLen, ch.elementCount, loadedSnapshots, loadedReferences, errorMessage)) return false;
            break;
        case CHUNK_LOAD:
            if (!readLoadChunk(chunkBytes, chunkLen, loadedLoads, errorMessage)) return false;
            hasLoadChunk = true;
            break;
        case CHUNK_BIMM:
            // Facultatif : sans lui (fichier ≤ 1.2), les GlobalId sont attribués au chargement
            readBimChunk(chunkBytes, chunkLen, loadedBim, errorMessage);
            break;
        case CHUNK_SETT:
            // Facultatif (format ≥ 1.4) : sans lui, réglages d'analyse par défaut. Le contenu est
            // validé à la relecture par AnalysisContext::fromJson (lecture tolérante).
            loadedSettings.assign(reinterpret_cast<const char*>(chunkBytes), chunkLen);
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
    snapshot.cables = std::move(loadedCables);
    if (hasLoadChunk)
    {
        snapshot.loadSnapshot = std::move(loadedLoads);
    }
    else
    {
        // Fichier antérieur au format 1.1 (aucun chunk LOAD) : cas de charge et combinaisons
        // Eurocodes par défaut, comme pour un nouveau projet. Auparavant le modèle chargé se
        // retrouvait sans AUCUN cas de charge (snapshot de charges vide).
        snapshot.loadSnapshot = TSA::Model::LoadManager().createSnapshot();
    }
    snapshot.calculationSnapshots = std::move(loadedSnapshots);
    snapshot.definitionReferences = std::move(loadedReferences);
    snapshot.bim = std::move(loadedBim);

    // Calcul des identifiants suivants
    int maxN = 0, maxB = 0, maxC = 0, maxS = 0, maxW = 0, maxF = 0, maxT = 0, maxCab = 0;
    for (const auto& [id, _] : snapshot.nodes) maxN = std::max(maxN, id);
    for (const auto& [id, _] : snapshot.beams) maxB = std::max(maxB, id);
    for (const auto& [id, _] : snapshot.columns) maxC = std::max(maxC, id);
    for (const auto& [id, _] : snapshot.slabs) maxS = std::max(maxS, id);
    for (const auto& [id, _] : snapshot.walls) maxW = std::max(maxW, id);
    for (const auto& [id, _] : snapshot.foundations) maxF = std::max(maxF, id);
    for (const auto& [id, _] : snapshot.trussMembers) maxT = std::max(maxT, id);
    for (const auto& [id, _] : snapshot.cables) maxCab = std::max(maxCab, id);

    snapshot.nextNodeId = maxN + 1;
    snapshot.nextBeamId = maxB + 1;
    snapshot.nextColumnId = maxC + 1;
    snapshot.nextSlabId = maxS + 1;
    snapshot.nextWallId = maxW + 1;
    snapshot.nextFoundationId = maxF + 1;
    snapshot.nextTrussMemberId = maxT + 1;
    snapshot.nextCableId = maxCab + 1;
    snapshot.actionName = "Chargement Projet .tsa";

    // Application dans le modèle -> déclenche automatiquement onModelCleared() chez tous les observateurs (OccView, ModelTree)
    model.restoreSnapshot(snapshot);
    model.setAnalysisSettingsJson(loadedSettings);

    // Validation normative post-chargement (ISO/IEC 25010 - Intégrité et robustesse)
    auto report = TSA::Standards::ModelValidator::validate(model);
    if (!report.isValid())
    {
        TSA_LOG_WARN("TSAFileReader", "IntegrityWarning",
                     "Le projet chargé comporte " + std::to_string(report.errorCount()) + " anomalie(s) normative(s).");
        for (const auto& issue : report.issues())
        {
            if (issue.severity == TSA::Standards::ValidationSeverity::Error)
            {
                TSA_LOG_WARN("TSAFileReader", "ModelValidationError",
                             "[" + issue.category + "] " + issue.message + " (Entité: " + std::to_string(issue.entityId) + ")");
            }
        }
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
