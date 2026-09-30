#include "LoadValidation.h"
#include "../Model/Model.h"
#include "../Model/Load/LoadManager.h"

#include <cmath>
#include <sstream>

namespace TSA::Analysis
{

bool ValidationReport::hasWarnings() const noexcept
{
    for (const auto& msg : m_messages)
    {
        if (msg.severity == ValidationSeverity::Warning)
            return true;
    }
    return false;
}

std::vector<std::string> ValidationReport::errors() const
{
    std::vector<std::string> errs;
    for (const auto& msg : m_messages)
    {
        if (msg.severity == ValidationSeverity::Error)
            errs.push_back("[" + msg.category + "] " + msg.message);
    }
    return errs;
}

std::vector<std::string> ValidationReport::warnings() const
{
    std::vector<std::string> warns;
    for (const auto& msg : m_messages)
    {
        if (msg.severity == ValidationSeverity::Warning)
            warns.push_back("[" + msg.category + "] " + msg.message);
    }
    return warns;
}

std::string ValidationReport::summary() const
{
    std::ostringstream oss;
    if (isValid())
    {
        oss << "Validation réussie. Le modèle est prêt pour le calcul OpenSees.";
        if (hasWarnings())
        {
            oss << " (" << warnings().size() << " avertissement(s)).";
        }
    }
    else
    {
        oss << "Échec de validation : " << errors().size() << " erreur(s) détectée(s).";
    }
    return oss.str();
}

ValidationReport LoadValidation::validateModel(const TSA::Model::Model& model)
{
    ValidationReport report;
    const auto& lm = model.loadManager();

    // 1. Vérification de la géométrie de base
    if (model.nodes().empty())
    {
        report.addError("Géométrie", "Le modèle structural ne contient aucun nœud.");
        return report;
    }

    if (model.beams().empty() && model.columns().empty() && model.trussMembers().empty())
    {
        report.addWarning("Géométrie", "Le modèle ne comporte aucun élément linéaire filaire.");
    }

    // 2. Vérification des conditions aux limites (appuis)
    bool hasSupport = false;
    for (const auto& [id, n] : model.nodes())
    {
        if (n.supportType() != TSA::Model::SupportType::Free)
        {
            hasSupport = true;
            break;
        }
    }
    if (!hasSupport)
    {
        report.addError("Conditions aux Limites",
                        "Aucun appui (Encastrement, Articulation, Appui simple) n'est défini. La structure est cinématiquement instable.");
    }

    // 3. Validation des cas de charges
    if (lm.loadCases().empty())
    {
        report.addError("Charges", "Aucun cas de charge (Load Case) n'est défini dans le modèle.");
    }

    // 4. Validation des charges nodales
    for (const auto& [id, nl] : lm.nodalLoads())
    {
        if (!model.getNode(nl.nodeId()))
        {
            report.addError("Charge Nodale",
                            "La charge nodale NL#" + std::to_string(id) + " référence le nœud inexistant N" + std::to_string(nl.nodeId()));
        }

        if (!lm.getLoadCase(nl.loadCaseId()))
        {
            report.addError("Charge Nodale",
                            "La charge nodale NL#" + std::to_string(id) + " est assignée à un cas de charge inexistant Cas#" + std::to_string(nl.loadCaseId()));
        }

        if (std::isnan(nl.fx()) || std::isnan(nl.fy()) || std::isnan(nl.fz()) ||
            std::isnan(nl.mx()) || std::isnan(nl.my()) || std::isnan(nl.mz()))
        {
            report.addError("Charge Nodale", "La charge nodale NL#" + std::to_string(id) + " contient une valeur non numérique (NaN).");
        }

        if (!nl.hasForce() && !nl.hasMoment())
        {
            report.addWarning("Charge Nodale", "La charge nodale NL#" + std::to_string(id) + " a une intensité nulle.");
        }
    }

    // 5. Validation des charges sur barres
    for (const auto& [id, ml] : lm.memberLoads())
    {
        int elemId = ml.elementId();
        bool elemExists = (model.getBeam(elemId) != nullptr) ||
                          (model.getColumn(elemId) != nullptr) ||
                          (model.getTrussMember(elemId) != nullptr);

        if (!elemExists)
        {
            report.addError("Charge sur Barre",
                            "La charge ML#" + std::to_string(id) + " référence l'élément inexistant #" + std::to_string(elemId));
        }

        if (!lm.getLoadCase(ml.loadCaseId()))
        {
            report.addError("Charge sur Barre",
                            "La charge ML#" + std::to_string(id) + " est assignée au cas de charge inexistant Cas#" + std::to_string(ml.loadCaseId()));
        }

        if (std::isnan(ml.q1()) || std::isnan(ml.q2()))
        {
            report.addError("Charge sur Barre", "La charge ML#" + std::to_string(id) + " contient une intensité non numérique (NaN).");
        }

        if (std::abs(ml.q1()) < 1e-9 && std::abs(ml.q2()) < 1e-9)
        {
            report.addWarning("Charge sur Barre", "La charge ML#" + std::to_string(id) + " a une intensité nulle.");
        }
    }

    // 6. Validation des combinaisons
    for (const auto& [id, combo] : lm.combinations())
    {
        if (combo.caseFactors().empty())
        {
            report.addWarning("Combinaisons", "La combinaison '" + combo.name() + "' est vide (aucun cas de charge pondéré).");
        }
        for (const auto& [caseId, factor] : combo.caseFactors())
        {
            if (!lm.getLoadCase(caseId))
            {
                report.addError("Combinaisons", "La combinaison '" + combo.name() + "' référence le cas de charge inexistant Cas#" + std::to_string(caseId));
            }
        }
    }

    return report;
}

} // namespace TSA::Analysis
