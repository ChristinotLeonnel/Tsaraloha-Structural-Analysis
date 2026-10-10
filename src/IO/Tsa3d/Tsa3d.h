#pragma once

// TSA3D : format d'échange 3D documenté et versionné de TSA / TSALab (spécification : docs/TSA3D.md,
// schéma : docs/schemas/tsa3d-1.schema.json). JSON UTF-8 ; un seul espace d'identifiants par document.
//
// Composants (sans interface graphique, couche modèle) :
//   Tsa3d::read       octets → document JSON (erreur de syntaxe avec position)
//   Tsa3d::validate   document → rapport (format, version, champs requis, types, unités, identifiants en
//                     double, références, géométrie incohérente)
//   Tsa3d::importDocument  document validé → modèle TSA (instantané : identifiants conservés quand ils
//                     suivent la convention TSA), champs inconnus conservés (Model::exchangeExtensionsJson)
//   Tsa3d::exportModel     modèle (+ résultats facultatifs) → document, pertes éventuelles signalées
//
// Les données d'entrée et les résultats sont séparés : un bloc « results » importé n'est jamais chargé
// comme résultats de calcul valides ; un bloc « mesh » importé n'est jamais appliqué au modèle.

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <map>
#include <string>
#include <vector>

namespace TSA::Model
{
class Model;
}
namespace TSA::Analysis
{
class ResultsModel;
}

namespace TSA::IO::Tsa3d
{

inline constexpr char kFormatName[] = "TSA3D";
inline constexpr int kVersionMajor = 1;
inline constexpr int kVersionMinor = 0;
QString versionString();   ///< « 1.0 »

enum class Severity
{
    Info,
    Warning,
    Error
};

struct Issue
{
    Severity severity = Severity::Error;
    QString path;      ///< chemin JSON, ex. « members[3].nodes[1] »
    QString message;
};

struct Report
{
    std::vector<Issue> issues;
    void error(const QString& path, const QString& msg) { issues.push_back({ Severity::Error, path, msg }); }
    void warning(const QString& path, const QString& msg) { issues.push_back({ Severity::Warning, path, msg }); }
    void info(const QString& path, const QString& msg) { issues.push_back({ Severity::Info, path, msg }); }
    bool hasErrors() const;
    int count(Severity s) const;
    /// Une ligne par problème : « ERREUR members[3].nodes[1] : nœud « N9 » inconnu ».
    QStringList lines() const;
};

/// Analyse le texte JSON ; false + rapport (ligne, colonne) si la syntaxe est invalide.
bool read(const QByteArray& bytes, QJsonObject* document, Report* report);
bool readFile(const QString& path, QJsonObject* document, Report* report);

/// Validation complète. Version de format majeure supérieure : refusée (aucune migration destructive) ;
/// version mineure supérieure : acceptée avec avertissement (champs inconnus conservés).
Report validate(const QJsonObject& document);

struct ImportOptions
{
    bool keepUnknownFields = true;   ///< conserver champs et extensions inconnus dans le modèle
};

struct ImportResult
{
    bool ok = false;
    Report report;
    std::map<std::string, std::string> idMap;   ///< identifiant TSA3D → « famille:id interne »
    int nodes = 0, members = 0, surfaces = 0, foundations = 0, loadCases = 0, loads = 0, combinations = 0;
};

/// Remplace le contenu du modèle par celui du document (comme l'ouverture d'un projet). Le document est
/// validé d'abord : en cas d'erreur, le modèle n'est pas modifié.
ImportResult importDocument(const QJsonObject& document, TSA::Model::Model& model, const ImportOptions& options = {});

struct ExportOptions
{
    QString projectName;
    QString description;
    const TSA::Analysis::ResultsModel* results = nullptr;   ///< résultats à joindre (seulement s'ils sont valides)
    bool includeMesh = true;        ///< maillage réellement transmis au moteur (si résultats valides)
    bool includeResults = false;    ///< déplacements et réactions (statut « calculé, non validé »)
};

struct ExportResult
{
    QJsonObject document;
    Report report;                  ///< pertes : données TSA non représentées en TSA3D v1
};

ExportResult exportModel(const TSA::Model::Model& model, const ExportOptions& options = {});
bool writeFile(const QString& path, const QJsonObject& document, QString* error = nullptr);

} // namespace TSA::IO::Tsa3d
