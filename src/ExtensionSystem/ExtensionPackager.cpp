#include "ExtensionPackager.h"
#include "../Core/AppPaths.h"
#include "LibraryManager.h"
#include "LibraryValidator.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QDataStream>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStandardPaths>
#include <QUuid>
#include <QtEndian>

#include <ProductIdentity.h>

#include <set>

namespace TSA::ExtensionSystem
{

namespace
{
// Plafonds d'un paquet (fichier non fiable) : vérifiés à l'inspection, avant toute allocation ou écriture.
constexpr quint32 kMaxPackageFiles = 10000;
constexpr quint64 kMaxFileBytes = 256ull * 1024 * 1024;
constexpr quint64 kMaxTotalBytes = 1024ull * 1024 * 1024;

/// L'identifiant sert de nom de dossier d'installation : seul le format des identifiants de bibliothèque est admis
/// (minuscules, chiffres, « _ », « - », segments séparés par « . »), jamais « .. », « / » ni « \ ».
QString invalidIdMessage(const std::string& id)
{
    return QString("Identifiant d'extension invalide « %1 » : minuscules, chiffres, « _ », « - », segments séparés par « . ».")
        .arg(QString::fromStdString(id));
}

/// Noms de fichiers réservés par Windows (avec ou sans extension) : CON, PRN, AUX, NUL, COM1…COM9, LPT1…LPT9.
bool isWindowsReservedName(const QString& part)
{
    const QString base = part.section('.', 0, 0).toUpper();
    static const QSet<QString> reserved = { "CON", "PRN", "AUX", "NUL" };
    if (reserved.contains(base)) return true;
    return base.size() == 4 && (base.startsWith("COM") || base.startsWith("LPT")) && base[3] >= '1' && base[3] <= '9';
}

/// Identifiants des extensions livrées avec l'application (dossier en lecture seule) : aucun paquet installé par
/// l'utilisateur ne peut les reprendre (une extension officielle n'est jamais remplacée en silence).
std::set<std::string> shippedExtensionIds()
{
    std::set<std::string> ids;
    const QDir root(TSA::Core::AppPaths::shippedExtensionsDir());
    for (const QString& sub : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
    {
        QFile f(root.filePath(sub + "/manifest.json"));
        if (!f.open(QIODevice::ReadOnly)) continue;
        const QJsonDocument doc = QJsonDocument::fromJson(f.read(1 << 20));
        if (const auto m = ExtensionManifest::fromJson(doc.object()); m && m->isValid()) ids.insert(m->id);
    }
    return ids;
}

/// Dossier de préparation d'une installation, supprimé à la destruction sauf après la bascule finale.
struct StagingDir
{
    QString path;
    ~StagingDir()
    {
        if (path.isEmpty()) return;
        QDir(path).removeRecursively();
        QDir().rmdir(QFileInfo(path).absolutePath());   // « .staging » retiré s'il est vide
    }
};
} // namespace

bool ExtensionPackager::isSafeRelativePath(const QString& relPath)
{
    if (relPath.trimmed().isEmpty()) return false;

    // Normaliser les séparateurs
    QString p = relPath;
    p.replace('\\', '/');

    // Rejeter les chemins absolus (Windows drive ou Unix root)
    if (p.startsWith('/') || p.contains(':')) return false;

    // Rejeter les tentatives de Path Traversal
    QStringList parts = p.split('/', Qt::SkipEmptyParts);
    for (const QString& part : parts)
    {
        if (part == ".." || part == ".") return false;
        // Noms que Windows ne sait pas créer ou interprète comme des périphériques
        if (part.endsWith('.') || part.endsWith(' ') || isWindowsReservedName(part)) return false;
        for (const QChar c : part)
            if (c.unicode() < 0x20 || c.unicode() == 0x7f || QStringLiteral("<>\"|?*").contains(c)) return false;
    }

    return true;
}

bool ExtensionPackager::createPackage(const QString& sourceDirectory,
                                      const QString& outputPackagePath,
                                      QString* outError)
{
    QDir srcDir(sourceDirectory);
    if (!srcDir.exists())
    {
        if (outError) *outError = QString("Le répertoire source n'existe pas : %1").arg(sourceDirectory);
        return false;
    }

    // 1. Vérification de manifest.json
    QString manifestPath = srcDir.filePath("manifest.json");
    QFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::ReadOnly))
    {
        if (outError) *outError = QString("Fichier manifest.json introuvable ou inaccessible dans : %1").arg(sourceDirectory);
        return false;
    }

