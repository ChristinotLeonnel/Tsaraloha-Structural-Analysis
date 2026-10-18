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
#include "../Analysis/LoadResolver.h"
#include "../Analysis/LoadValidation.h"
#include "../Analysis/OpenSeesAnalysisBuilder.h"

#include <gp_Vec.hxx>

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cmath>
#include <functional>
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
            if (!issue.normativeRef.empty())
                msg += QCoreApplication::translate("TSA::Standards::ModelValidator", " [Réf: %1]")
                           .arg(QString::fromStdString(issue.normativeRef)).toStdString();
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
            return QCoreApplication::translate("TSA::Standards::ModelValidator",
                                               "Validation réussie : Modèle conforme aux normes avec %1 avertissement(s).")
                .arg(warningCount());
        }
        return QCoreApplication::translate("TSA::Standards::ModelValidator",
                                           "Validation réussie : Modèle structural 100% conforme et intègre.");
    }
    return QCoreApplication::translate("TSA::Standards::ModelValidator",
                                       "Échec de validation : %1 erreur(s) et %2 avertissement(s) détectés.")
        .arg(errorCount()).arg(warningCount());
}

bool ModelValidator::validateSection(const TSA::Model::Section& s, std::string* errorMsg)
{
    double A = s.area();
    double Iy = s.iy();
    double Iz = s.iz();

    if (std::isnan(A) || std::isinf(A) || A <= 0.0)
    {
        if (errorMsg) *errorMsg = QCoreApplication::translate("TSA::Standards::ModelValidator",
            "L'aire de section A est invalide (doit être > 0 et finie).").toStdString();
        return false;
    }
    if (std::isnan(Iy) || std::isinf(Iy) || Iy <= 0.0)
    {
        if (errorMsg) *errorMsg = QCoreApplication::translate("TSA::Standards::ModelValidator",
            "Le moment quadratique Iy est invalide (doit être > 0).").toStdString();
        return false;
    }
    if (std::isnan(Iz) || std::isinf(Iz) || Iz <= 0.0)
    {
        if (errorMsg) *errorMsg = QCoreApplication::translate("TSA::Standards::ModelValidator",
            "Le moment quadratique Iz est invalide (doit être > 0).").toStdString();
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
        if (errorMsg) *errorMsg = QCoreApplication::translate("TSA::Standards::ModelValidator",
            "Le module d'élasticité E est invalide (doit être > 0 et fini).").toStdString();
        return false;
    }
    if (std::isnan(nu) || std::isinf(nu) || nu < 0.0 || nu >= 0.5)
    {
        if (errorMsg) *errorMsg = QCoreApplication::translate("TSA::Standards::ModelValidator",
            "Le coefficient de Poisson nu doit être dans l'intervalle physique [0.0, 0.5[.").toStdString();
        return false;
    }
    if (std::isnan(rho) || std::isinf(rho) || rho <= 0.0)
    {
        if (errorMsg) *errorMsg = QCoreApplication::translate("TSA::Standards::ModelValidator",
            "La masse volumique rho doit être positive.").toStdString();
        return false;
    }
    return true;
}

