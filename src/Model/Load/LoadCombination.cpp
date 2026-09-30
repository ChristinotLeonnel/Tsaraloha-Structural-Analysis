#include "LoadCombination.h"

namespace TSA::Model
{

LoadCombination::LoadCombination(int id, const std::string& name, LoadCombinationType type)
    : m_id(id)
    , m_name(name)
    , m_type(type)
{
    if (m_name.empty() && m_id > 0)
    {
        m_name = "Comb" + std::to_string(m_id);
    }
}

std::string LoadCombination::formula(const std::map<int, LoadCase>& allCases) const
{
    if (m_caseFactors.empty())
    {
        return "(Vide)";
    }

    std::ostringstream oss;
    bool first = true;
    for (const auto& [caseId, coef] : m_caseFactors)
    {
        if (!first)
        {
            if (coef >= 0.0)
                oss << " + ";
            else
                oss << " - ";
        }
        else if (coef < 0.0)
        {
            oss << "-";
        }

        double absCoef = std::abs(coef);
        if (std::abs(absCoef - 1.0) > 1e-4)
        {
            oss << std::fixed << std::setprecision(2) << absCoef << "*";
        }

        auto it = allCases.find(caseId);
        if (it != allCases.end())
        {
            oss << it->second.name();
        }
        else
        {
            oss << "Cas#" << caseId;
        }

        first = false;
    }

    return oss.str();
}

} // namespace TSA::Model