    QByteArray manifestBytes = manifestFile.readAll();
    manifestFile.close();

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(manifestBytes, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject())
    {
        if (outError) *outError = QString("manifest.json JSON invalide : %1").arg(parseErr.errorString());
        return false;
    }

    auto optManifest = ExtensionManifest::fromJson(doc.object());
    if (!optManifest.has_value() || !optManifest->isValid())
    {
        if (outError) *outError = "Le manifest.json ne contient pas un id ou un nom valide.";
        return false;
    }
    if (!LibraryValidator::isValidId(optManifest->id))
    {
        if (outError) *outError = invalidIdMessage(optManifest->id);
        return false;
    }

    // 2. Scan récursif des fichiers
    struct FileEntry
    {
        QString relPath;
        quint64 uncompressedSize = 0;
        QByteArray sha256Bytes;
        QByteArray compressedData;
    };

    std::vector<FileEntry> entries;
    QDirIterator it(sourceDirectory, QDir::Files, QDirIterator::Subdirectories);

    while (it.hasNext())
    {
        QString fullPath = it.next();
        QString relPath = srcDir.relativeFilePath(fullPath);
        relPath.replace('\\', '/');

        // Ne pas inclure d'éventuels fichiers temporaires ou le package de sortie lui-même
        if (relPath.endsWith(".tsalib", Qt::CaseInsensitive) || relPath.startsWith("."))
        {
            continue;
        }

        QFile f(fullPath);
        if (!f.open(QIODevice::ReadOnly))
        {
            if (outError) *outError = QString("Impossible de lire le fichier : %1").arg(relPath);
            return false;
        }

        QByteArray rawData = f.readAll();
        f.close();

        FileEntry entry;
        entry.relPath = relPath;
        entry.uncompressedSize = static_cast<quint64>(rawData.size());
        entry.sha256Bytes = QCryptographicHash::hash(rawData, QCryptographicHash::Sha256);
        entry.compressedData = qCompress(rawData, 9);

        entries.push_back(std::move(entry));
    }

    // 3. Écriture du fichier binaire .tsalib
    QFileInfo outInfo(outputPackagePath);
    QDir outDir = outInfo.dir();
    if (!outDir.exists())
    {
        outDir.mkpath(".");
    }

    QFile outFile(outputPackagePath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        if (outError) *outError = QString("Impossible de créer le fichier package : %1").arg(outputPackagePath);
        return false;
    }

    QDataStream stream(&outFile);
    stream.setVersion(QDataStream::Qt_6_0);
    stream.setByteOrder(QDataStream::LittleEndian);

    // Header
    quint64 magic = PACKAGE_MAGIC;
    quint32 version = CURRENT_FORMAT_VERSION;
    quint32 flags = 0;

    stream << magic;
    stream << version;
    stream << flags;

    // Manifest brut
    stream << manifestBytes;

    // Table des fichiers
    quint32 count = static_cast<quint32>(entries.size());
    stream << count;

    for (const auto& entry : entries)
    {
        stream << entry.relPath;
        stream << entry.uncompressedSize;
        stream << entry.sha256Bytes;
        stream << entry.compressedData;
    }

    outFile.flush();
    outFile.close();

    return true;
}

