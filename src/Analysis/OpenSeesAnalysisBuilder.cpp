#include "OpenSeesAnalysisBuilder.h"
#include "OpenSeesModelMap.h"
#include "LoadResolver.h"
#include <sstream>
#include <iomanip>
#include <cmath>

namespace TSA::Analysis
{

namespace
{
int springMaterialCount(const OpenSeesModelMap& map)
{
    int n = 0;
    for (const auto& s : map.springs()) n += static_cast<int>(s.dofs.size());
    return n;
}

std::string recorderPrecision()
{
    return " -precision " + std::to_string(kRecorderPrecision);
}
} // namespace

std::string OpenSeesAnalysisBuilder::joinTags(const std::vector<int>& tags)
{
    std::string s;
    for (int t : tags)
    {
        if (!s.empty()) s += ' ';
        s += std::to_string(t);
    }
    return s;
}

std::string OpenSeesAnalysisBuilder::buildScript(const CalculationSnapshot& snapshot,
                                                const AnalysisParameters& params)
{
    return buildScript(snapshot, OpenSeesModelMap::build(snapshot, params), params);
}

std::string OpenSeesAnalysisBuilder::buildScript(const CalculationSnapshot& snapshot,
                                                const OpenSeesModelMap& map,
                                                const AnalysisParameters& params)
{
    std::ostringstream tcl;
    tcl << std::setprecision(17); // 17 chiffres significatifs : aller-retour double exact (std::fixed tronquait les inerties)

    tcl << "# ==============================================================================\n";
    tcl << "# TSA (Tsaraloha Structural Analysis) — Modèle de Calcul OpenSees\n";
    tcl << "# Unités : " << (params.useKiloNewtons ? "kN, m, kPa, kNm" : "N, m, Pa, Nm") << "\n";
    tcl << "# Tags d'éléments : uniques (1..N), correspondance TSA ↔ OpenSees dans OpenSeesModelMap\n";
    tcl << "# ==============================================================================\n\n";

    tcl << "wipe\n";
    tcl << "model BasicBuilder -ndm 3 -ndf 6\n\n";

    tcl << buildNodes(snapshot);
    tcl << buildBoundaryConditions(snapshot, map);
    tcl << buildElements(snapshot, map, params);
    tcl << buildRecorders(map, params);
    tcl << buildLoads(snapshot, params);
    tcl << buildAnalysisCommands(snapshot, params);

    return tcl.str();
}

std::string OpenSeesAnalysisBuilder::buildMatrixScript(const CalculationSnapshot& snapshot,
                                                      const OpenSeesModelMap& map,
                                                      const AnalysisParameters& params,
                                                      bool withGlobalStiffness)
{
    std::ostringstream tcl;
    tcl << "# ==============================================================================\n";
    tcl << "# TSA — Passage « matrices » (mode Advanced) : aucune charge, aucune résolution.\n";
    tcl << "# Rigidités à l'état de référence non déformé (= rigidité linéaire pour des éléments\n";
    tcl << "# élastiques en transformation Linear).\n";
    tcl << "# ==============================================================================\n\n";
    tcl << "wipe\n";
    tcl << "model BasicBuilder -ndm 3 -ndf 6\n\n";
    tcl << buildNodes(snapshot);
    tcl << buildBoundaryConditions(snapshot, map);
    tcl << buildElements(snapshot, map, params);

    if (!map.basicStiffnessBeamTags().empty())
    {
        tcl << "recorder Element -file \"" << params.beamBasicStiffnessOutputFile << "\"" << recorderPrecision()
            << " -ele " << joinTags(map.basicStiffnessBeamTags()) << " basicStiffness\n";
    }
    if (!map.basicStiffnessTrussTags().empty())
    {
        tcl << "recorder Element -file \"" << params.trussBasicStiffnessOutputFile << "\"" << recorderPrecision()
            << " -ele " << joinTags(map.basicStiffnessTrussTags()) << " basicStiffness\n";
    }

    // Même gestion des contraintes et même numérotation que l'analyse principale.
    tcl << "constraints " << toTclString(params.constraintHandler) << "\n";
    tcl << "numberer RCM\n";
    tcl << "system " << (withGlobalStiffness ? "FullGeneral" : "BandGeneral") << "\n";
    tcl << "test NormDispIncr 1e-8 10\n";
    tcl << "algorithm Linear\n";
    tcl << "integrator LoadControl 0.0\n";
    tcl << "analysis Static\n";
    tcl << "initialize\n";
    tcl << "record\n\n";

    tcl << "set tsaDofFile [open \"" << params.dofMapOutputFile << "\" w]\n";
    tcl << "foreach tsaNode {" << joinTags(map.structuralNodeTags()) << "} {\n";
    tcl << "  puts $tsaDofFile \"$tsaNode [nodeDOFs $tsaNode]\"\n";
    tcl << "}\n";
    tcl << "close $tsaDofFile\n";

    if (withGlobalStiffness)
    {
        // printA -ret : %.10e (11 chiffres significatifs) ; la sortie fichier de printA ignore
        // -precision dans OpenSees 3.8.0 (6 chiffres) : non utilisée.
        tcl << "set tsaK [printA -ret]\n";
        tcl << "set tsaKFile [open \"" << params.globalStiffnessOutputFile << "\" w]\n";
        tcl << "puts $tsaKFile $tsaK\n";
        tcl << "close $tsaKFile\n";
    }
    tcl << "puts \"TSA_OPS_MATRICES_DONE\"\n";
    tcl << "wipe\n";
    return tcl.str();
}

std::string OpenSeesAnalysisBuilder::buildNodes(const CalculationSnapshot& snapshot)
{
    std::ostringstream tcl;
    tcl << std::setprecision(17); // 17 chiffres significatifs : aller-retour double exact (std::fixed tronquait les inerties)
    tcl << "# ------------------------------------------------------------------------------\n";
    tcl << "# Nœuds structuraux (node $nodeTag $x $y $z)\n";
    tcl << "# ------------------------------------------------------------------------------\n";

    for (const auto& [id, n] : snapshot.nodes())
    {
        tcl << "node " << id << " " << n.x << " " << n.y << " " << n.z;
        if (!n.name.empty())
        {
            tcl << " ;# " << n.name;
        }
        tcl << "\n";
    }
    tcl << "\n";
    return tcl.str();
}

std::string OpenSeesAnalysisBuilder::buildBoundaryConditions(const CalculationSnapshot& snapshot)
{
    return buildBoundaryConditions(snapshot, OpenSeesModelMap::build(snapshot, AnalysisParameters{}));
}

std::string OpenSeesAnalysisBuilder::buildBoundaryConditions(const CalculationSnapshot& snapshot,
                                                            const OpenSeesModelMap& map)
{
    std::ostringstream tcl;
    tcl << std::setprecision(17); // 17 chiffres significatifs : aller-retour double exact (std::fixed tronquait les inerties)
    tcl << "# ------------------------------------------------------------------------------\n";
    tcl << "# Conditions aux limites (fix $nodeTag $u1 $u2 $u3 $r1 $r2 $r3)\n";
    tcl << "# ------------------------------------------------------------------------------\n";

    for (const auto& [id, n] : snapshot.nodes())
    {
        if (n.fixTx || n.fixTy || n.fixTz || n.fixRx || n.fixRy || n.fixRz)
        {
            tcl << "fix " << id << " "
                << (n.fixTx ? 1 : 0) << " "
                << (n.fixTy ? 1 : 0) << " "
                << (n.fixTz ? 1 : 0) << " "
                << (n.fixRx ? 1 : 0) << " "
                << (n.fixRy ? 1 : 0) << " "
                << (n.fixRz ? 1 : 0) << "\n";
        }
    }
    tcl << "\n";

    // Appuis élastiques : nœud auxiliaire entièrement fixé + zeroLength (uniaxialMaterial Elastic
    // par DDL). Tags fournis par OpenSeesModelMap (au-delà des tags TSA : aucune collision).
    if (!map.springs().empty())
    {
        tcl << "# ------------------------------------------------------------------------------\n";
        tcl << "# Appuis élastiques (ressorts via zeroLength)\n";
        tcl << "# ------------------------------------------------------------------------------\n";

        int matTag = 1;
        for (const auto& s : map.springs())
        {
            const auto* n = snapshot.getNode(s.nodeId);
            if (!n) continue;
            tcl << "node " << s.auxNodeTag << " " << n->x << " " << n->y << " " << n->z
                << " ;# auxiliaire ressort N" << s.nodeId << "\n";
            tcl << "fix " << s.auxNodeTag << " 1 1 1 1 1 1\n";

            std::string matTags;
            std::string dirs;
            for (std::size_t i = 0; i < s.dofs.size(); ++i)
            {
                const int m = matTag++;
                tcl << "uniaxialMaterial Elastic " << m << " " << s.stiffness[i] << "\n";
                if (!matTags.empty()) { matTags += " "; dirs += " "; }
                matTags += std::to_string(m);
                dirs += std::to_string(s.dofs[i] + 1);
            }
            tcl << "element zeroLength " << s.elementTag << " " << s.auxNodeTag << " " << s.nodeId
                << " -mat " << matTags << " -dir " << dirs << "\n";
        }
        tcl << "\n";
    }

    return tcl.str();
}

std::string OpenSeesAnalysisBuilder::buildElements(const CalculationSnapshot& snapshot,
                                                  const OpenSeesModelMap& map,
                                                  const AnalysisParameters& params)
{
    std::ostringstream tcl;
    tcl << std::setprecision(17); // 17 chiffres significatifs : aller-retour double exact (std::fixed tronquait les inerties)
    tcl << "# ------------------------------------------------------------------------------\n";
    tcl << "# Repères locaux & Transformations géométriques (geomTransf " << toTclString(params.geomTransf) << ")\n";
    tcl << "# ------------------------------------------------------------------------------\n";

    for (const auto& e : map.elements())
    {
        if (!e.isBeamColumn()) continue;
        tcl << "geomTransf " << toTclString(params.geomTransf) << " " << e.transfTag << " "
            << e.vecxz[0] << " " << e.vecxz[1] << " " << e.vecxz[2]
            << " ;# " << e.key.label() << "\n";
    }
    tcl << "\n";

    tcl << "# ------------------------------------------------------------------------------\n";
    tcl << "# Éléments finis (elasticBeamColumn, truss, corotTruss) — commentaire = élément TSA\n";
    tcl << "# ------------------------------------------------------------------------------\n";

    const double scaleForce = params.useKiloNewtons ? 1e-3 : 1.0;
    // Les tags de matériaux 1..S sont pris par les ressorts (buildBoundaryConditions).
    int matTag = springMaterialCount(map) + 1;

    for (const auto& e : map.elements())
    {
        const auto* el = snapshot.getElementByTag(e.tag);
        if (!el) continue;

        const double A = el->section.area();
        const double E = el->material.mechanical.youngModulus * scaleForce;

        if (el->type == SnapshotElement::ElementType::Cable)
        {
            double Ac = A;
            if (Ac < 1e-8) Ac = 1e-4; // Sécurité section minimale câble
            const int baseMat = matTag++;
            tcl << "uniaxialMaterial Elastic " << baseMat << " " << E << "\n";
            int mat = baseMat;
            if (el->initialTension > 1e-4)
            {
                // Précontrainte / Tension initiale du câble (InitStrain: eps0 = T0 / (E * A))
                const double t0Scaled = el->initialTension * (params.useKiloNewtons ? 1.0 : 1000.0);
                const double eps0 = t0Scaled / (E * Ac);
                mat = matTag++;
                tcl << "uniaxialMaterial InitStrain " << mat << " " << baseMat << " " << eps0 << "\n";
            }
            tcl << "element corotTruss " << e.tag << " " << e.nodeI << " " << e.nodeJ << " "
                << Ac << " " << mat << " ;# " << e.key.label() << " câble\n";
        }
        else if (el->type == SnapshotElement::ElementType::Truss)
        {
            // OpenSees 3.8.0 : element truss|corotTruss $tag $iNode $jNode $A $matTag
            // (la forme « $A $E » est refusée : « Invalid matTag »).
            const int mat = matTag++;
            tcl << "uniaxialMaterial Elastic " << mat << " " << E << "\n";
            tcl << "element " << e.opsClass << " " << e.tag << " " << e.nodeI << " " << e.nodeJ << " "
                << A << " " << mat << " ;# " << e.key.label() << "\n";
        }
        else
        {
            const double nu = el->material.mechanical.poissonRatio;
            const double G = E / (2.0 * (1.0 + nu));
            const double J = el->section.it();
            const double Iy = el->section.iy();
            const double Iz = el->section.iz();
            const double massDens = A * el->material.density * (params.useKiloNewtons ? 1e-3 : 1.0);
            tcl << "element elasticBeamColumn " << e.tag << " " << e.nodeI << " " << e.nodeJ << " "
                << A << " " << E << " " << G << " " << J << " " << Iy << " " << Iz << " " << e.transfTag
                << " -mass " << massDens << " ;# " << e.key.label() << " " << el->section.name << "\n";
        }
    }
    tcl << "\n";

    return tcl.str();
}

std::string OpenSeesAnalysisBuilder::buildRecorders(const OpenSeesModelMap& map,
                                                   const AnalysisParameters& params)
{
    std::ostringstream tcl;
    tcl << "# ------------------------------------------------------------------------------\n";
    tcl << "# Enregistreurs de résultats (recorders) — " << kRecorderPrecision << " chiffres significatifs\n";
    tcl << "# ------------------------------------------------------------------------------\n";

    if (!map.structuralNodeTags().empty())
    {
        tcl << "recorder Node -file \"" << params.dispOutputFile << "\"" << recorderPrecision()
            << " -node " << joinTags(map.structuralNodeTags()) << " -dof 1 2 3 4 5 6 disp\n";
    }
    // Réactions : nœuds fixés puis nœuds auxiliaires des ressorts (reportées sur le nœud TSA).
    if (!map.reactionNodeTags().empty())
    {
        tcl << "recorder Node -file \"" << params.reactOutputFile << "\"" << recorderPrecision()
            << " -node " << joinTags(map.reactionNodeTags()) << " -dof 1 2 3 4 5 6 reaction\n";
    }
    // ElasticBeam3d localForce : 12 valeurs [N Vy Vz T My Mz]_i,j (forces sur l'élément, repère local).
    if (!map.beamColumnTags().empty())
    {
        tcl << "recorder Element -file \"" << params.forceOutputFile << "\"" << recorderPrecision()
            << " -ele " << joinTags(map.beamColumnTags()) << " localForce\n";
    }
    // Truss / CorotTruss basicForce : 1 valeur (effort normal, traction > 0). « localForce » n'est
    // pas utilisable : 12 valeurs pour Truss, aucune réponse pour CorotTruss (OpenSees 3.8.0).
    if (!map.axialTags().empty())
    {
        tcl << "recorder Element -file \"" << params.axialOutputFile << "\"" << recorderPrecision()
            << " -ele " << joinTags(map.axialTags()) << " basicForce\n";
    }
    if (params.extractionLevel == ExtractionLevel::Advanced)
    {
        if (!map.allElementTags().empty())
        {
            tcl << "recorder Element -file \"" << params.globalForceOutputFile << "\"" << recorderPrecision()
                << " -ele " << joinTags(map.allElementTags()) << " globalForce\n";
        }
        if (!map.beamColumnTags().empty())
        {
            tcl << "recorder Element -file \"" << params.basicForceOutputFile << "\"" << recorderPrecision()
                << " -ele " << joinTags(map.beamColumnTags()) << " basicForce\n";
        }
    }
    tcl << "\n";

    return tcl.str();
}

std::string OpenSeesAnalysisBuilder::buildLoads(const CalculationSnapshot& snapshot,
                                               const AnalysisParameters& params)
{
    std::ostringstream tcl;
    tcl << std::setprecision(17); // 17 chiffres significatifs : aller-retour double exact (std::fixed tronquait les inerties)
    tcl << "# ------------------------------------------------------------------------------\n";
    tcl << "# Chargements appliqués\n";
    tcl << "# ------------------------------------------------------------------------------\n";

    tcl << "timeSeries Linear 1\n";

    double forceScale = params.useKiloNewtons ? 1.0 : 1000.0;

    auto writeLoads = [&](int patternId, const std::string& patternName, int filterCaseId, double factor, bool includeSW) {
        tcl << "pattern Plain " << patternId << " 1 {\n";
        tcl << "  # Pattern " << patternName << " (facteur = " << factor << ")\n";

        // Charges nodales
        for (const auto& nl : snapshot.nodalLoads())
        {
            if (filterCaseId > 0 && nl.loadCaseId() != filterCaseId) continue;

            double fx = nl.fx() * factor * forceScale;
            double fy = nl.fy() * factor * forceScale;
            double fz = nl.fz() * factor * forceScale;
            double mx = nl.mx() * factor * forceScale;
            double my = nl.my() * factor * forceScale;
            double mz = nl.mz() * factor * forceScale;

            tcl << "  load " << nl.nodeId() << " " << fx << " " << fy << " " << fz << " "
                << mx << " " << my << " " << mz << " ;# " << nl.name() << "\n";
        }

        // Charges sur barres résolues dans leurs repères locaux
        for (const auto& ml : snapshot.memberLoads())
        {
            if (filterCaseId > 0 && ml.loadCaseId() != filterCaseId) continue;

            const auto* el = snapshot.findElementForLoad(ml);
            if (!el) continue;

            LocalMemberLoadComponents comp = LoadResolver::resolveMemberLoadToLocal(ml, snapshot);

            if (el->type == SnapshotElement::ElementType::Truss || el->type == SnapshotElement::ElementType::Cable)
            {
                const auto* n1 = snapshot.getNode(el->startNodeId);
                const auto* n2 = snapshot.getNode(el->endNodeId);
                if (n1 && n2)
                {
                    gp_Pnt p1(n1->x, n1->y, n1->z);
                    gp_Pnt p2(n2->x, n2->y, n2->z);
                    gp_Vec gVec = LoadResolver::localVectorToGlobal(comp.wx, comp.wy, comp.wz, p1, p2, el->rotation);
                    double totalMult = factor * forceScale * (ml.type() == TSA::Model::LoadType::MemberPoint ? 1.0 : el->length) * 0.5;
                    double hfx = gVec.X() * totalMult;
                    double hfy = gVec.Y() * totalMult;
                    double hfz = gVec.Z() * totalMult;
                    const std::string who = (el->type == SnapshotElement::ElementType::Truss ? "Treillis #" : "Câble #") + std::to_string(el->id);
                    tcl << "  load " << el->startNodeId << " " << hfx << " " << hfy << " " << hfz << " 0 0 0 ;# " << who << " (charge → nœuds)\n";
                    tcl << "  load " << el->endNodeId << " " << hfx << " " << hfy << " " << hfz << " 0 0 0 ;# " << who << " (charge → nœuds)\n";
                }
            }
            else if (ml.type() == TSA::Model::LoadType::MemberPoint)
            {
                double px = comp.wx * factor * forceScale;
                double py = comp.wy * factor * forceScale;
                double pz = comp.wz * factor * forceScale;
                double pos = ml.x1();
                if (!ml.isRelativePosition() && el->length > 1e-4)
                {
                    pos /= el->length;
                }
                // eleLoad -ele $tag -type -beamPoint $Py $Pz $xL $Px
                tcl << "  eleLoad -ele " << el->tag << " -type -beamPoint "
                    << py << " " << pz << " " << pos << " " << px << "\n";
            }
            else
            {
                double wx = comp.wx * factor * forceScale;
                double wy = comp.wy * factor * forceScale;
                double wz = comp.wz * factor * forceScale;
                // eleLoad -ele $tag -type -beamUniform $Wy $Wz $Wx
                tcl << "  eleLoad -ele " << el->tag << " -type -beamUniform "
                    << wy << " " << wz << " " << wx << "\n";
            }
        }

        // Poids propre automatique décomposé
        if (includeSW)
        {
            for (const auto& [tag, el] : snapshot.elements())
            {
                double A = el.section.area();
                double rho = el.material.density; // kg/m3
                double g = 9.81;
                double linWeight = A * rho * g * (params.useKiloNewtons ? 1e-3 : 1.0) * factor;

                if (linWeight > 1e-5)
                {
                    if (el.type == SnapshotElement::ElementType::Truss || el.type == SnapshotElement::ElementType::Cable)
                    {
                        double halfW = linWeight * el.length * 0.5;
                        const std::string who = (el.type == SnapshotElement::ElementType::Truss ? "Poids propre treillis #" : "Poids propre câble #") + std::to_string(el.id);
                        tcl << "  load " << el.startNodeId << " 0 0 " << (-halfW) << " 0 0 0 ;# " << who << "\n";
                        tcl << "  load " << el.endNodeId << " 0 0 " << (-halfW) << " 0 0 0 ;# " << who << "\n";
                    }
                    else
                    {
                        const auto* n1 = snapshot.getNode(el.startNodeId);
                        const auto* n2 = snapshot.getNode(el.endNodeId);
                        if (n1 && n2)
                        {
                            gp_Pnt p1(n1->x, n1->y, n1->z);
                            gp_Pnt p2(n2->x, n2->y, n2->z);
                            LocalMemberLoadComponents swComp = LoadResolver::decomposeGlobalVectorToLocal(
                                gp_Vec(0.0, 0.0, -linWeight), p1, p2, el.rotation
                            );
                            tcl << "  eleLoad -ele " << tag << " -type -beamUniform "
                                << swComp.wy << " " << swComp.wz << " " << swComp.wx << " ;# Poids propre\n";
                        }
                    }
                }
            }
        }

        tcl << "}\n\n";
    };

    if (params.targetCombinationId > 0)
    {
        auto it = snapshot.combinations().find(params.targetCombinationId);
        if (it != snapshot.combinations().end())
        {
            int pId = 1;
            for (const auto& [caseId, factor] : it->second.caseFactors())
            {
                bool includeSW = false;
                auto lcIt = snapshot.loadCases().find(caseId);
                if (lcIt != snapshot.loadCases().end())
                {
                    includeSW = lcIt->second.isSelfWeightIncluded();
                }
                writeLoads(pId++, "Combo Case " + std::to_string(caseId), caseId, factor, includeSW);
            }
        }
    }
    else
    {
        int filterCaseId = (params.targetLoadCaseId > 0) ? params.targetLoadCaseId : 0;
        writeLoads(1, "Cas Principal", filterCaseId, 1.0, params.includeSelfWeight);
    }

    return tcl.str();
}

std::string OpenSeesAnalysisBuilder::buildAnalysisCommands(const CalculationSnapshot& /*snapshot*/,
                                                          const AnalysisParameters& params)
{
    std::ostringstream tcl;
    tcl << "# ------------------------------------------------------------------------------\n";
    tcl << "# Résolution du calcul structural\n";
    tcl << "# ------------------------------------------------------------------------------\n";

    if (params.type == AnalysisType::Modal)
    {
        tcl << "set numModes " << params.numEigenmodes << "\n";
        tcl << "set eigenvalues [eigen -fullGenLapack $numModes]\n";
        tcl << "puts \"TSA_OPS_MODAL_START\"\n";
        tcl << "for {set i 0} {$i < $numModes} {incr i} {\n";
        tcl << "  set lambda [lindex $eigenvalues $i]\n";
        tcl << "  puts [format \"MODE %d LAMBDA %e\" [expr {$i+1}] $lambda]\n";
        tcl << "}\n";
        tcl << "puts \"TSA_OPS_MODAL_END\"\n";
    }
    else if (params.type == AnalysisType::NonLinearStatic)
    {
        tcl << "constraints " << toTclString(params.constraintHandler) << "\n";
        tcl << "numberer RCM\n";
        tcl << "system " << toTclString(params.systemSolver) << "\n";
        tcl << "test NormDispIncr " << params.tolerance << " " << params.maxIterations << " 0\n";
        tcl << "algorithm " << toTclString(params.algorithmType) << "\n";

        switch (params.integratorType)
        {
        case IntegratorType::DisplacementControl:
            tcl << "integrator DisplacementControl " << params.controlNodeId << " "
                << params.controlDof << " " << params.dispIncrement << "\n";
            break;
        case IntegratorType::ArcLength:
        {
            double s = (params.stepSize > 0.0) ? params.stepSize : 0.05;
            tcl << "integrator ArcLength " << s << " 1.0\n";
            break;
        }
        case IntegratorType::MinUnbalDispNorm:
        {
            double s = (params.stepSize > 0.0) ? params.stepSize : 0.05;
            tcl << "integrator MinUnbalDispNorm " << s << "\n";
            break;
        }
        case IntegratorType::LoadControl:
        default:
        {
            double stepSize = (params.stepSize > 0.0) ? params.stepSize : (1.0 / std::max(1, params.numSteps));
            tcl << "integrator LoadControl " << stepSize << "\n";
            break;
        }
        }

        tcl << "analysis Static\n";
        tcl << "set ok 0\n";
        tcl << "for {set i 1} {$i <= " << params.numSteps << "} {incr i} {\n";
        tcl << "  set ok [analyze 1]\n";
        tcl << "  if {$ok != 0} {\n";
        tcl << "    puts \"TSA_OPS_CONVERGENCE_FAIL Step $i\"\n";
        tcl << "    break\n";
        tcl << "  }\n";
        tcl << "}\n";
        tcl << "if {$ok == 0} {\n";
        tcl << "  puts \"TSA_OPS_SUCCESS Non-Linear Static Converged\"\n";
        tcl << "}\n";
    }
    else
    {
        // Linear Static
        tcl << "constraints " << toTclString(params.constraintHandler) << "\n";
        tcl << "numberer RCM\n";
        tcl << "system " << toTclString(params.systemSolver) << "\n";
        tcl << "test NormDispIncr " << params.tolerance << " " << params.maxIterations << "\n";
        tcl << "algorithm Linear\n";
        tcl << "integrator LoadControl 1.0\n";
        tcl << "analysis Static\n";
        tcl << "set ok [analyze 1]\n";
        tcl << "if {$ok == 0} {\n";
        tcl << "  puts \"TSA_OPS_SUCCESS Linear Static Converged\"\n";
        tcl << "} else {\n";
        tcl << "  puts \"TSA_OPS_FAIL Linear Static Analysis Failed\"\n";
        tcl << "}\n";
    }

    tcl << "wipe\n";

    return tcl.str();
}

} // namespace TSA::Analysis
