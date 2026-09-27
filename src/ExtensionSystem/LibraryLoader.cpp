#include "LibraryLoader.h"
#include "LibraryRegistry.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonObject>

namespace TSA::ExtensionSystem
{

LibraryLoader::LibraryLoader(LibraryRegistry* registry)
    : m_registry(registry ? registry : &LibraryRegistry::instance())
{
}

bool LibraryLoader::loadExtension(const QString& extensionDirPath, ExtensionManifest* outManifest, ValidationResult* outResult)
{
    ValidationResult val = m_validator.validateExtensionDirectory(extensionDirPath);
    if (!val.valid)
    {
        if (outResult) *outResult = val;
        return false;
    }

    QDir dir(extensionDirPath);
    QFile manifestFile(dir.filePath("manifest.json"));
    if (!manifestFile.open(QIODevice::ReadOnly))
    {
        if (outResult) outResult->addError("Impossible d'ouvrir manifest.json");
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(manifestFile.readAll());
    std::string parseErr;
    auto manifest = ExtensionManifest::fromJson(doc.object(), &parseErr);
    if (!manifest)
    {
        if (outResult) outResult->addError(parseErr);
        return false;
    }
    if (outManifest) *outManifest = *manifest;

    m_loadedExtensions[manifest->id] = extensionDirPath;

    // Indexation et chargement des sous-dossiers par catégorie
    static const std::vector<std::pair<QString, std::string>> categories = {
        { "Materials", "Materials" },
        { "Sections", "Sections" },
        { "Profiles", "Sections" },
        { "Cables", "Cables" },
        { "Standards", "Standards" }
    };

    for (const auto& [folderName, cat] : categories)
    {
        QString subDirPath = dir.filePath(folderName);
        if (QDir(subDirPath).exists())
        {
            scanCategoryDirectory(subDirPath, cat, manifest->id);
        }
    }

    // Chargement effectif de tous les fichiers indexés pour cette extension
    for (auto& [id, item] : m_fileIndex)
    {
        if (!item.isLoaded && item.filePath.startsWith(extensionDirPath))
        {
            loadDefinitionById(id);
        }
    }

    if (outResult) *outResult = val;
    return true;
}

bool LibraryLoader::indexExtension(const QString& extensionDirPath, ExtensionManifest* outManifest)
{
    QDir dir(extensionDirPath);
    QFile manifestFile(dir.filePath("manifest.json"));
    if (!manifestFile.open(QIODevice::ReadOnly)) return false;

    QJsonDocument doc = QJsonDocument::fromJson(manifestFile.readAll());
    auto manifest = ExtensionManifest::fromJson(doc.object());
    if (!manifest) return false;
    if (outManifest) *outManifest = *manifest;

    m_loadedExtensions[manifest->id] = extensionDirPath;

    static const std::vector<std::pair<QString, std::string>> categories = {
        { "Materials", "Materials" },
        { "Sections", "Sections" },
        { "Profiles", "Sections" },
        { "Cables", "Cables" },
        { "Standards", "Standards" }
    };

    for (const auto& [folderName, cat] : categories)
    {
        QString subDirPath = dir.filePath(folderName);
        if (QDir(subDirPath).exists())
        {
            scanCategoryDirectory(subDirPath, cat, manifest->id);
        }
    }
    return true;
}

void LibraryLoader::scanCategoryDirectory(const QString& dirPath, const std::string& category, const std::string& /*libraryId*/)
{
    QDirIterator it(dirPath, QStringList() << "*.json", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        QString filePath = it.next();
        QFileInfo fi(filePath);
        std::string defId = fi.baseName().toLower().toStdString();

        // Si l'id n'est pas qualifié, on préfixe par catégorie simplifiée
        if (defId.find('.') == std::string::npos)
        {
            if (category == "Materials") defId = "material." + defId;
            else if (category == "Sections") defId = "section." + defId;
            else if (category == "Cables") defId = "cable." + defId;
        }

        DefinitionFileIndex entry;
        entry.id = defId;
        entry.category = category;
        entry.filePath = filePath;
        entry.isLoaded = false;
        m_fileIndex[defId] = entry;
    }
}

bool LibraryLoader::loadDefinitionById(const std::string& id)
{
    auto it = m_fileIndex.find(id);
    if (it == m_fileIndex.end()) return false;

    auto& entry = it->second;
    if (entry.isLoaded) return true;

    ValidationResult res;
    if (entry.category == "Materials")
    {
        auto mat = loadMaterialFile(entry.filePath, &res);
        if (mat && m_registry)
        {
            m_registry->registerMaterial(*mat);
            entry.isLoaded = true;
            return true;
        }
    }
    else if (entry.category == "Sections")
    {
        auto sec = loadSectionFile(entry.filePath, &res);
        if (sec && m_registry)
        {
            m_registry->registerSection(*sec);
            entry.isLoaded = true;
            return true;
        }
    }
    else if (entry.category == "Cables")
    {
        auto cab = loadCableFile(entry.filePath, &res);
        if (cab && m_registry)
        {
            m_registry->registerCable(*cab);
            entry.isLoaded = true;
            return true;
        }
    }
    return false;
}

bool LibraryLoader::reloadFile(const QString& filePath)
{
    QFileInfo fi(filePath);
    if (!fi.exists()) return false;

    // Trouver l'ID correspondant
    for (auto& [id, entry] : m_fileIndex)
    {
        if (entry.filePath == filePath)
        {
            entry.isLoaded = false;
            return loadDefinitionById(id);
        }
    }

    // Fichier nouveau non encore indexé
    std::string defId = fi.baseName().toLower().toStdString();
    DefinitionFileIndex entry;
    entry.id = defId;
    entry.filePath = filePath;
    entry.isLoaded = false;

    if (filePath.contains("/Materials/", Qt::CaseInsensitive)) entry.category = "Materials";
    else if (filePath.contains("/Sections/", Qt::CaseInsensitive) || filePath.contains("/Profiles/", Qt::CaseInsensitive)) entry.category = "Sections";
    else if (filePath.contains("/Cables/", Qt::CaseInsensitive)) entry.category = "Cables";
    else entry.category = "Materials";

    m_fileIndex[defId] = entry;
    return loadDefinitionById(defId);
}

bool LibraryLoader::unloadExtension(const std::string& extensionId)
{
    auto it = m_loadedExtensions.find(extensionId);
    if (it == m_loadedExtensions.end()) return false;

    QString extPath = it->second;
    m_loadedExtensions.erase(it);

    if (m_registry)
    {
        m_registry->removeDefinitionsByLibraryId(extensionId);
    }

    // Nettoyage de l'index de fichiers
    for (auto fit = m_fileIndex.begin(); fit != m_fileIndex.end(); )
    {
        if (fit->second.filePath.startsWith(extPath))
        {
            fit = m_fileIndex.erase(fit);
        }
        else
        {
            ++fit;
        }
    }
    return true;
}

std::optional<MaterialDefinition> LibraryLoader::loadMaterialFile(const QString& filePath, ValidationResult* outResult)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (outResult) outResult->addError("Impossible de lire : " + filePath.toStdString());
        return std::nullopt;
    }

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseErr);
    if (parseErr.error != QJsonParseError::NoError)
    {
        if (outResult) outResult->addError("Erreur JSON (" + filePath.toStdString() + ") : " + parseErr.errorString().toStdString());
        return std::nullopt;
    }

    std::string err;
    auto mat = MaterialDefinition::fromJson(doc.object(), &err);
    if (!mat)
    {
        if (outResult) outResult->addError(err);
        return std::nullopt;
    }

    QFileInfo fi(filePath);
    QString baseDir = fi.dir().path();
    ValidationResult val = m_validator.validateMaterial(*mat, baseDir);
    if (outResult) *outResult = val;

    return mat;
}

