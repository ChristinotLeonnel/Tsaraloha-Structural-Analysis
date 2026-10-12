#pragma once

// Banc de validation de TSA (docs/BENCHMARKS.md) : cas de référence décrits en JSON (format « tsa.benchmark/1 »),
// exécutés par la chaîne de calcul réelle (modèle TSA → AnalysisManager → moteur → ResultsModel), comparés à des
// références analytiques écrites sous forme de formules (évaluées, jamais recopiées) ou à des résultats externes
// importés (CSV / JSON, par exemple exportés d'un autre logiciel). Chaque comparaison indique la valeur de
// référence, la valeur TSA, les écarts absolu et relatif, la tolérance mixte |x − x_ref| ≤ ε_abs + ε_rel·|x_ref| et
// l'origine de la valeur (exacte ou interpolée). Indépendant de l'interface graphique.

#include <QJsonObject>
#include <QString>

#include <map>
#include <string>
#include <vector>

namespace TSA::Validation
{

/// Évaluateur d'expressions (+ − * / ^, parenthèses, sqrt, abs, sin, cos, tan, ln, exp, pi) sur des paramètres nommés.
double evaluateExpression(const std::string& expression, const std::map<std::string, double>& parameters, std::string* error = nullptr);

struct Tolerance
{
    double rel = 1e-6;
    double abs = 0.0;
    bool accepts(double value, double reference) const;
};

struct BenchmarkCheck
{
    std::string quantity;   ///< « node:2:uz », « reaction:1:fz », « member:B1:My:absmax », « modal:f1 »…
    std::string label;      ///< « flèche à mi-portée »
    std::string formula;    ///< expression analytique (paramètres du cas) ou vide (valeur numérique)
    double expected = 0.0;
    std::string unit;
    Tolerance tolerance;
    std::vector<std::string> engines;   ///< vide = tous les moteurs du cas
};

struct BenchmarkCase
{
    std::string id, title, category, description;
    std::vector<std::string> sources, hypotheses, engines;
    std::map<std::string, double> parameters;
    QJsonObject model, analysis;
    std::vector<BenchmarkCheck> checks;
    std::string file;
};

/// Lecture et validation d'un cas ; les erreurs (champ manquant, formule invalide, quantité inconnue) sont listées
/// et le cas n'est pas exécuté.
bool parseBenchmark(const QJsonObject& json, BenchmarkCase* out, std::vector<std::string>* errors);
bool loadBenchmarkFile(const QString& path, BenchmarkCase* out, std::vector<std::string>* errors);
std::vector<QString> benchmarkFiles(const QString& directory);   ///< *.json, récursif, ordre alphabétique

/// Point d'une étude de convergence en maillage : valeur obtenue par le moteur pour une taille de maille.
struct ConvergencePoint
{
    double meshSize = 0.0;      ///< taille cible transmise au mailleur (m)
    std::string meshInfo;       ///< composition du maillage réellement produit (quadrangles, triangles)
    bool available = false;     ///< calcul abouti et grandeur présente dans les résultats
    double value = 0.0;
};

struct CheckOutcome
{
    std::string caseId, engine, quantity, label, formula, unit, source;   ///< source : « analytique » ou référence externe
    double reference = 0.0, value = 0.0;
    double absError = 0.0, relError = 0.0;   ///< relError = NaN si |référence| ≈ 0
    Tolerance tolerance;
    bool available = false;    ///< valeur obtenue par TSA
    bool interpolated = false; ///< valeur interpolée entre deux stations (et non une valeur du moteur)
    bool passed = false;
    std::string note;
    /// Étude de convergence (cas déclarant « meshSizes ») : un point par taille, dans l'ordre du cas ; la vérification
    /// porte sur le dernier (le plus fin). Vide pour un calcul unique.
    std::vector<ConvergencePoint> convergence;
};

struct CaseOutcome
{
    std::string caseId, title, category, engine;
    bool ran = false, skipped = false;
    std::string message;          ///< échec du calcul, moteur indisponible…
    double seconds = 0.0;
    std::vector<CheckOutcome> checks;
    bool passed() const;
};

struct ExternalReference
{
    std::string caseId, quantity, source, unit;
    double value = 0.0;
    Tolerance tolerance;
    bool hasTolerance = false;
};

/// Références externes : CSV « case,quantity,value[,unit][,source][,rel_tol][,abs_tol] » (en-tête obligatoire) ou JSON
/// { "format": "tsa.reference/1", "references": [ { "case", "quantity", "value", ... } ] }.
std::vector<ExternalReference> loadExternalReferences(const QString& path, std::vector<std::string>* errors);

struct RunOptions
{
    std::vector<std::string> engines;   ///< filtre (vide = moteurs déclarés par chaque cas)
    std::vector<ExternalReference> external;
    Tolerance externalDefault { 0.02, 1e-9 };   ///< 2 % par défaut pour une référence externe sans tolérance
};

std::vector<CaseOutcome> runBenchmark(const BenchmarkCase& benchmark, const RunOptions& options = {});

struct BenchmarkReport
{
    std::string tsaVersion, engineVersions, timestamp, machine;
    std::vector<CaseOutcome> cases;
    int checksPassed() const;
    int checksFailed() const;
    int checksUnavailable() const;
    int casesSkipped() const;
};

QString reportMarkdown(const BenchmarkReport& report);
QString reportCsv(const BenchmarkReport& report);
QJsonObject reportJson(const BenchmarkReport& report);
/// Écrit report-<horodatage>.{md,csv,json} et latest.json dans directory (l'historique n'est jamais écrasé).
bool writeReport(const BenchmarkReport& report, const QString& directory, QString* error = nullptr);

} // namespace TSA::Validation
