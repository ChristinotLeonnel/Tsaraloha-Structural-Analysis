#include "ExtensionScaffolder.h"
#include "LibraryValidator.h"
#include "ExtensionPackager.h"
#include "LibraryManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>
#include <QStandardPaths>

namespace TSA::ExtensionSystem
{

bool ExtensionScaffolder::scaffold(const ExtensionTemplateOptions& options,
                                   QString* outDirectory,
                                   QString* outError)
{
    // 1. Validation des paramètres obligatoires
    if (!LibraryValidator::isValidId(options.id))
    {
        if (outError)
        {
            *outError = QString("Identifiant invalide : '%1'. Format attendu : minuscules, chiffres, points, tirets ou underscores.")
                            .arg(QString::fromStdString(options.id));
        }
        return false;
    }

    if (options.name.empty())
    {
        if (outError)
        {
            *outError = "Le nom de la bibliothèque ne peut pas être vide.";
        }
        return false;
    }

    // 2. Détermination du répertoire cible
    QString targetDir = options.targetDirectory;
    if (targetDir.trimmed().isEmpty())
    {
        auto searchPaths = LibraryManager::instance().searchPaths();
        if (!searchPaths.isEmpty())
        {
            targetDir = QDir(searchPaths.first()).filePath(QString::fromStdString(options.id));
        }
        else
        {
            targetDir = QDir(QDir::currentPath() + "/Extensions").filePath(QString::fromStdString(options.id));
        }
    }

    QDir dir(targetDir);
    if (!dir.exists())
    {
        if (!dir.mkpath("."))
        {
            if (outError)
            {
                *outError = QString("Impossible de créer le répertoire cible : %1").arg(targetDir);
            }
            return false;
        }
    }

    // 3. Construction et écriture de manifest.json
    QJsonObject manifestObj;
    manifestObj["id"] = QString::fromStdString(options.id);
    manifestObj["name"] = QString::fromStdString(options.name);
    manifestObj["version"] = QString::fromStdString(options.version.toString());
    manifestObj["format_version"] = "1.0";
    manifestObj["minimum_tsa_version"] = "0.1.0";
    manifestObj["author"] = QString::fromStdString(options.author.empty() ? "TSA Engineering User" : options.author);
    manifestObj["license"] = QString::fromStdString(options.license.empty() ? "MIT" : options.license);
    manifestObj["description"] = QString::fromStdString(options.description.empty() ? "Bibliothèque personnalisée pour TSA." : options.description);
    if (!options.website.empty())
    {
        manifestObj["website"] = QString::fromStdString(options.website);
    }
    manifestObj["kind"] = (options.kind == ExtensionKind::CodeExtension) ? "code" : "data";

    QJsonArray categoriesArr;
    if (options.includeMaterials) categoriesArr.append("materials");
    if (options.includeSections) categoriesArr.append("sections");
    if (options.includeProfiles) categoriesArr.append("profiles");
    if (options.includeCables) categoriesArr.append("cables");
    if (options.includeTextures) categoriesArr.append("textures");
    if (options.includeStandards) categoriesArr.append("standards");
    manifestObj["categories"] = categoriesArr;

    QFile manifestFile(dir.filePath("manifest.json"));
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        if (outError)
        {
            *outError = QString("Impossible d'écrire manifest.json dans : %1").arg(targetDir);
        }
        return false;
    }
    manifestFile.write(QJsonDocument(manifestObj).toJson(QJsonDocument::Indented));
    manifestFile.close();

    // 4. Création des sous-répertoires et modèles d'exemples
    if (options.includeMaterials)
    {
        dir.mkpath("Materials");
        MaterialDefinition sampleMat;
        sampleMat.id = "custom_material_c30";
        sampleMat.name = "Matériau Béton C30/37 Type";
        sampleMat.category = "Concrete";
        sampleMat.version = options.version;
        sampleMat.standard.name = "EN 1992-1-1";
        sampleMat.standard.edition = "2004";
        sampleMat.standard.clause = "Tableau 3.1";
        sampleMat.standard.source = "CEN";
        sampleMat.youngModulus = PhysicalValue(33000.0, "MPa");
        sampleMat.density = PhysicalValue(2500.0, "kg/m3");
        sampleMat.poissonRatio = 0.20;
        sampleMat.thermalCoeff = PhysicalValue(1.0e-5, "1/K");
        sampleMat.fck = PhysicalValue(30.0, "MPa");
        sampleMat.visual.baseColor = "#7A8288";
        sampleMat.visual.roughness = 0.85;
        sampleMat.visual.metallic = 0.05;
        sampleMat.visual.textures["albedo"] = "Textures/concrete.png";

        createMaterialFile(targetDir, "sample_material.json", sampleMat);
    }

