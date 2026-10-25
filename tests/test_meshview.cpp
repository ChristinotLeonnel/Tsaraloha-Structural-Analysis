// Suite « meshview » : visualisation du maillage dans la vue 3D (tests 335-346, docs/MESH_VISUALIZATION.md).
// Modèle d'affichage indépendant du moteur (topologies Gmsh, identifiants jamais supposés consécutifs, faces
// extérieures / intérieures des volumes, arêtes uniques, groupes, coupe, erreurs de conversion), source de
// données MeshVS, étiquettes Gmsh conservées (génération, JSON, import multi-groupes), champs de résultats
// (aucune valeur inventée), affichage réel dans OccView (modes, sélection par la détection OCCT, crochet de clic),
// grands maillages, régénérations et cycles affichage / effacement répétés.

#include "test_common.h"

#include "Analysis/LoadResolver.h"
#include "Analysis/ResultsModel.h"
#include "Mesh/MeshDisplayModel.h"
#include "Mesh/MeshGenerator.h"
#include "Mesh/MeshGeometry.h"
#include "Mesh/MeshProject.h"
#include "Mesh/MeshResultField.h"
#include "Viewer/MeshVSDataSource.h"
#include "Viewer/MeshVisual.h"
#include "Viewer/OccView.h"
#include "IO/TSAFile.h"
#include "Analysis/Engine/AnalysisContext.h"
#include "Model/Load/LoadManager.h"

#include <AIS_ListOfInteractive.hxx>
#include <MeshVS_Mesh.hxx>
#include <MeshVS_MeshEntityOwner.hxx>
#include <NCollection_HArray1.hxx>
#include <NCollection_Sequence.hxx>

#include <QMouseEvent>
#include <QPointer>
#include <QTemporaryDir>
#include <QThread>

#include <fstream>
#include <map>
#include <set>

using namespace TSA::Mesh;

namespace
{
/// Bloc structuré nx × ny × nz d'hexaèdres (pas 1 m), identifiants NON consécutifs et éléments en ordre inverse.
FeMesh hexBlock(int nx, int ny, int nz, bool secondOrder = false)
{
    FeMesh m;
    auto nodeId = [&](int i, int j, int k) { return 1000 + 3 * ((k * (ny + 1) + j) * (nx + 1) + i); };
    for (int k = 0; k <= nz; ++k)
        for (int j = 0; j <= ny; ++j)
            for (int i = 0; i <= nx; ++i)
            {
                FeNode n;
                n.id = nodeId(i, j, k);
                n.x = i, n.y = j, n.z = k;
                m.nodes.push_back(n);
            }
    std::map<std::pair<int, int>, int> mids;
    auto mid = [&](int a, int b) {
        const auto key = std::make_pair(std::min(a, b), std::max(a, b));
        if (auto it = mids.find(key); it != mids.end()) return it->second;
        FeNode n;
        n.id = 900000 + 5 * int(mids.size());
        const FeNode *A = nullptr, *B = nullptr;
        for (const auto& x : m.nodes)
        {
            if (x.id == a) A = &x;
            if (x.id == b) B = &x;
        }
        n.x = 0.5 * (A->x + B->x), n.y = 0.5 * (A->y + B->y), n.z = 0.5 * (A->z + B->z);
        m.nodes.push_back(n);
        return mids[key] = n.id;
    };
    std::vector<FeElement> els;
    for (int k = 0; k < nz; ++k)
        for (int j = 0; j < ny; ++j)
            for (int i = 0; i < nx; ++i)
            {
                FeElement e;
                e.id = 50 + 7 * int(els.size());
                e.type = secondOrder ? FeType::Hex20 : FeType::Hex8;
                e.nodes = { nodeId(i, j, k), nodeId(i + 1, j, k), nodeId(i + 1, j + 1, k), nodeId(i, j + 1, k),
                            nodeId(i, j, k + 1), nodeId(i + 1, j, k + 1), nodeId(i + 1, j + 1, k + 1), nodeId(i, j + 1, k + 1) };
                e.source = "F1";
                els.push_back(e);
            }
    if (secondOrder)
    {
        const auto* topo = elementTopology(FeType::Hex20);
        for (auto& e : els)
        {
            const std::vector<int> corners = e.nodes;
            for (const auto& ed : topo->edges) e.nodes.push_back(mid(corners[std::size_t(ed[0])], corners[std::size_t(ed[1])]));
        }
    }
    m.elements.assign(els.rbegin(), els.rend());
    return m;
}

FeNode node(int id, double x, double y, double z, int tsa = 0)
{
    FeNode n;
    n.id = id;
    n.x = x, n.y = y, n.z = z;
    n.tsaNode = tsa;
    return n;
}

FeElement element(int id, FeType t, std::vector<int> nodes, const std::string& src = "X")
{
    FeElement e;
    e.id = id;
    e.type = t;
    e.nodes = std::move(nodes);
    e.source = src;
    return e;
}

/// Maillage mixte : 1D (Line2, Line3), 2D (Tri3, Tri6, Quad4, Quad8), 3D (Tet4, Tet10), identifiants épars.
FeMesh mixedMesh()
{
    FeMesh m;
    m.nodes = { node(11, 0, 0, 0), node(13, 1, 0, 0), node(17, 1, 1, 0), node(19, 0, 1, 0), node(23, 0.5, 0, 0), node(29, 1, 0.5, 0),
                node(31, 0.5, 0.5, 0), node(37, 0.5, 1, 0), node(41, 0, 0.5, 0), node(43, 0, 0, 1), node(47, 0.5, 0.5, 0.5),
                node(53, 0, 0, 0.5), node(59, 0.5, 0, 0.5), node(61, 2, 0, 0), node(67, 3, 0, 0), node(71, 2.5, 0, 0) };
    m.elements = { element(5, FeType::Line2, { 61, 67 }, "B1"),
                   element(6, FeType::Line3, { 67, 61, 71 }, "B2"),
                   element(9, FeType::Tri3, { 11, 13, 31 }, "SL1"),
                   element(14, FeType::Tri6, { 13, 17, 31, 29, 47, 59 }, "SL1"),
                   element(21, FeType::Quad4, { 11, 13, 17, 19 }, "W1"),
                   element(22, FeType::Quad8, { 11, 13, 17, 19, 23, 29, 37, 41 }, "W1"),
                   element(30, FeType::Tet4, { 11, 13, 19, 43 }, "F1"),
                   element(31, FeType::Tet10, { 13, 17, 19, 43, 29, 31, 41, 53, 47, 59 }, "F1") }; // face {13, 19, 43} partagée
    m.groups.push_back({ "SL1", 2, { 9, 14 }, {} });
    m.groups.push_back({ "F1", 3, { 30, 31 }, {} });
    m.groups.push_back({ "poutres", 1, { 5, 6 }, {} });
    m.groups.push_back({ "appuis", 0, {}, { 11, 61 } });
    return m;
}

void pump(int n = 20)
{
    for (int k = 0; k < n; ++k)
    {
        QApplication::processEvents();
        QThread::msleep(5);
    }
}

/// Entité du maillage parmi toutes celles détectées sous le curseur (comme le crochet d'OccView).
bool pickAny(const Handle(AIS_InteractiveContext)& ctx, TSA::Viewer::MeshVisual& visual, TSA::Viewer::MeshVisual::Pick* pick)
{
    for (ctx->InitDetected(); ctx->MoreDetected(); ctx->NextDetected())
        if (visual.handlePick(ctx->DetectedCurrentOwner(), pick)) return true;
    return false;
}

int displayedCount(const Handle(AIS_InteractiveContext)& ctx)
{
    AIS_ListOfInteractive list;
    ctx->DisplayedObjects(list);
    return list.Extent();
}

double ms(std::chrono::steady_clock::time_point t0)
{
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}
} // namespace

