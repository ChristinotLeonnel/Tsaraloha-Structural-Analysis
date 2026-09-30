#include "OpenSeesAdapter.h"
#include "LoadResolver.h"
#include "../Model/Model.h"
#include "../Model/Load/LoadManager.h"
#include "../Coordinate/CoordinateTransformationService.h"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace TSA::Analysis
{

std::string OpenSeesAdapter::generateTclScript(const TSA::Model::Model& model,
                                               const OpenSeesOptions& options)
{
    std::ostringstream tcl;
    tcl << std::fixed << std::setprecision(6);

    tcl << "# ==============================================================================\n";
    tcl << "# TSA (Tsaraloha Structural Analysis) -> OpenSees Structural Analysis Model\n";
    tcl << "# Unités : " << (options.useKiloNewtons ? "kN, m, kPa, kNm" : "N, m, Pa, Nm") << "\n";
    tcl << "# ==============================================================================\n\n";

    tcl << "wipe\n";
    tcl << "model BasicBuilder -ndm 3 -ndf 6\n\n";

    // 1. Définition des Nœuds
    tcl << "# ------------------------------------------------------------------------------\n";
    tcl << "# Nœuds structuraux (node $nodeTag $x $y $z)\n";
    tcl << "# ------------------------------------------------------------------------------\n";
    for (const auto& [id, n] : model.nodes())
    {
        tcl << "node " << id << " " << n.x() << " " << n.y() << " " << n.z();
        if (!n.name().empty())
        {
            tcl << " ;# " << n.name();
        }
        tcl << "\n";
    }
    tcl << "\n";

    // 2. Conditions aux limites (Appuis / Single-Point Constraints)
    tcl << "# ------------------------------------------------------------------------------\n";
    tcl << "# Conditions aux limites (fix $nodeTag $u1 $u2 $u3 $r1 $r2 $r3)\n";
    tcl << "# ------------------------------------------------------------------------------\n";
    for (const auto& [id, n] : model.nodes())
    {
        const auto& supp = n.support();
        if (supp.isFree()) continue;

        // Détermine les flags de fixation par DDL
        int u1 = (supp.tx() == TSA::Model::DOFState::Fixed) ? 1 : 0;
        int u2 = (supp.ty() == TSA::Model::DOFState::Fixed) ? 1 : 0;
        int u3 = (supp.tz() == TSA::Model::DOFState::Fixed) ? 1 : 0;
        int r1 = (supp.rx() == TSA::Model::DOFState::Fixed) ? 1 : 0;
        int r2 = (supp.ry() == TSA::Model::DOFState::Fixed) ? 1 : 0;
        int r3 = (supp.rz() == TSA::Model::DOFState::Fixed) ? 1 : 0;

        // Anti-singularité : bloquer Rx pour articulations/rouleaux si non-ressort
        if ((supp.isPinned() || supp.isRoller()) && supp.rx() != TSA::Model::DOFState::Spring)
        {
            r1 = 1;
        }

        if (u1 || u2 || u3 || r1 || r2 || r3)
        {
            tcl << "fix " << id << " "
                << u1 << " " << u2 << " " << u3 << " "
                << r1 << " " << r2 << " " << r3
                << " ;# " << supp.typeName() << "\n";
        }
    }
    tcl << "\n";

    // 3. Transformations géométriques (geomTransf)
    tcl << "# ------------------------------------------------------------------------------\n";
    tcl << "# Transformations géométriques (geomTransf Linear $transfTag $vecxzX $vecxzY $vecxzZ)\n";
    tcl << "# ------------------------------------------------------------------------------\n";
    // Pour chaque élément, calculer l'orientation du plan local x-z
    int transfTag = 1;
    std::map<int, int> elemToTransf;

    auto processElemTransf = [&](int elemId, int startNodeId, int endNodeId, double rotDeg) {
        const auto* n1 = model.getNode(startNodeId);
        const auto* n2 = model.getNode(endNodeId);
        if (!n1 || !n2) return;

        gp_Pnt p1(n1->x(), n1->y(), n1->z());
        gp_Pnt p2(n2->x(), n2->y(), n2->z());

        gp_Ax3 frame = LoadResolver::computeElementLocalAxes(p1, p2, rotDeg);
        gp_Dir dirZ = frame.Direction(); // Vecteur normal de l'axe local Z

        int tag = transfTag++;
        elemToTransf[elemId] = tag;
        tcl << "geomTransf Linear " << tag << " " << dirZ.X() << " " << dirZ.Y() << " " << dirZ.Z()
            << " ;# Element #" << elemId << "\n";
    };

    for (const auto& [id, b] : model.beams())
    {
        processElemTransf(id, b.startNodeId(), b.endNodeId(), b.rotation());
    }
    for (const auto& [id, col] : model.columns())
    {
        processElemTransf(id, col.startNodeId(), col.endNodeId(), col.rotation());
    }
    for (const auto& [id, tr] : model.trussMembers())
    {
        processElemTransf(id, tr.startNodeId(), tr.endNodeId(), 0.0);
    }
    tcl << "\n";

    // 4. Éléments élastiques (elasticBeamColumn)
    tcl << "# ------------------------------------------------------------------------------\n";
    tcl << "# Éléments finis (element elasticBeamColumn $eleTag $iNode $jNode $A $E $G $J $Iy $Iz $transfTag)\n";
    tcl << "# ------------------------------------------------------------------------------\n";

    double scaleForce = options.useKiloNewtons ? 1e-3 : 1.0; // Pa -> kPa si useKiloNewtons

    auto writeBeamColumnElement = [&](int eleId, int startN, int endN,
                                      const TSA::Model::Section& sec,
                                      const TSA::Model::Material& mat) {
        double A = sec.area();
        double E = mat.mechanical.youngModulus * scaleForce;
        double nu = mat.mechanical.poissonRatio;
        double G = E / (2.0 * (1.0 + nu));
        double J = sec.it();
        double Iy = sec.iy();
        double Iz = sec.iz();
        int tTag = elemToTransf.count(eleId) ? elemToTransf[eleId] : 1;

        tcl << "element elasticBeamColumn " << eleId << " " << startN << " " << endN << " "
            << A << " " << E << " " << G << " " << J << " " << Iy << " " << Iz << " " << tTag
            << " ;# Sec: " << sec.name << "\n";
    };

    for (const auto& [id, b] : model.beams())
    {
        writeBeamColumnElement(id, b.startNodeId(), b.endNodeId(), b.section(), b.material());
    }
    for (const auto& [id, col] : model.columns())
    {
        writeBeamColumnElement(id, col.startNodeId(), col.endNodeId(), col.section(), col.material());
    }
    for (const auto& [id, tr] : model.trussMembers())
    {
        double A = tr.section().area();
        double E = tr.material().mechanical.youngModulus * scaleForce;
        tcl << "element truss " << id << " " << tr.startNodeId() << " " << tr.endNodeId() << " "
            << A << " " << E << " ;# Treillis\n";
    }
    tcl << "\n";

    // 5. Cas de charges et Patterns
    tcl << "# ------------------------------------------------------------------------------\n";
    tcl << "# Chargements & Patterns (timeSeries Linear + pattern Plain)\n";
    tcl << "# ------------------------------------------------------------------------------\n";
    tcl << "timeSeries Linear 1\n\n";

    const auto& lm = model.loadManager();

    auto writeLoadsForCase = [&](int caseId, double factor) {
        const auto* lc = lm.getLoadCase(caseId);
        std::string caseName = lc ? lc->name() : ("Cas#" + std::to_string(caseId));

        tcl << "pattern Plain " << caseId << " 1 {\n";
        tcl << "  # --- Cas de charge : " << caseName << " (Facteur = " << factor << ") ---\n";

        // A. Charges nodales
        for (const auto& nl : lm.nodalLoadsForCase(caseId))
        {
            double fx = nl.fx() * factor;
            double fy = nl.fy() * factor;
            double fz = nl.fz() * factor;
            double mx = nl.mx() * factor;
            double my = nl.my() * factor;
            double mz = nl.mz() * factor;

            tcl << "  load " << nl.nodeId() << " " << fx << " " << fy << " " << fz << " "
                << mx << " " << my << " " << mz << " ;# " << nl.name() << "\n";
        }

        // B. Charges sur barres
        for (const auto& ml : lm.memberLoadsForCase(caseId))
        {
            const auto* tr = model.getTrussMember(ml.elementId());
            if (tr)
            {
                // Les éléments treillis n'acceptent pas eleLoad dans OpenSees :
                // décomposer en charges nodales équivalentes aux deux nœuds d'extrémité
                double L = tr->length(model);
                double totalForce = ml.q1() * L * factor;
                double fx = 0.0, fy = 0.0, fz = 0.0;
                if (ml.direction() == TSA::Model::LoadDirection::GlobalX) fx = totalForce * 0.5;
                else if (ml.direction() == TSA::Model::LoadDirection::GlobalY) fy = totalForce * 0.5;
                else if (ml.direction() == TSA::Model::LoadDirection::GlobalZ) fz = totalForce * 0.5;
                else if (ml.direction() == TSA::Model::LoadDirection::Gravity) fz = -std::abs(totalForce) * 0.5;
                else fz = -std::abs(totalForce) * 0.5;

                tcl << "  load " << tr->startNodeId() << " " << fx << " " << fy << " " << fz
                    << " 0.0 0.0 0.0 ;# Treillis #" << ml.elementId() << "\n";
                tcl << "  load " << tr->endNodeId() << " " << fx << " " << fy << " " << fz
                    << " 0.0 0.0 0.0 ;# Treillis #" << ml.elementId() << "\n";
                continue;
            }

            LocalMemberLoadComponents localComp = LoadResolver::resolveMemberLoadToLocal(ml, model);

            if (ml.type() == TSA::Model::LoadType::MemberPoint)
            {
                double px = localComp.wx * factor;
                double py = localComp.wy * factor;
                double pz = localComp.wz * factor;
                double pos = ml.x1();
                if (!ml.isRelativePosition())
                {
                    double L = 1.0;
                    if (const auto* b = model.getBeam(ml.elementId())) L = b->length(model);
                    pos = (L > 1e-4) ? (pos / L) : 0.5;
                }
                tcl << "  eleLoad -ele " << ml.elementId() << " -type -beamPoint "
                    << py << " " << pz << " " << pos << " " << px << "\n";
            }
            else
            {
                // Charge répartie uniforme ou linéique moyenne
                double wx = localComp.wx * factor;
                double wy = localComp.wy * factor;
                double wz = localComp.wz * factor;
                tcl << "  eleLoad -ele " << ml.elementId() << " -type -beamUniform "
                    << wy << " " << wz << " " << wx << "\n";
            }
        }

        // C. Poids propre automatique (si activé dans le cas)
        if (options.includeSelfWeight && lc && lc->isSelfWeightIncluded())
        {
            tcl << "  # --- Poids propre automatique gravitaire (-Z) ---\n";
            double swFactor = lc->selfWeightFactor() * factor;

            auto applyElementSelfWeight = [&](int elemId) {
                double q = lm.calculateElementLinearWeight(elemId, model) * swFactor;
                if (q > 1e-6)
                {
                    // Vecteur gravitaire mondial (0, 0, -q)
                    const auto* b = model.getBeam(elemId);
                    int sNode = b ? b->startNodeId() : 0;
                    int eNode = b ? b->endNodeId() : 0;
                    double rot = b ? b->rotation() : 0.0;
                    if (!b)
                    {
                        const auto* col = model.getColumn(elemId);
                        if (col)
                        {
                            sNode = col->startNodeId();
                            eNode = col->endNodeId();
                            rot = col->rotation();
                        }
                    }

                    const auto* n1 = model.getNode(sNode);
                    const auto* n2 = model.getNode(eNode);
                    if (n1 && n2)
                    {
                        gp_Pnt p1(n1->x(), n1->y(), n1->z());
                        gp_Pnt p2(n2->x(), n2->y(), n2->z());
                        LocalMemberLoadComponents swLocal =
                            LoadResolver::decomposeGlobalVectorToLocal(gp_Vec(0.0, 0.0, -q), p1, p2, rot);

                        tcl << "  eleLoad -ele " << elemId << " -type -beamUniform "
                            << swLocal.wy << " " << swLocal.wz << " " << swLocal.wx << "\n";
                    }
                }
            };

            auto applyTrussSelfWeight = [&](int trId) {
                const auto* tr = model.getTrussMember(trId);
                if (!tr) return;
                double q = lm.calculateElementLinearWeight(trId, model) * swFactor;
                if (q > 1e-6)
                {
                    double L = tr->length(model);
                    double halfW = (q * L) * 0.5;
                    tcl << "  load " << tr->startNodeId() << " 0.0 0.0 " << (-halfW)
                        << " 0.0 0.0 0.0 ;# Poids propre treillis #" << trId << "\n";
                    tcl << "  load " << tr->endNodeId() << " 0.0 0.0 " << (-halfW)
                        << " 0.0 0.0 0.0 ;# Poids propre treillis #" << trId << "\n";
                }
            };

            for (const auto& [id, _] : model.beams()) applyElementSelfWeight(id);
            for (const auto& [id, _] : model.columns()) applyElementSelfWeight(id);
            for (const auto& [id, _] : model.trussMembers()) applyTrussSelfWeight(id);
        }

        tcl << "}\n\n";
    };

    if (options.targetCombinationId > 0)
    {
        const auto* combo = lm.getCombination(options.targetCombinationId);
        if (combo)
        {
            tcl << "# Combinaison sélectionnée : " << combo->name() << "\n";
            for (const auto& [caseId, factor] : combo->caseFactors())
            {
                writeLoadsForCase(caseId, factor);
            }
        }
    }
    else if (options.targetLoadCaseId > 0)
    {
        writeLoadsForCase(options.targetLoadCaseId, 1.0);
    }
    else
    {
        // Exporter tous les cas de charges définis
        for (const auto& [caseId, lc] : lm.loadCases())
        {
            writeLoadsForCase(caseId, 1.0);
        }
    }

    // 6. Commandes d'Analyse OpenSees
    if (options.includeAnalysisCommands)
    {
        tcl << "# ------------------------------------------------------------------------------\n";
        tcl << "# Résolution du calcul structural statique linéaire\n";
        tcl << "# ------------------------------------------------------------------------------\n";
        tcl << "constraints Transformation\n";
        tcl << "numberer RCM\n";
        tcl << "system BandGeneral\n";
        tcl << "test NormDispIncr 1.0e-6 15\n";
        tcl << "algorithm Linear\n";
        tcl << "integrator LoadControl 1.0\n";
        tcl << "analysis Static\n";
        tcl << "set ok [analyze 1]\n";
        tcl << "if {$ok == 0} {\n";
        tcl << "  puts \"[TSA-OpenSees] Calcul terminé avec succès !\"\n";
        tcl << "} else {\n";
        tcl << "  puts \"[TSA-OpenSees] Échec de la résolution du calcul.\"\n";
        tcl << "}\n";
    }

    return tcl.str();
}

bool OpenSeesAdapter::exportToFile(const std::string& filePath,
                                   const TSA::Model::Model& model,
                                   const OpenSeesOptions& options)
{
    std::string script = generateTclScript(model, options);
    std::ofstream ofs(filePath);
    if (!ofs.is_open())
    {
        return false;
    }
    ofs << script;
    return true;
}

} // namespace TSA::Analysis
