#pragma once

#include "TSAFileFormat.h"
#include "../Model/Model.h"
#include "../Grid/GridManager.h"

#include <string>
#include <vector>
#include <memory>
#include <QString>
#include <QImage>

namespace TSA::IO
{

// -----------------------------------------------------------------------------
// Fournisseurs de compression
// -----------------------------------------------------------------------------
class ICompressionProvider
{
public:
    virtual ~ICompressionProvider() = default;
    virtual bool compress(const uint8_t* inData, size_t inSize, std::vector<uint8_t>& outData) = 0;
    virtual bool decompress(const uint8_t* inData, size_t inSize, std::vector<uint8_t>& outData) = 0;
};

class QtZlibCompressionProvider : public ICompressionProvider
{
public:
    bool compress(const uint8_t* inData, size_t inSize, std::vector<uint8_t>& outData) override;
    bool decompress(const uint8_t* inData, size_t inSize, std::vector<uint8_t>& outData) override;
};

// -----------------------------------------------------------------------------
// Fournisseurs de chiffrement / protection
// -----------------------------------------------------------------------------
class IEncryptionProvider
{
public:
    virtual ~IEncryptionProvider() = default;
    virtual bool encrypt(const uint8_t* inData, size_t inSize, std::vector<uint8_t>& outData, const std::string& key) = 0;
    virtual bool decrypt(const uint8_t* inData, size_t inSize, std::vector<uint8_t>& outData, const std::string& key) = 0;
};

class StandardEncryptionProvider : public IEncryptionProvider
{
public:
    bool encrypt(const uint8_t* inData, size_t inSize, std::vector<uint8_t>& outData, const std::string& key) override;
    bool decrypt(const uint8_t* inData, size_t inSize, std::vector<uint8_t>& outData, const std::string& key) override;
};

// -----------------------------------------------------------------------------
// Écrivain de fichier .tsa
// -----------------------------------------------------------------------------
class TSAFileWriter
{
public:
    TSAFileWriter();
    ~TSAFileWriter() = default;

    void setCompressionEnabled(bool enabled) { m_useCompression = enabled; }
    void setCompressionProvider(std::shared_ptr<ICompressionProvider> provider) { m_compressor = provider; }

    void setEncryptionEnabled(bool enabled, const std::string& password = "")
    {
        m_useEncryption = enabled;
        m_password = password;
    }
    void setEncryptionProvider(std::shared_ptr<IEncryptionProvider> provider) { m_encryptor = provider; }

    void setThumbnail(const QImage& thumbnail) { m_thumbnail = thumbnail; }

    bool saveToFile(const std::string& filePath,
                    const TSA::Model::Model& model,
                    const TSA::Grid::GridManager* gridManager = nullptr,
                    const std::string& projectName = "Projet TSA",
                    const std::string& author = "TSA User",
                    std::string* errorMessage = nullptr);

private:
    void writeProjectChunk(std::vector<uint8_t>& buffer, const std::string& projectName, const std::string& author);
    void writeThumbnailChunk(std::vector<uint8_t>& buffer, const QByteArray& pngData);
    void writeCoordinateChunk(std::vector<uint8_t>& buffer, const TSA::Coordinate::CoordinateSystem* cs);
    void writeGridChunk(std::vector<uint8_t>& buffer, const TSA::Grid::GridManager* gm);
    void writeNodeChunk(std::vector<uint8_t>& buffer, const std::map<int, TSA::Model::Node>& nodes);
    void writeBarChunk(std::vector<uint8_t>& buffer, const std::map<int, TSA::Model::Beam>& beams);
    void writeColumnChunk(std::vector<uint8_t>& buffer, const std::map<int, TSA::Model::Column>& columns);
    void writeSlabChunk(std::vector<uint8_t>& buffer, const std::map<int, TSA::Model::Slab>& slabs);
    void writeWallChunk(std::vector<uint8_t>& buffer, const std::map<int, TSA::Model::Wall>& walls);
    void writeFoundationChunk(std::vector<uint8_t>& buffer, const std::map<int, TSA::Model::Foundation>& foundations);
    void writeTrussChunk(std::vector<uint8_t>& buffer, const std::map<int, TSA::Model::TrussMember>& trussMembers);
    void writeCableChunk(std::vector<uint8_t>& buffer, const std::map<int, TSA::Model::Cable>& cables);

private:
    bool m_useCompression = true;
    bool m_useEncryption = false;
    std::string m_password;
    QImage m_thumbnail;
    std::shared_ptr<ICompressionProvider> m_compressor;
    std::shared_ptr<IEncryptionProvider> m_encryptor;
};

// -----------------------------------------------------------------------------
// Lecteur de fichier .tsa
// -----------------------------------------------------------------------------
class TSAFileReader
{
public:
    TSAFileReader();
    ~TSAFileReader() = default;

    void setCompressionProvider(std::shared_ptr<ICompressionProvider> provider) { m_compressor = provider; }
    void setEncryptionProvider(std::shared_ptr<IEncryptionProvider> provider) { m_encryptor = provider; }

