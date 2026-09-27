#include "LibraryValidator.h"
#include <QRegularExpression>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <set>

namespace TSA::ExtensionSystem
{

bool LibraryValidator::isValidId(const std::string& id)
{
    if (id.empty() || id.length() > 128) return false;
    // Format id : minuscules, chiffres, points, tirets ou underscores (ex: "org.tsaraloha.tsalib", "concrete.c25_30")
    static const QRegularExpression regex(R"(^[a-z0-9_-]+(\.[a-z0-9_-]+)*$)");
    return regex.match(QString::fromStdString(id)).hasMatch();
}

bool LibraryValidator::isSupportedPhysicalUnit(const std::string& unit)
{
    static const std::set<std::string> validUnits = {
        "pa", "kpa", "mpa", "gpa", "bar", "n/mm2",
        "kg/m3", "kg/m^3", "t/m3", "g/cm3",
        "n", "kn", "mn",
        "m", "dm", "cm", "mm",
        "m2", "cm2", "mm2",
        "m3", "cm3", "mm3",
        "m4", "cm4", "mm4",
        "deg", "°", "rad",
        "1/k", "k^-1"
    };
    std::string u = unit;
    std::transform(u.begin(), u.end(), u.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return validUnits.find(u) != validUnits.end();
}

ValidationResult LibraryValidator::validateManifest(const QJsonObject& manifestJson) const
{
    ValidationResult res;
    std::string parseErr;
    auto manifest = ExtensionManifest::fromJson(manifestJson, &parseErr);
    if (!manifest)
    {
        res.addError("Échec de parsing du manifest : " + parseErr);
        return res;
    }
    return validateManifest(*manifest);
}

ValidationResult LibraryValidator::validateManifest(const ExtensionManifest& manifest) const
{
    ValidationResult res;

    if (!isValidId(manifest.id))
    {
        res.addError("Identifiant de manifest invalide : '" + manifest.id + "'. Format attendu : minuscules, chiffres et points.");
    }
    if (manifest.name.empty())
    {
        res.addError("Le champ 'name' du manifest ne peut pas être vide.");
    }
    if (manifest.version.major < 0 || manifest.version.minor < 0)
    {
        res.addError("Version de manifest invalide.");
    }
    if (manifest.categories.empty())
    {
        res.addWarning("Le manifest ne déclare aucune catégorie dans 'categories'.");
    }

    return res;
}

ValidationResult LibraryValidator::validateMaterial(const MaterialDefinition& mat, const QString& libraryBasePath) const
{
    ValidationResult res;

    if (!isValidId(mat.id))
    {
        res.addError("Identifiant de matériau invalide : '" + mat.id + "'.");
    }
    if (mat.name.empty())
    {
        res.addError("Nom de matériau vide pour l'ID '" + mat.id + "'.");
    }

    // Validation des unités
    if (!mat.density.unit.empty() && !isSupportedPhysicalUnit(mat.density.unit))
    {
        res.addError("Unité de masse volumique non reconnue : '" + mat.density.unit + "'.");
    }
    if (!mat.youngModulus.unit.empty() && !isSupportedPhysicalUnit(mat.youngModulus.unit))
    {
        res.addError("Unité de module d'Young non reconnue : '" + mat.youngModulus.unit + "'.");
    }

    // Cohérence physique
    if (mat.density.toBaseSI() <= 0.0)
    {
        res.addWarning("Masse volumique nulle ou négative pour le matériau '" + mat.id + "'.");
    }
    if (mat.youngModulus.toBaseSI() <= 0.0)
    {
        res.addWarning("Module d'Young nul ou négatif pour le matériau '" + mat.id + "'.");
    }
    if (mat.poissonRatio < -1.0 || mat.poissonRatio >= 0.5)
    {
        res.addError("Coefficient de Poisson aberrant (" + std::to_string(mat.poissonRatio) + "). Plage théorique autorisée : [-1.0, 0.5[.");
    }

    // Vérification des textures référencées sur le disque
    if (!libraryBasePath.isEmpty())
    {
        for (const auto& [texKey, texRelPath] : mat.visual.textures)
        {
            if (texRelPath.empty()) continue;
            QString fullPath = QDir(libraryBasePath).filePath(QString::fromStdString(texRelPath));
            if (!QFile::exists(fullPath))
            {
                res.addWarning("Texture introuvable sur le disque pour '" + mat.id + "' [" + texKey + "] : " + texRelPath);
            }
        }
    }

    return res;
}

ValidationResult LibraryValidator::validateSection(const SectionDefinition& sec, const QString& /*libraryBasePath*/) const
{
    ValidationResult res;

    if (!isValidId(sec.id))
    {
        res.addError("Identifiant de section invalide : '" + sec.id + "'.");
    }
    if (sec.name.empty())
    {
        res.addError("Nom de section vide pour l'ID '" + sec.id + "'.");
    }
    if (sec.width <= 0.0 && sec.diameter <= 0.0)
    {
        res.addError("Dimensions principales nulles ou négatives pour la section '" + sec.id + "'.");
    }

    return res;
}

ValidationResult LibraryValidator::validateCable(const CableCatalogDefinition& cable, const QString& /*libraryBasePath*/) const
{
    ValidationResult res;

    if (!isValidId(cable.id))
    {
        res.addError("Identifiant de câble invalide : '" + cable.id + "'.");
    }
    if (cable.name.empty())
    {
        res.addError("Nom de câble vide pour l'ID '" + cable.id + "'.");
    }
    if (cable.nominalDiameter <= 0.0)
    {
        res.addError("Diamètre nominal nul ou négatif pour le câble '" + cable.id + "'.");
    }
    if (cable.elasticModulus <= 0.0)
    {
        res.addWarning("Module d'élasticité nul ou négatif pour le câble '" + cable.id + "'.");
    }

    return res;
}

ValidationResult LibraryValidator::validateExtensionDirectory(const QString& extensionDirPath) const
{
    ValidationResult res;
    QDir dir(extensionDirPath);

    if (!dir.exists())
    {
        res.addError("Le répertoire d'extension n'existe pas : " + extensionDirPath.toStdString());
        return res;
    }

    QString manifestPath = dir.filePath("manifest.json");
    if (!QFile::exists(manifestPath))
    {
        res.addError("Fichier 'manifest.json' manquant dans le répertoire : " + extensionDirPath.toStdString());
        return res;
    }

    QFile file(manifestPath);
    if (!file.open(QIODevice::ReadOnly))
    {
        res.addError("Impossible de lire le fichier 'manifest.json' : " + manifestPath.toStdString());
        return res;
    }

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseErr);
    if (parseErr.error != QJsonParseError::NoError)
    {
        res.addError("Erreur de syntaxe JSON dans 'manifest.json' : " + parseErr.errorString().toStdString());
        return res;
    }

    if (!doc.isObject())
    {
        res.addError("Le fichier 'manifest.json' doit être un objet JSON racine.");
        return res;
    }

    auto manifestRes = validateManifest(doc.object());
    for (const auto& err : manifestRes.errors) res.addError(err);
    for (const auto& warn : manifestRes.warnings) res.addWarning(warn);

    return res;
}

} // namespace TSA::ExtensionSystem
