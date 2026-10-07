#include "OpenSeesAnalysisBuilder.h"
#include "OpenSeesModelMap.h"
#include "LoadResolver.h"
#include <algorithm>
#include <set>
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

    // Nœuds reliés uniquement à des treillis / câbles : en -ndf 6, leurs rotations n'ont aucune
    // rigidité (matrice singulière, BUG-019). Elles sont bloquées, sauf si un ressort y agit ; un moment
    // nodal sur un tel nœud ne serait pas repris (ModelValidator l'indique).
    std::set<int> frameNodes, axialNodes;
    for (const auto& [tag, el] : snapshot.elements())
    {
        const bool axial = el.type == SnapshotElement::ElementType::Truss || el.type == SnapshotElement::ElementType::Cable;
        (axial ? axialNodes : frameNodes).insert(el.startNodeId);
        (axial ? axialNodes : frameNodes).insert(el.endNodeId);
    }
    std::map<int, std::set<int>> springDofs;
    for (const auto& sp : map.springs())
        springDofs[sp.nodeId].insert(sp.dofs.begin(), sp.dofs.end());

    for (const auto& [id, n] : snapshot.nodes())
    {
        const bool lockRotations = axialNodes.count(id) && !frameNodes.count(id);
        auto rotation = [&](bool fixed, int dof) {
            return fixed || (lockRotations && !springDofs[id].count(dof));
        };
        const bool fixes[6] = { n.fixTx, n.fixTy, n.fixTz, rotation(n.fixRx, 3), rotation(n.fixRy, 4), rotation(n.fixRz, 5) };
        if (std::none_of(std::begin(fixes), std::end(fixes), [](bool f) { return f; })) continue;
        tcl << "fix " << id;
        for (bool f : fixes) tcl << " " << (f ? 1 : 0);
        if (lockRotations && !(n.fixRx && n.fixRy && n.fixRz)) tcl << " ;# rotations bloquées : nœud relié uniquement à des barres articulées";
        tcl << "\n";
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
                << " -mass " << massDens;
            // Rotules d'extrémité (moments de flexion) : code 1 = nœud i, 2 = nœud j, 3 = les deux.
            // Les autres relâchements (N, V, T) ne sont pas proposés par elasticBeamColumn (ModelValidator).
            const int releaseY = (el->startRelease.my ? 1 : 0) + (el->endRelease.my ? 2 : 0);
            const int releaseZ = (el->startRelease.mz ? 1 : 0) + (el->endRelease.mz ? 2 : 0);
            if (releaseY) tcl << " -releasey " << releaseY;
            if (releaseZ) tcl << " -releasez " << releaseZ;
            tcl << " ;# " << e.key.label() << " " << el->section.name << "\n";
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

void BeamElementLoad::fixedEndForces(double length, double atI[6], double atJ[6]) const
{
    for (int k = 0; k < 6; ++k) atI[k] = atJ[k] = 0.0;
    const double L = length;
    if (L <= 1e-12) return;

    // Charge ponctuelle (px, py, pz) en x = s sur une barre bi-encastrée : réactions exercées SUR la
    // barre (mêmes signes que les efforts d'encastrement d'ElasticBeam3d dans OpenSees).
    auto addPoint = [&](double px, double py, double pz, double s) {
        s = std::clamp(s, 0.0, L);
        const double a = s, b = L - s, L2 = L * L, L3 = L2 * L;
        atI[0] -= px * b / L;
        atJ[0] -= px * a / L;
        const double ryJ = -py * a * a * (a + 3.0 * b) / L3;  // flexion dans le plan x-y (moments Mz)
        atJ[1] += ryJ;
        atI[1] += -py - ryJ;
        atI[5] -= py * a * b * b / L2;
        atJ[5] += py * a * a * b / L2;
        const double rzJ = -pz * a * a * (a + 3.0 * b) / L3;  // flexion dans le plan x-z (moments My)
        atJ[2] += rzJ;
        atI[2] += -pz - rzJ;
        atI[4] += pz * a * b * b / L2;
        atJ[4] -= pz * a * a * b / L2;
    };

    if (kind == Kind::Point)
    {
        addPoint(wx, wy, wz, relativePosition * L);
        return;
    }
    // Charge répartie linéaire : intégrale de Gauss à 4 points, exacte (intégrande de degré 4).
    const bool linear = kind == Kind::Linear;
    const double a = linear ? relativePosition * L : 0.0;
    const double b = linear ? relativeEnd * L : L;
    if (b <= a) return;
    const double xB = linear ? wxB : wx, yB = linear ? wyB : wy, zB = linear ? wzB : wz;
    static const double kNodes[4] = { -0.8611363115940526, -0.3399810435848563, 0.3399810435848563, 0.8611363115940526 };
    static const double kWeights[4] = { 0.3478548451374538, 0.6521451548625461, 0.6521451548625461, 0.3478548451374538 };
    const double mid = 0.5 * (a + b), half = 0.5 * (b - a);
    for (int g = 0; g < 4; ++g)
    {
        const double t = 0.5 * (kNodes[g] + 1.0); // 0 en a, 1 en b
        const double w = kWeights[g] * half;
        addPoint((wx + (xB - wx) * t) * w, (wy + (yB - wy) * t) * w, (wz + (zB - wz) * t) * w, mid + half * kNodes[g]);
    }
}

MemberLoadResultant memberLoadResultant(const TSA::Model::MemberLoad& ml, const CalculationSnapshot& snapshot)
{
    MemberLoadResultant r;
    const auto* el = snapshot.findElementForLoad(ml);
    const auto* n1 = el ? snapshot.getNode(el->startNodeId) : nullptr;
    const auto* n2 = el ? snapshot.getNode(el->endNodeId) : nullptr;
    if (!n1 || !n2 || ml.type() == TSA::Model::LoadType::MemberMoment) return r; // moment réparti : pas de force

    const gp_Pnt p1(n1->x, n1->y, n1->z), p2(n2->x, n2->y, n2->z);
    auto globalIntensity = [&](double q) {
        TSA::Model::MemberLoad at = ml;
        at.setQ1(q);
        const LocalMemberLoadComponents c = LoadResolver::resolveMemberLoadToLocal(at, snapshot);
        return LoadResolver::localVectorToGlobal(c.wx, c.wy, c.wz, p1, p2, el->rotation);
    };
    const double L = el->length;
    auto absolute = [&](double x) { return std::clamp(ml.isRelativePosition() ? x * L : x, 0.0, L); };
    switch (ml.type())
    {
    case TSA::Model::LoadType::MemberPoint:
        r.force = globalIntensity(ml.q1());
        r.centroid = absolute(ml.x1());
        break;
    case TSA::Model::LoadType::MemberLinear:
    {
        const double a = absolute(ml.x1());
        const double b = ml.x2() > ml.x1() ? absolute(ml.x2()) : L;
        const double span = std::max(0.0, b - a);
        const gp_Vec va = globalIntensity(ml.q1()), vb = globalIntensity(ml.q2());
        r.force = (va + vb) * (0.5 * span);
        const double ma = va.Magnitude(), mb = vb.Magnitude();
        r.centroid = a + (ma + mb > 1e-12 ? span * (ma + 2.0 * mb) / (3.0 * (ma + mb)) : 0.5 * span);
        break;
    }
    default:
        r.force = globalIntensity(ml.q1()) * L;
        r.centroid = 0.5 * L;
        break;
    }
    return r;
}

std::vector<LoadPatternSpec> OpenSeesAnalysisBuilder::loadPatterns(const CalculationSnapshot& snapshot,
                                                                   const AnalysisParameters& params)
{
    std::vector<LoadPatternSpec> patterns;
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
                patterns.push_back({ pId++, "Combo Case " + std::to_string(caseId), caseId, factor, includeSW });
            }
        }
    }
    else
    {
        const int filterCaseId = (params.targetLoadCaseId > 0) ? params.targetLoadCaseId : 0;
        patterns.push_back({ 1, "Cas Principal", filterCaseId, 1.0, params.includeSelfWeight });
    }
    return patterns;
}

