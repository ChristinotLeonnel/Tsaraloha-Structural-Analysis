#include "ModelValidator.h"
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
    std::map<int, TSA::Coordinate::Point3D> nodeCoords;

    for (const auto& [id, n] : nodes)
    {
        std::string nodeErr;
        if (!validateNode(n, &nodeErr))
        {
            report.addError("Nœuds", nodeErr, id, "ISO/IEC 25010 §4.2.5");
        }

        // Vérification des doublons géométriques (tolérance 1 mm = 1e-3 m)
        TSA::Coordinate::Point3D pt(n.x(), n.y(), n.z());
        for (const auto& [existingId, existingPt] : nodeCoords)
        {
            if (pt.distance(existingPt) < 1e-3)
            {
                report.addWarning("Nœuds",
                    "Nœud géométriquement coïncident avec le nœud " + std::to_string(existingId) +
                    " (distance < 1 mm).", id);
            }
        }
        nodeCoords[id] = pt;

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

} // namespace TSA::Standards