std::optional<SectionDefinition> LibraryLoader::loadSectionFile(const QString& filePath, ValidationResult* outResult)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (outResult) outResult->addError("Impossible de lire : " + filePath.toStdString());
        return std::nullopt;
    }

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseErr);
    if (parseErr.error != QJsonParseError::NoError)
    {
        if (outResult) outResult->addError("Erreur JSON (" + filePath.toStdString() + ") : " + parseErr.errorString().toStdString());
        return std::nullopt;
    }

    std::string err;
    auto sec = SectionDefinition::fromJson(doc.object(), &err);
    if (!sec)
    {
        if (outResult) outResult->addError(err);
        return std::nullopt;
    }

    QFileInfo fi(filePath);
    ValidationResult val = m_validator.validateSection(*sec, fi.dir().path());
    if (outResult) *outResult = val;

    return sec;
}

std::optional<CableCatalogDefinition> LibraryLoader::loadCableFile(const QString& filePath, ValidationResult* outResult)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (outResult) outResult->addError("Impossible de lire : " + filePath.toStdString());
        return std::nullopt;
    }

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseErr);
    if (parseErr.error != QJsonParseError::NoError)
    {
        if (outResult) outResult->addError("Erreur JSON (" + filePath.toStdString() + ") : " + parseErr.errorString().toStdString());
        return std::nullopt;
    }

    std::string err;
    auto cab = CableCatalogDefinition::fromJson(doc.object(), &err);
    if (!cab)
    {
        if (outResult) outResult->addError(err);
        return std::nullopt;
    }

    QFileInfo fi(filePath);
    ValidationResult val = m_validator.validateCable(*cab, fi.dir().path());
    if (outResult) *outResult = val;

    return cab;
}

} // namespace TSA::ExtensionSystem