std::map<int, std::vector<BeamElementLoad>> OpenSeesAnalysisBuilder::beamElementLoads(const CalculationSnapshot& snapshot,
                                                                                     const AnalysisParameters& params,
                                                                                     const LoadPatternSpec& pattern)
{
    std::map<int, std::vector<BeamElementLoad>> loads;
    const double forceScale = params.useKiloNewtons ? 1.0 : 1000.0;

    // Charges sur barres résolues dans leurs repères locaux
    for (const auto& ml : snapshot.memberLoads())
    {
        if (pattern.caseId > 0 && ml.loadCaseId() != pattern.caseId) continue;
        const auto* el = snapshot.findElementForLoad(ml);
        if (!el || el->type == SnapshotElement::ElementType::Truss || el->type == SnapshotElement::ElementType::Cable) continue;
        if (ml.type() == TSA::Model::LoadType::MemberMoment) continue; // non transmis (ModelValidator l'indique)

        const double scale = pattern.factor * forceScale;
        const LocalMemberLoadComponents comp = LoadResolver::resolveMemberLoadToLocal(ml, snapshot);
        BeamElementLoad load;
        load.wx = comp.wx * scale;
        load.wy = comp.wy * scale;
        load.wz = comp.wz * scale;
        const double L = el->length;
        auto relative = [&](double x) {
            if (!ml.isRelativePosition()) x = L > 1e-4 ? x / L : 0.0;
            return std::clamp(x, 0.0, 1.0);
        };
        if (ml.type() == TSA::Model::LoadType::MemberPoint)
        {
            load.kind = BeamElementLoad::Kind::Point;
            load.relativePosition = relative(ml.x1());
        }
        else if (ml.type() == TSA::Model::LoadType::MemberLinear)
        {
            // Même lecture des positions que le moteur Custom2D : x2 <= x1 → jusqu'au nœud j.
            TSA::Model::MemberLoad atEnd = ml;
            atEnd.setQ1(ml.q2());
            const LocalMemberLoadComponents compB = LoadResolver::resolveMemberLoadToLocal(atEnd, snapshot);
            load.kind = BeamElementLoad::Kind::Linear;
            load.relativePosition = relative(ml.x1());
            load.relativeEnd = ml.x2() > ml.x1() ? relative(ml.x2()) : 1.0;
            load.wxB = compB.wx * scale;
            load.wyB = compB.wy * scale;
            load.wzB = compB.wz * scale;
            load.comment = "Charge trapézoïdale (forces d'encastrement parfait)";
        }
        loads[el->tag].push_back(load);
    }

    // Poids propre automatique décomposé (poutres et poteaux ; treillis et câbles : aux nœuds)
    if (pattern.includeSelfWeight)
    {
        for (const auto& [tag, el] : snapshot.elements())
        {
            if (el.type == SnapshotElement::ElementType::Truss || el.type == SnapshotElement::ElementType::Cable) continue;
            const double linWeight = el.section.area() * el.material.density * 9.81 * (params.useKiloNewtons ? 1e-3 : 1.0) * pattern.factor;
            if (linWeight <= 1e-5) continue;
            const auto* n1 = snapshot.getNode(el.startNodeId);
            const auto* n2 = snapshot.getNode(el.endNodeId);
            if (!n1 || !n2) continue;
            const LocalMemberLoadComponents sw = LoadResolver::decomposeGlobalVectorToLocal(
                gp_Vec(0.0, 0.0, -linWeight), gp_Pnt(n1->x, n1->y, n1->z), gp_Pnt(n2->x, n2->y, n2->z), el.rotation);
            BeamElementLoad load;
            load.wx = sw.wx;
            load.wy = sw.wy;
            load.wz = sw.wz;
            load.comment = "Poids propre";
            loads[tag].push_back(load);
        }
    }
    return loads;
}

