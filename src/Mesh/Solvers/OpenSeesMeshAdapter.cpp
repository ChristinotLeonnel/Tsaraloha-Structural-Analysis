#include "OpenSeesMeshAdapter.h"
#include "../SurfaceLoads.h"
#include "../../Model/Model.h"
#include "../../Model/Load/LoadManager.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <set>
#include <sstream>

namespace TSA::Mesh
{

using TSA::Analysis::ElementKey;
using TSA::Analysis::StructuralElementKind;

std::optional<ElementKey> memberKeyFromRef(const std::string& ref)
{
    if (ref.size() < 2 || !std::isdigit(static_cast<unsigned char>(ref[1]))) return std::nullopt;
    const int id = std::atoi(ref.c_str() + 1);
    switch (ref[0])
    {
    case 'B': return ElementKey { StructuralElementKind::Beam, id };
    case 'C': return ElementKey { StructuralElementKind::Column, id };
    case 'T': return ElementKey { StructuralElementKind::Truss, id };
    case 'K': return ElementKey { StructuralElementKind::Cable, id };
    default: return std::nullopt;
    }
}

bool SolverCompatibility::blocking() const
{
    for (const auto& i : items)
        if (i.level == SolverCheckItem::Level::Blocking) return true;
    return false;
}
bool SolverCompatibility::hasWarnings() const
{
    for (const auto& i : items)
        if (i.level == SolverCheckItem::Level::Warning) return true;
    return false;
}
std::string SolverCompatibility::summary() const
{
    std::ostringstream s;
    s << engine << " : " << transmittedElements << " élément(s) transmis, " << ignoredElements << " non transmis, " << estimatedDofs << " DDL estimés";
    return s.str();
}

std::string OpenSeesMeshAdapter::openSeesElementFor(FeType t, int family, bool* convertible)
{
    *convertible = false;
    switch (t)
    {
    case FeType::Line2:
        if (family == 0 || family == 1)
        {
            *convertible = true;
            return "elasticBeamColumn (2 nœuds, geomTransf Linear de la barre, 6 DDL par nœud)";
        }
        if (family == 2 || family == 3)
            return "truss / corotTruss : un seul élément par barre (une chaîne de barres articulées subdivisée serait un mécanisme)";
        return "aucun (ligne de contour : élément de frontière, sans rigidité)";
    case FeType::Line3: return "aucun élément OpenSees de barre à 3 nœuds dans l'adaptateur : régénérer au premier ordre";
    case FeType::Tri3:
        *convertible = family == -1;
        return "ShellDKGT (triangle à 3 nœuds, plaque de Kirchhoff discrète + membrane, 6 DDL par nœud)";
    case FeType::Quad4:
        *convertible = family == -1;
        return "ShellMITC4 (quadrangle à 4 nœuds, Mindlin-Reissner à cisaillement interpolé + membrane, 6 DDL par nœud)";
    case FeType::Tri6: return "aucune coque triangulaire à 6 nœuds dans OpenSees";
    case FeType::Quad8: return "aucune coque à 8 nœuds dans OpenSees (ShellMITC9 exige 9 nœuds)";
    case FeType::Tet4:
    case FeType::Tet10:
    case FeType::Hex8:
    case FeType::Hex20:
        return "éléments de volume OpenSees (FourNodeTetrahedron, stdBrick…) à 3 DDL par nœud : incompatibles avec le modèle à 6 DDL "
               "(BasicBuilder -ndm 3 -ndf 6) utilisé par TSA";
    }
    return "inconnu";
}

SolverCompatibility OpenSeesMeshAdapter::check(const FeMesh& mesh, const MeshGeometry& geo, const std::string& engine,
                                               const TSA::Analysis::AnalysisCapabilities& caps, std::size_t dofLimit, const TSA::Model::Model* model)
{
    SolverCompatibility r;
    r.engine = engine;
    using L = SolverCheckItem::Level;
    auto add = [&](L level, const std::string& cat, const std::string& msg, std::vector<std::string> objs = {}) { r.items.push_back({ level, cat, msg, objs }); };
    auto list = [](const std::set<std::string>& s) { return std::vector<std::string>(s.begin(), s.end()); };

    std::set<std::string> frame1, frame2, axial, surfaces, volumes, unknown;
    std::size_t n1second = 0, n2 = 0, n3 = 0;
    for (const auto& e : mesh.elements)
    {
        if (e.boundary) continue;
        const int dim = feTypeDimension(e.type);
        if (dim == 1)
        {
            const auto key = memberKeyFromRef(e.source);
            if (!key || !geo.member(e.source))
            {
                unknown.insert(e.source);
                continue;
            }
            if (feTypeIsSecondOrder(e.type)) ++n1second, frame2.insert(e.source);
            else if (key->kind == StructuralElementKind::Beam || key->kind == StructuralElementKind::Column) frame1.insert(e.source);
            else axial.insert(e.source);
        }
        else if (dim == 2)
            ++n2, surfaces.insert(e.source);
        else
            ++n3, volumes.insert(e.source);
    }
    if (!unknown.empty()) add(L::Blocking, "Correspondance", "objets maillés absents du modèle : maillage obsolète ou importé, à régénérer", list(unknown));

    // Ordre et types des éléments 1D
    if (n1second)
    {
        bool conv = false;
        add(L::Blocking, "Ordre des éléments",
            std::to_string(n1second) + " élément(s) 1D du second ordre : " + openSeesElementFor(FeType::Line3, 0, &conv), list(frame2));
    }
    // Connectivité et ordre des nœuds des barres : chaîne d'éléments du nœud TSA de départ au nœud d'arrivée.
    std::set<std::string> badChain;
    for (const auto& ref : frame1)
    {
        const auto* m = geo.member(ref);
        std::map<int, int> degree;
        bool endsOk = false;
        for (const auto& e : mesh.elements)
            if (e.source == ref && !e.boundary) ++degree[e.nodes[0]], ++degree[e.nodes[1]];
        std::vector<int> ends;
        for (const auto& [n, d] : degree)
            if (d == 1) ends.push_back(n);
            else if (d != 2) ends.clear(), ends.push_back(-1), ends.push_back(-1);
        if (ends.size() == 2 && ends[0] > 0)
        {
            const int t0 = mesh.node(ends[0]).tsaNode, t1 = mesh.node(ends[1]).tsaNode;
            endsOk = (t0 == m->nodeI && t1 == m->nodeJ) || (t0 == m->nodeJ && t1 == m->nodeI);
        }
        if (!endsOk) badChain.insert(ref);
    }
    if (!badChain.empty())
        add(L::Blocking, "Connectivité", "chaîne d'éléments non continue ou sans les nœuds TSA d'extrémité : maillage à régénérer", list(badChain));
    for (const auto& b : badChain) frame1.erase(b);

    if (!frame1.empty())
    {
        if (!caps.supportsFrame)
            add(L::Blocking, "Types d'éléments", engine + " ne calcule pas les barres fléchies", list(frame1));
        else if (!caps.supportsMesh)
            add(L::Warning, "Types d'éléments", engine + " ne prend pas en charge les barres subdivisées : un élément fini par barre sera transmis", list(frame1));
        else
        {
            std::size_t sub = 0;
            for (const auto& ref : frame1)
            {
                // Positions relatives des nœuds intérieurs, mesurées depuis le nœud I de la barre (projection sur l'axe).
                const auto* m = geo.member(ref);
                const P3 ab = m->b - m->a;
                std::set<int> nodes;
                for (const auto& e : mesh.elements)
                    if (e.source == ref && !e.boundary) nodes.insert(e.nodes[0]), nodes.insert(e.nodes[1]);
                std::vector<double> oriented;
                for (int n : nodes)
                {
                    const auto& fn = mesh.node(n);
                    const double t = (P3 { fn.x, fn.y, fn.z } - m->a).dot(ab) / ab.dot(ab);
                    if (t > 1e-9 && t < 1.0 - 1e-9) oriented.push_back(t);
                }
                std::sort(oriented.begin(), oriented.end());
                if (!oriented.empty()) r.memberSubdivisions[*memberKeyFromRef(ref)] = oriented;
                sub += oriented.size() + 1;
            }
            r.transmittedElements += sub;
            bool conv = false;
            add(L::Ok, "Types d'éléments",
                std::to_string(frame1.size()) + " poutre(s) / poteau(x) → " + std::to_string(sub) + " éléments " + openSeesElementFor(FeType::Line2, 0, &conv));
            add(L::Ok, "Transformations et orientations",
                "chaque sous-élément reprend l'axe, la rotation de section et le vecteur geomTransf de sa barre (barres rectilignes : axes locaux identiques)");
        }
    }
    if (!axial.empty())
    {
        bool conv = false;
        add(L::Warning, "Types d'éléments", openSeesElementFor(FeType::Line2, 2, &conv), list(axial));
        r.transmittedElements += axial.size();
    }

    // Éléments 2D : coques (formulation « coque », premier ordre) si le moteur les calcule ; sinon exclus, et dit
    std::size_t surfaceOnlyNodes = 0;
    if (!surfaces.empty())
    {
        bool conv = false;
        if (!caps.supportsShell || !model)
        {
            const bool reject = caps.planarElementPolicy == TSA::Analysis::UnsupportedElementPolicy::Reject;
            add(reject ? L::Blocking : L::Warning, "Types d'éléments",
                std::to_string(n2) + " élément(s) de surface non transmis" +
                    (!caps.supportsShell ? " : " + engine + " ne calcule pas les coques" : " (modèle non fourni à l'adaptateur)") +
                    (reject ? " (calcul refusé tant qu'ils sont dans la portée)." : " ; les dalles et voiles sont exclus du calcul."),
                list(surfaces));
            r.ignoredElements += n2;
            r.strategies.push_back("Calculer les barres seules : dalles et voiles exclus (avertissement au lancement du calcul)");
        }
        else
        {
            std::set<std::string> plate, membrane, nonStructural, secondOrder, shells;
            for (const auto& ref : surfaces)
            {
                const auto* sg = geo.surface(ref);
                if (!sg) continue;
                bool second = false;
                for (const auto& e : mesh.elements)
                    if (!e.boundary && e.source == ref && feTypeDimension(e.type) == 2 && feTypeIsSecondOrder(e.type)) second = true;
                switch (sg->formulation)
                {
                case TSA::Model::SurfaceFormulation::NonStructural: nonStructural.insert(ref); break;
                case TSA::Model::SurfaceFormulation::Plate: plate.insert(ref); break;
                case TSA::Model::SurfaceFormulation::Membrane: membrane.insert(ref); break;
                default: (second ? secondOrder : shells).insert(ref); break;
                }
            }
            if (!plate.empty())
                add(L::Blocking, "Formulation des surfaces",
                    "formulation « plaque seule » non disponible dans OpenSees en 3D (aucun élément de plaque sans membrane) : choisir « coque » "
                    "(la membrane ajoute la rigidité dans le plan) ou « non structurale »",
                    list(plate));
            if (!membrane.empty())
                add(L::Blocking, "Formulation des surfaces",
                    "formulation « membrane seule » non transmise : les éléments de membrane OpenSees (quad, tri31) sont à 2 DDL par nœud, "
                    "incompatibles avec le modèle à 6 DDL de TSA ; une coque à faible rigidité de flexion serait une rigidité fictive",
                    list(membrane));
            if (!secondOrder.empty())
                add(L::Blocking, "Ordre des éléments", "éléments de surface du second ordre : " + openSeesElementFor(FeType::Quad8, -1, &conv) + " — régénérer au premier ordre",
                    list(secondOrder));
            if (!nonStructural.empty())
            {
                std::size_t n = 0;
                for (const auto& e : mesh.elements)
                    if (!e.boundary && nonStructural.count(e.source) && feTypeDimension(e.type) == 2) ++n;
                add(L::Warning, "Types d'éléments", "surface(s) non structurale(s) : non transmises (ni rigidité, ni poids propre, ni charges)", list(nonStructural));
                r.ignoredElements += n;
            }
            if (!plate.empty() || !membrane.empty()) r.strategies.push_back("Choisir la formulation « coque » des surfaces concernées (propriétés de la surface)");

            // Éléments de coque
            auto& sm = r.surfaces;
            std::set<int> usedNodes;
            for (const auto& e : mesh.elements)
            {
                if (e.boundary || !shells.count(e.source) || (e.type != FeType::Tri3 && e.type != FeType::Quad4)) continue;
                const auto* sg = geo.surface(e.source);
                TSA::Analysis::SurfaceMeshElement se;
                se.meshElement = e.id;
                se.source = e.source;
                se.meshNodes = e.nodes;
                se.formulation = sg->formulation;
                if (sg->kind == SurfaceGeom::Kind::Slab)
                {
                    const auto* slab = model->getSlab(sg->id);
                    if (!slab) continue;
                    se.thickness = slab->thickness();
                    se.material = slab->material();
                }
                else
                {
                    const auto* wall = model->getWall(sg->id);
                    if (!wall) continue;
                    se.thickness = wall->thickness();
                    se.material = wall->material();
                }
                usedNodes.insert(e.nodes.begin(), e.nodes.end());
                sm.elements.push_back(se);
            }
            for (int id : usedNodes)
            {
                const auto& n = mesh.node(id);
                sm.nodes.push_back({ id, n.x, n.y, n.z, n.tsaNode });
                if (!n.tsaNode) ++surfaceOnlyNodes;
            }
            // Connexions : une surface dont aucun nœud n'est partagé avec une barre, un appui ou une autre surface est
            // isolée (mécanisme). Aucune liaison rigide n'est ajoutée par défaut (voile excentré, dalle décalée : à relier
            // explicitement, par des nœuds communs).
            {
                std::set<int> memberNodes;
                for (const auto& e : mesh.elements)
                    if (!e.boundary && feTypeDimension(e.type) == 1) memberNodes.insert(e.nodes.begin(), e.nodes.end());
                std::map<int, std::set<std::string>> surfacesAtNode;
                for (const auto& e : sm.elements)
                    for (int n : e.meshNodes) surfacesAtNode[n].insert(e.source);
                std::set<std::string> isolated;
                for (const auto& ref : shells)
                {
                    const auto* sg = geo.surface(ref);
                    bool linked = false;
                    if (sg && sg->kind == SurfaceGeom::Kind::Slab && model->getSlab(sg->id)) linked = !model->getSlab(sg->id)->edgeSupports().empty();
                    if (sg && sg->kind == SurfaceGeom::Kind::Wall && model->getWall(sg->id)) linked = !model->getWall(sg->id)->edgeSupports().empty();
                    for (const auto& [n, refs] : surfacesAtNode)
                    {
                        if (!refs.count(ref)) continue;
                        const int t = mesh.node(n).tsaNode;
                        const auto* tn = t > 0 ? model->getNode(t) : nullptr;
                        if (memberNodes.count(n) || (tn && tn->support().isSupported()) || refs.size() > 1)
                        {
                            linked = true;
                            break;
                        }
                    }
                    if (!linked) isolated.insert(ref);
                }
                if (!isolated.empty())
                    add(L::Blocking, "Connexions",
                        "surface(s) sans aucun nœud commun avec une barre, un appui ou une autre surface (mécanisme) ; aucune liaison rigide "
                        "n'est créée automatiquement (voile excentré, dalle décalée) : relier la surface par des nœuds communs",
                        list(isolated));
            }
            // Taille, types et qualité des éléments transmis (note de calcul)
            {
                auto& q = sm.mesh;
                q.minEdge = 1e300;
                q.minAngle = 180.0;
                for (const auto& e : sm.elements)
                {
                    (e.meshNodes.size() == 4 ? q.quads : q.triangles)++;
                    double lo = 1e300, hi = 0;
                    const std::size_t n = e.meshNodes.size();
                    for (std::size_t k = 0; k < n; ++k)
                    {
                        const auto& a = mesh.node(e.meshNodes[k]);
                        const auto& b = mesh.node(e.meshNodes[(k + 1) % n]);
                        const auto& c = mesh.node(e.meshNodes[(k + n - 1) % n]);
                        const double ab[3] = { b.x - a.x, b.y - a.y, b.z - a.z }, ac[3] = { c.x - a.x, c.y - a.y, c.z - a.z };
                        const double lab = std::sqrt(ab[0] * ab[0] + ab[1] * ab[1] + ab[2] * ab[2]);
                        const double lac = std::sqrt(ac[0] * ac[0] + ac[1] * ac[1] + ac[2] * ac[2]);
                        lo = std::min(lo, lab), hi = std::max(hi, lab);
                        if (lab > 0 && lac > 0)
                        {
                            const double cosA = std::clamp((ab[0] * ac[0] + ab[1] * ac[1] + ab[2] * ac[2]) / (lab * lac), -1.0, 1.0);
                            q.minAngle = std::min(q.minAngle, std::acos(cosA) * 180.0 / 3.14159265358979323846);
                        }
                    }
                    q.minEdge = std::min(q.minEdge, lo), q.maxEdge = std::max(q.maxEdge, hi);
                    if (lo > 0) q.maxAspect = std::max(q.maxAspect, hi / lo);
                }
                q.nodes = sm.nodes.size();
                if (q.empty()) q = {};
                else if (q.minAngle < 15.0 || q.maxAspect > 10.0)
                    add(L::Warning, "Qualité du maillage",
                        "éléments de coque déformés (angle minimal " + std::to_string(int(q.minAngle)) + "°, rapport d'arêtes " + std::to_string(q.maxAspect).substr(0, 5) +
                            ") : précision locale réduite, raffiner ou régulariser le maillage");
            }
            std::set<std::string> badMaterial;
            for (const auto& e : sm.elements)
                if (e.thickness <= 0 || e.material.mechanical.youngModulus <= 0) badMaterial.insert(e.source);
            if (!badMaterial.empty()) add(L::Blocking, "Sections et matériaux", "surface(s) sans épaisseur ou sans module d'Young", list(badMaterial));
            if (!sm.elements.empty())
            {
                r.transmittedElements += sm.elements.size();
                std::size_t quads = 0, tris = 0;
                for (const auto& e : sm.elements) (e.meshNodes.size() == 4 ? quads : tris)++;
                add(L::Ok, "Types d'éléments",
                    std::to_string(shells.size()) + " surface(s) → " + std::to_string(quads) + " ShellMITC4 et " + std::to_string(tris) + " ShellDKGT");
                add(L::Ok, "Connexions",
                    "nœuds des coques partagés avec les barres et les nœuds TSA situés sur leurs arêtes (maillage conforme : arête commune = une courbe du "
                    "mailleur) ; aucune liaison rigide ni excentricité ajoutée (plans moyens confondus avec les axes des barres)");
            }

            // Poids propre des surfaces : ρ · g · e (kN/m²) réparti en charges nodales cohérentes
            for (const auto& ref : shells)
            {
                const auto* sg = geo.surface(ref);
                double rho = 0, h = 0;
                for (const auto& e : sm.elements)
                    if (e.source == ref)
                    {
                        rho = e.material.mechanical.density;
                        h = e.thickness;
                        break;
                    }
                const double q = rho * 9.81 * h / 1000.0;
                if (q <= 0 || !sg) continue;
                SurfacePressure p;
                p.surface = ref;
                p.value = q;
                p.direction = SurfacePressure::Direction::Gravity;
                const auto res = distributeSurfaceLoad(mesh, geo, p);
                if (!res.ok)
                {
                    add(L::Blocking, "Charges", "poids propre de " + ref + " non converti : " + (res.errors.empty() ? std::string() : res.errors.front()), { ref });
                    continue;
                }
                for (const auto& f : res.forces) sm.selfWeight.push_back({ f.node, 0, f.fx, f.fy, f.fz, "Poids propre " + ref });
                sm.selfWeightBySurface[ref] = -res.resultant.z;
            }
            if (!sm.selfWeightBySurface.empty())
            {
                double total = 0;
                for (const auto& [ref, w] : sm.selfWeightBySurface) total += w;
                std::ostringstream os;
                os.precision(6);
                os << "poids propre des surfaces : " << total << " kN (ρ·g·e × aire nette), réparti en charges nodales cohérentes, appliqué aux cas qui "
                   << "incluent le poids propre";
                sm.notes.push_back(os.str());
            }
        }
    }

    // Charges surfaciques du modèle : converties sur les coques transmises, jamais ignorées en silence
    if (model)
    {
        std::set<std::string> transmitted;
        for (const auto& e : r.surfaces.elements) transmitted.insert(e.source);
        std::size_t converted = 0;
        for (const auto& [id, l] : model->loadManager().surfaceLoads())
        {
            const std::string ref = l.surfaceRef();
            const std::string who = "charge surfacique " + std::to_string(id) + (l.name.empty() ? std::string() : " « " + l.name + " »") + " (" + ref + ")";
            if (!transmitted.count(ref))
            {
                add(L::Blocking, "Charges", who + " : surface non transmise au calcul (non maillée, non structurale, formulation non prise en charge ou moteur sans coques)",
                    { ref });
                continue;
            }
            SurfacePressure::Direction dir = SurfacePressure::Direction::Gravity;
            switch (l.direction)
            {
            case TSA::Model::SurfaceLoadDirection::GlobalX: dir = SurfacePressure::Direction::GlobalX; break;
            case TSA::Model::SurfaceLoadDirection::GlobalY: dir = SurfacePressure::Direction::GlobalY; break;
            case TSA::Model::SurfaceLoadDirection::GlobalZ: dir = SurfacePressure::Direction::GlobalZ; break;
            case TSA::Model::SurfaceLoadDirection::LocalNormal: dir = SurfacePressure::Direction::LocalNormal; break;
            default: break;
            }
            SurfaceLoadResult res;
            if (l.distribution == TSA::Model::SurfaceLoadDistribution::Point)
                res = distributeSurfacePointLoad(mesh, geo, { ref, l.u, l.v, l.value, dir });
            else
            {
                SurfacePressure p;
                p.surface = ref;
                p.value = l.value;
                p.direction = dir;
                p.projected = l.projected;
                p.gradU = l.gradU;
                p.gradV = l.gradV;
                p.zone = l.zone;
                p.zoneU0 = l.zoneU0, p.zoneV0 = l.zoneV0, p.zoneU1 = l.zoneU1, p.zoneV1 = l.zoneV1;
                res = distributeSurfaceLoad(mesh, geo, p);
            }
            if (!res.ok)
            {
                add(L::Blocking, "Charges", who + " non convertie : " + (res.errors.empty() ? std::string("erreur inconnue") : res.errors.front()), { ref });
                continue;
            }
            for (const auto& f : res.forces) r.surfaces.loads.push_back({ f.node, l.loadCaseId, f.fx, f.fy, f.fz, who });
            ++converted;
        }
        if (converted)
            r.surfaces.notes.push_back(std::to_string(converted) + " charge(s) surfacique(s) converties en charges nodales cohérentes (∫ Nᵢ p dA), résultante et moment contrôlés");

        // Appuis linéiques des bords : chaque nœud du maillage situé sur le bord reçoit les blocages et la raideur linéique
        // multipliée par sa longueur d'influence (demi-distances aux nœuds voisins sur le bord).
        std::size_t supportedNodes = 0;
        auto edgeSupports = [&](const std::string& ref, const std::vector<std::array<double, 3>>& corners, bool closed,
                                const std::vector<TSA::Model::SurfaceEdgeSupport>& list) {
            if (list.empty()) return;
            if (!transmitted.count(ref))
            {
                add(L::Blocking, "Appuis", "appui(s) linéique(s) de " + ref + " : surface non transmise au calcul (non maillée, non structurale ou formulation non prise en charge)", { ref });
                return;
            }
            std::set<int> nodes;
            std::set<std::pair<int, int>> links;   // arêtes des éléments : un intervalle du bord sans arête est un vide (porte)
            for (const auto& e : r.surfaces.elements)
                if (e.source == ref)
                {
                    nodes.insert(e.meshNodes.begin(), e.meshNodes.end());
                    for (std::size_t k = 0; k < e.meshNodes.size(); ++k)
                    {
                        const int u = e.meshNodes[k], v = e.meshNodes[(k + 1) % e.meshNodes.size()];
                        links.insert({ std::min(u, v), std::max(u, v) });
                    }
                }
            auto linked = [&](int u, int v) { return links.count({ std::min(u, v), std::max(u, v) }) > 0; };
            const int edges = int(corners.size()) - (closed ? 0 : 1);
            std::map<std::pair<int, int>, int> fixedTwice;   // (bord, DDL) bloqué par plusieurs appuis : redondant
            for (std::size_t si = 0; si < list.size(); ++si)
            {
                const auto& sp = list[si];
                const std::string who = "appui linéique " + std::to_string(si + 1) + (sp.name.empty() ? std::string() : " « " + sp.name + " »") + " de " + ref;
                if (!sp.any())
                {
                    add(L::Warning, "Appuis", who + " : aucun DDL bloqué ni raideur, sans effet", { ref });
                    continue;
                }
                if (sp.edge < -1 || sp.edge >= edges)
                {
                    add(L::Blocking, "Appuis", who + " : bord " + std::to_string(sp.edge + 1) + " inexistant (" + std::to_string(edges) + " bord(s))", { ref });
                    continue;
                }
                for (int d = 0; d < 6; ++d)
                    if (sp.fixed[d] && sp.stiffness[d] > 0.0)
                        add(L::Warning, "Appuis", who + " : DDL " + std::to_string(d + 1) + " à la fois bloqué et élastique, la raideur est sans effet", { ref });
                const int e0 = sp.edge < 0 ? 0 : sp.edge, e1 = sp.edge < 0 ? edges - 1 : sp.edge;
                std::size_t count = 0;
                for (int ei = e0; ei <= e1; ++ei)
                {
                    for (int d = 0; d < 6; ++d)
                        if (sp.fixed[d] && ++fixedTwice[{ ei, d }] == 2)
                            add(L::Warning, "Appuis", ref + " bord " + std::to_string(ei + 1) + " : DDL " + std::to_string(d + 1) + " bloqué par plusieurs appuis (condition redondante)", { ref });
                    const auto& A = corners[std::size_t(ei)];
                    const auto& B = corners[std::size_t(ei + 1) % corners.size()];
                    const double ab[3] = { B[0] - A[0], B[1] - A[1], B[2] - A[2] };
                    const double L2 = ab[0] * ab[0] + ab[1] * ab[1] + ab[2] * ab[2];
                    if (L2 < 1e-12) continue;
                    const double Lg = std::sqrt(L2);
                    std::vector<std::pair<double, int>> on;   // (abscisse, nœud)
                    for (int id : nodes)
                    {
                        const auto& n = mesh.node(id);
                        const double ap[3] = { n.x - A[0], n.y - A[1], n.z - A[2] };
                        const double t = (ap[0] * ab[0] + ap[1] * ab[1] + ap[2] * ab[2]) / L2;
                        if (t < -1e-9 || t > 1 + 1e-9) continue;
                        const double dx = ap[0] - t * ab[0], dy = ap[1] - t * ab[1], dz = ap[2] - t * ab[2];
                        if (std::sqrt(dx * dx + dy * dy + dz * dz) <= 1e-5) on.push_back({ std::clamp(t, 0.0, 1.0) * Lg, id });
                    }
                    std::sort(on.begin(), on.end());
                    for (std::size_t k = 0; k < on.size(); ++k)
                    {
                        // Longueur d'influence : demi-intervalles vers les voisins du bord reliés par une arête d'élément
                        const double lo = k > 0 && linked(on[k - 1].second, on[k].second) ? (on[k].first - on[k - 1].first) / 2 : 0.0;
                        const double hi = k + 1 < on.size() && linked(on[k].second, on[k + 1].second) ? (on[k + 1].first - on[k].first) / 2 : 0.0;
                        TSA::Analysis::SurfaceNodeSupport ns;
                        ns.meshNode = on[k].second;
                        ns.fixed = sp.fixed;
                        ns.length = lo + hi;
                        for (int d = 0; d < 6; ++d) ns.stiffness[d] = sp.fixed[d] ? 0.0 : sp.stiffness[d] * ns.length;
                        ns.label = who + ", bord " + std::to_string(ei + 1);
                        r.surfaces.supports.push_back(ns);
                        ++count;
                    }
                }
                if (!count)
                    add(L::Blocking, "Appuis", who + " : aucun nœud du maillage sur le bord visé (régénérer le maillage)", { ref });
                supportedNodes += count;
            }
        };
        for (const auto& [id, s] : model->slabs())
        {
            std::vector<std::array<double, 3>> c;
            for (int nid : s.nodeIds())
                if (const auto* n = model->getNode(nid)) c.push_back({ n->x(), n->y(), n->z() });
            edgeSupports("SL" + std::to_string(id), c, true, s.edgeSupports());
        }
        for (const auto& [id, w] : model->walls())
        {
            const auto* na = model->getNode(w.startNodeId());
            const auto* nb = model->getNode(w.endNodeId());
            if (!na || !nb) continue;
            double dx = nb->x() - na->x(), dy = nb->y() - na->y();
            const double l = std::hypot(dx, dy);
            if (l < 1e-12) continue;
            dx /= l, dy /= l;
            const double ox = -dy * w.offset(), oy = dx * w.offset();
            const std::array<double, 3> a { na->x() + ox, na->y() + oy, na->z() }, b { nb->x() + ox, nb->y() + oy, nb->z() };
            edgeSupports("W" + std::to_string(id), { a, b, { b[0], b[1], b[2] + w.height() }, { a[0], a[1], a[2] + w.height() } }, true, w.edgeSupports());
        }
        if (supportedNodes)
            r.surfaces.notes.push_back(std::to_string(supportedNodes) + " nœud(s) du maillage portés par des appuis linéiques de bord (raideur linéique × longueur "
                                       "d'influence de chaque nœud)");
    }

    if (!volumes.empty())
    {
        bool conv = false;
        add(L::Warning, "Types d'éléments",
            std::to_string(n3) + " élément(s) de volume non transmis : " + openSeesElementFor(FeType::Hex8, -1, &conv) + " ; les appuis définis aux nœuds restent appliqués",
            list(volumes));
        r.ignoredElements += n3;
        r.strategies.push_back("Conserver les appuis (éventuellement élastiques) des nœuds de semelle à la place du volume maillé");
    }

    // DDL : nœuds TSA + nœuds intermédiaires des barres subdivisées, 6 DDL (BasicBuilder -ndm 3 -ndf 6).
    std::size_t internal = 0;
    for (const auto& [k, pos] : r.memberSubdivisions) internal += pos.size();
    r.estimatedDofs = (geo.tsaNodes.size() + internal + surfaceOnlyNodes) * 6;
    add(L::Ok, "Degrés de liberté", "6 DDL par nœud (UX UY UZ RX RY RZ), cohérents avec les éléments de barre transmis");
    if (r.estimatedDofs > dofLimit)
        add(L::Blocking, "Ressources",
            std::to_string(r.estimatedDofs) + " DDL estimés, au-delà de la limite de " + std::to_string(dofLimit) + " : réduire la finesse ou la portée du calcul");
    else if (r.estimatedDofs > dofLimit / 4)
        add(L::Warning, "Ressources", std::to_string(r.estimatedDofs) + " DDL estimés : calcul long et mémoire importante");

    // Sections, matériaux, appuis, charges, groupes
    std::vector<std::string> noSection;
    for (const auto& ref : frame1)
        if (const auto* m = geo.member(ref); m && (m->section.empty() || m->material.empty())) noSection.push_back(ref);
    if (!noSection.empty()) add(L::Blocking, "Sections et matériaux", "barre(s) sans section ou sans matériau", noSection);
    else if (!frame1.empty() || !axial.empty())
        add(L::Ok, "Sections et matériaux", "chaque sous-élément reprend la section, le matériau et les relâchements d'extrémité (aux seules extrémités de la barre)");
    add(L::Ok, "Appuis", "appuis aux nœuds TSA, inchangés ; les nœuds intermédiaires du maillage sont libres");
    add(L::Ok, "Charges",
        "charges sur barres réparties exactement sur les sous-éléments (portions, charge ponctuelle sur le sous-élément porteur ou à un nœud du maillage) ; "
        "charges nodales inchangées ; poids propre calculé par sous-élément ; charges surfaciques et poids propre des coques convertis en charges nodales "
        "cohérentes");
    add(L::Ok, "Groupes", "les éléments sont rattachés à leur barre par la référence TSA (groupes physiques Gmsh), jamais par les étiquettes Gmsh");
    if (r.strategies.empty() && !r.blocking()) r.strategies.push_back("Transmettre le maillage tel quel");
    if (n1second) r.strategies.insert(r.strategies.begin(), "Régénérer le maillage au premier ordre (même finesse) pour transmettre les barres subdivisées");
    return r;
}

} // namespace TSA::Mesh
