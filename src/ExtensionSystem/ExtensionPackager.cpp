#include "ExtensionPackager.h"
#include "../Core/AppPaths.h"
#include "LibraryManager.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QDataStream>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace TSA::ExtensionSystem
{

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

    quint32 count = 0;
    stream >> count;

    result.totalUncompressedBytes = 0;
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

        PackageFileInfo info;
        info.relativePath = relPath;
        info.uncompressedSize = uncompressedSize;
        info.compressedSize = static_cast<quint64>(compressedData.size());
        info.sha256Hex = QString::fromLatin1(sha256Bytes.toHex());

        result.files.append(info);
        result.totalUncompressedBytes += uncompressedSize;
    }

    result.valid = true;
    return result;
}

bool ExtensionPackager::installPackage(const QString& packagePath,
                                      const QString& destinationDir,
                                      QString* outInstalledDir,
                                      QString* outError)
{
    auto inspection = inspectPackage(packagePath);
    if (!inspection.isValid())
    {
        if (outError) *outError = inspection.errorMessage;
        return false;
    }

    // Déterminer le dossier d'installation cible
    QString targetDir = destinationDir;
    if (targetDir.isEmpty())
    {
        // Installation dans le dossier de l'utilisateur : le dossier de l'application peut être en
        // lecture seule (Program Files) et serait remplacé par une mise à jour.
        targetDir = TSA::Core::AppPaths::userExtensionsDir();
        targetDir = QDir(targetDir).filePath(QString::fromStdString(inspection.manifest.id));
    }

    QDir dest(targetDir);
    if (!dest.exists())
    {
        if (!dest.mkpath("."))
        {
            if (outError) *outError = QString("Impossible de créer le répertoire cible : %1").arg(targetDir);
            return false;
        }
    }

    // Extraction des fichiers
    QFile file(packagePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (outError) *outError = "Impossible de rouvrir le package pour extraction.";
        return false;
    }

    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_0);
    stream.setByteOrder(QDataStream::LittleEndian);

    quint64 magic = 0;
    quint32 ver = 0;
    quint32 flags = 0;
    QByteArray manifestBytes;
    quint32 count = 0;

    stream >> magic >> ver >> flags >> manifestBytes >> count;

    bool manifestFoundInFiles = false;

    for (quint32 i = 0; i < count; ++i)
    {
        QString relPath;
        quint64 uncompressedSize = 0;
        QByteArray sha256Bytes;
        QByteArray compressedData;

        stream >> relPath >> uncompressedSize >> sha256Bytes >> compressedData;

        if (!isSafeRelativePath(relPath))
        {
            if (outError) *outError = QString("Tentative de Path Traversal détectée : '%1'").arg(relPath);
            return false;
        }

        if (relPath.compare("manifest.json", Qt::CaseInsensitive) == 0)
        {
            manifestFoundInFiles = true;
        }

        QByteArray rawData = qUncompress(compressedData);
        if (static_cast<quint64>(rawData.size()) != uncompressedSize)
        {
            if (outError) *outError = QString("Erreur de décompression pour le fichier : %1").arg(relPath);
            return false;
        }

        QByteArray checkHash = QCryptographicHash::hash(rawData, QCryptographicHash::Sha256);
        if (checkHash != sha256Bytes)
        {
            if (outError) *outError = QString("Somme de contrôle SHA-256 corrompue pour le fichier : %1").arg(relPath);
            return false;
        }

        QString outFilePath = dest.filePath(relPath);
        QFileInfo fi(outFilePath);
        QDir().mkpath(fi.dir().absolutePath());

        QFile outF(outFilePath);
        if (!outF.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            if (outError) *outError = QString("Impossible d'écrire le fichier extrait : %1").arg(outFilePath);
            return false;
        }
        outF.write(rawData);
        outF.close();
    }

    // Si manifest.json n'était pas dans la liste des fichiers, l'écrire depuis le header
    if (!manifestFoundInFiles)
    {
        QString mPath = dest.filePath("manifest.json");
        QFile mf(mPath);
        if (mf.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            mf.write(manifestBytes);
            mf.close();
        }
    }

    if (outInstalledDir)
    {
        *outInstalledDir = dest.canonicalPath();
    }

    return true;
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
