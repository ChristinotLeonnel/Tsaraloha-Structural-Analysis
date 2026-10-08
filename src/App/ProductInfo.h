#pragma once

// Accès Qt à l'identité du produit compilé (TSA ou TSALab), voir <ProductIdentity.h>.
// Toute valeur propre au produit (nom, extension, filtres de fichiers, QSettings…) passe par ici :
// ne pas réintroduire de littéral « TSA » / « .tsa » dans ces usages.

#include <ProductIdentity.h>

#include <QSettings>
#include <QString>

#include <memory>

#ifndef TSA_SOURCE_DIR
#define TSA_SOURCE_DIR ""
#endif

namespace TSA::Product
{

inline QString name() { return QString::fromLatin1(kName); }
inline QString longName() { return QString::fromLatin1(kLongName); }
/// « TSA (Tsaraloha Structural Analysis) ».
inline QString displayName() { return QStringLiteral("%1 (%2)").arg(name(), longName()); }
inline QString version() { return QString::fromLatin1(kVersion); }

inline QString projectExtension() { return QString::fromLatin1(kProjectExtension); }
inline bool hasLegacyFormat() { return kLegacyExtension[0] != '\0'; }

/// Fichier au format natif du produit.
inline bool isNativeProjectFile(const QString& path)
{
    return path.endsWith(projectExtension(), Qt::CaseInsensitive);
}

/// Fichier ouvrable par le produit (format natif, ou format importé s'il y en a un).
inline bool isOpenableProjectFile(const QString& path)
{
    return isNativeProjectFile(path)
        || (hasLegacyFormat() && path.endsWith(QLatin1String(kLegacyExtension), Qt::CaseInsensitive));
}

/// Ajoute l'extension native si absente ; un format importé est remplacé (« Essai.tsa » → « Essai.tsalab »).
inline QString withProjectExtension(QString path)
{
    if (hasLegacyFormat() && path.endsWith(QLatin1String(kLegacyExtension), Qt::CaseInsensitive)
        && !isNativeProjectFile(path))
        path.chop(int(sizeof(kLegacyExtension)) - 1);
    if (!isNativeProjectFile(path)) path += projectExtension();
    return path;
}

/// Filtre des boîtes « Ouvrir ».
inline QString openFileFilter()
{
    const QString ext = QString::fromLatin1(kProjectExtension + 1);
    if (!hasLegacyFormat())
        return QStringLiteral("%1 Project (*.%2);;Tous les fichiers (*.*)").arg(name(), ext);
    const QString legacy = QString::fromLatin1(kLegacyExtension + 1);
    return QStringLiteral("Projets %1 (*.%2 *.%3);;Projets %1 natifs (*.%2);;Modèles %4 (*.%3);;Tous les fichiers (*.*)")
        .arg(name(), ext, legacy, QString::fromLatin1(kLegacyProductName));
}

/// Filtre des boîtes « Enregistrer ».
inline QString saveFileFilter()
{
    const QString ext = QString::fromLatin1(kProjectExtension + 1);
    if (!hasLegacyFormat())
        return QStringLiteral("%1 Project (*.%2);;Tous les fichiers (*.*)").arg(name(), ext);
    return QStringLiteral("Projet %1 (*.%2)").arg(name(), ext);
}

/// Paramètres de l'application (projets récents…).
inline std::unique_ptr<QSettings> appSettings()
{
    return std::make_unique<QSettings>(QString::fromLatin1(kOrganizationName), QString::fromLatin1(kSettingsApplication));
}

/// Racine des sources communes (définie par CMake) : ressources de développement (Extensions/,
/// thirdparty/OpenSees). Vide hors d'un poste de développement.
inline QString sourceDirectory() { return QString::fromUtf8(TSA_SOURCE_DIR); }

} // namespace TSA::Product
