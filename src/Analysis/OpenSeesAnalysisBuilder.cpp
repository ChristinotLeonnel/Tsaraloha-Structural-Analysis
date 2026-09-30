#include "OpenSeesAnalysisBuilder.h"
#include "LoadResolver.h"
#include <sstream>
#include <iomanip>
#include <cmath>

namespace TSA::Analysis
{

std::string OpenSeesAnalysisBuilder::buildScript(const CalculationSnapshot& snapshot,
                                                const AnalysisParameters& params)
{
    std::ostringstream tcl;
    tcl << std::fixed << std::setprecision(6);

    tcl << "# ==============================================================================\n";
    tcl << "# TSA (Tsaraloha Structural Analysis) — Modèle de Calcul OpenSees\n";
    tcl << "# Unités : " << (params.useKiloNewtons ? "kN, m, kPa, kNm" : "N, m, Pa, Nm") << "\n";
    tcl << "# ==============================================================================\n\n";

    tcl << "wipe\n";
    tcl << "model BasicBuilder -ndm 3 -ndf 6\n\n";

    tcl << buildNodes(snapshot);
    tcl << buildBoundaryConditions(snapshot);
    tcl << buildElements(snapshot, params.useKiloNewtons);
    tcl << buildRecorders(snapshot, params);
    tcl << buildLoads(snapshot, params);
    tcl << buildAnalysisCommands(snapshot, params);

    return tcl.str();
}

std::string OpenSeesAnalysisBuilder::buildNodes(const CalculationSnapshot& snapshot)
{
    std::ostringstream tcl;
    tcl << std::fixed << std::setprecision(6);
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
    std::ostringstream tcl;
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
    return tcl.str();
}

std::string OpenSeesAnalysisBuilder::buildElements(const CalculationSnapshot& snapshot, bool useKiloNewtons)
{
    std::ostringstream tcl;
    tcl << std::fixed << std::setprecision(6);
    tcl << "# ------------------------------------------------------------------------------\n";
    tcl << "# Repères locaux & Transformations géométriques (geomTransf Linear)\n";
    tcl << "# ------------------------------------------------------------------------------\n";

    int transfTag = 1;
    std::map<int, int> elemToTransf;

    for (const auto& [id, el] : snapshot.elements())
    {
        if (el.type == SnapshotElement::ElementType::Truss) continue;

        const auto* n1 = snapshot.getNode(el.startNodeId);
        const auto* n2 = snapshot.getNode(el.endNodeId);
        if (!n1 || !n2) continue;

        gp_Pnt p1(n1->x, n1->y, n1->z);
        gp_Pnt p2(n2->x, n2->y, n2->z);

        gp_Ax3 frame = LoadResolver::computeElementLocalAxes(p1, p2, el.rotation);
        gp_Dir dirZ = frame.Direction();

        int tag = transfTag++;
        elemToTransf[id] = tag;
        tcl << "geomTransf Linear " << tag << " " << dirZ.X() << " " << dirZ.Y() << " " << dirZ.Z()
            << " ;# Element #" << id << "\n";
    }
    tcl << "\n";

    tcl << "# ------------------------------------------------------------------------------\n";
    tcl << "# Éléments finis (elasticBeamColumn & truss)\n";
    tcl << "# ------------------------------------------------------------------------------\n";

    double scaleForce = useKiloNewtons ? 1e-3 : 1.0;

    for (const auto& [id, el] : snapshot.elements())
    {
        if (el.type == SnapshotElement::ElementType::Truss)
        {
            double A = el.section.area();
            double E = el.material.mechanical.youngModulus * scaleForce;
            tcl << "element truss " << id << " " << el.startNodeId << " " << el.endNodeId << " "
                << A << " " << E << " ;# Treillis\n";
        }
        else
        {
            double A = el.section.area();
            double E = el.material.mechanical.youngModulus * scaleForce;
            double nu = el.material.mechanical.poissonRatio;
            double G = E / (2.0 * (1.0 + nu));
            double J = el.section.it();
            double Iy = el.section.iy();
            double Iz = el.section.iz();
            int tTag = elemToTransf.count(id) ? elemToTransf[id] : 1;
            double massDens = A * el.material.density * (useKiloNewtons ? 1e-3 : 1.0);
            tcl << "element elasticBeamColumn " << id << " " << el.startNodeId << " " << el.endNodeId << " "
                << A << " " << E << " " << G << " " << J << " " << Iy << " " << Iz << " " << tTag
                << " -mass " << massDens << " ;# " << el.section.name << "\n";
        }
    }
    tcl << "\n";

    return tcl.str();
}

std::string OpenSeesAnalysisBuilder::buildRecorders(const CalculationSnapshot& snapshot,
                                                   const AnalysisParameters& params)
{
    std::ostringstream tcl;
    tcl << "# ------------------------------------------------------------------------------\n";
    tcl << "# Enregistreurs de résultats (recorders)\n";
    tcl << "# ------------------------------------------------------------------------------\n";

    // 1. Déplacements à tous les nœuds
    std::string allNodes;
    for (const auto& [id, _] : snapshot.nodes())
    {
        allNodes += std::to_string(id) + " ";
    }
    if (!allNodes.empty())
    {
        tcl << "recorder Node -file \"" << params.dispOutputFile
            << "\" -node " << allNodes << "-dof 1 2 3 4 5 6 disp\n";
    }

    // 2. Réactions aux appuis
    std::string supportNodes;
    for (const auto& [id, n] : snapshot.nodes())
    {
        if (n.fixTx || n.fixTy || n.fixTz || n.fixRx || n.fixRy || n.fixRz)
        {
            supportNodes += std::to_string(id) + " ";
        }
    }
    if (!supportNodes.empty())
    {
        tcl << "recorder Node -file \"" << params.reactOutputFile
            << "\" -node " << supportNodes << "-dof 1 2 3 4 5 6 reaction\n";
    }

    // 3. Efforts dans les éléments
    std::string allElements;
    for (const auto& [id, _] : snapshot.elements())
    {
        allElements += std::to_string(id) + " ";
    }
    if (!allElements.empty())
    {
        tcl << "recorder Element -file \"" << params.forceOutputFile
            << "\" -ele " << allElements << "localForce\n";
    }
    tcl << "\n";

    return tcl.str();
}

std::string OpenSeesAnalysisBuilder::buildLoads(const CalculationSnapshot& snapshot,
                                               const AnalysisParameters& params)
{
    std::ostringstream tcl;
    tcl << std::fixed << std::setprecision(6);
    tcl << "# ------------------------------------------------------------------------------\n";
    tcl << "# Chargements appliqués\n";
    tcl << "# ------------------------------------------------------------------------------\n";

    tcl << "timeSeries Linear 1\n";

    auto writeLoads = [&](int patternId, const std::string& patternName, double factor) {
        tcl << "pattern Plain " << patternId << " 1 {\n";
        tcl << "  # Pattern " << patternName << " (facteur = " << factor << ")\n";

        // Charges nodales
        for (const auto& nl : snapshot.nodalLoads())
        {
            if (params.targetLoadCaseId > 0 && nl.loadCaseId() != params.targetLoadCaseId) continue;

            double fx = nl.fx() * factor;
            double fy = nl.fy() * factor;
            double fz = nl.fz() * factor;
            double mx = nl.mx() * factor;
            double my = nl.my() * factor;
            double mz = nl.mz() * factor;

            tcl << "  load " << nl.nodeId() << " " << fx << " " << fy << " " << fz << " "
                << mx << " " << my << " " << mz << " ;# " << nl.name() << "\n";
        }

        // Charges sur barres
        for (const auto& ml : snapshot.memberLoads())
        {
            if (params.targetLoadCaseId > 0 && ml.loadCaseId() != params.targetLoadCaseId) continue;

            const auto* el = snapshot.getElement(ml.elementId());
            if (!el) continue;

            if (el->type == SnapshotElement::ElementType::Truss)
            {
                double L = el->length;
                double totalF = ml.q1() * L * factor;
                double halfF = totalF * 0.5;
                tcl << "  load " << el->startNodeId << " 0 0 " << (-halfF) << " 0 0 0\n";
                tcl << "  load " << el->endNodeId << " 0 0 " << (-halfF) << " 0 0 0\n";
            }
            else if (ml.type() == TSA::Model::LoadType::MemberPoint)
            {
                double p = ml.q1() * factor;
                double pos = ml.x1();
                if (!ml.isRelativePosition() && el->length > 1e-4)
                {
                    pos /= el->length;
                }
                // eleLoad -ele $tag -type -beamPoint $Py $Pz $xL $Px
                tcl << "  eleLoad -ele " << ml.elementId() << " -type -beamPoint 0.0 " << (-p) << " " << pos << " 0.0\n";
            }
            else
            {
                double q = ml.q1() * factor;
                // eleLoad -ele $tag -type -beamUniform $Wy $Wz $Wx
                tcl << "  eleLoad -ele " << ml.elementId() << " -type -beamUniform 0.0 " << (-q) << " 0.0\n";
            }
        }

        // Poids propre
        if (params.includeSelfWeight)
        {
            for (const auto& [id, el] : snapshot.elements())
            {
                double A = el.section.area();
                double rho = el.material.density; // kg/m3
                double g = 9.81;
                double linWeight = A * rho * g * (params.useKiloNewtons ? 1e-3 : 1.0) * factor;

                if (linWeight > 1e-5)
                {
                    if (el.type == SnapshotElement::ElementType::Truss)
                    {
                        double halfW = linWeight * el.length * 0.5;
                        tcl << "  load " << el.startNodeId << " 0 0 " << (-halfW) << " 0 0 0\n";
                        tcl << "  load " << el.endNodeId << " 0 0 " << (-halfW) << " 0 0 0\n";
                    }
                    else
                    {
                        tcl << "  eleLoad -ele " << id << " -type -beamUniform 0.0 " << (-linWeight) << " 0.0\n";
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
                writeLoads(pId++, "Combo Case " + std::to_string(caseId), factor);
            }
        }
    }
    else
    {
        writeLoads(1, "Cas Principal", 1.0);
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
        tcl << "constraints Transformation\n";
        tcl << "numberer RCM\n";
        tcl << "system BandGeneral\n";
        tcl << "test NormDispIncr " << params.tolerance << " " << params.maxIterations << " 0\n";
        tcl << "algorithm " << params.algorithm << "\n";
        double stepSize = 1.0 / std::max(1, params.numSteps);
        tcl << "integrator LoadControl " << stepSize << "\n";
        tcl << "analysis Static\n";
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
        tcl << "constraints Transformation\n";
        tcl << "numberer RCM\n";
        tcl << "system BandGeneral\n";
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

    tcl << "record\n";
    tcl << "wipe\n";

    return tcl.str();
}

} // namespace TSA::Analysis
