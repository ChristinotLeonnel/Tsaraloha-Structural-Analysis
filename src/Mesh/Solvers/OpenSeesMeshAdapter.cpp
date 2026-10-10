#include "OpenSeesMeshAdapter.h"

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
    case FeType::Tri3: return "ShellDKGT existe dans OpenSees mais l'adaptateur TSA ne transmet pas encore les coques";
    case FeType::Quad4: return "ShellMITC4 existe dans OpenSees mais l'adaptateur TSA ne transmet pas encore les coques";
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
                                               const TSA::Analysis::AnalysisCapabilities& caps, std::size_t dofLimit)
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

    // Éléments 2D et 3D
    if (!surfaces.empty())
    {
        bool conv = false;
        const bool reject = caps.planarElementPolicy == TSA::Analysis::UnsupportedElementPolicy::Reject;
        std::string detail;
        if (mesh.countOf(FeType::Quad4)) detail += " quadrangles : " + openSeesElementFor(FeType::Quad4, -1, &conv) + ".";
        if (mesh.countOf(FeType::Tri3)) detail += " triangles : " + openSeesElementFor(FeType::Tri3, -1, &conv) + ".";
        if (mesh.countOf(FeType::Quad8) || mesh.countOf(FeType::Tri6)) detail += " second ordre : " + openSeesElementFor(FeType::Quad8, -1, &conv) + ".";
        add(reject ? L::Blocking : L::Warning, "Types d'éléments",
            std::to_string(n2) + " élément(s) de surface non transmis" + (reject ? " (calcul refusé tant qu'ils sont dans la portée)." : " ; les dalles et voiles sont exclus du calcul.") +
                detail,
            list(surfaces));
        r.ignoredElements += n2;
        r.strategies.push_back("Calculer les barres seules : dalles et voiles exclus (avertissement au lancement du calcul)");
        r.strategies.push_back("Représenter les planchers par un grillage de poutres (éléments 1D transmissibles)");
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
    r.estimatedDofs = (geo.tsaNodes.size() + internal) * 6;
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
        "charges nodales inchangées ; poids propre calculé par sous-élément ; aucune charge surfacique transmise (coques non transmises)");
    add(L::Ok, "Groupes", "les éléments sont rattachés à leur barre par la référence TSA (groupes physiques Gmsh), jamais par les étiquettes Gmsh");
    if (r.strategies.empty() && !r.blocking()) r.strategies.push_back("Transmettre le maillage tel quel");
    if (n1second) r.strategies.insert(r.strategies.begin(), "Régénérer le maillage au premier ordre (même finesse) pour transmettre les barres subdivisées");
    return r;
}

} // namespace TSA::Mesh
