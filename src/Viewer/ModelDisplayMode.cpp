#include "ModelDisplayMode.h"

#include "../Analysis/ResultsModel.h"

#include <BRepBuilderAPI_MakeEdge.hxx>

#include <set>

namespace TSA::Viewer
{

using namespace TSA::Analysis;

const char* modelDisplayModeName(ModelDisplayMode mode)
{
    switch (mode)
    {
    case ModelDisplayMode::Physical: return "Modèle physique";
    case ModelDisplayMode::Analytical: return "Modèle filaire analytique";
    case ModelDisplayMode::FiniteElement: return "Modèle éléments finis";
    case ModelDisplayMode::Overlay: return "Superposition physique / analytique";
    }
    return "?";
}

TopoDS_Shape analyticalAxisShape(const gp_Pnt& a, const gp_Pnt& b)
{
    if (a.Distance(b) < 1e-9) return {};
    BRepBuilderAPI_MakeEdge edge(a, b);
    return edge.IsDone() ? edge.Shape() : TopoDS_Shape();
}

Quantity_Color analyticalColor(StructuralElementKind kind)
{
    // Valeurs sRGB (telles qu'affichées ; Quantity_TOC_RGB serait interprété en linéaire et éclairci).
    switch (kind)
    {
    case StructuralElementKind::Beam: return Quantity_Color(0.26, 0.65, 0.96, Quantity_TOC_sRGB);   // bleu
    case StructuralElementKind::Column: return Quantity_Color(0.94, 0.33, 0.31, Quantity_TOC_sRGB); // rouge
    case StructuralElementKind::Truss: return Quantity_Color(0.40, 0.73, 0.42, Quantity_TOC_sRGB);  // vert
    case StructuralElementKind::Cable: return Quantity_Color(1.00, 0.65, 0.15, Quantity_TOC_sRGB);  // orange
    }
    return Quantity_Color(Quantity_NOC_WHITE);
}

StructuralElementKind analyticalKindOf(TSA::Model::BarRole role)
{
    switch (role)
    {
    case TSA::Model::BarRole::Column: return StructuralElementKind::Column;
    case TSA::Model::BarRole::Brace:
    case TSA::Model::BarRole::Tie:
    case TSA::Model::BarRole::Truss: return StructuralElementKind::Truss;
    default: return StructuralElementKind::Beam;
    }
}

SolverMeshStatus solverMeshStatus(const ResultsModel* results, std::size_t planarElements)
{
    SolverMeshStatus st;
    const bool computed = results && (!results->solverMesh().empty() || !results->allDisplacements().empty() ||
                                      !results->allElementResults().empty());
    if (!computed)
    {
        st.message = "Maillage du solveur indisponible : aucun calcul. Lancez un calcul : le maillage réellement "
                     "transmis au moteur sera affiché (axes analytiques affichés en attendant).";
        return st;
    }
    if (!results->isValid())
    {
        st.message = "Maillage du solveur non affiché : le modèle a changé depuis le dernier calcul (résultats "
                     "obsolètes). Relancez le calcul.";
        return st;
    }
    const SolverMesh& mesh = results->solverMesh();
    if (mesh.empty())
    {
        st.message = "Le moteur du dernier calcul ne fournit pas son maillage : rien n'est affiché à sa place "
                     "(axes analytiques seulement).";
        return st;
    }
    st.available = true;
    st.message = "Maillage du solveur — " + mesh.summary() + ".";
    const std::size_t surfaces = mesh.cellCount(SolverCellType::Triangle) + mesh.cellCount(SolverCellType::Quadrilateral);
    if (planarElements > 0 && surfaces == 0)
        st.message += " Dalles et voiles : non maillés par ce moteur, absents du calcul (affichés en arêtes).";
    return st;
}

SolverMeshGeometry buildSolverMeshGeometry(const SolverMesh& mesh, const std::function<bool(const ElementKey&)>& visible)
{
    SolverMeshGeometry g;
    std::set<int> usedNodes;
    auto pnt = [&](int tag, gp_Pnt& out) {
        const SolverMeshNode* n = mesh.node(tag);
        if (!n) return false;
        out.SetCoord(n->x, n->y, n->z);
        return true;
    };
    auto edge = [&](int a, int b) {
        gp_Pnt pa, pb;
        if (pnt(a, pa) && pnt(b, pb) && pa.Distance(pb) > 1e-9) g.segments.emplace_back(pa, pb);
    };
    for (const auto& c : mesh.cells)
    {
        if (c.source && visible && !visible(*c.source))
        {
            ++g.hiddenCells;
            continue;
        }
        ++g.cells;
        const auto& n = c.nodes;
        for (int t : n) usedNodes.insert(t);
        switch (c.type)
        {
        case SolverCellType::Line:
        case SolverCellType::ZeroLength:
            if (n.size() >= 2) edge(n[0], n[1]);
            break;
        case SolverCellType::Triangle:
        case SolverCellType::Quadrilateral:
            for (std::size_t i = 0; i < n.size(); ++i) edge(n[i], n[(i + 1) % n.size()]);
            break;
        case SolverCellType::Tetrahedron:
            if (n.size() >= 4)
                for (int i = 0; i < 4; ++i)
                    for (int j = i + 1; j < 4; ++j) edge(n[i], n[j]);
            break;
        case SolverCellType::Hexahedron:
            if (n.size() >= 8)
                for (int i = 0; i < 4; ++i)
                {
                    edge(n[i], n[(i + 1) % 4]);
                    edge(n[4 + i], n[4 + (i + 1) % 4]);
                    edge(n[i], n[4 + i]);
                }
            break;
        }
    }
    for (int tag : usedNodes)
    {
        const SolverMeshNode* n = mesh.node(tag);
        if (!n) continue;
        (n->tsaNodeId > 0 ? g.modelNodes : g.solverNodes).emplace_back(n->x, n->y, n->z);
    }
    return g;
}

} // namespace TSA::Viewer