bool ModelValidator::validateNode(const TSA::Model::Node& n, std::string* errorMsg)
{
    if (n.id() <= 0)
    {
        if (errorMsg) *errorMsg = QCoreApplication::translate("TSA::Standards::ModelValidator",
            "L'identifiant du nœud doit être un entier strictement positif.").toStdString();
        return false;
    }
    if (std::isnan(n.x()) || std::isinf(n.x()) ||
        std::isnan(n.y()) || std::isinf(n.y()) ||
        std::isnan(n.z()) || std::isinf(n.z()))
    {
        if (errorMsg) *errorMsg = QCoreApplication::translate("TSA::Standards::ModelValidator",
            "Les coordonnées du nœud contiennent des valeurs NaN ou infinies.").toStdString();
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
        report.addError(QCoreApplication::translate("TSA::Standards::ModelValidator", "Géométrie").toStdString(),
                        QCoreApplication::translate("TSA::Standards::ModelValidator",
                            "Le modèle structural ne contient aucun nœud.").toStdString(), 0, "EN 1990");
        return report;
    }

    bool hasSupport = false;

    // Doublons géométriques (tolérance 1 mm), hachage spatial : O(N log N) au lieu de O(N²).
    for (const auto& [dupId, keeperId] : model.findCoincidentNodes(1e-3))
    {
        report.addWarning(QCoreApplication::translate("TSA::Standards::ModelValidator", "Nœuds").toStdString(),
            QCoreApplication::translate("TSA::Standards::ModelValidator",
                "Nœud géométriquement coïncident avec le nœud %1"
                " (distance < 1 mm). Utiliser « Fusionner les nœuds confondus ».").arg(keeperId).toStdString(), dupId);
    }

    for (const auto& [id, n] : nodes)
    {
        std::string nodeErr;
        if (!validateNode(n, &nodeErr))
        {
            report.addError(QCoreApplication::translate("TSA::Standards::ModelValidator", "Nœuds").toStdString(),
                            nodeErr, id, "ISO/IEC 25010 §4.2.5");
        }


        // Détection des appuis
        if (n.support().isSupported() || n.supportType() != TSA::Model::SupportType::Free)
        {
            hasSupport = true;
        }
    }

    // Appuis linéiques des bords de dalles et de voiles (appliqués aux nœuds du maillage au calcul)
    for (const auto& [sid, sl] : model.slabs())
        if (!sl.edgeSupports().empty()) hasSupport = true;
    for (const auto& [wid, wl] : model.walls())
        if (!wl.edgeSupports().empty()) hasSupport = true;
    // 2. Contrôle de stabilité cinématique globale
    if (!hasSupport)
    {
        report.addError(QCoreApplication::translate("TSA::Standards::ModelValidator", "Conditions aux Limites").toStdString(),
            QCoreApplication::translate("TSA::Standards::ModelValidator",
                "Aucun appui (Encastrement, Articulation, Appui simple ou Ressort) n'est défini. "
                "La structure est cinématiquement instable.").toStdString(), 0, "EN 1990 §2.1");
    }

    // 3. Contrôle des éléments linéaires (Poutres, Poteaux, Bielles, Câbles)
    // typeName : texte source français (marqué QT_TRANSLATE_NOOP aux appels), comparé tel quel ci-dessous ;
    // seule la catégorie affichée (typeLabel) est traduite.
    auto validateLinear = [&](int elemId, int startId, int endId,
                              const TSA::Model::Section& sec, const TSA::Model::Material& mat,
                              const std::string& typeName)
    {
        const std::string typeLabel =
            QCoreApplication::translate("TSA::Standards::ModelValidator", typeName.c_str()).toStdString();
        if (startId <= 0 || endId <= 0 || startId == endId)
        {
            report.addError(typeLabel, QCoreApplication::translate("TSA::Standards::ModelValidator",
                "Les nœuds d'extrémité sont identiques ou invalides.").toStdString(), elemId, "RDM");
            return;
        }

        auto itStart = nodes.find(startId);
        auto itEnd = nodes.find(endId);
        if (itStart == nodes.end() || itEnd == nodes.end())
        {
            report.addError(typeLabel, QCoreApplication::translate("TSA::Standards::ModelValidator",
                "Fait référence à un nœud inexistant dans le modèle.").toStdString(), elemId);
            return;
        }

        TSA::Coordinate::Point3D p1(itStart->second.x(), itStart->second.y(), itStart->second.z());
        TSA::Coordinate::Point3D p2(itEnd->second.x(), itEnd->second.y(), itEnd->second.z());
        double length = p1.distance(p2);

        if (length < 1e-4) // < 0.1 mm
        {
            report.addError(typeLabel, QCoreApplication::translate("TSA::Standards::ModelValidator",
                "La longueur de l'élément est nulle ou inférieure à 0.1 mm.").toStdString(), elemId, "RDM");
        }

        std::string secErr;
        if (!validateSection(sec, &secErr))
        {
            report.addError(QCoreApplication::translate("TSA::Standards::ModelValidator", "Sections").toStdString(),
                            secErr, elemId, "EN 1990");
        }
        else if (std::string(typeName) == "Poutre" || std::string(typeName) == "Poteau")
        {
            // Poutres et poteaux : élément de poutre 3D, la rigidité de torsion G·J doit exister (sinon
            // la rotation autour de l'axe de la barre n'a aucune rigidité : matrice singulière).
            const double J = sec.it();
            if (!std::isfinite(J) || J <= 0.0)
                report.addError(QCoreApplication::translate("TSA::Standards::ModelValidator", "Sections").toStdString(),
                                QCoreApplication::translate("TSA::Standards::ModelValidator",
                                    "La constante de torsion de Saint-Venant J de la section « %1"
                                    " » est nulle ou invalide : la torsion ne peut pas être calculée.")
                                    .arg(QString::fromStdString(sec.name)).toStdString(), elemId, "RDM");
        }

        std::string matErr;
        if (!validateMaterial(mat, &matErr))
        {
            report.addError(QCoreApplication::translate("TSA::Standards::ModelValidator", "Matériaux").toStdString(),
                            matErr, elemId, "EN 1992 / EN 1993");
        }
    };

    for (const auto& [id, b] : model.beams())
        validateLinear(id, b.startNodeId(), b.endNodeId(), b.section(), b.material(),
                       QT_TRANSLATE_NOOP("TSA::Standards::ModelValidator", "Poutre"));

    for (const auto& [id, c] : model.columns())
        validateLinear(id, c.startNodeId(), c.endNodeId(), c.section(), c.material(),
                       QT_TRANSLATE_NOOP("TSA::Standards::ModelValidator", "Poteau"));

    for (const auto& [id, t] : model.trussMembers())
        validateLinear(id, t.startNodeId(), t.endNodeId(), t.section(), t.material(),
                       QT_TRANSLATE_NOOP("TSA::Standards::ModelValidator", "Treillis"));

    for (const auto& [id, cb] : model.cables())
        validateLinear(id, cb.startNodeId(), cb.endNodeId(), cb.section(), cb.material(),
                       QT_TRANSLATE_NOOP("TSA::Standards::ModelValidator", "Câble"));

    // 4. Contrôle des éléments surfaciques (Dalles, Voiles)
    const std::string slabsCat = QCoreApplication::translate("TSA::Standards::ModelValidator", "Dalles").toStdString();
    for (const auto& [id, sl] : model.slabs())
    {
        if (sl.nodeIds().size() < 3)
        {
            report.addError(slabsCat, QCoreApplication::translate("TSA::Standards::ModelValidator",
                "Une dalle requiert au moins 3 nœuds de contour.").toStdString(), id);
        }
        if (sl.thickness() <= 0.0 || std::isnan(sl.thickness()))
        {
            report.addError(slabsCat, QCoreApplication::translate("TSA::Standards::ModelValidator",
                "L'épaisseur de la dalle doit être strictement positive.").toStdString(), id);
        }
    }

    const std::string wallsCat = QCoreApplication::translate("TSA::Standards::ModelValidator", "Voiles").toStdString();
    for (const auto& [id, w] : model.walls())
    {
        if (w.startNodeId() <= 0 || w.endNodeId() <= 0 || w.startNodeId() == w.endNodeId())
        {
            report.addError(wallsCat, QCoreApplication::translate("TSA::Standards::ModelValidator",
                "Les nœuds de base du voile sont invalides ou identiques.").toStdString(), id);
        }
        if (w.height() <= 0.0 || std::isnan(w.height()))
        {
            report.addError(wallsCat, QCoreApplication::translate("TSA::Standards::ModelValidator",
                "La hauteur du voile doit être strictement positive.").toStdString(), id);
        }
        if (w.thickness() <= 0.0 || std::isnan(w.thickness()))
        {
            report.addError(wallsCat, QCoreApplication::translate("TSA::Standards::ModelValidator",
                "L'épaisseur du voile doit être strictement positive.").toStdString(), id);
        }
    }

    // 5. Validation des charges et combinaisons
    const std::string loadsCat = QCoreApplication::translate("TSA::Standards::ModelValidator", "Charges").toStdString();
    auto loadRep = TSA::Analysis::LoadValidation::validateModel(model);
    for (const auto& err : loadRep.errors())
    {
        report.addError(loadsCat, err, 0, "EN 1991");
    }
    for (const auto& warn : loadRep.warnings())
    {
        report.addWarning(loadsCat, warn);
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
    if (totalElements == 0 && model.slabs().empty() && model.walls().empty())
    {
        report.addError(QCoreApplication::translate("TSA::Standards::ModelValidator", "Modèle Calcul").toStdString(),
            QCoreApplication::translate("TSA::Standards::ModelValidator",
                "Aucun élément structural filaire (Poutre, Poteau, Treillis ou Câble) n'est présent pour le calcul.").toStdString(),
            0, "EN 1990");
    }

    // 2b. Dalles et voiles : pas de maillage EF, non transmis au solveur (BUG-002)
    if ((!model.slabs().empty() || !model.walls().empty()) && !params.surfacesTransmitted)
    {
        report.addWarning(QCoreApplication::translate("TSA::Standards::ModelValidator", "Modèle Calcul").toStdString(),
            QCoreApplication::translate("TSA::Standards::ModelValidator",
                "%1 dalle(s) et %2"
                " voile(s) : calculés en coques uniquement à partir du maillage du projet à jour (formulation « coque ») ; "
                "sans maillage transmis, ni leur rigidité ni leurs charges ne sont prises en compte.")
                .arg(model.slabs().size()).arg(model.walls().size()).toStdString(), 0, "EN 1990 §5.1");
    }
    const std::string loadsCat = QCoreApplication::translate("TSA::Standards::ModelValidator", "Charges").toStdString();

    // 2c. Charges sur barre : chaque charge doit désigner un élément calculé (famille + id).
    {
        const auto snap = TSA::Analysis::CalculationSnapshot::capture(model);
        for (const auto& ml : snap.memberLoads())
        {
            if (!snap.findElementForLoad(ml))
            {
                report.addWarning(loadsCat,
                    QCoreApplication::translate("TSA::Standards::ModelValidator",
                        "La charge sur barre #%1 (élément %2"
                        ") ne désigne aucun élément calculé de façon univoque : elle sera ignorée par le calcul.")
                        .arg(ml.id()).arg(ml.elementId()).toStdString(),
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
            report.addWarning(QCoreApplication::translate("TSA::Standards::ModelValidator", "Nœuds Orphelins").toStdString(),
                QCoreApplication::translate("TSA::Standards::ModelValidator",
                    "Le nœud N%1 est isolé (non connecté, non chargé, sans appui).").arg(id).toStdString(),
                id, "ISO/IEC 25010");
        }
    }

    // 4. Données que les moteurs de calcul ne transmettent pas (jamais ignorées en silence)
    for (const auto& [id, b] : model.beams())
    {
        auto unsupported = [](const TSA::Model::EndRelease& r) { return r.fx || r.fy || r.fz || r.mx; };
        if (unsupported(b.startRelease()) || unsupported(b.endRelease()))
        {
            report.addWarning(QCoreApplication::translate("TSA::Standards::ModelValidator", "Relâchements").toStdString(),
                QCoreApplication::translate("TSA::Standards::ModelValidator",
                    "La poutre #%1 a un relâchement d'effort normal, d'effort tranchant ou de torsion : "
                    "il est ignoré par le calcul (seules les rotules de flexion My / Mz sont transmises).").arg(id).toStdString(),
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
                report.addWarning(loadsCat,
                    QCoreApplication::translate("TSA::Standards::ModelValidator",
                        "Le nœud N%1 n'est relié qu'à des treillis / câbles : le moment nodal de la charge « "
                        "%2 » ne peut pas être repris par la structure (ignoré).")
                        .arg(QString::number(nl.nodeId()), QString::fromStdString(nl.name())).toStdString(),
                    nl.nodeId(), "RDM");
            }
        }
    }
    for (const auto& [loadId, nl] : lm.nodalLoads())
    {
        // Un nœud n'a pas de repère local : les composantes sont appliquées dans le repère global.
        if (nl.coordSystem() == TSA::Model::LoadCoordSystem::Local)
        {
            report.addWarning(loadsCat,
                QCoreApplication::translate("TSA::Standards::ModelValidator",
                    "La charge nodale « %1 » (nœud N%2) est déclarée en repère local : "
                    "un nœud n'a pas de repère local, ses forces et moments sont appliqués selon les axes globaux X, Y, Z. "
                    "Pour une torsion, donner le moment autour de l'axe global parallèle à la barre.")
                    .arg(QString::fromStdString(nl.name()), QString::number(nl.nodeId())).toStdString(),
                nl.nodeId(), "RDM");
        }
    }
    // Charges conservées mais jamais appliquées : jamais tues avant un calcul.
    {
        // Charges surfaciques d'un fichier TSA3D : conservées (export fidèle), sans surface calculée.
        const QJsonObject root = QJsonDocument::fromJson(QByteArray::fromStdString(model.exchangeExtensionsJson())).object();
        const QJsonArray surface = root.value("document").toObject().value("loads").toObject().value("surface").toArray();
        if (!surface.isEmpty())
            report.addWarning(loadsCat,
                QCoreApplication::translate("TSA::Standards::ModelValidator",
                    "%1 charge(s) surfacique(s) importée(s) d'un fichier TSA3D sont conservées mais NON "
                    "appliquées : aucun élément surfacique n'est calculé. Les résultats n'en tiennent pas compte ; les convertir en "
                    "charges sur barres ou nodales pour les inclure.").arg(surface.size()).toStdString(), 0, "EN 1991");
        int explicitSelfWeight = 0;
        for (const auto& [loadId, ml] : lm.memberLoads())
            if (ml.type() == TSA::Model::LoadType::SelfWeight) ++explicitSelfWeight;
        if (explicitSelfWeight > 0)
            report.addWarning(loadsCat,
                QCoreApplication::translate("TSA::Standards::ModelValidator",
                    "%1 charge(s) sur barre de type « poids propre » explicite : OpenSees l'applique "
                    "comme une charge uniforme vers le bas d'intensité q1, le moteur 2D l'ignore. Le poids propre se règle par "
                    "l'option « Inclure le poids propre » (double comptage possible).").arg(explicitSelfWeight).toStdString(),
                0, "EN 1991");
    }

    // Moments répartis (couple par unité de longueur) : transmis aux poutres et poteaux par forces nodales
    // équivalentes ; jamais devinés ni ignorés en silence.
    const bool anyMemberMoment = std::any_of(lm.memberLoads().begin(), lm.memberLoads().end(),
                                             [](const auto& kv) { return kv.second.type() == TSA::Model::LoadType::MemberMoment; });
    if (anyMemberMoment)
    {
        const auto snap = TSA::Analysis::CalculationSnapshot::capture(model);
        for (const auto& [loadId, ml] : lm.memberLoads())
        {
            if (ml.type() != TSA::Model::LoadType::MemberMoment) continue;
            if (TSA::Analysis::LoadResolver::usesMagnitudeOnly(ml))
            {
                report.addError(loadsCat,
                    QCoreApplication::translate("TSA::Standards::ModelValidator",
                        "Le moment réparti « %1 » n'a pas d'axe défini (direction « gravité » ou axe incompatible avec le "
                        "repère) : indiquer l'axe du vecteur moment (local x pour une torsion, ou un axe global).")
                        .arg(QString::fromStdString(ml.name())).toStdString(),
                    loadId, "RDM");
                continue;
            }
            const auto* el = snap.findElementForLoad(ml);
            if (el && (el->type == TSA::Analysis::SnapshotElement::ElementType::Truss
                       || el->type == TSA::Analysis::SnapshotElement::ElementType::Cable))
            {
                report.addWarning(loadsCat,
                    QCoreApplication::translate("TSA::Standards::ModelValidator",
                        "Le moment réparti « %1 » s'applique à une barre articulée (treillis ou câble), sans rigidité en "
                        "rotation : il n'est pas transmis au calcul.")
                        .arg(QString::fromStdString(ml.name())).toStdString(),
                    loadId, "RDM");
            }
        }
    }

    // 4b. Mécanisme de torsion : barres fléchies alignées (un seul axe) dont la rotation autour de cet axe
    //     n'est bloquée par aucun appui (y compris les blocages automatiques du calcul 3D) : matrice
    //     singulière. Détecté avant le calcul, avec les barres concernées.
    {
        using TSA::Analysis::SnapshotElement;
        const auto snap = TSA::Analysis::CalculationSnapshot::capture(model, nullptr, params.supportRestraints);
        const std::string supportsCat = QCoreApplication::translate("TSA::Standards::ModelValidator", "Appuis").toStdString();
        for (const auto& w : snap.restraintWarnings()) report.addWarning(supportsCat, w, 0, "RDM");
        std::map<int, int> parent;
        std::function<int(int)> root = [&](int a) { return parent[a] == a ? a : parent[a] = root(parent[a]); };
        std::vector<const SnapshotElement*> frames;
        for (const auto& [tag, e] : snap.elements())
        {
            if (e.type == SnapshotElement::ElementType::Truss || e.type == SnapshotElement::ElementType::Cable) continue;
            frames.push_back(&e);
            for (int id : { e.startNodeId, e.endNodeId })
                if (!parent.count(id)) parent[id] = id;
            parent[root(e.startNodeId)] = root(e.endNodeId);
        }
        std::map<int, std::vector<const SnapshotElement*>> components;
        for (const auto* e : frames) components[root(e->startNodeId)].push_back(e);
        for (const auto& [r, members] : components)
        {
            auto dirOf = [&](const SnapshotElement* e) {
                const auto* a = snap.getNode(e->startNodeId);
                const auto* b = snap.getNode(e->endNodeId);
                gp_Vec d(b->x - a->x, b->y - a->y, b->z - a->z);
                return d.Magnitude() > 1e-12 ? d.Normalized() : gp_Vec();
            };
            const gp_Vec d0 = dirOf(members.front());
            if (d0.Magnitude() < 0.5) continue;
            if (!std::all_of(members.begin(), members.end(), [&](const auto* e) { return dirOf(e).Crossed(d0).Magnitude() < 1e-9; }))
                continue; // barres non alignées : la torsion de l'une est retenue par la flexion des autres
            const double dk[3] = { std::abs(d0.X()), std::abs(d0.Y()), std::abs(d0.Z()) };
            bool restrained = false;
            for (const auto& [id, p] : parent)
            {
                if (root(id) != r) continue;
                const auto* n = snap.getNode(id);
                if (!n) continue;
                const bool fixedR[3] = { n->fixRx || n->kRx > 0.0, n->fixRy || n->kRy > 0.0, n->fixRz || n->kRz > 0.0 };
                for (int k = 0; k < 3; ++k) restrained |= fixedR[k] && dk[k] > 1e-6;
            }
            if (restrained) continue;
            std::string list;
            for (std::size_t i = 0; i < members.size() && i < 5; ++i)
                list += (i ? ", " : "")
                        + (members[i]->type == SnapshotElement::ElementType::Column
                               ? QCoreApplication::translate("TSA::Standards::ModelValidator", "poteau %1")
                               : QCoreApplication::translate("TSA::Standards::ModelValidator", "poutre %1"))
                              .arg(members[i]->id).toStdString();
            if (members.size() > 5) list += "…";
            report.addError(QCoreApplication::translate("TSA::Standards::ModelValidator", "Stabilité").toStdString(),
                QCoreApplication::translate("TSA::Standards::ModelValidator",
                    "Mécanisme de torsion : %1 (barres alignées) peuvent tourner librement autour de leur axe, aucun appui "
                    "ne bloquant cette rotation (matrice singulière). Correction : bloquer la rotation autour de l'axe de la barre "
                    "sur au moins un nœud (encastrement, appui à fourche défini, ressort en rotation), ou choisir le mode "
                    "« appui à fourche » des appuis articulés dans les paramètres d'analyse si la barre est parallèle à un axe global.")
                    .arg(QString::fromStdString(list)).toStdString(),
                members.front()->id, "RDM");
        }
    }

    // 5. Cas de charge ou combinaison ciblés (calcul statique)
    if (params.targetCombinationId > 0)
    {
        const auto* combo = lm.getCombination(params.targetCombinationId);
        if (!combo)
        {
            report.addError(QCoreApplication::translate("TSA::Standards::ModelValidator", "Combinaisons").toStdString(),
                QCoreApplication::translate("TSA::Standards::ModelValidator",
                    "La combinaison ciblée pour le calcul (ID %1) n'existe pas.").arg(params.targetCombinationId).toStdString(),
                params.targetCombinationId, "EN 1990");
        }
        else if (combo->caseFactors().empty())
        {
            report.addError(QCoreApplication::translate("TSA::Standards::ModelValidator", "Combinaisons").toStdString(),
                QCoreApplication::translate("TSA::Standards::ModelValidator",
                    "La combinaison ciblée '%1' ne contient aucun cas de charge pondéré.")
                    .arg(QString::fromStdString(combo->name())).toStdString(),
                params.targetCombinationId, "EN 1990");
        }
    }
    else if (params.targetLoadCaseId > 0)
    {
        const auto* lc = lm.getLoadCase(params.targetLoadCaseId);
        if (!lc)
        {
            report.addError(loadsCat,
                QCoreApplication::translate("TSA::Standards::ModelValidator",
                    "Le cas de charge ciblé pour le calcul (ID %1) n'existe pas.").arg(params.targetLoadCaseId).toStdString(),
                params.targetLoadCaseId, "EN 1991");
        }
    }

    return report;
}

} // namespace TSA::Standards
