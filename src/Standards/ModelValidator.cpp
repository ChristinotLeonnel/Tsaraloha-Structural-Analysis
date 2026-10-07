#include "ModelValidator.h"
#include "../Analysis/CalculationSnapshot.h"
#include "../Model/Model.h"
#include "../Model/Node.h"
#include "../Model/Beam.h"
#include "../Model/Column.h"
#include "../Model/TrussMember.h"
#include "../Model/Cable/Cable.h"
#include "../Model/Slab.h"
#include "../Model/Wall.h"
#include "../Model/Section.h"
#include "../Model/Material.h"
#include "../Analysis/LoadValidation.h"
#include "../Analysis/OpenSeesAnalysisBuilder.h"

#include <cmath>
#include <sstream>
#include <set>

namespace TSA::Standards
{

bool ModelValidationReport::hasWarnings() const noexcept
{
    for (const auto& issue : m_issues)
    {
        if (issue.severity == ValidationSeverity::Warning) return true;
    }
    return false;
}

size_t ModelValidationReport::errorCount() const
{
    size_t cnt = 0;
    for (const auto& issue : m_issues)
    {
        if (issue.severity == ValidationSeverity::Error) ++cnt;
    }
    return cnt;
}

size_t ModelValidationReport::warningCount() const
{
    size_t cnt = 0;
    for (const auto& issue : m_issues)
    {
        if (issue.severity == ValidationSeverity::Warning) ++cnt;
    }
    return cnt;
}

std::vector<std::string> ModelValidationReport::formattedErrors() const
{
    std::vector<std::string> errs;
    for (const auto& issue : m_issues)
    {
        if (issue.severity == ValidationSeverity::Error)
        {
            std::string msg = "[" + issue.category + "] ";
            if (issue.entityId > 0) msg += "(ID " + std::to_string(issue.entityId) + ") ";
            msg += issue.message;
            if (!issue.normativeRef.empty()) msg += " [Réf: " + issue.normativeRef + "]";
            errs.push_back(msg);
        }
    }
    return errs;
}

std::vector<std::string> ModelValidationReport::formattedWarnings() const
{
    std::vector<std::string> warns;
    for (const auto& issue : m_issues)
    {
        if (issue.severity == ValidationSeverity::Warning)
        {
            std::string msg = "[" + issue.category + "] ";
            if (issue.entityId > 0) msg += "(ID " + std::to_string(issue.entityId) + ") ";
            msg += issue.message;
            warns.push_back(msg);
        }
    }
    return warns;
}

QString ModelValidationReport::summary() const
{
    if (isValid())
    {
        if (hasWarnings())
        {
            return QStringLiteral("Validation réussie : Modèle conforme aux normes avec %1 avertissement(s).")
                .arg(warningCount());
        }
        return QStringLiteral("Validation réussie : Modèle structural 100% conforme et intègre.");
    }
    return QStringLiteral("Échec de validation : %1 erreur(s) et %2 avertissement(s) détectés.")
        .arg(errorCount()).arg(warningCount());
}

bool ModelValidator::validateSection(const TSA::Model::Section& s, std::string* errorMsg)
{
    double A = s.area();
    double Iy = s.iy();
    double Iz = s.iz();

    if (std::isnan(A) || std::isinf(A) || A <= 0.0)
    {
        if (errorMsg) *errorMsg = "L'aire de section A est invalide (doit être > 0 et finie).";
        return false;
    }
    if (std::isnan(Iy) || std::isinf(Iy) || Iy <= 0.0)
    {
        if (errorMsg) *errorMsg = "Le moment quadratique Iy est invalide (doit être > 0).";
        return false;
    }
    if (std::isnan(Iz) || std::isinf(Iz) || Iz <= 0.0)
    {
        if (errorMsg) *errorMsg = "Le moment quadratique Iz est invalide (doit être > 0).";
        return false;
    }
    return true;
}

bool ModelValidator::validateMaterial(const TSA::Model::Material& m, std::string* errorMsg)
{
    double E = m.E;
    double nu = m.nu;
    double rho = m.density;

    if (std::isnan(E) || std::isinf(E) || E <= 0.0)
    {
        if (errorMsg) *errorMsg = "Le module d'élasticité E est invalide (doit être > 0 et fini).";
        return false;
    }
    if (std::isnan(nu) || std::isinf(nu) || nu < 0.0 || nu >= 0.5)
    {
        if (errorMsg) *errorMsg = "Le coefficient de Poisson nu doit être dans l'intervalle physique [0.0, 0.5[.";
        return false;
    }
    if (std::isnan(rho) || std::isinf(rho) || rho <= 0.0)
    {
        if (errorMsg) *errorMsg = "La masse volumique rho doit être positive.";
        return false;
    }
    return true;
}

bool ModelValidator::validateNode(const TSA::Model::Node& n, std::string* errorMsg)
{
    if (n.id() <= 0)
    {
        if (errorMsg) *errorMsg = "L'identifiant du nœud doit être un entier strictement positif.";
        return false;
    }
    if (std::isnan(n.x()) || std::isinf(n.x()) ||
        std::isnan(n.y()) || std::isinf(n.y()) ||
        std::isnan(n.z()) || std::isinf(n.z()))
    {
        if (errorMsg) *errorMsg = "Les coordonnées du nœud contiennent des valeurs NaN ou infinies.";
        return false;
    }
    return true;
}

ModelValidationReport ModelValidator::validate(const TSA::Model::Model& model)
{
    ModelValidationReport report;

    // 1. Contrôle des nœuds
    const auto& nodes = model.nodes();
    if (nodes.empty())
    {
        report.addError("Géométrie", "Le modèle structural ne contient aucun nœud.", 0, "EN 1990");
        return report;
    }

    bool hasSupport = false;

    // Doublons géométriques (tolérance 1 mm), hachage spatial : O(N log N) au lieu de O(N²).
    for (const auto& [dupId, keeperId] : model.findCoincidentNodes(1e-3))
    {
        report.addWarning("Nœuds",
            "Nœud géométriquement coïncident avec le nœud " + std::to_string(keeperId) +
            " (distance < 1 mm). Utiliser « Fusionner les nœuds confondus ».", dupId);
    }

    for (const auto& [id, n] : nodes)
    {
        std::string nodeErr;
        if (!validateNode(n, &nodeErr))
        {
            report.addError("Nœuds", nodeErr, id, "ISO/IEC 25010 §4.2.5");
        }


        // Détection des appuis
        if (n.support().isSupported() || n.supportType() != TSA::Model::SupportType::Free)
        {
            hasSupport = true;
        }
    }

    // 2. Contrôle de stabilité cinématique globale
    if (!hasSupport)
    {
        report.addError("Conditions aux Limites",
            "Aucun appui (Encastrement, Articulation, Appui simple ou Ressort) n'est défini. "
            "La structure est cinématiquement instable.", 0, "EN 1990 §2.1");
    }

    // 3. Contrôle des éléments linéaires (Poutres, Poteaux, Bielles, Câbles)
    auto validateLinear = [&](int elemId, int startId, int endId,
                              const TSA::Model::Section& sec, const TSA::Model::Material& mat,
                              const std::string& typeName)
    {
        if (startId <= 0 || endId <= 0 || startId == endId)
        {
            report.addError(typeName, "Les nœuds d'extrémité sont identiques ou invalides.", elemId, "RDM");
            return;
        }

        auto itStart = nodes.find(startId);
        auto itEnd = nodes.find(endId);
        if (itStart == nodes.end() || itEnd == nodes.end())
        {
            report.addError(typeName, "Fait référence à un nœud inexistant dans le modèle.", elemId);
            return;
        }

        TSA::Coordinate::Point3D p1(itStart->second.x(), itStart->second.y(), itStart->second.z());
        TSA::Coordinate::Point3D p2(itEnd->second.x(), itEnd->second.y(), itEnd->second.z());
        double length = p1.distance(p2);

        if (length < 1e-4) // < 0.1 mm
        {
            report.addError(typeName, "La longueur de l'élément est nulle ou inférieure à 0.1 mm.", elemId, "RDM");
        }

        std::string secErr;
        if (!validateSection(sec, &secErr))
        {
            report.addError("Sections", secErr, elemId, "EN 1990");
        }

        std::string matErr;
        if (!validateMaterial(mat, &matErr))
        {
            report.addError("Matériaux", matErr, elemId, "EN 1992 / EN 1993");
        }
    };

    for (const auto& [id, b] : model.beams())
        validateLinear(id, b.startNodeId(), b.endNodeId(), b.section(), b.material(), "Poutre");

    for (const auto& [id, c] : model.columns())
        validateLinear(id, c.startNodeId(), c.endNodeId(), c.section(), c.material(), "Poteau");

    for (const auto& [id, t] : model.trussMembers())
        validateLinear(id, t.startNodeId(), t.endNodeId(), t.section(), t.material(), "Treillis");

    for (const auto& [id, cb] : model.cables())
        validateLinear(id, cb.startNodeId(), cb.endNodeId(), cb.section(), cb.material(), "Câble");

    // 4. Contrôle des éléments surfaciques (Dalles, Voiles)
    for (const auto& [id, sl] : model.slabs())
    {
        if (sl.nodeIds().size() < 3)
        {
            report.addError("Dalles", "Une dalle requiert au moins 3 nœuds de contour.", id);
        }
        if (sl.thickness() <= 0.0 || std::isnan(sl.thickness()))
        {
            report.addError("Dalles", "L'épaisseur de la dalle doit être strictement positive.", id);
        }
    }

    for (const auto& [id, w] : model.walls())
    {
        if (w.startNodeId() <= 0 || w.endNodeId() <= 0 || w.startNodeId() == w.endNodeId())
        {
            report.addError("Voiles", "Les nœuds de base du voile sont invalides ou identiques.", id);
        }
        if (w.height() <= 0.0 || std::isnan(w.height()))
        {
            report.addError("Voiles", "La hauteur du voile doit être strictement positive.", id);
        }
        if (w.thickness() <= 0.0 || std::isnan(w.thickness()))
        {
            report.addError("Voiles", "L'épaisseur du voile doit être strictement positive.", id);
        }
    }

    // 5. Validation des charges et combinaisons
    auto loadRep = TSA::Analysis::LoadValidation::validateModel(model);
    for (const auto& err : loadRep.errors())
    {
        report.addError("Charges", err, 0, "EN 1991");
    }
    for (const auto& warn : loadRep.warnings())
    {
        report.addWarning("Charges", warn);
    }

    return report;
}

ModelValidationReport ModelValidator::validateForAnalysis(const TSA::Model::Model& model, const TSA::Analysis::AnalysisParameters& params)
{
    // 1. Validation de base géométrique et physique du modèle
    ModelValidationReport report = validate(model);

    // 2. Vérification de la présence d'éléments porteurs
    size_t totalElements = model.beams().size() + model.columns().size() +
                           model.trussMembers().size() + model.cables().size();
    if (totalElements == 0)
    {
        report.addError("Modèle Calcul",
            "Aucun élément structural filaire (Poutre, Poteau, Treillis ou Câble) n'est présent pour le calcul.",
            0, "EN 1990");
    }

    // 2b. Dalles et voiles : pas de maillage EF, non transmis au solveur (BUG-002)
    if (!model.slabs().empty() || !model.walls().empty())
    {
        report.addWarning("Modèle Calcul",
            std::to_string(model.slabs().size()) + " dalle(s) et " + std::to_string(model.walls().size()) +
            " voile(s) ne sont pas pris en compte par le calcul (ni rigidité, ni charges) : "
            "seuls les éléments filaires sont transmis à OpenSees.", 0, "EN 1990 §5.1");
    }

    // 2c. Charges sur barre : chaque charge doit désigner un élément calculé (famille + id).
    {
        const auto snap = TSA::Analysis::CalculationSnapshot::capture(model);
        for (const auto& ml : snap.memberLoads())
        {
            if (!snap.findElementForLoad(ml))
            {
                report.addWarning("Charges",
                    "La charge sur barre #" + std::to_string(ml.id()) + " (élément " + std::to_string(ml.elementId())
                    + ") ne désigne aucun élément calculé de façon univoque : elle sera ignorée par le calcul.",
                    ml.id(), "EN 1991");
            }
        }
    }

    // 3. Détection des nœuds orphelins (ni appui, ni charge, ni relié à aucun élément)
    std::set<int> connectedNodes;
    for (const auto& [id, b] : model.beams()) { connectedNodes.insert(b.startNodeId()); connectedNodes.insert(b.endNodeId()); }
    for (const auto& [id, c] : model.columns()) { connectedNodes.insert(c.startNodeId()); connectedNodes.insert(c.endNodeId()); }
    for (const auto& [id, t] : model.trussMembers()) { connectedNodes.insert(t.startNodeId()); connectedNodes.insert(t.endNodeId()); }
    for (const auto& [id, cb] : model.cables()) { connectedNodes.insert(cb.startNodeId()); connectedNodes.insert(cb.endNodeId()); }
    for (const auto& [id, s] : model.slabs()) { for (int nid : s.nodeIds()) connectedNodes.insert(nid); }
    for (const auto& [id, w] : model.walls()) { connectedNodes.insert(w.startNodeId()); connectedNodes.insert(w.endNodeId()); }
    for (const auto& [id, f] : model.foundations()) { connectedNodes.insert(f.nodeId()); }

    const auto& lm = model.loadManager();
    std::set<int> loadedNodes;
    for (const auto& [id, nl] : lm.nodalLoads()) loadedNodes.insert(nl.nodeId());

    for (const auto& [id, n] : model.nodes())
    {
        bool isConnected = connectedNodes.find(id) != connectedNodes.end();
        bool isSupported = n.support().isSupported() || n.supportType() != TSA::Model::SupportType::Free;
        bool isLoaded = loadedNodes.find(id) != loadedNodes.end();

        if (!isConnected && !isSupported && !isLoaded)
        {
            report.addWarning("Nœuds Orphelins",
                "Le nœud N" + std::to_string(id) + " est isolé (non connecté, non chargé, sans appui).",
                id, "ISO/IEC 25010");
        }
    }

    // 4. Données que les moteurs de calcul ne transmettent pas (jamais ignorées en silence)
    for (const auto& [id, b] : model.beams())
    {
        auto unsupported = [](const TSA::Model::EndRelease& r) { return r.fx || r.fy || r.fz || r.mx; };
        if (unsupported(b.startRelease()) || unsupported(b.endRelease()))
        {
            report.addWarning("Relâchements",
                "La poutre #" + std::to_string(id) + " a un relâchement d'effort normal, d'effort tranchant ou de torsion : "
                "il est ignoré par le calcul (seules les rotules de flexion My / Mz sont transmises).",
                id, "RDM");
        }
    }
    {
        // Nœuds reliés uniquement à des barres articulées : rotations bloquées au calcul (BUG-019),
        // un moment nodal y serait repris par l'appui fictif et non par la structure.
        std::set<int> frameNodes, axialNodes;
        for (const auto& [id, b] : model.beams()) { frameNodes.insert(b.startNodeId()); frameNodes.insert(b.endNodeId()); }
        for (const auto& [id, c] : model.columns()) { frameNodes.insert(c.startNodeId()); frameNodes.insert(c.endNodeId()); }
        for (const auto& [id, t] : model.trussMembers()) { axialNodes.insert(t.startNodeId()); axialNodes.insert(t.endNodeId()); }
        for (const auto& [id, c] : model.cables()) { axialNodes.insert(c.startNodeId()); axialNodes.insert(c.endNodeId()); }
        for (const auto& [loadId, nl] : lm.nodalLoads())
        {
            const bool hasMoment = std::abs(nl.mx()) > 1e-12 || std::abs(nl.my()) > 1e-12 || std::abs(nl.mz()) > 1e-12;
            if (hasMoment && axialNodes.count(nl.nodeId()) && !frameNodes.count(nl.nodeId()))
            {
                report.addWarning("Charges",
                    "Le nœud N" + std::to_string(nl.nodeId()) + " n'est relié qu'à des treillis / câbles : le moment nodal de la charge « "
                    + nl.name() + " » ne peut pas être repris par la structure (ignoré).",
                    nl.nodeId(), "RDM");
            }
        }
    }
    for (const auto& [loadId, ml] : lm.memberLoads())
    {
        if (ml.type() == TSA::Model::LoadType::MemberMoment)
        {
            report.addWarning("Charges",
                "La charge sur barre « " + ml.name() + " » est un moment réparti : ce type de charge n'est pas pris en compte par le calcul.",
                loadId, "EN 1991");
        }
    }

    // 5. Cas de charge ou combinaison ciblés (calcul statique)
    if (params.targetCombinationId > 0)
    {
        const auto* combo = lm.getCombination(params.targetCombinationId);
        if (!combo)
        {
            report.addError("Combinaisons",
                "La combinaison ciblée pour le calcul (ID " + std::to_string(params.targetCombinationId) + ") n'existe pas.",
                params.targetCombinationId, "EN 1990");
        }
        else if (combo->caseFactors().empty())
        {
            report.addError("Combinaisons",
                "La combinaison ciblée '" + combo->name() + "' ne contient aucun cas de charge pondéré.",
                params.targetCombinationId, "EN 1990");
        }
    }
    else if (params.targetLoadCaseId > 0)
    {
        const auto* lc = lm.getLoadCase(params.targetLoadCaseId);
        if (!lc)
        {
            report.addError("Charges",
                "Le cas de charge ciblé pour le calcul (ID " + std::to_string(params.targetLoadCaseId) + ") n'existe pas.",
                params.targetLoadCaseId, "EN 1991");
        }
    }

    return report;
}

} // namespace TSA::Standards
