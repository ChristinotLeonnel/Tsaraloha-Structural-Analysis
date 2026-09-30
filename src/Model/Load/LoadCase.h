#pragma once

#include "LoadEnums.h"
#include <string>

namespace TSA::Model
{

/**
 * @brief Représente un cas de charge (Load Case / Load Pattern).
 * Groupe les charges selon leur nature physique et réglementaire (Permanent, Exploitation, etc.).
 */
class LoadCase
{
public:
    LoadCase() = default;
    LoadCase(int id, const std::string& name,
             LoadCaseCategory category = LoadCaseCategory::Dead,
             bool includeSelfWeight = false,
             double selfWeightFactor = 1.0,
             const std::string& description = "");

    // Identifiants
    int id() const noexcept { return m_id; }
    void setId(int id) noexcept { m_id = id; }

    const std::string& name() const noexcept { return m_name; }
    void setName(const std::string& name) { m_name = name; }

    LoadCaseCategory category() const noexcept { return m_category; }
    void setCategory(LoadCaseCategory cat) noexcept { m_category = cat; }

    const std::string& description() const noexcept { return m_description; }
    void setDescription(const std::string& desc) { m_description = desc; }

    // Poids propre automatique
    bool isSelfWeightIncluded() const noexcept { return m_includeSelfWeight; }
    void setSelfWeightIncluded(bool inc) noexcept { m_includeSelfWeight = inc; }

    double selfWeightFactor() const noexcept { return m_selfWeightFactor; }
    void setSelfWeightFactor(double factor) noexcept { m_selfWeightFactor = factor; }

    // Tag OpenSees Pattern
    int patternTag() const noexcept { return m_id; }

private:
    int m_id = 1;
    std::string m_name = "G";
    LoadCaseCategory m_category = LoadCaseCategory::Dead;
    bool m_includeSelfWeight = false;
    double m_selfWeightFactor = 1.0;
    std::string m_description;
};

} // namespace TSA::Model