    bool loadFromFile(const std::string& filePath,
                      TSA::Model::Model& model,
                      TSA::Grid::GridManager* gridManager = nullptr,
                      const std::string& password = "",
                      std::string* outProjectName = nullptr,
                      std::string* outAuthor = nullptr,
                      QImage* outThumbnail = nullptr,
                      std::string* errorMessage = nullptr);

    // Lecture uniquement du Header pour inspection préalable
    static bool readHeader(const std::string& filePath, TSAFileHeader& header, std::string* errorMessage = nullptr);
    static bool extractThumbnail(const std::string& filePath, QImage& outThumbnail, std::string* errorMessage = nullptr);

private:
    bool parsePayload(const uint8_t* data, size_t size,
                      TSA::Model::Model& model,
                      TSA::Grid::GridManager* gridManager,
                      std::string* outProjectName,
                      std::string* outAuthor,
                      QImage* outThumbnail,
                      std::string* errorMessage);

    bool readProjectChunk(const uint8_t* data, size_t size, std::string* outProjectName, std::string* outAuthor, std::string* errorMessage);
    bool readThumbnailChunk(const uint8_t* data, size_t size, QImage* outThumbnail, std::string* errorMessage);
    bool readCoordinateChunk(const uint8_t* data, size_t size, TSA::Coordinate::CoordinateSystem* cs, std::string* errorMessage);
    bool readGridChunk(const uint8_t* data, size_t size, TSA::Grid::GridManager* gm, std::string* errorMessage);
    bool readNodeChunk(const uint8_t* data, size_t size, uint32_t count, std::map<int, TSA::Model::Node>& nodes, std::string* errorMessage);
    bool readBarChunk(const uint8_t* data, size_t size, uint32_t count, std::map<int, TSA::Model::Beam>& beams, std::string* errorMessage);
    bool readColumnChunk(const uint8_t* data, size_t size, uint32_t count, std::map<int, TSA::Model::Column>& columns, std::string* errorMessage);
    bool readSlabChunk(const uint8_t* data, size_t size, uint32_t count, std::map<int, TSA::Model::Slab>& slabs, std::string* errorMessage);
    bool readWallChunk(const uint8_t* data, size_t size, uint32_t count, std::map<int, TSA::Model::Wall>& walls, std::string* errorMessage);
    bool readFoundationChunk(const uint8_t* data, size_t size, uint32_t count, std::map<int, TSA::Model::Foundation>& foundations, std::string* errorMessage);
    bool readTrussChunk(const uint8_t* data, size_t size, uint32_t count, std::map<int, TSA::Model::TrussMember>& trussMembers, std::string* errorMessage);
    bool readCableChunk(const uint8_t* data, size_t size, uint32_t count, std::map<int, TSA::Model::Cable>& cables, std::string* errorMessage);

private:
    std::shared_ptr<ICompressionProvider> m_compressor;
    std::shared_ptr<IEncryptionProvider> m_encryptor;
};

// -----------------------------------------------------------------------------
// Façade unifiée ProjectIO pour l'interface graphique Qt
// -----------------------------------------------------------------------------
class TSAProjectIO
{
public:
    static bool saveProject(const QString& filePath,
                            const TSA::Model::Model& model,
                            const TSA::Grid::GridManager* gridManager = nullptr,
                            const QString& projectName = QString(),
                            const QString& author = QString(),
                            bool compress = true,
                            const QImage& thumbnail = QImage(),
                            QString* errorMessage = nullptr);

    static bool loadProject(const QString& filePath,
                            TSA::Model::Model& model,
                            TSA::Grid::GridManager* gridManager = nullptr,
                            QString* outProjectName = nullptr,
                            QString* outAuthor = nullptr,
                            QImage* outThumbnail = nullptr,
                            QString* errorMessage = nullptr);

    static bool saveToFile(const QString& filePath,
                           const TSA::Model::Model& model,
                           const TSA::Grid::GridManager* gridManager = nullptr,
                           std::string* errorMessage = nullptr);

    static bool saveToFile(const QString& filePath,
                           const TSA::Model::Model& model,
                           const TSA::Grid::GridManager* gridManager,
                           const QImage& thumbnail,
                           std::string* errorMessage = nullptr);

    static bool loadFromFile(const QString& filePath,
                             TSA::Model::Model& model,
                             TSA::Grid::GridManager* gridManager = nullptr,
                             std::string* errorMessage = nullptr);

    static bool loadFromFile(const QString& filePath,
                             TSA::Model::Model& model,
                             TSA::Grid::GridManager* gridManager,
                             QImage* outThumbnail,
                             std::string* errorMessage = nullptr);

    static bool extractThumbnail(const QString& filePath,
                                 QImage& outThumbnail,
                                 QString* errorMessage = nullptr);

    static bool isTSAFile(const QString& filePath);
};

} // namespace TSA::IO
