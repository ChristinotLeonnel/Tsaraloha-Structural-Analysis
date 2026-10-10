#pragma once

// Paquet de templates exportable « .tsatemplate » (docs/TEMPLATES.md).
//
// Un fichier JSON unique, autonome et portable :
//   { "format": "tsa-template-package/1",
//     "manifest": { id, name, version, description, author, license, engine, dataSchema, applications,
//                   reports: [{ id, name, description, entry, requires }], options: [...], basedOn, baseHash },
//     "files": { "main.html": { "text": "…" }, "logo.png": { "base64": "…" } } }
// Les paquets intégrés et les sources d'un auteur peuvent aussi être un DOSSIER (manifest.json + fichiers) ;
// l'export produit toujours le fichier unique.
//
// Un paquet est une DONNÉE : texte de template, styles, images. La validation refuse tout ce qui
// permettrait d'exécuter du code ou de rendre le document dépendant de l'extérieur (scripts, gestionnaires
// d'événements, javascript:, objets embarqués, ressources distantes, @import), les chemins hors du paquet,
// les types de fichiers non prévus, les syntaxes de template invalides et les partiels absents.

#include "TemplateEngine.h"

#include <QByteArray>
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>

#include <vector>

namespace TSA::Templates
{

inline constexpr char kPackageFormat[] = "tsa-template-package/1";
inline constexpr char kEngineVersion[] = "tsa-template/1";
inline constexpr char kPackageExtension[] = ".tsatemplate";

struct TemplateOption
{
    QString id, label;
    QString type;               ///< text, color, bool, number, choice, image (URI data:image/… fourni par l'hôte)
    QJsonValue defaultValue;
    QStringList choices;        ///< type choice
};

struct TemplateReport
{
    QString id, name, description;
    QString entry;              ///< fichier principal (HTML)
    QStringList required;       ///< (JSON « requires ») chemins de données obligatoires (« project.title », « model.nodes »…)
};

struct TemplateManifest
{
    QString id, name, version, description, author, license;
    QString engine = QString::fromLatin1(kEngineVersion);
    QString dataSchema;         ///< « tsa-report-data/1 » (modèle et résultats) ou « tsa-ndc/1 » (note structurée)
    QStringList applications;   ///< TSA, TSALab (vide : toutes)
    std::vector<TemplateReport> reports;
    std::vector<TemplateOption> options;
    QString basedOn;            ///< copie personnalisée : « id@version » d'origine
    QString baseHash;           ///< empreinte du contenu d'origine (détection des modifications)
    QJsonObject raw;            ///< manifeste complet (champs inconnus conservés à l'export)

    const TemplateReport* report(const QString& reportId) const;
};

class TemplatePackage
{
public:
    static bool loadFile(const QString& path, TemplatePackage* out, QStringList* errors);
    /// Dossier « manifest.json » + fichiers (sources d'auteur, paquets intégrés dans les ressources Qt).
    static bool loadDirectory(const QString& dir, TemplatePackage* out, QStringList* errors);
    static bool fromJson(const QJsonObject& json, TemplatePackage* out, QStringList* errors);

    QJsonObject toJson() const;
    bool saveFile(const QString& path, QString* error = nullptr) const;

    /// Validation complète (structure, sécurité, chemins, syntaxe, partiels, options). Vide = valide.
    QStringList validate() const;
    /// Empreinte SHA-256 du contenu (manifeste hors basedOn/baseHash, fichiers) : détecte les modifications.
    QString contentHash() const;

    const TemplateManifest& manifest() const { return m_manifest; }
    TemplateManifest& manifest() { return m_manifest; }
    QStringList files() const { return m_files.keys(); }
    bool hasFile(const QString& name) const { return m_files.contains(name); }
    QByteArray fileBytes(const QString& name) const { return m_files.value(name); }
    QString fileText(const QString& name) const { return QString::fromUtf8(m_files.value(name)); }
    void setFile(const QString& name, const QByteArray& bytes) { m_files.insert(name, bytes); }
    void removeFile(const QString& name) { m_files.remove(name); }

    /// Partiel « nom » : fichier exact, sinon partials/nom.html, sinon nom.html.
    std::optional<QString> resolvePartial(const QString& name) const;
    /// Chemins requis absents des données.
    QStringList missingRequired(const QString& reportId, const QJsonObject& data) const;
    /// Valeurs d'options effectives : défauts du manifeste + valeurs fournies valides (*errors sinon).
    QJsonObject effectiveOptions(const QJsonObject& values, QStringList* errors = nullptr) const;

    /// Rendu d'un rapport du paquet. Les données reçoivent en plus « options » (effectives), « assets »
    /// (images du paquet en URI data:, clé = chemin avec « / » et « . » remplacés par « _ ») et « template »
    /// (id, nom, version). Refus explicite si des données obligatoires manquent.
    RenderResult render(const QString& reportId, const QJsonObject& data, const QJsonObject& optionValues = {}) const;

private:
    TemplateManifest m_manifest;
    QMap<QString, QByteArray> m_files;
};

/// Valeur au chemin pointé « a.b.c » (indéfinie si absente).
QJsonValue valueAtPath(const QJsonObject& data, const QString& path);

} // namespace TSA::Templates