    if (options.includeSections)
    {
        dir.mkpath("Sections");
        SectionDefinition sampleSec;
        sampleSec.id = "sample_rect_300x500";
        sampleSec.name = "Section Rectangulaire 300x500";
        sampleSec.category = "Concrete";
        sampleSec.shapeType = "Rectangular";
        sampleSec.version = options.version;
        sampleSec.width = 0.30;
        sampleSec.height = 0.50;
        sampleSec.area = 0.15;
        sampleSec.ix = (0.30 * 0.50 * 0.50 * 0.50) / 12.0;
        sampleSec.iy = (0.50 * 0.30 * 0.30 * 0.30) / 12.0;
        sampleSec.wx = (0.30 * 0.50 * 0.50) / 6.0;
        sampleSec.wy = (0.50 * 0.30 * 0.30) / 6.0;
        sampleSec.defaultMaterialId = "concrete_c25_30";
        sampleSec.standard.name = "Eurocode 2";
        sampleSec.visual.baseColor = "#95A5A6";

        createSectionFile(targetDir, "sample_rect_300x500.json", sampleSec, false);
    }

    if (options.includeProfiles)
    {
        dir.mkpath("Profiles");
        SectionDefinition sampleProf;
        sampleProf.id = "sample_ipe240";
        sampleProf.name = "Profilé IPE 240 Type";
        sampleProf.category = "Steel";
        sampleProf.shapeType = "IShape";
        sampleProf.version = options.version;
        sampleProf.width = 0.120;
        sampleProf.height = 0.240;
        sampleProf.webThickness = 0.0062;
        sampleProf.flangeThickness = 0.0098;
        sampleProf.filletRadius = 0.015;
        sampleProf.area = 0.003896;
        sampleProf.ix = 3.892e-5;
        sampleProf.iy = 2.836e-6;
        sampleProf.wx = 3.243e-4;
        sampleProf.wy = 4.727e-5;
        sampleProf.defaultMaterialId = "steel_s235";
        sampleProf.standard.name = "EN 10365";
        sampleProf.standard.edition = "2017";
        sampleProf.visual.baseColor = "#4682B4";
        sampleProf.visual.metallic = 0.85;
        sampleProf.visual.roughness = 0.35;

        createSectionFile(targetDir, "sample_ipe240.json", sampleProf, true);
    }

    if (options.includeCables)
    {
        dir.mkpath("Cables");
        CableCatalogDefinition sampleCable;
        sampleCable.id = "sample_strand_15_7";
        sampleCable.name = "Toron 7 fils 15.7mm Y1860";
        sampleCable.category = "PrestressingStrand";
        sampleCable.grade = "Y1860S7";
        sampleCable.version = options.version;
        sampleCable.nominalDiameter = 0.0157;
        sampleCable.metallicArea = 0.000150;
        sampleCable.linearMass = 1.18;
        sampleCable.elasticModulus = 1.95e11;
        sampleCable.density = 7850.0;
        sampleCable.characteristicStrength = 1.86e9;
        sampleCable.minimumBreakingForce = 2.79e5;
        sampleCable.defaultInitialTension = 1.95e5;
        sampleCable.tensionOnly = true;
        sampleCable.standard.name = "EN 10138-3";
        sampleCable.standard.edition = "2009";

        createCableFile(targetDir, "sample_strand_15_7.json", sampleCable);
    }

    if (options.includeTextures)
    {
        dir.mkpath("Textures");
        QJsonObject texturesObj;
        texturesObj["version"] = "1.0";
        QJsonArray texArr;

        QJsonObject texItem;
        texItem["id"] = "custom_diffuse";
        texItem["name"] = "Texture de base";
        texItem["file"] = "Textures/diffuse.png";
        texItem["type"] = "albedo";
        texArr.append(texItem);

        texturesObj["textures"] = texArr;

        QFile texFile(dir.filePath("Textures/textures.json"));
        if (texFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            texFile.write(QJsonDocument(texturesObj).toJson(QJsonDocument::Indented));
            texFile.close();
        }
    }

    if (options.includeStandards)
    {
        dir.mkpath("Standards");
        createStandardFile(targetDir, "standard_eurocode.json", "EN1990",
                           "Eurocode - Bases de calcul des structures",
                           "2002", "Principes généraux de sécurité et d'aptitude au service");
    }

