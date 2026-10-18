#include "LoadValidation.h"
#include "../Model/Model.h"
#include "../Model/Load/LoadManager.h"

#include <QCoreApplication>

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
    QString text;
    if (isValid())
    {
        text = QCoreApplication::translate("TSA::Analysis::LoadValidation",
                                           "Validation réussie. Le modèle est prêt pour le calcul OpenSees.");
        if (hasWarnings())
        {
            text += QCoreApplication::translate("TSA::Analysis::LoadValidation", " (%1 avertissement(s)).")
                        .arg(warnings().size());
        }
    }
    else
    {
        text = QCoreApplication::translate("TSA::Analysis::LoadValidation",
                                           "Échec de validation : %1 erreur(s) détectée(s).").arg(errors().size());
    }
    return text.toStdString();
}

ValidationReport LoadValidation::validateModel(const TSA::Model::Model& model)
{
    ValidationReport report;
    const auto& lm = model.loadManager();

    // 1. Vérification de la géométrie de base
    if (model.nodes().empty())
    {
        report.addError(QCoreApplication::translate("TSA::Analysis::LoadValidation", "Géométrie").toStdString(),
                        QCoreApplication::translate("TSA::Analysis::LoadValidation",
                            "Le modèle structural ne contient aucun nœud.").toStdString());
        return report;
    }

    // Dalles et voiles : éléments structuraux calculés en coques à partir du maillage (sinon signalé par ModelValidator)
    if (model.beams().empty() && model.columns().empty() && model.trussMembers().empty() && model.cables().empty() && model.slabs().empty() &&
        model.walls().empty())
    {
        report.addWarning(QCoreApplication::translate("TSA::Analysis::LoadValidation", "Géométrie").toStdString(),
                          QCoreApplication::translate("TSA::Analysis::LoadValidation",
                              "Le modèle ne comporte aucun élément linéaire filaire.").toStdString());
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
    // Appuis linéiques des bords de dalles et de voiles (appliqués aux nœuds du maillage au calcul)
    for (const auto& [sid, sl] : model.slabs())
        if (!sl.edgeSupports().empty()) hasSupport = true;
    for (const auto& [wid, wl] : model.walls())
        if (!wl.edgeSupports().empty()) hasSupport = true;
    if (!hasSupport)
    {
        report.addError(QCoreApplication::translate("TSA::Analysis::LoadValidation", "Conditions aux Limites").toStdString(),
                        // Laissé en français : StructuralChecker (src/AI) repère ce message par le mot « appui »
                        // pour éviter un doublon de STAB_NO_SUPPORT. À traduire quand ce test sera indépendant du texte.
                        "Aucun appui (Encastrement, Articulation, Appui simple) n'est défini. La structure est cinématiquement instable.");
    }

    // 3. Validation des cas de charges
    if (lm.loadCases().empty())
    {
        report.addError(QCoreApplication::translate("TSA::Analysis::LoadValidation", "Charges").toStdString(),
                        QCoreApplication::translate("TSA::Analysis::LoadValidation",
                            "Aucun cas de charge (Load Case) n'est défini dans le modèle.").toStdString());
    }

    // 4. Validation des charges nodales
    const std::string nodalCat = QCoreApplication::translate("TSA::Analysis::LoadValidation", "Charge Nodale").toStdString();
    for (const auto& [id, nl] : lm.nodalLoads())
    {
        if (!model.getNode(nl.nodeId()))
        {
            report.addError(nodalCat,
                            QCoreApplication::translate("TSA::Analysis::LoadValidation",
                                "La charge nodale NL#%1 référence le nœud inexistant N%2")
                                .arg(id).arg(nl.nodeId()).toStdString());
        }

        if (!lm.getLoadCase(nl.loadCaseId()))
        {
            report.addError(nodalCat,
                            QCoreApplication::translate("TSA::Analysis::LoadValidation",
                                "La charge nodale NL#%1 est assignée à un cas de charge inexistant Cas#%2")
                                .arg(id).arg(nl.loadCaseId()).toStdString());
        }

        if (std::isnan(nl.fx()) || std::isnan(nl.fy()) || std::isnan(nl.fz()) ||
            std::isnan(nl.mx()) || std::isnan(nl.my()) || std::isnan(nl.mz()))
        {
            report.addError(nodalCat, QCoreApplication::translate("TSA::Analysis::LoadValidation",
                "La charge nodale NL#%1 contient une valeur non numérique (NaN).").arg(id).toStdString());
        }

        if (!nl.hasForce() && !nl.hasMoment())
        {
            report.addWarning(nodalCat, QCoreApplication::translate("TSA::Analysis::LoadValidation",
                "La charge nodale NL#%1 a une intensité nulle.").arg(id).toStdString());
        }
    }

    // 5. Validation des charges sur barres
    for (const auto& [id, ml] : lm.memberLoads())
    {
        int elemId = ml.elementId();
        bool elemExists = (model.getBeam(elemId) != nullptr) ||
                          (model.getColumn(elemId) != nullptr) ||
                          (model.getTrussMember(elemId) != nullptr) ||
                          (model.getCable(elemId) != nullptr);

        const std::string memberCat =
            QCoreApplication::translate("TSA::Analysis::LoadValidation", "Charge sur Barre").toStdString();
        if (!elemExists)
        {
            report.addError(memberCat,
                            QCoreApplication::translate("TSA::Analysis::LoadValidation",
                                "La charge ML#%1 référence l'élément inexistant #%2").arg(id).arg(elemId).toStdString());
        }

        if (!lm.getLoadCase(ml.loadCaseId()))
        {
            report.addError(memberCat,
                            QCoreApplication::translate("TSA::Analysis::LoadValidation",
                                "La charge ML#%1 est assignée au cas de charge inexistant Cas#%2")
                                .arg(id).arg(ml.loadCaseId()).toStdString());
        }

        if (std::isnan(ml.q1()) || std::isnan(ml.q2()))
        {
            report.addError(memberCat, QCoreApplication::translate("TSA::Analysis::LoadValidation",
                "La charge ML#%1 contient une intensité non numérique (NaN).").arg(id).toStdString());
        }

        if (std::abs(ml.q1()) < 1e-9 && std::abs(ml.q2()) < 1e-9)
        {
            report.addWarning(memberCat, QCoreApplication::translate("TSA::Analysis::LoadValidation",
                "La charge ML#%1 a une intensité nulle.").arg(id).toStdString());
        }
    }

    // 6. Validation des combinaisons
    const std::string comboCat = QCoreApplication::translate("TSA::Analysis::LoadValidation", "Combinaisons").toStdString();
    for (const auto& [id, combo] : lm.combinations())
    {
        if (combo.caseFactors().empty())
        {
            report.addWarning(comboCat, QCoreApplication::translate("TSA::Analysis::LoadValidation",
                "La combinaison '%1' est vide (aucun cas de charge pondéré).")
                .arg(QString::fromStdString(combo.name())).toStdString());
        }
        for (const auto& [caseId, factor] : combo.caseFactors())
        {
            if (!lm.getLoadCase(caseId))
            {
                report.addError(comboCat, QCoreApplication::translate("TSA::Analysis::LoadValidation",
                    "La combinaison '%1' référence le cas de charge inexistant Cas#%2")
                    .arg(QString::fromStdString(combo.name()), QString::number(caseId)).toStdString());
            }
        }
    }

    return report;
}

} // namespace TSA::Analysis