bool runSuite_MeshView(int& passed)
{
    // -------------------------------------------------------------------------
    // TEST 335 : topologies (ordre Gmsh) et types pris en charge
    // -------------------------------------------------------------------------
    {
        const std::map<FeType, int> expected { { FeType::Line2, 2 }, { FeType::Line3, 3 }, { FeType::Tri3, 3 }, { FeType::Tri6, 6 }, { FeType::Quad4, 4 },
                                               { FeType::Quad8, 8 }, { FeType::Tet4, 4 }, { FeType::Tet10, 10 }, { FeType::Hex8, 8 }, { FeType::Hex20, 20 } };
        for (const auto& [t, n] : expected)
        {
            const auto* topo = elementTopology(t);
            TEST_CHECK(topo && topo->nodeCount == n && feTypeNodeCount(t) == n && topo->dimension == feTypeDimension(t),
                       "Test 335: topologie " << feTypeName(t) << " (" << n << " nœuds)");
        }
        // Faces orientées vers l'extérieur : normale (Newell) · (centre face − centre élément) > 0.
        auto outward = [](FeType t, const std::vector<std::array<double, 3>>& p) {
            const auto* topo = elementTopology(t);
            std::array<double, 3> c {};
            for (int k = 0; k < topo->cornerCount; ++k)
                for (int d = 0; d < 3; ++d) c[d] += p[std::size_t(k)][d] / topo->cornerCount;
            for (const auto& f : topo->faces)
            {
                std::array<double, 3> nrm {}, fc {};
                for (std::size_t i = 0; i < f.size(); ++i)
                {
                    const auto& a = p[std::size_t(f[i])];
                    const auto& b = p[std::size_t(f[(i + 1) % f.size()])];
                    nrm[0] += (a[1] - b[1]) * (a[2] + b[2]);
                    nrm[1] += (a[2] - b[2]) * (a[0] + b[0]);
                    nrm[2] += (a[0] - b[0]) * (a[1] + b[1]);
                    for (int d = 0; d < 3; ++d) fc[d] += a[d] / f.size();
                }
                if (nrm[0] * (fc[0] - c[0]) + nrm[1] * (fc[1] - c[1]) + nrm[2] * (fc[2] - c[2]) <= 0) return false;
            }
            return true;
        };
        TEST_CHECK(outward(FeType::Tet4, { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } }), "Test 335: faces du tétraèdre orientées vers l'extérieur");
        TEST_CHECK(outward(FeType::Hex8, { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { 1, 0, 1 }, { 1, 1, 1 }, { 0, 1, 1 } }),
                   "Test 335: faces de l'hexaèdre orientées vers l'extérieur");
        // Contour d'une face du second ordre : coins et milieux alternés (Quad8 : 0 4 1 5 2 6 3 7).
        const auto* q8 = elementTopology(FeType::Quad8);
        TEST_CHECK(faceOutline(*q8, q8->faces.front()) == std::vector<int>({ 0, 4, 1, 5, 2, 6, 3, 7 }), "Test 335: contour Quad8 avec milieux");
        const auto* t10 = elementTopology(FeType::Tet10);
        // Face {0,2,1} : arêtes 0-2 (milieu 6), 2-1 (milieu 5), 1-0 (milieu 4).
        TEST_CHECK(faceOutline(*t10, t10->faces.front()) == std::vector<int>({ 0, 6, 2, 5, 1, 4 }), "Test 335: contour d'une face Tet10 (milieux Gmsh)");
        std::cout << "[PASS] Test 335: Topologies des éléments (ordre Gmsh)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 336 : maillage mixte, identifiants épars, arêtes uniques, faces des volumes
    // -------------------------------------------------------------------------
    {
        const FeMesh m = mixedMesh();
        MeshDisplayModel model;
        TEST_CHECK(model.build(m) && model.issues().empty(), "Test 336: maillage mixte construit sans erreur (" << model.issues().size() << ")");
        const auto byDim = model.countByDimension();
        TEST_CHECK(byDim[1] == 2 && byDim[2] == 4 && byDim[3] == 2, "Test 336: 2 éléments 1D, 4 2D, 2 3D");
        TEST_CHECK(model.elementIndex(31) == 7 && model.elementIndex(5) == 0 && model.elementIndex(4) == -1 && model.nodeIndex(71) == 15 && model.nodeIndex(12) == -1,
                   "Test 336: correspondances identifiant ↔ indice (identifiants épars)");
        TEST_CHECK(model.element(14)->type == FeType::Tri6 && model.node(47)->z == 0.5, "Test 336: accès par identifiant métier");
        // Arêtes uniques : aucune paire (a, b) en double.
        std::set<std::pair<int, int>> seen;
        bool dup = false;
        for (const auto& e : model.edges()) dup |= !seen.insert({ e.a, e.b }).second || e.a >= e.b;
        TEST_CHECK(!dup && !model.edges().empty(), "Test 336: arêtes uniques (" << model.edges().size() << ")");
        // Deux triangles partageant une arête : 5 arêtes, pas 6.
        FeMesh two;
        two.nodes = { node(2, 0, 0, 0), node(4, 1, 0, 0), node(6, 1, 1, 0), node(8, 0, 1, 0) };
        two.elements = { element(100, FeType::Tri3, { 2, 4, 6 }), element(7, FeType::Tri3, { 2, 6, 8 }) };
        MeshDisplayModel tm;
        tm.build(two);
        TEST_CHECK(tm.edges().size() == 5, "Test 336: deux triangles → 5 arêtes uniques (" << tm.edges().size() << ")");
        // Bloc 2 × 2 × 2 hexaèdres : 24 faces extérieures, 12 intérieures ; 48 arêtes en surface, 54 en tout.
        const FeMesh block = hexBlock(2, 2, 2);
        MeshDisplayModel bm;
        bm.build(block);
        std::size_t shown = 0;
        for (int id : bm.visibleElements()) shown += bm.volumeFaces(id).size();
        TEST_CHECK(bm.exteriorFaceCount() == 24 && bm.interiorFaceCount() == 12 && shown == 24, "Test 336: bloc 2×2×2 : faces extérieures " << bm.exteriorFaceCount()
                                                                                                    << ", intérieures " << bm.interiorFaceCount() << ", affichées " << shown);
        TEST_CHECK(bm.edges().size() == 48, "Test 336: bloc 2×2×2 : 48 arêtes de surface (" << bm.edges().size() << ")");
        MeshDisplayFilter all;
        all.volumeExteriorOnly = false;
        bm.build(block, all);
        shown = 0;
        for (int id : bm.visibleElements()) shown += bm.volumeFaces(id).size();
        TEST_CHECK(shown == 48 && bm.edges().size() == 54, "Test 336: inspection : 48 faces affichées, 54 arêtes (" << shown << ", " << bm.edges().size() << ")");
        // Second ordre : arêtes avec nœud milieu.
        const FeMesh block2 = hexBlock(1, 1, 1, true);
        MeshDisplayModel b2;
        TEST_CHECK(b2.build(block2) && b2.issues().empty() && b2.edges().size() == 12, "Test 336: Hex20 : 12 arêtes");
        bool mids = true;
        for (const auto& e : b2.edges()) mids &= e.mid >= 900000;
        TEST_CHECK(mids && b2.volumeFaces(block2.elements[0].id).front().size() == 8, "Test 336: Hex20 : milieux d'arêtes et faces à 8 sommets");
        // Deux tétraèdres partageant une face : 6 extérieures, 1 intérieure.
        FeMesh tets;
        tets.nodes = { node(1, 0, 0, 0), node(2, 1, 0, 0), node(3, 0, 1, 0), node(4, 0, 0, 1), node(5, 1, 1, 1) };
        tets.elements = { element(10, FeType::Tet4, { 1, 2, 3, 4 }), element(20, FeType::Tet4, { 2, 3, 4, 5 }) };
        MeshDisplayModel tt;
        tt.build(tets);
        TEST_CHECK(tt.exteriorFaceCount() == 6 && tt.interiorFaceCount() == 1 && tt.edges().size() == 9, "Test 336: deux tétraèdres : 6 faces extérieures, 1 intérieure, 9 arêtes");
        std::cout << "[PASS] Test 336: Maillage mixte, identifiants épars, faces et arêtes uniques" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 337 : source de données MeshVS (identifiants métier, liens, faces, volumes)
    // -------------------------------------------------------------------------
    {
        const FeMesh m = mixedMesh();
        MeshDisplayModel model;
        model.build(m);
        Handle(TSA::Viewer::MeshVSDataSource) ds = new TSA::Viewer::MeshVSDataSource(model);
        TEST_CHECK(ds->GetAllNodes().Extent() == int(model.visibleNodes().size()) && ds->GetAllElements().Extent() == 8 && ds->GetAllElements().Contains(31) &&
                       !ds->GetAllElements().Contains(1),
                   "Test 337: éléments et nœuds de la source = identifiants métier");
        NCollection_Array1<double> coords(1, 3 * 32);
        int nb = 0;
        MeshVS_EntityType type;
        TEST_CHECK(ds->GetGeom(47, false, coords, nb, type) && nb == 1 && type == MeshVS_ET_Node && coords(1) == 0.5 && coords(3) == 0.5, "Test 337: nœud 47");
        TEST_CHECK(ds->GetGeom(6, true, coords, nb, type) && nb == 2 && type == MeshVS_ET_Link && coords(1) == 3 && coords(4) == 2,
                   "Test 337: Line3 → lien entre les nœuds d'extrémité");
        NCollection_Array1<int> ids(1, 32);
        TEST_CHECK(ds->GetGeom(22, true, coords, nb, type) && nb == 8 && type == MeshVS_ET_Face && ds->GetNodesByElement(22, ids, nb) && ids(1) == 11 && ids(2) == 23 &&
                       ids(3) == 13 && ids(8) == 41,
                   "Test 337: Quad8 → face de 8 sommets (coins et milieux alternés)");
        occ::handle<NCollection_HArray1<NCollection_Sequence<int>>> topo;
        TEST_CHECK(ds->GetGeom(31, true, coords, nb, type) && nb == 10 && type == MeshVS_ET_Volume && ds->Get3DGeom(31, nb, topo) && !topo.IsNull() &&
                       topo->Length() == 3 && topo->Value(1).Length() == 6,
                   "Test 337: Tet10 → volume, 3 faces extérieures de 6 sommets (la 4e est partagée avec le Tet4)");
        TEST_CHECK(!ds->GetGeom(999, true, coords, nb, type) && !ds->GetGeom(12, false, coords, nb, type), "Test 337: identifiant inconnu refusé");
        // Bloc 3 × 3 × 3 : l'hexaèdre central n'a aucune face extérieure → absent de la source (ni tracé ni sélectionnable).
        const FeMesh block = hexBlock(3, 3, 3);
        MeshDisplayModel bm;
        bm.build(block);
        Handle(TSA::Viewer::MeshVSDataSource) bs = new TSA::Viewer::MeshVSDataSource(bm);
        TEST_CHECK(bs->GetAllElements().Extent() == 26 && bm.visibleElements().size() == 27, "Test 337: volume entièrement intérieur non tracé (26 / 27)");
        // Déformée : position = initiale + facteur × déplacement, maillage inchangé.
        std::unordered_map<int, std::array<double, 3>> u { { 47, { 0.1, 0.0, -0.2 } } };
        ds->setDisplacements(&u, 10.0);
        const gp_Pnt p = ds->position(47), q = ds->position(11);
        TEST_CHECK(std::abs(p.X() - 1.5) < 1e-12 && std::abs(p.Z() + 1.5) < 1e-12 && q.Distance(gp_Pnt(0, 0, 0)) < 1e-12 && model.node(47)->x == 0.5,
                   "Test 337: positions déformées (×10), nœud sans valeur immobile, maillage intact");
        std::cout << "[PASS] Test 337: Source de données MeshVS" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 338 : erreurs de conversion (jamais d'élément deviné)
    // -------------------------------------------------------------------------
    {
        FeMesh m = mixedMesh();
        m.elements.push_back(element(40, FeType::Quad4, { 11, 13, 17 }));          // nœuds manquants
        m.elements.push_back(element(41, FeType::Tri3, { 11, 13, 9999 }));         // nœud inexistant
        m.elements.push_back(element(9, FeType::Line2, { 61, 67 }));               // identifiant dupliqué
        m.nodes.push_back(node(13, 5, 5, 5));                                      // nœud dupliqué
        MeshDisplayModel model;
        TEST_CHECK(model.build(m), "Test 338: les éléments valides restent affichés");
        std::set<int> bad;
        for (const auto& i : model.issues()) bad.insert(i.elementId);
        TEST_CHECK(bad.count(40) && bad.count(41) && bad.count(9) && model.visibleElements().size() == 8 && !model.isElementVisible(40) && !model.isElementVisible(41),
                   "Test 338: 3 éléments écartés et signalés (" << model.issues().size() << " problèmes)");
        TEST_CHECK(model.node(13)->x == 1.0, "Test 338: nœud dupliqué : le premier est conservé");
        // Type inconnu dans le projet : signalé, jamais converti silencieusement.
        QJsonObject o = meshToJson(mixedMesh());
        QJsonArray els = o.value("elements").toArray();
        QJsonArray e0 = els.at(4).toArray();
        e0.replace(0, QStringLiteral("Pyramid5"));
        els.replace(4, e0);
        o.insert("elements", els);
        const FeMesh back = meshFromJson(o);
        TEST_CHECK(back.elements[4].source.find("type inconnu") != std::string::npos, "Test 338: type inconnu repéré dans la source (" << back.elements[4].source << ")");
        MeshDisplayModel bm;
        bm.build(back);
        bool flagged = false;
        for (const auto& i : bm.issues()) flagged |= i.elementId == back.elements[4].id;
        TEST_CHECK(flagged && !bm.isElementVisible(back.elements[4].id), "Test 338: élément de type inconnu écarté de l'affichage");
        std::cout << "[PASS] Test 338: Erreurs de conversion signalées" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 339 : groupes (appartenance, isolation), filtres de dimension, coupe par éléments
    // -------------------------------------------------------------------------
    {
        const FeMesh m = mixedMesh();
        MeshDisplayModel model;
        model.build(m);
        TEST_CHECK(model.groupsOfElement(14) == std::vector<std::string>({ "SL1" }) && model.groupsOfNode(61) == std::vector<std::string>({ "appuis" }) &&
                       model.groupsOfElement(21).empty(),
                   "Test 339: groupes d'un élément et d'un nœud");
        MeshDisplayFilter f;
        f.isolatedGroups = { "F1", "poutres" };
        model.build(m, f);
        const auto byDim = model.countByDimension();
        TEST_CHECK(byDim[1] == 2 && byDim[2] == 0 && byDim[3] == 2 && !model.isElementVisible(14), "Test 339: isolation de deux groupes");
        f = {};
        f.show2D = false;
        model.build(m, f);
        TEST_CHECK(model.countByDimension()[2] == 0 && model.countByDimension()[1] == 2, "Test 339: masquage des éléments 2D");
        // Coupe x ≤ 1 dans un bloc 2 × 2 × 2 : 4 hexaèdres, 16 faces exposées (dont la face de coupe).
        const FeMesh block = hexBlock(2, 2, 2);
        f = {};
        f.cut = true;
        f.cutNormal = { 1, 0, 0 };
        f.cutOffset = 1.0;
        model.build(block, f);
        std::size_t shown = 0;
        for (int id : model.visibleElements()) shown += model.volumeFaces(id).size();
        TEST_CHECK(model.visibleElements().size() == 4 && shown == 16, "Test 339: coupe : 4 éléments, 16 faces (" << model.visibleElements().size() << ", " << shown << ")");
        // Isolation d'un élément intérieur d'un bloc : toutes ses faces deviennent visibles.
        FeMesh b3 = hexBlock(3, 3, 3);
        b3.groups.push_back({ "centre", 3, { b3.elements[13].id }, {} });
        f = {};
        f.isolatedGroups = { "centre" };
        model.build(b3, f);
        TEST_CHECK(model.visibleElements().size() == 1 && model.volumeFaces(b3.elements[13].id).size() == 6, "Test 339: élément intérieur isolé : 6 faces");
        // Étiquettes : sous-ensemble réparti sur toute la liste.
        std::vector<int> ids(10000), sub;
        for (int i = 0; i < 10000; ++i) ids[std::size_t(i)] = 3 * i + 1;
        TEST_CHECK(MeshDisplayModel::labelSubset(ids, 100, &sub) && sub.size() == 100 && sub.front() == 1 && sub.back() > 3 * 9800, "Test 339: étiquettes réparties");
        std::cout << "[PASS] Test 339: Groupes, filtres et coupe" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 340 : étiquettes Gmsh conservées (génération réelle, JSON) et groupes physiques
    // -------------------------------------------------------------------------
    if (!meshEngineCapabilities().available)
        std::cout << "[SKIP] Test 340: Gmsh indisponible (" << meshEngineCapabilities().unavailableReason << ")" << std::endl;
    else
    {
        Model mdl;
        const int a = mdl.addNode(0, 0, 0), b = mdl.addNode(4, 0, 0), c = mdl.addNode(4, 3, 0), d = mdl.addNode(0, 3, 0);
        mdl.addSlab({ a, b, c, d }, 0.2, "D1");
        const int top = mdl.addNode(0, 0, 3);
        mdl.addBeam(top, mdl.addNode(4, 0, 3));
        mdl.addFoundation(a, 1.5, 1.5, 0.5, "S1");
        MeshSettingsSet s;
        s.global.globalSize = 0.5;
        s.global.mesh2D = s.global.mesh3D = true;
        const auto r = MeshGenerator::generate(MeshGeometry::extract(mdl), s);
        TEST_CHECK(r.ok, "Test 340: génération Gmsh");
        std::set<std::size_t> nodeTags, elemTags;
        bool allTagged = true, entities = true;
        for (const auto& n : r.mesh.nodes) allTagged &= n.gmshTag != 0 && nodeTags.insert(n.gmshTag).second;
        for (const auto& e : r.mesh.elements)
        {
            allTagged &= e.gmshTag != 0 && elemTags.insert(e.gmshTag).second;
            entities &= e.gmshEntityDim == feTypeDimension(e.type) && e.gmshEntityTag > 0;
        }
        TEST_CHECK(allTagged && entities, "Test 340: étiquettes Gmsh uniques pour chaque nœud et élément, entité d'origine conservée");
        std::size_t physical = 0;
        for (const auto& g : r.mesh.groups) physical += g.gmshPhysicalDim >= 0 && g.gmshPhysicalTag > 0;
        TEST_CHECK(physical >= 3, "Test 340: groupes physiques Gmsh repérés (" << physical << ")");
        const FeMesh back = meshFromJson(meshToJson(r.mesh));
        bool same = back.nodes.size() == r.mesh.nodes.size() && back.elements.size() == r.mesh.elements.size() && back.groups.size() == r.mesh.groups.size();
        for (std::size_t i = 0; same && i < back.nodes.size(); ++i) same = back.nodes[i].gmshTag == r.mesh.nodes[i].gmshTag;
        for (std::size_t i = 0; same && i < back.elements.size(); ++i)
            same = back.elements[i].gmshTag == r.mesh.elements[i].gmshTag && back.elements[i].gmshEntityDim == r.mesh.elements[i].gmshEntityDim &&
                   back.elements[i].gmshEntityTag == r.mesh.elements[i].gmshEntityTag;
        for (std::size_t i = 0; same && i < back.groups.size(); ++i)
            same = back.groups[i].gmshPhysicalTag == r.mesh.groups[i].gmshPhysicalTag && back.groups[i].gmshPhysicalDim == r.mesh.groups[i].gmshPhysicalDim;
        TEST_CHECK(same, "Test 340: étiquettes Gmsh relues à l'identique (projet)");
        // Ancien projet (sans étiquettes) : relu avec étiquettes inconnues (0), sans erreur.
        QJsonObject old = meshToJson(r.mesh);
        old.remove("gmshNodeTags");
        old.remove("gmshElements");
        const FeMesh legacy = meshFromJson(old);
        TEST_CHECK(legacy.elements.size() == r.mesh.elements.size() && legacy.nodes.front().gmshTag == 0 && legacy.elements.front().gmshEntityDim == -1,
                   "Test 340: projet antérieur relu (étiquettes inconnues)");
        MeshDisplayModel model;
        TEST_CHECK(model.build(r.mesh) && model.issues().empty() && model.countByDimension()[1] > 0 && model.countByDimension()[2] > 0 && model.countByDimension()[3] > 0,
                   "Test 340: maillage Gmsh réel (1D + 2D + 3D) entièrement affichable");
        std::cout << "[PASS] Test 340: Étiquettes Gmsh et groupes physiques conservés" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 341 : import .msh — étiquettes non consécutives, entité dans deux groupes physiques
    // -------------------------------------------------------------------------
    if (!meshEngineCapabilities().available)
        std::cout << "[SKIP] Test 341: Gmsh indisponible" << std::endl;
    else
    {
        QTemporaryDir dir;
        const std::string path = dir.filePath("deux_groupes.msh").toStdString();
        {
            std::ofstream f(path);
            f << "$MeshFormat\n4.1 0 8\n$EndMeshFormat\n"
                 "$PhysicalNames\n2\n2 10 \"A\"\n2 20 \"B\"\n$EndPhysicalNames\n"
                 "$Entities\n0 0 1 0\n1 0 0 0 1 1 0 2 10 20 0\n$EndEntities\n"
                 "$Nodes\n1 4 11 47\n2 1 0 4\n11\n25\n30\n47\n0 0 0\n1 0 0\n1 1 0\n0 1 0\n$EndNodes\n"
                 "$Elements\n1 2 101 205\n2 1 2 2\n101 11 25 30\n205 11 30 47\n$EndElements\n";
        }
        FeMesh imported;
        std::vector<std::string> warn;
        std::string err;
        TEST_CHECK(MeshGenerator::importMesh(path, &imported, &warn, &err) && imported.elements.size() == 2 && imported.nodes.size() == 4,
                   "Test 341: import .msh (" << err << ")");
        std::set<std::size_t> ntags, etags;
        for (const auto& n : imported.nodes) ntags.insert(n.gmshTag);
        for (const auto& e : imported.elements) etags.insert(e.gmshTag);
        TEST_CHECK(ntags == std::set<std::size_t>({ 11, 25, 30, 47 }) && etags == std::set<std::size_t>({ 101, 205 }),
                   "Test 341: étiquettes Gmsh non consécutives conservées, identifiants TSA distincts");
        const FeGroup *ga = nullptr, *gb = nullptr;
        for (const auto& g : imported.groups)
        {
            if (g.name == "A") ga = &g;
            if (g.name == "B") gb = &g;
        }
        TEST_CHECK(ga && gb && ga->elements.size() == 2 && gb->elements.size() == 2 && ga->gmshPhysicalTag == 10 && gb->gmshPhysicalTag == 20,
                   "Test 341: l'entité appartient aux deux groupes physiques (A : " << (ga ? ga->elements.size() : 0) << ", B : " << (gb ? gb->elements.size() : 0) << ")");
        // Connectivité : le nœud Gmsh 30 est partagé par les deux triangles.
        int shared = 0;
        for (const auto& n : imported.nodes)
            if (n.gmshTag == 30) shared = n.id;
        TEST_CHECK(shared && std::count(imported.elements[0].nodes.begin(), imported.elements[0].nodes.end(), shared) == 1 &&
                       std::count(imported.elements[1].nodes.begin(), imported.elements[1].nodes.end(), shared) == 1,
                   "Test 341: connectivité traduite par étiquette (nœud Gmsh 30 partagé)");
        MeshDisplayModel model;
        model.build(imported);
        TEST_CHECK(model.groupsOfElement(imported.elements[0].id) == std::vector<std::string>({ "A", "B" }) && model.edges().size() == 5,
                   "Test 341: affichage : groupes A et B, 5 arêtes uniques");
        std::cout << "[PASS] Test 341: Import .msh (étiquettes, groupes multiples)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 342 : champs de résultats — valeurs exactes, interpolées, absentes (jamais inventées)
    // -------------------------------------------------------------------------
    {
        Model mdl;
        const int n1 = mdl.addNode(0, 0, 0), n2 = mdl.addNode(4, 0, 0);
        const int beam = mdl.addBeam(n1, n2);
        FeMesh m;
        m.nodes = { node(1, 0, 0, 0, n1), node(2, 1, 0, 0), node(3, 3, 0, 0), node(4, 4, 0, 0, n2), node(5, 0, 2, 0), node(6, 1, 0.5, 0) };
        const std::string ref = "B" + std::to_string(beam);
        m.elements = { element(1, FeType::Line2, { 1, 2 }, ref), element(2, FeType::Line2, { 2, 3 }, ref), element(3, FeType::Line2, { 3, 4 }, ref),
                       element(4, FeType::Tri3, { 1, 2, 5 }, "SL9"), element(5, FeType::Line2, { 2, 6 }, ref) };
        TSA::Analysis::ResultsModel res;
        {
            const auto set = ResultsModelMeshAdapter(mdl, res).extract(m);
            TEST_CHECK(set.fields.empty() && !set.notes.empty(), "Test 342: résultats absents → aucun champ");
        }
        res.setValid(true);
        TSA::Analysis::NodeDisplacement d1, d2;
        d1.uz = -0.001;
        d2.ux = 0.002, d2.uz = -0.003;
        res.setNodeDisplacement(n1, d1);
        res.setNodeDisplacement(n2, d2);
        TSA::Analysis::ElementResults er;
        er.elementId = beam;
        er.kind = TSA::Analysis::StructuralElementKind::Beam;
        er.length = 4.0;
        er.startForces.position = 0;
        er.endForces.position = 4;
        er.startForces.ux = 0.0, er.startForces.uz = -0.001;
        er.endForces.ux = 0.002, er.endForces.uz = -0.003;
        TSA::Analysis::StationForces mid;
        mid.position = 2.0;
        mid.ux = 0.001, mid.uz = -0.010, mid.uy = 0.004;
        er.intermediateStations.push_back(mid);
        res.setElementResults(er);
        const auto set = ResultsModelMeshAdapter(mdl, res).extract(m);
        const auto* u = set.find("Déplacement");
        TEST_CHECK(u && u->components == 3 && u->location == FieldLocation::Node, "Test 342: champ de déplacement");
        TEST_CHECK(u->values.at(1)[2] == -0.001 && u->values.at(4)[0] == 0.002 && u->values.at(4)[2] == -0.003, "Test 342: nœuds TSA : valeurs exactes du solveur");
        // Nœud 2 (x = 1) : entre stations 0 et 2 (t = 0,5) ; nœud 3 (x = 3) : entre 2 et 4 (t = 0,5).
        const gp_Pnt p1(0, 0, 0), p2(4, 0, 0);
        const gp_Vec g2 = TSA::Analysis::LoadResolver::localVectorToGlobal(0.0005, 0.002, -0.0055, p1, p2, 0.0);
        const gp_Vec g3 = TSA::Analysis::LoadResolver::localVectorToGlobal(0.0015, 0.002, -0.0065, p1, p2, 0.0);
        TEST_CHECK(u->has(2) && std::abs(u->values.at(2)[0] - g2.X()) < 1e-15 && std::abs(u->values.at(2)[1] - g2.Y()) < 1e-15 &&
                       std::abs(u->values.at(2)[2] - g2.Z()) < 1e-15 && std::abs(u->values.at(3)[2] - g3.Z()) < 1e-15,
                   "Test 342: nœuds intérieurs : interpolation linéaire entre stations, repère local → global");
        TEST_CHECK(std::abs(g2.X() - 0.0005) < 1e-15, "Test 342: barre selon X : déplacement axial local = composante X globale");
        TEST_CHECK(!u->has(5) && !u->has(6) && !set.notes.empty(), "Test 342: nœud de surface et nœud hors de l'axe sans valeur (" << set.notes.front() << ")");
        double v = 0;
        TEST_CHECK(u->scalar(4, &v) && std::abs(v - std::sqrt(0.002 * 0.002 + 0.003 * 0.003)) < 1e-15 && !u->scalar(5, &v), "Test 342: norme ; aucune valeur pour le nœud 5");
        // Résultats obsolètes : aucun champ.
        res.setValid(false);
        TEST_CHECK(ResultsModelMeshAdapter(mdl, res).extract(m).fields.empty(), "Test 342: résultats obsolètes → aucun champ");
        std::cout << "[PASS] Test 342: Champs de résultats sur le maillage" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 343 : affichage réel dans OccView — modes, étiquettes, résultats, légende
    // Vue OCCT : contexte OpenGL sur une vraie fenêtre ; la plateforme Qt « offscreen » n'en fournit pas (arrêt du
    // programme : 0xC0000409 sous Windows, BadWindow sous X11) : test annoncé comme sauté, jamais compté réussi.
    // -------------------------------------------------------------------------
    if (QGuiApplication::platformName() == QLatin1String("offscreen"))
        std::cout << "[SKIP] Test 343: vue 3D indisponible sur la plateforme Qt « offscreen » (lancer avec QT_QPA_PLATFORM=windows)" << std::endl;
    else
    {
        OccView view;
        view.resize(900, 600);
        view.show();
        pump();
        const Handle(AIS_InteractiveContext)& ctx = view.context();
        const int baseline = displayedCount(ctx);
        TSA::Viewer::MeshVisual visual(ctx, view.view());
        const FeMesh m = mixedMesh();
        visual.show(m, TSA::Viewer::MeshVisual::Mode::Generated);
        pump(5);
        TEST_CHECK(!visual.meshObject().IsNull() && ctx->IsDisplayed(visual.meshObject()) && visual.edgeSegmentCount() > 0,
                   "Test 343: maillage affiché dans le contexte OCCT de la vue 3D");
        // Arêtes : une seule fois chacune (arêtes à milieu tracées en deux segments).
        std::size_t expectedSegments = 0;
        for (const auto& e : visual.model().edges()) expectedSegments += e.mid ? 2 : 1;
        TEST_CHECK(visual.edgeSegmentCount() == expectedSegments, "Test 343: segments = arêtes uniques (" << visual.edgeSegmentCount() << " / " << expectedSegments << ")");
        auto o = visual.options();
        for (auto style : { TSA::Viewer::MeshViewOptions::Style::Wireframe, TSA::Viewer::MeshViewOptions::Style::Faces, TSA::Viewer::MeshViewOptions::Style::Shrink,
                            TSA::Viewer::MeshViewOptions::Style::FacesEdges })
        {
            o.style = style;
            visual.setOptions(o);
            pump(2);
            const bool edges = style == TSA::Viewer::MeshViewOptions::Style::Wireframe || style == TSA::Viewer::MeshViewOptions::Style::FacesEdges;
            TEST_CHECK(ctx->IsDisplayed(visual.meshObject()) && (visual.edgeSegmentCount() > 0) == edges, "Test 343: mode d'affichage " << int(style));
        }
        o.transparency = 0.5;
        o.showNodes = true;
        o.nodeLabels = o.elementLabels = true;
        o.maxLabels = 5;
        visual.setOptions(o);
        TEST_CHECK(visual.labelCount() == 10 && visual.labelsTruncated(), "Test 343: étiquettes limitées (" << visual.labelCount() << ")");
        o.maxLabels = 1000;
        visual.setOptions(o);
        TEST_CHECK(visual.labelCount() == visual.model().visibleNodes().size() + 8 && !visual.labelsTruncated(), "Test 343: toutes les étiquettes");
        o.filter.isolatedGroups = { "F1" };
        visual.setOptions(o);
        TEST_CHECK(visual.model().visibleElements().size() == 2, "Test 343: isolation appliquée à l'affichage");
        o = TSA::Viewer::MeshViewOptions();
        visual.setOptions(o);
        // Résultats : couleurs, légende, déformée ; un nœud sans valeur est dit « non calculé ».
        MeshResultField u;
        u.name = "Déplacement";
        u.unit = "m";
        u.components = 3;
        u.values[61] = { 0, 0, -0.01 };
        u.values[67] = { 0, 0, -0.02 };
        visual.setResults(&u, &u, 10.0);
        pump(2);
        TEST_CHECK(visual.hasResults() && visual.deformationScale() == 10.0 && displayedCount(ctx) > baseline + 2, "Test 343: champ, légende et déformée affichés");
        TEST_CHECK(visual.describeNode(61).find("-0.01") != std::string::npos && visual.describeNode(11).find("non calculé") != std::string::npos,
                   "Test 343: propriétés : valeur calculée / « non calculé »");
        visual.setDeformationScale(0.0);
        visual.setResults(nullptr, nullptr, 0.0);
        TEST_CHECK(!visual.hasResults(), "Test 343: résultats retirés");
        if (const QString dir = qEnvironmentVariable("TSA_TEST_DUMP_DIR"); !dir.isEmpty())
        {
            const FeMesh block = hexBlock(4, 3, 2);
            visual.show(block, TSA::Viewer::MeshVisual::Mode::Generated);
            view.viewIsometric();
            visual.fit();
            pump(5);
            view.captureViewImage(900, 600).save(dir + "/meshview_block.png");
            o.style = TSA::Viewer::MeshViewOptions::Style::Wireframe;
            visual.setOptions(o);
            pump(5);
            view.captureViewImage(900, 600).save(dir + "/meshview_block_wire.png");
            o.style = TSA::Viewer::MeshViewOptions::Style::Shrink;
            visual.setOptions(o);
            pump(5);
            view.captureViewImage(900, 600).save(dir + "/meshview_block_shrink.png");
            o.style = TSA::Viewer::MeshViewOptions::Style::FacesEdges;
            o.filter.cut = true;
            o.filter.cutNormal = { 1, 0, 0 };
            o.filter.cutOffset = 2.0;
            visual.setOptions(o);
            pump(5);
            view.captureViewImage(900, 600).save(dir + "/meshview_block_cut.png");
            o = TSA::Viewer::MeshViewOptions();
            visual.setOptions(o);
            MeshResultField uz;
            uz.name = "Déplacement";
            uz.unit = "m";
            uz.components = 3;
            for (const auto& n : block.nodes)
                if (n.x > 0.5) uz.values[n.id] = { 0.0, 0.0, -0.001 * n.x * n.x };
            visual.setResults(&uz, &uz, 50.0);
            pump(5);
            view.captureViewImage(900, 600).save(dir + "/meshview_block_results.png");
            visual.setResults(nullptr, nullptr, 0.0);
            o = TSA::Viewer::MeshViewOptions();
            visual.setOptions(o);
        }
        visual.clear();
        pump(2);
        TEST_CHECK(displayedCount(ctx) == baseline && !visual.visible(), "Test 343: effacement : contexte revenu à l'état initial");
        std::cout << "[PASS] Test 343: Affichage dans OccView (modes, étiquettes, résultats)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 344 : sélection — détection OCCT réelle, propriétés, crochet de clic d'OccView
    // -------------------------------------------------------------------------
    {
        OccView view;
        view.resize(900, 600);
        view.show();
        pump();
        const Handle(AIS_InteractiveContext)& ctx = view.context();
        TSA::Viewer::MeshVisual visual(ctx, view.view());
        FeMesh m;
        m.nodes = { node(101, 0, 0, 0), node(103, 4, 0, 0), node(107, 4, 4, 0), node(109, 0, 4, 0) };
        m.elements = { element(77, FeType::Quad4, { 101, 103, 107, 109 }, "D7") };
        m.elements[0].gmshTag = 4242;
        m.elements[0].gmshEntityDim = 2;
        m.elements[0].gmshEntityTag = 3;
        m.groups.push_back({ "D7", 2, { 77 }, {} });
        visual.show(m, TSA::Viewer::MeshVisual::Mode::Generated);
        view.viewTop();
        visual.fit();
        pump(5);
        // Hors mode sélection : le maillage n'est pas détectable (la sélection du modèle n'est pas perturbée).
        int px = 0, py = 0;
        view.view()->Convert(2.0, 2.0, 0.0, px, py);
        ctx->MoveTo(px, py, view.view(), false);
        TSA::Viewer::MeshVisual::Pick pick;
        TEST_CHECK(!pickAny(ctx, visual, &pick), "Test 344: maillage non sélectionnable hors mode sélection");
        visual.setPickMode(true);
        pump(2);
        ctx->MoveTo(px, py, view.view(), false);
        TEST_CHECK(pickAny(ctx, visual, &pick) && pick.element && pick.id == 77,
                   "Test 344: clic au centre de la face → élément 77 (identifiant métier)");
        TEST_CHECK(pick.text.find("Quad4") != std::string::npos && pick.text.find("4242") != std::string::npos && pick.text.find("2:3") != std::string::npos &&
                       pick.text.find("D7") != std::string::npos,
                   "Test 344: propriétés : type, étiquette et entité Gmsh, groupe");
        view.view()->Convert(4.0, 4.0, 0.0, px, py);
        ctx->MoveTo(px, py, view.view(), false);
        TEST_CHECK(pickAny(ctx, visual, &pick) && !pick.element && pick.id == 107 &&
                       pick.text.find("Nœud 107") != std::string::npos,
                   "Test 344: clic sur un coin → nœud 107");
        // Crochet d'OccView : un clic réel (événements souris) sur le maillage est transmis au calque.
        int hooked = 0;
        view.setOverlayPickHandlers(
            [&](const Handle(SelectMgr_EntityOwner)& owner) {
                TSA::Viewer::MeshVisual::Pick p;
                if (!visual.handlePick(owner, &p)) return false;
                hooked = p.element ? p.id : -p.id;
                return true;
            },
            [&](const Handle(SelectMgr_EntityOwner)& owner) { return QString::fromStdString(visual.hoverText(owner)); });
        view.view()->Convert(2.0, 2.0, 0.0, px, py);
        const double dpr = view.devicePixelRatioF();
        const QPointF pos(px / dpr, py / dpr);
        QMouseEvent press(QEvent::MouseButtonPress, pos, view.mapToGlobal(pos), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease, pos, view.mapToGlobal(pos), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&view, &press);
        QApplication::sendEvent(&view, &release);
        pump(2);
        TEST_CHECK(hooked == 77, "Test 344: clic souris dans OccView → crochet du maillage (élément " << hooked << ")");
        visual.setPickMode(false);
        pump(2);
        ctx->MoveTo(px, py, view.view(), false);
        TEST_CHECK(!pickAny(ctx, visual, &pick), "Test 344: sélection désactivée");
        visual.clear();
        std::cout << "[PASS] Test 344: Sélection de nœuds et d'éléments" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 345 : grands maillages — temps de construction et d'affichage
    // -------------------------------------------------------------------------
    {
        const auto t0 = std::chrono::steady_clock::now();
        const FeMesh big = hexBlock(60, 60, 30); // 108 000 hexaèdres, 115 351 nœuds
        const double tMake = ms(t0);
        const auto t1 = std::chrono::steady_clock::now();
        MeshDisplayModel model;
        model.build(big);
        const double tModel = ms(t1);
        const std::size_t surfaceFaces = 2 * (60 * 60 + 60 * 30 + 60 * 30);
        TEST_CHECK(model.exteriorFaceCount() == surfaceFaces, "Test 345: 108 000 hexaèdres : " << model.exteriorFaceCount() << " faces extérieures (attendu " << surfaceFaces << ")");
        OccView view;
        view.resize(900, 600);
        view.show();
        pump();
        TSA::Viewer::MeshVisual visual(view.context(), view.view());
        const auto t2 = std::chrono::steady_clock::now();
        visual.show(big, TSA::Viewer::MeshVisual::Mode::Generated);
        visual.fit();
        pump(2);
        const double tShow = ms(t2);
        const auto t3 = std::chrono::steady_clock::now();
        visual.setPickMode(true);
        int px = 0, py = 0;
        view.view()->Convert(30.0, 30.0, 30.0, px, py);
        view.context()->MoveTo(px, py, view.view(), false);
        const double tPick = ms(t3);
        std::cout << "  108 000 hexaèdres : création " << tMake << " ms, modèle d'affichage " << tModel << " ms, affichage OCCT " << tShow
                  << " ms (construction " << visual.lastBuildMs() << " ms), première sélection " << tPick << " ms, " << visual.edgeSegmentCount() << " arêtes" << std::endl;
        TEST_CHECK(visual.edgeSegmentCount() == model.edges().size() && tShow < 120000, "Test 345: affichage d'un grand maillage");
        visual.clear();
        std::cout << "[PASS] Test 345: Grand maillage (108 000 hexaèdres)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 346 : régénérations et cycles affichage / effacement répétés
    // -------------------------------------------------------------------------
    {
        OccView view;
        view.resize(600, 400);
        view.show();
        pump();
        const Handle(AIS_InteractiveContext)& ctx = view.context();
        const int baseline = displayedCount(ctx);
        bool ok = true;
        for (int cycle = 0; cycle < 15 && ok; ++cycle)
        {
            auto visual = std::make_unique<TSA::Viewer::MeshVisual>(ctx, view.view());
            for (int regen = 1; regen <= 3; ++regen)
            {
                const FeMesh m = regen == 2 ? mixedMesh() : hexBlock(regen + cycle % 3, 2, 1);
                visual->show(m, regen == 3 ? TSA::Viewer::MeshVisual::Mode::Preview : TSA::Viewer::MeshVisual::Mode::Generated);
                visual->setPickMode(cycle % 2 == 0);
                visual->highlight(m.elements.front().id, 0, false);
                ok &= visual->model().visibleElements().size() == m.elements.size();
            }
            if (cycle % 2) visual->clear();
            visual.reset(); // destruction sans effacement explicite (fermeture du projet)
            pump(1);
            ok &= displayedCount(ctx) == baseline;
        }
        TEST_CHECK(ok, "Test 346: 15 cycles × 3 régénérations : aucun objet résiduel dans la vue (" << displayedCount(ctx) << " / " << baseline << ")");
        {
            // Ordre réel de l'application : calque créé (setupMesh) AVANT le premier affichage de la vue, dont le
            // contexte OCCT n'existe pas encore ; le maillage doit pourtant s'afficher ensuite.
            OccView late;
            late.resize(500, 400);
            TEST_CHECK(late.context().IsNull(), "Test 346: contexte OCCT absent avant le premier affichage de la vue");
            QPointer<OccView> ref(&late);
            TSA::Viewer::MeshVisual lazy([ref]() { return ref ? ref->context() : Handle(AIS_InteractiveContext)(); },
                                         [ref]() { return ref ? ref->view() : Handle(V3d_View)(); });
            late.show();
            pump();
            lazy.show(mixedMesh(), TSA::Viewer::MeshVisual::Mode::Generated);
            TEST_CHECK(!lazy.meshObject().IsNull() && late.context()->IsDisplayed(lazy.meshObject()),
                       "Test 346: calque créé avant l'initialisation de la vue : maillage affiché ensuite");
            lazy.clear();
        }
        // Projet d'essai de l'interface (TSA_TEST_DUMP_DIR) : 4 poteaux encastrés, 4 poutres de rive, dalle, 2 semelles,
        // cas Q ; réglages de maillage 1D + 2D + 3D (maillage NON généré : l'application le génère avec Gmsh).
        if (const QString dir = qEnvironmentVariable("TSA_TEST_DUMP_DIR"); !dir.isEmpty())
        {
            Model ui;
            const int a = ui.addNode(0, 0, 0), b = ui.addNode(6, 0, 0), c = ui.addNode(0, 0, 3), d = ui.addNode(6, 0, 3);
            const int e = ui.addNode(6, 4, 3), f = ui.addNode(0, 4, 3);
            const int g = ui.addNode(6, 4, 0), h = ui.addNode(0, 4, 0);
            for (int n : { a, b, g, h }) ui.getNode(n)->setSupport(SupportDefinition::fixed());
            ui.addColumn(a, c, 0.3, 0.3, "P1");
            ui.addColumn(b, d, 0.3, 0.3, "P2");
            ui.addColumn(g, e, 0.3, 0.3, "P3");
            ui.addColumn(h, f, 0.3, 0.3, "P4");
            ui.addBeam(c, d, 0.3, 0.5, "Poutre 1");
            ui.addBeam(d, e, 0.3, 0.5, "Poutre 2");
            ui.addBeam(e, f, 0.3, 0.5, "Poutre 3");
            ui.addBeam(f, c, 0.3, 0.5, "Poutre 4");
            ui.addSlab({ c, d, e, f }, 0.2, "Dalle");
            ui.addFoundation(a, 1.5, 1.5, 0.5, "S1");
            ui.addFoundation(b, 1.5, 1.5, 0.5, "S2");
            const int lc = ui.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live, false, 1.0));
            ui.loadManager().addNodalLoad(NodalLoad(0, c, lc, 10.0, 0, -50.0, 0, 0, 0, LoadCoordSystem::Global, "F1"));
            ui.loadManager().addNodalLoad(NodalLoad(0, d, lc, 10.0, 0, -50.0, 0, 0, 0, LoadCoordSystem::Global, "F2"));
            TSA::Analysis::AnalysisContext actx;
            actx.engineId = "opensees";
            actx.dimension = TSA::Analysis::AnalysisDimension::Space3D;
            actx.loadCaseIds = { lc };
            actx.common.includeSelfWeight = false;
            ui.setAnalysisSettingsJson(QJsonDocument(actx.toJson()).toJson(QJsonDocument::Compact).toStdString());
            MeshProject mp;
            mp.settings.global.globalSize = 0.5;
            mp.settings.global.mesh2D = mp.settings.global.mesh3D = true;
            ui.setMeshProjectJson(QJsonDocument(mp.toJson(false)).toJson(QJsonDocument::Compact).toStdString());
            TSA::IO::TSAFileWriter writer;
            std::string err;
            const bool saved = writer.saveToFile((dir + "/meshview_ui.tsa").toStdString(), ui, nullptr, "Essai maillage", "TSA", &err);
            std::cout << "  projet d'essai de l'interface : " << (dir + "/meshview_ui.tsa").toStdString() << (saved ? "" : " — ÉCHEC " + err) << std::endl;
        }
        std::cout << "[PASS] Test 346: Régénérations et cycles répétés" << std::endl;
        ++passed;
    }

    return true;
}
