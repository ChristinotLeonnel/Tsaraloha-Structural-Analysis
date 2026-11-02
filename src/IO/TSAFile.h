#pragma once

#include "../Model/Load/LoadManager.h"
#include "TSAFileFormat.h"
#include "../Model/Model.h"
#include "../ExtensionSystem/ExtensionTypes.h"   // DefinitionReference, MechanicalSnapshot (noms ExtensionSystem)
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

/// Ancien « chiffrement » par XOR avec le mot de passe : LECTURE des fichiers antérieurs à 3.2 seulement, jamais écrit
/// (BUG-167 : mot de passe retrouvé à partir d'octets connus, aucune authentification). Écriture : TSAEncryption.h.
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

    /// Protection par mot de passe (UTF-8, non vide) : contenu chiffré AES-256-GCM, clé dérivée PBKDF2-HMAC-SHA-256
    /// (TSAEncryption.h). Chiffrement impossible : l'enregistrement échoue, jamais un fichier écrit en clair.
    void setEncryptionEnabled(bool enabled, const std::string& password = "")
    {
        m_useEncryption = enabled;
        m_password = password;
    }

    void setThumbnail(const QImage& thumbnail) { m_thumbnail = thumbnail; }

    bool saveToFile(const std::string& filePath,
                    const TSA::Model::Model& model,
                    const TSA::Grid::GridManager* gridManager = nullptr,
                    const std::string& projectName = TSA::Product::kDefaultProjectName,
                    const std::string& author = TSA::Product::kDefaultAuthor,
                    std::string* errorMessage = nullptr);

    /// Contenu persistant d'un instantané (éléments, charges, BIM, références de calcul), octet pour octet comme dans
    /// le fichier, sans en-tête ni compression : comparaison EXACTE de deux états (UndoManager, BUG-127).
    std::vector<uint8_t> snapshotContentBytes(const TSA::Model::Model::ModelStateSnapshot& snapshot);
    /// Contenu comparé par l'historique (UndoManager::sameContent) : snapshotContentBytes, plus les données écrites à part
    /// (ouvertures, surfaces, charges surfaciques, appuis de bord, propriétés des sections), BUG-175. L'empreinte des
    /// résultats (TSAProjectIO::modelFingerprint) n'en dépend pas : les résultats déjà enregistrés restent valides.
    std::vector<uint8_t> undoContentBytes(const TSA::Model::Model::ModelStateSnapshot& snapshot);

private:
    void writeProjectChunk(std::vector<uint8_t>& buffer, const std::string& projectName, const std::string& author);
    void writeThumbnailChunk(std::vector<uint8_t>& buffer, const QByteArray& pngData);
    void writeBimChunk(std::vector<uint8_t>& buffer, const TSA::BIM::BimModel& bim);
    void writeSettingsChunk(std::vector<uint8_t>& buffer, const std::string& json);
    /// Chunk JSON UTF-8 générique (TOPO…).
    void writeJsonChunk(std::vector<uint8_t>& buffer, uint32_t chunkId, const std::string& json);
    void writeCoordinateChunk(std::vector<uint8_t>& buffer, const TSA::Coordinate::CoordinateSystem* cs);
    void writeGridChunk(std::vector<uint8_t>& buffer, const TSA::Grid::GridManager* gm);
    void writeNodeChunk(std::vector<uint8_t>& buffer, const std::map<int, TSA::Model::Node>& nodes);
    void writeSupportChunk(std::vector<uint8_t>& buffer, const std::map<int, TSA::Model::Node>& nodes);
    void writeBarChunk(std::vector<uint8_t>& buffer, const std::map<int, TSA::Model::Beam>& beams);
    void writeColumnChunk(std::vector<uint8_t>& buffer, const std::map<int, TSA::Model::Column>& columns);
    void writeSlabChunk(std::vector<uint8_t>& buffer, const std::map<int, TSA::Model::Slab>& slabs);
    void writeWallChunk(std::vector<uint8_t>& buffer, const std::map<int, TSA::Model::Wall>& walls);
    void writeFoundationChunk(std::vector<uint8_t>& buffer, const std::map<int, TSA::Model::Foundation>& foundations);
    void writeTrussChunk(std::vector<uint8_t>& buffer, const std::map<int, TSA::Model::TrussMember>& trussMembers);
    void writeCableChunk(std::vector<uint8_t>& buffer, const std::map<int, TSA::Model::Cable>& cables);
    void writeSnapshotChunk(std::vector<uint8_t>& buffer,
                            const std::map<std::string, TSA::ExtensionSystem::MechanicalSnapshot>& snapshots,
                            const std::map<std::string, TSA::ExtensionSystem::DefinitionReference>& references);
    void writeLoadChunk(std::vector<uint8_t>& buffer, const TSA::Model::LoadManager::LoadSnapshot& loads);

private:
    bool m_useCompression = true;
    bool m_useEncryption = false;
    std::string m_password;
    QImage m_thumbnail;
    std::shared_ptr<ICompressionProvider> m_compressor;
};