PackageInspectionResult ExtensionPackager::inspectPackage(const QString& packagePath)
{
    PackageInspectionResult result;
    QFile file(packagePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        result.valid = false;
        result.errorMessage = QString("Impossible d'ouvrir le package : %1").arg(packagePath);
        return result;
    }

    // Calcul du SHA-256 complet du package
    QCryptographicHash pkgHash(QCryptographicHash::Sha256);
    while (!file.atEnd())
    {
        pkgHash.addData(file.read(65536));
    }
    result.packageSha256Hex = QString::fromLatin1(pkgHash.result().toHex());
    file.seek(0);

    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_0);
    stream.setByteOrder(QDataStream::LittleEndian);

    quint64 magic = 0;
    stream >> magic;
    if (magic != PACKAGE_MAGIC)
    {
        result.valid = false;
        result.errorMessage = "Format de package non reconnu (Magic bytes invalides).";
        return result;
    }

    quint32 ver = 0;
    stream >> ver;
    result.formatVersion = ver;
    if (ver > CURRENT_FORMAT_VERSION)
    {
        result.valid = false;
        result.errorMessage = QString("Version du format de package (%1) non supportée.").arg(ver);
        return result;
    }

    quint32 flags = 0;
    stream >> flags;

    QByteArray manifestBytes;
    stream >> manifestBytes;
    result.rawManifestJson = manifestBytes;

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(manifestBytes, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject())
    {
        result.valid = false;
        result.errorMessage = "Manifest JSON corrompu à l'intérieur du package.";
        return result;
    }

    auto optManifest = ExtensionManifest::fromJson(doc.object());
    if (!optManifest.has_value() || !optManifest->isValid())
    {
        result.valid = false;
        result.errorMessage = "Le manifest du package ne contient pas d'identifiant valide.";
        return result;
    }
    result.manifest = *optManifest;
    if (!LibraryValidator::isValidId(result.manifest.id))
    {
        result.valid = false;
        result.errorMessage = invalidIdMessage(result.manifest.id);
        return result;
    }

    quint32 count = 0;
    stream >> count;
    if (count > kMaxPackageFiles)
    {
        result.valid = false;
        result.errorMessage = QString("Package refusé : %1 fichiers (au plus %2).").arg(count).arg(kMaxPackageFiles);
        return result;
    }

    result.totalUncompressedBytes = 0;
    QSet<QString> seenPaths;   // comparaison insensible à la casse (Windows, macOS)
    for (quint32 i = 0; i < count; ++i)
    {
        if (stream.atEnd())
        {
            result.valid = false;
            result.errorMessage = "Package tronqué de manière inattendue.";
            return result;
        }

        QString relPath;
        quint64 uncompressedSize = 0;
        QByteArray sha256Bytes;
        QByteArray compressedData;

        stream >> relPath;
        stream >> uncompressedSize;
        stream >> sha256Bytes;
        stream >> compressedData;
        if (stream.status() != QDataStream::Ok)
        {
            result.valid = false;
            result.errorMessage = "Package tronqué ou corrompu (lecture d'une entrée interrompue).";
            return result;
        }
        // Le manifeste installé doit être celui qui a été validé (en-tête) : sinon un paquet « acme.lib » pourrait
        // s'installer sous un identifiant contrôlé et se déclarer « org.tsaraloha.tsalib » une fois sur le disque.
        if (relPath.compare("manifest.json", Qt::CaseInsensitive) == 0 &&
            sha256Bytes != QCryptographicHash::hash(manifestBytes, QCryptographicHash::Sha256))
        {
            result.valid = false;
            result.errorMessage = "Package refusé : manifest.json différent du manifeste validé de l'en-tête.";
            return result;
        }

        PackageFileInfo info;
        info.relativePath = relPath;
        info.uncompressedSize = uncompressedSize;
        info.compressedSize = static_cast<quint64>(compressedData.size());
        info.sha256Hex = QString::fromLatin1(sha256Bytes.toHex());

        if (!isSafeRelativePath(relPath))
        {
            result.valid = false;
            result.errorMessage = QString("Tentative de Path Traversal détectée : '%1'").arg(relPath);
            return result;
        }
        const QString key = QDir::cleanPath(QString(relPath).replace('\\', '/')).toLower();
        if (seenPaths.contains(key))
        {
            result.valid = false;
            result.errorMessage = QString("Package refusé : fichier en double (casse ignorée) : '%1'").arg(relPath);
            return result;
        }
        seenPaths.insert(key);
        if (uncompressedSize > kMaxFileBytes || result.totalUncompressedBytes + uncompressedSize > kMaxTotalBytes)
        {
            result.valid = false;
            result.errorMessage = QString("Package refusé : taille décompressée excessive (%1, au plus %2 Mio par fichier et %3 Mio au total).")
                                      .arg(relPath)
                                      .arg(kMaxFileBytes >> 20)
                                      .arg(kMaxTotalBytes >> 20);
            return result;
        }

        result.files.append(info);
        result.totalUncompressedBytes += uncompressedSize;
    }
    if (!stream.atEnd())
    {
        result.valid = false;
        result.errorMessage = "Package refusé : données inattendues après la dernière entrée.";
        return result;
    }

    result.valid = true;
    return result;
}

