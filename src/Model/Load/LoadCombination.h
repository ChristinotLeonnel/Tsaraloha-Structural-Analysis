#pragma once

// ============================================================
// STRUCTURAL STANDARD
// Standard   : EN 1990:2002+A1:2005 (Eurocode 0 - Bases de calcul)
// Clause     : §6.4.3 (Expressions 6.10, 6.10a/b) & §6.5.3 (ELS)
// Requirement: REQ-CALC-EC0-001 (Load Combinations ULS / SLS)
// Test ID    : TSA_LoadsTests / TSA_OpenSeesTests
// ============================================================

#include "LoadEnums.h"
#include "LoadCase.h"
#include <string>
#include <map>
#include <sstream>
#include <iomanip>

namespace TSA::Model
{

/**
 * @brief Représente une combinaison d'actions structurales (Load Combination)
 * selon les normes Eurocodes (EN 1990) ou personnalisée.
 * Exemple : 1.35 G + 1.50 Q
 */
class LoadCombination
{
public:
    LoadCombination() = default;
    LoadCombination(int id, const std::string& name,
                    LoadCombinationType type = LoadCombinationType::ULS_Fundamental);
    LoadCombination(int id, const std::string& name,
                    LoadCombinationType type,
                    const std::map<int, double>& caseFactors,
                    const std::string& description = "")
        : m_id(id)
        , m_name(name)
        , m_type(type)
        , m_caseFactors(caseFactors)
    {
        (void)description;
    }

    // Identifiants
    int id() const noexcept { return m_id; }
    void setId(int id) noexcept { m_id = id; }

    const std::string& name() const noexcept { return m_name; }
    void setName(const std::string& name) { m_name = name; }

    LoadCombinationType type() const noexcept { return m_type; }
    void setType(LoadCombinationType type) noexcept { m_type = type; }

    // Coefficients des cas de charge
    const std::map<int, double>& caseFactors() const noexcept { return m_caseFactors; }
    void setCaseFactors(const std::map<int, double>& factors) { m_caseFactors = factors; }

    void setFactor(int loadCaseId, double factor)
    {
        if (std::abs(factor) < 1e-9)
        {
            m_caseFactors.erase(loadCaseId);
        }
        else
        {
            m_caseFactors[loadCaseId] = factor;
        }
    }

    double factor(int loadCaseId) const
    {
        auto it = m_caseFactors.find(loadCaseId);
        return (it != m_caseFactors.end()) ? it->second : 0.0;
    }

    bool hasCase(int loadCaseId) const
    {
        return m_caseFactors.find(loadCaseId) != m_caseFactors.end();
    }

    void removeCase(int loadCaseId)
    {
        m_caseFactors.erase(loadCaseId);
    }

    void clear()
    {
        m_caseFactors.clear();
    }

    // Formule lisible (ex: "1.35*G + 1.50*Q")
    std::string formula(const std::map<int, LoadCase>& allCases) const;

private:
    int m_id = 1;
    std::string m_name;
    LoadCombinationType m_type = LoadCombinationType::ULS_Fundamental;
    std::map<int, double> m_caseFactors; // loadCaseId -> factor
};

} // namespace TSA::Model