/// Raison d'un échec de lecture utile à l'interface (demander ou redemander le mot de passe).
enum class LoadFailure
{
    None,
    PasswordRequired,        ///< projet protégé, aucun mot de passe donné
    WrongPassword,           ///< mot de passe refusé (ou contenu chiffré modifié : indiscernable, par construction)
    EncryptionUnavailable,   ///< projet protégé, chiffrement indisponible sur ce poste
    Other
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
    void setEncryptionProvider(std::shared_ptr<IEncryptionProvider> provider) { m_encryptor = provider; }   ///< ancien XOR (lecture)

    /// Raison du dernier échec de loadFromFile (None après un succès).
    LoadFailure lastFailure() const { return m_lastFailure; }

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
    /// Format ≥ 1.2 : aperçu non compressé placé après le payload (lecture directe, sans décompression).
    static bool extractPreviewBlock(const std::string& filePath, QImage& outImage);

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
    bool readBimChunk(const uint8_t* data, size_t size, TSA::BIM::BimModel& outBim, std::string* errorMessage);
    bool readNodeChunk(const uint8_t* data, size_t size, uint32_t count, std::map<int, TSA::Model::Node>& nodes, std::string* errorMessage);
    bool readSupportChunk(const uint8_t* data, size_t size, uint32_t count, std::map<int, TSA::Model::Node>& nodes, std::string* errorMessage);
    bool readBarChunk(const uint8_t* data, size_t size, uint32_t count, std::map<int, TSA::Model::Beam>& beams, std::string* errorMessage);
    bool readColumnChunk(const uint8_t* data, size_t size, uint32_t count, std::map<int, TSA::Model::Column>& columns, std::string* errorMessage);
    bool readSlabChunk(const uint8_t* data, size_t size, uint32_t count, std::map<int, TSA::Model::Slab>& slabs, std::string* errorMessage);
    bool readWallChunk(const uint8_t* data, size_t size, uint32_t count, std::map<int, TSA::Model::Wall>& walls, std::string* errorMessage);
    bool readFoundationChunk(const uint8_t* data, size_t size, uint32_t count, std::map<int, TSA::Model::Foundation>& foundations, std::string* errorMessage);
    bool readTrussChunk(const uint8_t* data, size_t size, uint32_t count, std::map<int, TSA::Model::TrussMember>& trussMembers, std::string* errorMessage);
    bool readCableChunk(const uint8_t* data, size_t size, uint32_t count, std::map<int, TSA::Model::Cable>& cables, std::string* errorMessage);
    bool readSnapshotChunk(const uint8_t* data, size_t size, uint32_t count,
                           std::map<std::string, TSA::ExtensionSystem::MechanicalSnapshot>& snapshots,
                           std::map<std::string, TSA::ExtensionSystem::DefinitionReference>& references,
                           std::string* errorMessage);
    bool readLoadChunk(const uint8_t* data, size_t size, TSA::Model::LoadManager::LoadSnapshot& loads,
                       std::string* errorMessage);

private:
    std::shared_ptr<ICompressionProvider> m_compressor;
    std::shared_ptr<IEncryptionProvider> m_encryptor;
    LoadFailure m_lastFailure = LoadFailure::None;
    uint16_t m_formatMajor = 0;   ///< version majeure du fichier en cours de lecture (1 : sections migrées)
};

// -----------------------------------------------------------------------------
// Façade unifiée ProjectIO pour l'interface graphique Qt
// -----------------------------------------------------------------------------
class TSAProjectIO
{
public:
    /// Empreinte du contenu persistant du modèle (SHA-256 hexadécimal de snapshotContentBytes, éléments, charges, BIM,
    /// références de calcul) : relie des résultats enregistrés au modèle qu'ils décrivent (bloc RSLT, décision D3).
    static std::string modelFingerprint(const TSA::Model::Model& model);

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

    // --- Projets protégés par mot de passe (BUG-167, format 3.2) ---------------------------------------------------
    enum class OpenStatus { Ok, PasswordRequired, WrongPassword, Failed };

    /// Le fichier est protégé par mot de passe (en-tête lu, rien déchiffré).
    static bool isPasswordProtected(const QString& filePath);
    /// Chiffrement authentifié disponible sur ce poste (Windows : toujours ; ailleurs : OpenSSL trouvé à la compilation).
    static bool encryptionAvailable();
    /// Octets du mot de passe : UTF-8 de la forme normalisée NFC (même mot de passe quelle que soit la saisie des accents).
    static std::string passwordBytes(const QString& password);

    /// Enregistre ; `password` vide : fichier non protégé (comme saveProject).
    static bool saveProtectedProject(const QString& filePath,
                                     const TSA::Model::Model& model,
                                     const TSA::Grid::GridManager* gridManager,
                                     const QString& projectName,
                                     const QString& author,
                                     const QString& password,
                                     const QImage& thumbnail = QImage(),
                                     QString* errorMessage = nullptr);

    /// Ouvre ; projet protégé : `password` exigé (PasswordRequired s'il est vide, WrongPassword s'il est refusé).
    static OpenStatus loadProtectedProject(const QString& filePath,
                                           TSA::Model::Model& model,
                                           TSA::Grid::GridManager* gridManager,
                                           const QString& password,
                                           QString* outProjectName = nullptr,
                                           QString* outAuthor = nullptr,
                                           QImage* outThumbnail = nullptr,
                                           QString* errorMessage = nullptr);
};

} // namespace TSA::IO