std::map<int, std::vector<BeamElementLoad>> OpenSeesAnalysisBuilder::beamElementLoads(const CalculationSnapshot& snapshot,
                                                                                     const AnalysisParameters& params)
{
    std::map<int, std::vector<BeamElementLoad>> all;
    for (const auto& pattern : loadPatterns(snapshot, params))
        for (auto& [tag, list] : beamElementLoads(snapshot, params, pattern))
            all[tag].insert(all[tag].end(), list.begin(), list.end());
    return all;
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

    const double forceScale = params.useKiloNewtons ? 1.0 : 1000.0;

    for (const auto& pattern : loadPatterns(snapshot, params))
    {
        const double factor = pattern.factor;
        tcl << "pattern Plain " << pattern.id << " 1 {\n";
        tcl << "  # Pattern " << pattern.name << " (facteur = " << factor << ")\n";

        // Charges nodales
        for (const auto& nl : snapshot.nodalLoads())
        {
            if (pattern.caseId > 0 && nl.loadCaseId() != pattern.caseId) continue;

            double fx = nl.fx() * factor * forceScale;
            double fy = nl.fy() * factor * forceScale;
            double fz = nl.fz() * factor * forceScale;
            double mx = nl.mx() * factor * forceScale;
            double my = nl.my() * factor * forceScale;
            double mz = nl.mz() * factor * forceScale;

            tcl << "  load " << nl.nodeId() << " " << fx << " " << fy << " " << fz << " "
                << mx << " " << my << " " << mz << " ;# " << nl.name() << "\n";
        }

        // Charges sur treillis et câbles : réparties pour moitié sur chaque nœud
        for (const auto& ml : snapshot.memberLoads())
        {
            if (pattern.caseId > 0 && ml.loadCaseId() != pattern.caseId) continue;
            const auto* el = snapshot.findElementForLoad(ml);
            if (!el || (el->type != SnapshotElement::ElementType::Truss && el->type != SnapshotElement::ElementType::Cable)) continue;
            const auto* n1 = snapshot.getNode(el->startNodeId);
            const auto* n2 = snapshot.getNode(el->endNodeId);
            if (!n1 || !n2) continue;

            // Une barre articulée ne reprend pas de charge transversale : la résultante est répartie
            // sur ses deux nœuds par la règle du levier (position du centre de la charge).
            const MemberLoadResultant res = memberLoadResultant(ml, snapshot);
            const gp_Vec total = res.force * (factor * forceScale);
            const double t = el->length > 1e-9 ? std::clamp(res.centroid / el->length, 0.0, 1.0) : 0.5;
            const gp_Vec atI = total * (1.0 - t), atJ = total * t;
            const std::string who = (el->type == SnapshotElement::ElementType::Truss ? "Treillis #" : "Câble #") + std::to_string(el->id);
            tcl << "  load " << el->startNodeId << " " << atI.X() << " " << atI.Y() << " " << atI.Z() << " 0 0 0 ;# " << who << " (charge → nœuds)\n";
            tcl << "  load " << el->endNodeId << " " << atJ.X() << " " << atJ.Y() << " " << atJ.Z() << " 0 0 0 ;# " << who << " (charge → nœuds)\n";
        }

        // Poids propre des treillis et câbles : aux nœuds
        if (pattern.includeSelfWeight)
        {
            for (const auto& [tag, el] : snapshot.elements())
            {
                if (el.type != SnapshotElement::ElementType::Truss && el.type != SnapshotElement::ElementType::Cable) continue;
                const double linWeight = el.section.area() * el.material.density * 9.81 * (params.useKiloNewtons ? 1e-3 : 1.0) * factor;
                if (linWeight <= 1e-5) continue;
                const double halfW = linWeight * el.length * 0.5;
                const std::string who = (el.type == SnapshotElement::ElementType::Truss ? "Poids propre treillis #" : "Poids propre câble #") + std::to_string(el.id);
                tcl << "  load " << el.startNodeId << " 0 0 " << (-halfW) << " 0 0 0 ;# " << who << "\n";
                tcl << "  load " << el.endNodeId << " 0 0 " << (-halfW) << " 0 0 0 ;# " << who << "\n";
            }
        }

        // Poutres et poteaux : eleLoad (même liste que la reconstruction des efforts le long des barres)
        for (const auto& [tag, list] : beamElementLoads(snapshot, params, pattern))
        {
            const auto* el = snapshot.getElementByTag(tag);
            for (const auto& load : list)
            {
                if (load.kind == BeamElementLoad::Kind::Linear)
                {
                    // Forces nodales équivalentes = −(forces d'encastrement parfait), en repère global.
                    const auto* n1 = el ? snapshot.getNode(el->startNodeId) : nullptr;
                    const auto* n2 = el ? snapshot.getNode(el->endNodeId) : nullptr;
                    if (!n1 || !n2) continue;
                    const gp_Pnt p1(n1->x, n1->y, n1->z), p2(n2->x, n2->y, n2->z);
                    double atI[6], atJ[6];
                    load.fixedEndForces(el->length, atI, atJ);
                    auto writeNodalLoad = [&](int nodeId, const double f[6]) {
                        const gp_Vec F = LoadResolver::localVectorToGlobal(-f[0], -f[1], -f[2], p1, p2, el->rotation);
                        const gp_Vec M = LoadResolver::localVectorToGlobal(-f[3], -f[4], -f[5], p1, p2, el->rotation);
                        tcl << "  load " << nodeId << " " << F.X() << " " << F.Y() << " " << F.Z() << " "
                            << M.X() << " " << M.Y() << " " << M.Z() << " ;# Élément " << tag << " : " << load.comment << "\n";
                    };
                    writeNodalLoad(el->startNodeId, atI);
                    writeNodalLoad(el->endNodeId, atJ);
                    continue;
                }
                if (load.point())
                {
                    // eleLoad -ele $tag -type -beamPoint $Py $Pz $xL $Px
                    tcl << "  eleLoad -ele " << tag << " -type -beamPoint "
                        << load.wy << " " << load.wz << " " << load.relativePosition << " " << load.wx;
                }
                else
                {
                    // eleLoad -ele $tag -type -beamUniform $Wy $Wz $Wx
                    tcl << "  eleLoad -ele " << tag << " -type -beamUniform "
                        << load.wy << " " << load.wz << " " << load.wx;
                }
                if (!load.comment.empty()) tcl << " ;# " << load.comment;
                tcl << "\n";
            }
        }

        tcl << "}\n\n";
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

    if (params.type == AnalysisType::NonLinearStatic)
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