bool ExtensionPackager::installPackage(const QString& packagePath,
                                      const QString& destinationDir,
                                      QString* outInstalledDir,
                                      QString* outError)
{
    auto fail = [outError](const QString& message) {
        if (outError) *outError = message;
        return false;
    };

    // Transaction (conception TSALIB, étape E0) : le paquet est COPIÉ dans un dossier de préparation, voisin de la
    // cible (même volume), puis inspecté et extrait depuis cette copie : il ne peut plus changer entre l'inspection et
    // l'extraction. Rien n'apparaît dans le dossier des extensions avant que tout soit extrait et vérifié ; la bascule
    // est un seul renommage. Une erreur ou une interruption ne laisse qu'un dossier de préparation, jamais une
    // extension partielle.
    const bool defaultLocation = destinationDir.isEmpty();
    const QString parentDir = defaultLocation ? TSA::Core::AppPaths::userExtensionsDir()
                                              : QFileInfo(QDir::cleanPath(QDir(destinationDir).absolutePath())).absolutePath();
    StagingDir staging;
    staging.path = QDir(parentDir).filePath(".staging/" + QUuid::createUuid().toString(QUuid::WithoutBraces));
    if (!QDir().mkpath(staging.path + "/tree"))
        return fail(QString("Impossible de créer le dossier de préparation : %1").arg(staging.path));
    const QString copy = staging.path + "/package.tsalib";
    if (!QFile::copy(packagePath, copy)) return fail(QString("Impossible de copier le package : %1").arg(packagePath));

    auto inspection = inspectPackage(copy);
    if (!inspection.isValid()) return fail(inspection.errorMessage);

    const QString targetDir = defaultLocation ? QDir(parentDir).filePath(QString::fromStdString(inspection.manifest.id))
                                              : QDir::cleanPath(QDir(destinationDir).absolutePath());
    if (defaultLocation && shippedExtensionIds().count(inspection.manifest.id))
        return fail(QString("Package refusé : l'identifiant « %1 » est celui d'une extension livrée avec %2 ; une extension "
                            "officielle n'est jamais remplacée par un package.")
                        .arg(QString::fromStdString(inspection.manifest.id), QString::fromLatin1(TSA::Product::kName)));
    // Version minimale vérifiée seulement si le manifeste la déclare (valeur par défaut du champ : 1.0.0)
    const bool declaresMinimum = QJsonDocument::fromJson(inspection.rawManifestJson).object().contains("minimum_tsa_version");
    if (const auto host = SemanticVersion::fromString(TSA::Product::kVersion); declaresMinimum && host && *host < inspection.manifest.minimumTsaVersion)
        return fail(QString("Package refusé : il demande %1 %2 ou plus récent (version installée : %3).")
                        .arg(QString::fromLatin1(TSA::Product::kName), QString::fromStdString(inspection.manifest.minimumTsaVersion.toString()),
                             QString::fromLatin1(TSA::Product::kVersion)));
    if (QDir(targetDir).exists() && !QDir(targetDir).isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System))
        return fail(QString("Extension déjà installée dans « %1 » : la désinstaller avant d'installer une autre version "
                            "(aucune mise à jour en place, qui mélangerait deux versions).").arg(targetDir));

    QFile file(copy);
    if (!file.open(QIODevice::ReadOnly)) return fail("Impossible d'ouvrir la copie du package pour extraction.");
    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_0);
    stream.setByteOrder(QDataStream::LittleEndian);

    quint64 magic = 0;
    quint32 ver = 0;
    quint32 flags = 0;
    QByteArray manifestBytes;
    quint32 count = 0;
    stream >> magic >> ver >> flags >> manifestBytes >> count;
    if (stream.status() != QDataStream::Ok || manifestBytes != inspection.rawManifestJson || count != static_cast<quint32>(inspection.files.size()))
        return fail("Le package a changé depuis son inspection : installation annulée.");

    const QDir tree(staging.path + "/tree");
    const QString treeRoot = QDir::cleanPath(tree.absolutePath()) + '/';
    for (quint32 i = 0; i < count; ++i)
    {
        QString relPath;
        quint64 uncompressedSize = 0;
        QByteArray sha256Bytes;
        QByteArray compressedData;

        stream >> relPath >> uncompressedSize >> sha256Bytes >> compressedData;
        if (stream.status() != QDataStream::Ok) return fail("Package tronqué ou corrompu (lecture d'une entrée interrompue).");
        if (!isSafeRelativePath(relPath)) return fail(QString("Tentative de Path Traversal détectée : '%1'").arg(relPath));
        // manifest.json de la table : identique à l'en-tête (vérifié à l'inspection) ; l'en-tête est écrit plus bas
        if (relPath.compare("manifest.json", Qt::CaseInsensitive) == 0) continue;

        // qUncompress alloue la taille annoncée par l'en-tête zlib de Qt : elle doit être celle, plafonnée, du paquet.
        if (compressedData.size() < 4 || qFromBigEndian<quint32>(compressedData.constData()) != uncompressedSize)
            return fail(QString("Taille annoncée incohérente pour le fichier : %1").arg(relPath));
        const QByteArray rawData = qUncompress(compressedData);
        if (static_cast<quint64>(rawData.size()) != uncompressedSize) return fail(QString("Erreur de décompression pour le fichier : %1").arg(relPath));
        if (QCryptographicHash::hash(rawData, QCryptographicHash::Sha256) != sha256Bytes)
            return fail(QString("Somme de contrôle SHA-256 corrompue pour le fichier : %1").arg(relPath));

        const QString outFilePath = QDir::cleanPath(tree.absoluteFilePath(relPath));
        if (!outFilePath.startsWith(treeRoot)) return fail(QString("Tentative de Path Traversal détectée : '%1'").arg(relPath));
        QDir().mkpath(QFileInfo(outFilePath).dir().absolutePath());
        QFile outF(outFilePath);
        // Création exclusive : un fichier déjà présent (doublon non détecté, lien) n'est jamais écrasé
        if (!outF.open(QIODevice::WriteOnly | QIODevice::NewOnly) || outF.write(rawData) != rawData.size() || !outF.flush())
            return fail(QString("Impossible d'écrire le fichier extrait : %1").arg(relPath));
        outF.close();
    }
    {
        // Le manifeste installé est toujours celui de l'en-tête, validé par l'inspection
        QFile mf(tree.filePath("manifest.json"));
        if (!mf.open(QIODevice::WriteOnly | QIODevice::NewOnly) || mf.write(manifestBytes) != manifestBytes.size() || !mf.flush())
            return fail("Impossible d'écrire le manifeste de l'extension.");
    }
    file.close();

    // Bascule : un seul renommage d'un dossier complet vers une cible absente (un dossier vide est d'abord retiré).
    if (QDir(targetDir).exists() && !QDir().rmdir(targetDir))
        return fail(QString("Extension déjà installée dans « %1 ».").arg(targetDir));
    QDir().mkpath(QFileInfo(targetDir).absolutePath());
    if (QDir(targetDir).exists() || !QDir().rename(tree.absolutePath(), targetDir))
        return fail(QString("Impossible d'activer l'extension dans « %1 » (dossier occupé ?).").arg(targetDir));

    if (outInstalledDir) *outInstalledDir = QDir(targetDir).canonicalPath();
    return true;   // StagingDir supprime la copie du package
}

bool ExtensionPackager::exportExtension(const std::string& extensionId,
                                        const QString& outputPackagePath,
                                        QString* outError)
{
    QString extDir = LibraryManager::instance().extensionPath(extensionId);
    if (extDir.isEmpty() || !QDir(extDir).exists())
    {
        if (outError) *outError = QString("Répertoire source introuvable pour l'extension : %1").arg(QString::fromStdString(extensionId));
        return false;
    }

    return createPackage(extDir, outputPackagePath, outError);
}

} // namespace TSA::ExtensionSystem
