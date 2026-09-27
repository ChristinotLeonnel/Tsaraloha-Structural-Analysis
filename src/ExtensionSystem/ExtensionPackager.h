#pragma once

#include <QString>
#include <QByteArray>
#include <QList>
#include <string>
#include "ExtensionTypes.h"

namespace TSA::ExtensionSystem
{

/**
 * @brief Métadonnées d'un fichier contenu dans une archive .tsalib.
 */
struct PackageFileInfo
{
    QString relativePath;           // ex: "Materials/concrete_c25_30.json"
    quint64 uncompressedSize = 0;   // Taille en octets non compressée
    quint64 compressedSize = 0;     // Taille en octets compressée (zlib)
    QString sha256Hex;              // Empreinte cryptographique SHA-256 du contenu brut
};

/**
 * @brief Résultat de l'inspection sécurisée d'un package .tsalib sans extraction intégrale.
 */
struct PackageInspectionResult
{
    bool valid = false;
    QString errorMessage;
    quint32 formatVersion = 1;
    ExtensionManifest manifest;
    QByteArray rawManifestJson;
    QString packageSha256Hex;
    quint64 totalUncompressedBytes = 0;
    QList<PackageFileInfo> files;

    bool isValid() const { return valid; }
};

/**
 * @brief Gestionnaire de packaging, d'inspection, d'extraction et de distribution .tsalib.
 * 
 * Format unifié et autonome (.tsalib) permettant de distribuer une bibliothèque complète
 * (manifest.json, fiches matériaux Eurocode, sections et profilés, câbles et torons, textures PBR)
 * en un seul fichier signé avec intégrité SHA-256 et protection contre le Path Traversal (Zip Slip).
 */
class ExtensionPackager
{
public:
    // Magic signature 8 octets : 'T', 'S', 'A', 'L', 'I', 'B', 0x01, 0x00
    static constexpr quint64 PACKAGE_MAGIC = 0x000142494C415354ULL;
    static constexpr quint32 CURRENT_FORMAT_VERSION = 1;

    /**
     * @brief Crée un package binaire compressé .tsalib à partir d'un répertoire source d'extension.
     * Le répertoire doit obligatoirement contenir un manifest.json valide à sa racine.
     * 
     * @param sourceDirectory Chemin vers le dossier source (ex: "e:/Book/Dev/TSA/Extensions/TSALib")
     * @param outputPackagePath Chemin du fichier package de sortie (ex: "e:/Dist/TSALib.tsalib")
     * @param outError Pointeur optionnel pour recevoir le message d'erreur en cas d'échec
     * @return true si le package a été créé et vérifié avec succès
     */
    static bool createPackage(const QString& sourceDirectory,
                              const QString& outputPackagePath,
                              QString* outError = nullptr);

    /**
     * @brief Inspecte un package .tsalib sans extraire la totalité des données sur le disque.
     * Vérifie la signature magic, désérialise le manifest et liste les métadonnées des fichiers.
     * 
     * @param packagePath Chemin vers le fichier .tsalib
     * @return PackageInspectionResult contenant les métadonnées et le statut de validité
     */
    static PackageInspectionResult inspectPackage(const QString& packagePath);

    /**
     * @brief Extrait et installe un package .tsalib dans le répertoire cible.
     * Si destinationDir est vide, l'extension est installée automatiquement dans le répertoire
     * officiel Extensions/<extensionId>/ de TSA.
     * Contrôle strictement les chemins pour empêcher toute vulnérabilité de type Path Traversal.
     * 
     * @param packagePath Chemin vers le fichier .tsalib
     * @param destinationDir Répertoire d'installation (ou chaîne vide pour le répertoire par défaut)
     * @param outInstalledDir Pointeur optionnel recevant le chemin absolu du dossier installé
     * @param outError Pointeur optionnel recevant l'erreur en cas d'échec
     * @return true si l'installation et l'extraction ont réussi à 100%
     */
    static bool installPackage(const QString& packagePath,
                               const QString& destinationDir = QString(),
                               QString* outInstalledDir = nullptr,
                               QString* outError = nullptr);

    /**
     * @brief Exporte une extension actuellement installée ou enregistrée vers un fichier .tsalib.
     * 
     * @param extensionId Identifiant de l'extension (ex: "org.tsaraloha.tsalib")
     * @param outputPackagePath Chemin du fichier package .tsalib de destination
     * @param outError Pointeur optionnel pour l'erreur
     * @return true si l'export a réussi
     */
    static bool exportExtension(const std::string& extensionId,
                                const QString& outputPackagePath,
                                QString* outError = nullptr);

    /**
     * @brief Valide si un chemin relatif est sécurisé (anti-path-traversal / Zip-Slip).
     */
    static bool isSafeRelativePath(const QString& relPath);
};

} // namespace TSA::ExtensionSystem