    // 5. Génération du README.md explicatif
    QString readmeContent = QString(
        "# %1\n\n"
        "**Identifiant :** `%2`  \n"
        "**Version :** `%3`  \n"
        "**Auteur :** %4  \n"
        "**Licence :** %5  \n\n"
        "## Description\n"
        "%6\n\n"
        "## Structure de la Bibliothèque\n\n"
        "- `manifest.json` : Fichier obligatoire déclarant la carte d'identité de l'extension.\n"
        "- `Materials/` : Définitions JSON des matériaux structuraux (unités SI : MPa, kg/m3, 1/K).\n"
        "- `Sections/` : Profilés géométriques standards (rectangulaires, circulaires, caissons).\n"
        "- `Profiles/` : Profilés industriels métalliques normalisés (IPE, HEA, HEB, tubes).\n"
        "- `Cables/` : Torons de précontrainte, câbles clos et haubans.\n"
        "- `Textures/` : Textures PBR pour le rendu 3D réaliste OpenCASCADE.\n"
        "- `Standards/` : Références normatives officielles.\n\n"
        "## Utilisation dans TSA\n\n"
        "1. Modifiez ou ajoutez vos fichiers JSON dans les sous-dossiers correspondants.\n"
        "2. Dans TSA, ouvrez **Structure & Sections** > **Gestionnaire TSALib...**.\n"
        "3. Cliquez sur **Recharger tout** : vos nouveaux matériaux et profilés sont immédiatement disponibles !\n"
        "4. Pour distribuer cette bibliothèque, cliquez sur **Exporter (.tsalib)...**.\n"
    ).arg(QString::fromStdString(options.name),
          QString::fromStdString(options.id),
          QString::fromStdString(options.version.toString()),
          QString::fromStdString(options.author.empty() ? "Non spécifié" : options.author),
          QString::fromStdString(options.license),
          QString::fromStdString(options.description.empty() ? "Bibliothèque créée avec l'assistant TSA." : options.description));

    QFile readmeFile(dir.filePath("README.md"));
    if (readmeFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        readmeFile.write(readmeContent.toUtf8());
        readmeFile.close();
    }

    // 6. Packaging immédiat en .tsalib si demandé
    if (options.createPackage)
    {
        QString pkgOut = options.packageOutputPath;
        if (pkgOut.trimmed().isEmpty())
        {
            pkgOut = targetDir + ".tsalib";
        }
        QString packErr;
        if (!ExtensionPackager::createPackage(targetDir, pkgOut, &packErr))
        {
            if (outError)
            {
                *outError = QString("Bibliothèque créée mais échec du packaging en .tsalib : %1").arg(packErr);
            }
            return false;
        }
    }

    if (outDirectory)
    {
        *outDirectory = QDir(targetDir).canonicalPath();
        if (outDirectory->isEmpty()) *outDirectory = targetDir;
    }

    return true;
}

bool ExtensionScaffolder::createMaterialFile(const QString& extensionDir,
                                            const QString& filename,
                                            const MaterialDefinition& material,
                                            QString* outError)
{
    QDir matDir(extensionDir);
    matDir.mkpath("Materials");
    QString filePath = matDir.filePath("Materials/" + filename);

    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        if (outError) *outError = QString("Impossible d'écrire le fichier matériau : %1").arg(filePath);
        return false;
    }

    f.write(QJsonDocument(material.toJson()).toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

bool ExtensionScaffolder::createSectionFile(const QString& extensionDir,
                                           const QString& filename,
                                           const SectionDefinition& section,
                                           bool isProfile,
                                           QString* outError)
{
    QDir secDir(extensionDir);
    QString subFolder = isProfile ? "Profiles" : "Sections";
    secDir.mkpath(subFolder);
    QString filePath = secDir.filePath(subFolder + "/" + filename);

    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        if (outError) *outError = QString("Impossible d'écrire le fichier section : %1").arg(filePath);
        return false;
    }

    f.write(QJsonDocument(section.toJson()).toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

bool ExtensionScaffolder::createCableFile(const QString& extensionDir,
                                         const QString& filename,
                                         const CableCatalogDefinition& cable,
                                         QString* outError)
{
    QDir cabDir(extensionDir);
    cabDir.mkpath("Cables");
    QString filePath = cabDir.filePath("Cables/" + filename);

    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        if (outError) *outError = QString("Impossible d'écrire le fichier câble : %1").arg(filePath);
        return false;
    }

    f.write(QJsonDocument(cable.toJson()).toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

bool ExtensionScaffolder::createStandardFile(const QString& extensionDir,
                                            const QString& filename,
                                            const QString& stdId,
                                            const QString& stdName,
                                            const QString& edition,
                                            const QString& scope,
                                            QString* outError)
{
    QDir stdDir(extensionDir);
    stdDir.mkpath("Standards");
    QString filePath = stdDir.filePath("Standards/" + filename);

    QJsonObject obj;
    obj["id"] = stdId;
    obj["name"] = stdName;
    obj["edition"] = edition;
    obj["scope"] = scope;

    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        if (outError) *outError = QString("Impossible d'écrire le fichier norme : %1").arg(filePath);
        return false;
    }

    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

} // namespace TSA::ExtensionSystem
