#pragma once

#include <string>
#include <vector>
#include <cmath>

namespace TSA::Standards
{

/**
 * @brief Données et résultats de validation d'un cas de référence analytique (Benchmark).
 * [NORM: ISO/IEC/IEEE 29119, EN 1990 §6, ISO 9001:2015]
 */
struct BenchmarkCase
{
    std::string id;              ///< Identifiant unique normalisé (ex: "EC3_STEEL_BEAM_001")
    std::string title;           ///< Intitulé du cas de charge et géométrie
    std::string standardRef;     ///< Norme de référence (ex: "EN 1993-1-1 §6.2")
    std::string formula;         ///< Expression analytique exacte
    std::string checkedQuantity; ///< Grandeur physique auditée
    double expectedValue = 0.0;  ///< Valeur théorique exacte
    double calculatedValue = 0.0;///< Valeur numérique obtenue par calcul FEM
    double tolerance = 0.01;     ///< Seuil de tolérance relative (ex: 0.01 = 1%)
    std::string unit;            ///< Unité physique SI (ex: "kN·m", "kN", "mm")
    bool passed = false;         ///< Statut de vérification numérique

    double relativeError() const
    {
        if (std::abs(expectedValue) < 1e-12) return std::abs(calculatedValue);
        return std::abs(calculatedValue - expectedValue) / std::abs(expectedValue);
    }
};

/**
 * @brief Registre central des benchmarks de validation numérique et analytique pour TSA.
 */
class AnalyticalBenchmarkRegistry
{
public:
    static AnalyticalBenchmarkRegistry& instance();

    const std::vector<BenchmarkCase>& allBenchmarks() const noexcept { return m_cases; }
    void registerBenchmark(const BenchmarkCase& benchmark);
    void clear();

    size_t totalCount() const noexcept { return m_cases.size(); }
    size_t passedCount() const noexcept;

    /// Génère un rapport Markdown tabulé de conformité des cas de référence analytiques
    std::string generateReportMarkdown() const;

    // Usines d'évaluation analytique
    static BenchmarkCase evaluateBeamPointLoad(const std::string& id, double P_kN, double L_m, double actualM_kNm);
    static BenchmarkCase evaluateBeamUniformLoad(const std::string& id, double q_kNm, double L_m, double actualM_kNm);
    static BenchmarkCase evaluateCantileverEndLoad(const std::string& id, double P_kN, double L_m, double actualM_kNm);

private:
    AnalyticalBenchmarkRegistry();
    std::vector<BenchmarkCase> m_cases;
};

} // namespace TSA::Standards
