#include "OpenSeesMeshAdapter.h"
#include "../SurfaceLoads.h"
#include "../../Model/Model.h"
#include "../../Model/Load/LoadManager.h"

#include <algorithm>
#include <cctype>
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
