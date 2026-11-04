// Suite « copypaste » : copie complète d'objets du modèle, quel que soit le chemin (presse-papier, outils de copie).
//   549  presse-papier : poutre sans charge, avec charge ponctuelle, plusieurs réparties, mélange, plusieurs éléments ;
//        valeurs exactes, charge rattachée à la nouvelle barre, identifiants uniques, aucun cas de charge dupliqué ;
//   550  attributs : matériau, section, rotation, relâchements, couleurs, appuis, groupe connecté, surfaces et leurs charges ;
//   551  collages répétés, Annuler, Rétablir, révision du modèle (invalidation des résultats) ;
//   552  outils Copier et Réseau, au clic et en saisie numérique : charges, voiles, fondations, ouvertures de dalle ;
//   553  copie par rotation : image des forces, des moments, des charges globales et locales, gravité conservée ;
//   554  copie miroir : moments axiaux, axes locaux, dalle (ouvertures, bords, charges en u, v), nœuds partagés, voiles ;
//   555  enregistrement puis relecture d'un projet contenant des copies ;
//   556  vue réelle 3D et 2D : charges des copies affichées ;
//   557  options du presse-papier, cas de charge homonymes, collage dans un autre projet.

#include "test_common.h"
#include <gp_Trsf.hxx>
#include <gp_Ax2.hxx>

#include "Analysis/LoadResolver.h"
#include "Coordinate/CoordinateTransformationService.h"
#include "Interaction/Tools/ModelingTool.h"
#include "Mesh/MeshGeometry.h"
#include "Model/CopyTransform.h"
#include "Model/Load/LoadManager.h"
#include "Model/StructuralClipboard.h"
#include "UndoRedo/EditTransaction.h"
#include "Viewer/OccView.h"

#include <NCollection_List.hxx>
#include <QApplication>
#include <QThread>

using namespace TSA::Interaction;
using TSA::Analysis::LoadResolver;
using TSA::Coordinate::CoordinateTransformationService;

namespace
{
constexpr double kTol = 1e-9;

bool same(double a, double b, double tol = kTol)
{
    return std::abs(a - b) <= tol;
}

bool sameVec(const gp_Vec& a, const gp_Vec& b, double tol = 1e-8)
{
    return (a - b).Magnitude() <= tol;
}

bool sameMemberLoad(const MemberLoad& a, const MemberLoad& b)
{
    return a.type() == b.type() && a.q1() == b.q1() && a.q2() == b.q2() && a.x1() == b.x1() && a.x2() == b.x2() && a.direction() == b.direction() &&
           a.coordSystem() == b.coordSystem() && a.isRelativePosition() == b.isRelativePosition() && a.name() == b.name() && a.targetType() == b.targetType();
}

std::vector<MemberLoad> loadsOnBeam(const Model& m, int beam)
{
    std::vector<MemberLoad> loads;
    for (const auto& [id, l] : m.loadManager().memberLoads())
        if (l.targetType() == MemberTargetType::Beam && l.elementId() == beam) loads.push_back(l);
    return loads;
}

std::vector<MemberLoad> loadsOnColumn(const Model& m, int column)
{
    std::vector<MemberLoad> loads;
    for (const auto& [id, l] : m.loadManager().memberLoads())
        if (l.targetType() == MemberTargetType::Column && l.elementId() == column) loads.push_back(l);
    return loads;
}

ClipboardSelection beamsSelection(std::set<int> beams)
{
    ClipboardSelection s;
    s.beams = std::move(beams);
    return s;
}

/// Toute charge vise un objet existant et un cas de charge existant.
bool referencesValid(const Model& m)
{
    const auto& lm = m.loadManager();
    for (const auto& [id, l] : lm.nodalLoads())
        if (!m.getNode(l.nodeId()) || !lm.getLoadCase(l.loadCaseId())) return false;
    for (const auto& [id, l] : lm.memberLoads())
    {
        const bool target = (l.targetType() == MemberTargetType::Beam && m.getBeam(l.elementId())) ||
                            (l.targetType() == MemberTargetType::Column && m.getColumn(l.elementId())) ||
                            (l.targetType() == MemberTargetType::Truss && m.getTrussMember(l.elementId())) ||
                            (l.targetType() == MemberTargetType::Cable && m.getCable(l.elementId()));
        if (!target || !lm.getLoadCase(l.loadCaseId())) return false;
    }
    for (const auto& [id, l] : lm.surfaceLoads())
    {
        const bool target = l.targetKind == SurfaceTargetKind::Slab ? m.getSlab(l.surfaceId) != nullptr : m.getWall(l.surfaceId) != nullptr;
        if (!target || !lm.getLoadCase(l.loadCaseId)) return false;
    }
    return true;
}

gp_Pnt pointOf(const Model& m, int node)
{
    const Node* n = m.getNode(node);
    return gp_Pnt(n->x(), n->y(), n->z());
}

gp_Vec linear(const gp_Vec& v, const gp_Trsf& t)
{
    return v.Transformed(t);
}

/// Vecteur résultant de charges de même intensité sur une barre, au sens de la résolution du calcul.
gp_Vec resolved(const std::vector<MemberLoad>& loads, const gp_Pnt& a, const gp_Pnt& b, double rotation)
{
    gp_Vec sum(0.0, 0.0, 0.0);
    for (const auto& l : loads) sum += LoadResolver::memberLoadVector(l, l.q1(), a, b, rotation);
    return sum;
}

std::unique_ptr<ModelingTool> tool(const std::string& id)
{
    ModelingToolRegistry r;
    registerBuiltInModelingTools(r);
    return r.create(id);
}

ToolPick at(double x, double y, double z)
{
    ToolPick p;
    p.point = gp_Pnt(x, y, z);
    return p;
}

void pump(int n = 10)
{
    for (int k = 0; k < n; ++k)
    {
        QApplication::processEvents();
        QThread::msleep(5);
    }
}

int displayedObjects(OccView& v)
{
    NCollection_List<Handle(AIS_InteractiveObject)> list;
    v.context()->DisplayedObjects(list);
    return list.Extent();
}

/// Dalle L inclinée sur rien : dalle rectangulaire 6 × 4 en z = 3, ouverture, appui de bord, charges en (u, v).
struct SlabCase
{
    int slab = 0;
    int pressure = 0, point = 0;
    SlabCase(Model& m, int loadCase)
    {
        const int a = m.addNode(1, 2, 3), b = m.addNode(7, 2, 3), c = m.addNode(7, 6, 3), d = m.addNode(1, 6, 3);
        slab = m.addSlab({ a, b, c, d }, 0.25);
        Slab* s = m.getSlab(slab);
        SlabOpening hole;
        hole.name = "T1";
        hole.polygon = { { 1.0, 1.0 }, { 2.0, 1.0 }, { 2.0, 2.5 }, { 1.0, 2.5 } };
        s->setOpenings({ hole });
        SurfaceEdgeSupport edge;
        edge.edge = 1;   // bord b → c
        edge.fixed[2] = true;
        s->setEdgeSupports({ edge });

        SurfaceLoad p;
        p.targetKind = SurfaceTargetKind::Slab;
        p.surfaceId = slab;
        p.loadCaseId = loadCase;
        p.name = "pression";
        p.value = -3.0;
        p.gradU = -0.5;
        p.zone = true;
        p.zoneU0 = 0.5, p.zoneV0 = 0.25, p.zoneU1 = 4.0, p.zoneV1 = 3.0;
        pressure = m.loadManager().addSurfaceLoad(p);

        SurfaceLoad f = p;
        f.id = 0;
        f.name = "ponctuelle";
        f.distribution = SurfaceLoadDistribution::Point;
        f.direction = SurfaceLoadDirection::GlobalX;
        f.value = 7.0;
        f.gradU = 0.0;
        f.zone = false;
        f.u = 5.0, f.v = 1.0;
        point = m.loadManager().addSurfaceLoad(f);
    }
};

std::vector<TSA::Mesh::P3> cornersOf(const Model& m, const Slab& s)
{
    std::vector<TSA::Mesh::P3> c;
    for (int id : s.nodeIds())
    {
        const Node* n = m.getNode(id);
        c.push_back({ n->x(), n->y(), n->z() });
    }
    return c;
}

gp_Pnt surfacePoint(const TSA::Mesh::PlaneFrame& f, double u, double v)
{
    const TSA::Mesh::P3 p = f.toGlobal(u, v);
    return gp_Pnt(p.x, p.y, p.z);
}
} // namespace

bool runSuite_CopyPaste(int& passed)
{
    // -------------------------------------------------------------------------
    // TEST 549 : presse-papier, poutres et charges
    // -------------------------------------------------------------------------
    {
        Model m;
        const int lcG = m.loadManager().addLoadCase(LoadCase(0, "G", LoadCaseCategory::Dead));
        const int lcQ = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        LoadCombination combo;
        combo.setName("ELU");
        combo.setCaseFactors({ { lcG, 1.35 }, { lcQ, 1.5 } });
        m.loadManager().addCombination(combo);

        const int bare = m.addBeam(m.addNode(0, 0, 3), m.addNode(6, 0, 3));
        const int pointed = m.addBeam(m.addNode(0, 2, 3), m.addNode(6, 2, 3));
        const int spread = m.addBeam(m.addNode(0, 4, 3), m.addNode(6, 4, 3));
        const int mixed = m.addBeam(m.addNode(0, 6, 3), m.addNode(6, 6, 3));

        auto& lm = m.loadManager();
        lm.addMemberLoad(MemberLoad::pointOnMember(0, pointed, lcQ, 12.5, 1.75, LoadDirection::GlobalY, false, "P1"));
        lm.addMemberLoad(MemberLoad::uniform(0, spread, lcG, -4.25, LoadDirection::GlobalZ));
        lm.addMemberLoad(MemberLoad::uniform(0, spread, lcQ, 2.5, LoadDirection::LocalZ, "vent"));
        lm.addMemberLoad(MemberLoad::trapezoidal(0, mixed, lcQ, 1.0, 3.0, 0.2, 0.8, LoadDirection::GlobalZ, true, "triangle"));
        lm.addMemberLoad(MemberLoad::pointOnMember(0, mixed, lcG, -8.0, 3.0, LoadDirection::LocalY));
        lm.addMemberLoad(MemberLoad::uniform(0, mixed, lcG, -1.5, LoadDirection::GlobalZ));

        const std::size_t cases = lm.loadCases().size(), combinations = lm.combinations().size();
        StructuralClipboard clip;   // options par défaut : ce que fait Ctrl+C
        clip.copyFrom(m, beamsSelection({ bare, pointed, spread, mixed }));
        TEST_CHECK(clip.options().loads && clip.loadCount() == 6, "Test 549: les charges sont copiées par défaut");

        const PasteResult r = clip.pasteUndoable(m, 20.0, 0.0, 3.0);
        TEST_CHECK(r.complete && r.beamIds.size() == 4 && r.memberLoadIds.size() == 6, "Test 549: 4 poutres et 6 charges collées");
        TEST_CHECK(lm.loadCases().size() == cases && lm.combinations().size() == combinations && r.createdLoadCaseIds.empty(),
                   "Test 549: aucun cas de charge ni combinaison dupliqué");
        TEST_CHECK(lm.memberLoads().size() == 12 && referencesValid(m), "Test 549: références valides, identifiants uniques (table par identifiant)");

        for (int source : { bare, pointed, spread, mixed })
        {
            const auto copy = r.elementMap.find({ ElementKind::Beam, source });
            TEST_CHECK(copy != r.elementMap.end(), "Test 549: poutre source retrouvée");
            auto from = loadsOnBeam(m, source), to = loadsOnBeam(m, copy->second.id);
            TEST_CHECK(from.size() == to.size(), "Test 549: autant de charges sur la copie que sur l'original");
            for (std::size_t i = 0; i < from.size(); ++i)
            {
                TEST_CHECK(sameMemberLoad(from[i], to[i]), "Test 549: valeurs, position, direction, repère et nom exacts");
                TEST_CHECK(from[i].loadCaseId() == to[i].loadCaseId(), "Test 549: même cas de charge");
                TEST_CHECK(from[i].id() != to[i].id(), "Test 549: nouvel identifiant de charge");
            }
        }
        std::cout << "[PASS] Test 549: Poutres avec charges ponctuelles et réparties" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 550 : attributs, appuis, groupe connecté, surfaces
    // -------------------------------------------------------------------------
    {
        Model m;
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        const int a = m.addNode(0, 0, 0), b = m.addNode(0, 0, 3), c = m.addNode(5, 0, 3);
        m.getNode(a)->setSupport(SupportDefinition::fixed());
        m.getNode(a)->setName("A");
        m.getNode(a)->setColor("#112233");
        const int column = m.addColumn(a, b, Section::heb(200), Material::steelS235(), 30.0);
        m.getColumn(column)->setColor("#445566");
        const int beam = m.addBar(b, c, Section::ipe(300), Material::steelS235(), BarRole::Beam);
        Beam* src = m.getBeam(beam);
        src->setRotation(15.0);
        src->setColor("#778899");
        EndRelease hinge;
        hinge.my = true, hinge.mz = true;
        src->setEndRelease(hinge);
        m.loadManager().addNodalLoad(NodalLoad(0, b, lc, 1.0, -2.0, 3.0));

        const int w1 = m.addNode(10, 0, 0), w2 = m.addNode(14, 0, 0);
        const int wall = m.addWall(w1, w2, 3.0, 0.22);
        m.getWall(wall)->setOffset(0.05);
        SurfaceLoad wallLoad;
        wallLoad.targetKind = SurfaceTargetKind::Wall;
        wallLoad.surfaceId = wall;
        wallLoad.loadCaseId = lc;
        wallLoad.value = -1.5;
        wallLoad.direction = SurfaceLoadDirection::LocalZ;
        m.loadManager().addSurfaceLoad(wallLoad);
        const SlabCase slab(m, lc);
        const int footing = m.addFoundation(a, 1.2, 1.8, 0.6);

        ClipboardSelection sel;
        sel.beams = { beam };
        sel.columns = { column };
        sel.walls = { wall };
        sel.slabs = { slab.slab };
        sel.foundations = { footing };
        StructuralClipboard clip;
        clip.copyFrom(m, sel);
        const std::size_t nodes = m.nodes().size();
        const PasteResult r = clip.pasteUndoable(m, 30.0, 30.0, 0.0);
        TEST_CHECK(r.complete && referencesValid(m), "Test 550: collage complet, références valides");
        TEST_CHECK(m.nodes().size() == nodes + clip.nodeCount(), "Test 550: nœud partagé par le groupe recopié une seule fois");

        const Beam* nb = m.getBeam(r.beamIds.at(0));
        const Column* nc = m.getColumn(r.columnIds.at(0));
        TEST_CHECK(nb && nb->section().name == src->section().name && nb->material().name == src->material().name, "Test 550: section et matériau");
        TEST_CHECK(same(nb->rotation(), 15.0) && nb->endRelease().my && nb->endRelease().mz && nb->color() == "#778899", "Test 550: rotation, relâchements, couleur");
        TEST_CHECK(nc && same(nc->rotation(), 30.0) && nc->color() == "#445566", "Test 550: poteau, rotation et couleur");
        TEST_CHECK(nc->endNodeId() == nb->startNodeId(), "Test 550: connexion du groupe conservée");
        const Node* base = m.getNode(nc->startNodeId());
        TEST_CHECK(base->support() == SupportDefinition::fixed() && base->name() == "A" && base->color() == "#112233", "Test 550: appui, nom et couleur du nœud");

        int nodal = 0;
        for (const auto& [id, l] : m.loadManager().nodalLoads())
            nodal += l.nodeId() == nc->endNodeId() && l.fx() == 1.0 && l.fy() == -2.0 && l.fz() == 3.0 ? 1 : 0;
        TEST_CHECK(nodal == 1, "Test 550: charge nodale reportée sur le nœud copié");

        const Wall* nw = m.getWall(r.wallIds.at(0));
        TEST_CHECK(nw && same(nw->offset(), 0.05) && same(nw->height(), 3.0), "Test 550: voile complet");
        const Slab* ns = m.getSlab(r.slabIds.at(0));
        TEST_CHECK(ns && ns->openings().size() == 1 && ns->edgeSupports().size() == 1, "Test 550: dalle complète");
        TEST_CHECK(r.surfaceLoadIds.size() == 3, "Test 550: charges surfaciques de la dalle et du voile");
        const Foundation* nf = m.getFoundation(r.foundationIds.at(0));
        TEST_CHECK(nf && same(nf->lengthB(), 1.8) && nf->nodeId() == nc->startNodeId(), "Test 550: fondation sur le nœud copié");
        std::cout << "[PASS] Test 550: Attributs, appuis, groupe connecté, surfaces" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 551 : collages répétés, Annuler, Rétablir
    // -------------------------------------------------------------------------
    {
        Model m;
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        const int beam = m.addBeam(m.addNode(0, 0, 0), m.addNode(5, 0, 0));
        m.loadManager().addMemberLoad(MemberLoad::pointOnMember(0, beam, lc, -10.0, 2.0, LoadDirection::GlobalZ));
        StructuralClipboard clip;
        clip.copyFrom(m, beamsSelection({ beam }));

        const auto revision = m.revision();
        for (int k = 1; k <= 3; ++k)
            TEST_CHECK(clip.pasteUndoable(m, 0.0, 2.0 * k, 0.0).complete, "Test 551: collage répété");
        TEST_CHECK(m.beams().size() == 4 && m.loadManager().memberLoads().size() == 4 && referencesValid(m), "Test 551: une charge sur chaque copie");
        TEST_CHECK(m.revision() > revision, "Test 551: révision incrémentée, résultats périmés");

        TEST_CHECK(m.undo(), "Test 551: Annuler");
        TEST_CHECK(m.beams().size() == 3 && m.loadManager().memberLoads().size() == 3, "Test 551: Annuler retire la poutre et sa charge");
        TEST_CHECK(m.redo(), "Test 551: Rétablir");
        TEST_CHECK(m.beams().size() == 4 && m.loadManager().memberLoads().size() == 4 && referencesValid(m), "Test 551: Rétablir les restaure");
        std::cout << "[PASS] Test 551: Collages répétés, Annuler, Rétablir" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 552 : outils de copie
    // -------------------------------------------------------------------------
    {
        Model m;
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        const int n1 = m.addNode(0, 0, 3), n2 = m.addNode(6, 0, 3);
        m.getNode(n1)->setSupport(SupportDefinition::pinned());
        const int beam = m.addBeam(n1, n2);
        m.loadManager().addMemberLoad(MemberLoad::pointOnMember(0, beam, lc, -12.0, 1.5, LoadDirection::GlobalZ));
        m.loadManager().addNodalLoad(NodalLoad(0, n2, lc, 0.0, 5.0, 0.0));
        const int wall = m.addWall(m.addNode(0, 10, 0), m.addNode(4, 10, 0), 3.0, 0.2);
        const int footing = m.addFoundation(m.addNode(10, 10, 0));
        const SlabCase slab(m, lc);

        ToolContext ctx;
        ctx.selection.beams = { beam };
        ctx.selection.walls = { wall };
        ctx.selection.foundations = { footing };
        ctx.selection.slabs = { slab.slab };

        auto copy = tool("copy");
        copy->addPick(at(0, 0, 0), ctx);
        copy->addPick(at(0, 0, 8), ctx);
        TEST_CHECK(copy->ready() && copy->apply(m, ctx).success, "Test 552: outil Copier au clic");
        TEST_CHECK(m.beams().size() == 2 && m.walls().size() == 2 && m.foundations().size() == 2 && m.slabs().size() == 2, "Test 552: poutre, voile, fondation et dalle copiés");
        TEST_CHECK(m.loadManager().memberLoads().size() == 2 && m.loadManager().nodalLoads().size() == 2 && m.loadManager().surfaceLoads().size() == 4,
                   "Test 552: charges copiées avec leurs éléments");
        TEST_CHECK(referencesValid(m), "Test 552: charges rattachées aux copies");
        int pinned = 0;
        for (const auto& [id, n] : m.nodes()) pinned += n.support().isPinned() ? 1 : 0;
        TEST_CHECK(pinned == 2, "Test 552: appui copié");
        int openings = 0, edges = 0;
        for (const auto& [id, s] : m.slabs()) openings += int(s.openings().size()), edges += int(s.edgeSupports().size());
        TEST_CHECK(openings == 2 && edges == 2, "Test 552: ouvertures et appuis de bord de la dalle copiés");

        auto numeric = tool("copy");   // fenêtre de saisie numérique : mêmes paramètres, mêmes données
        numeric->setParam("dx", 40.0);
        numeric->setParam("count", 2.0);
        TEST_CHECK(numeric->apply(m, ctx).success, "Test 552: copie numérique");
        TEST_CHECK(m.beams().size() == 4 && m.loadManager().memberLoads().size() == 4, "Test 552: copie numérique, 2 copies avec charges");
        auto array = tool("array_linear");
        array->setParam("dy", 40.0);
        array->setParam("count", 1.0);
        TEST_CHECK(array->apply(m, ctx).success && m.loadManager().memberLoads().size() == 5 && referencesValid(m), "Test 552: réseau linéaire avec charges");
        std::cout << "[PASS] Test 552: Outils de copie (charges, voiles, fondations, dalles complètes)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 553 : copie par rotation
    // -------------------------------------------------------------------------
    {
        for (double degrees : { 90.0, 30.0, -135.0 })
        {
            Model m;
            const int lc = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
            const gp_Pnt pa(1, 2, 3), pb(7, 2, 5), pc(1, 2, 0);
            const int a = m.addNode(pa.X(), pa.Y(), pa.Z()), b = m.addNode(pb.X(), pb.Y(), pb.Z()), c = m.addNode(pc.X(), pc.Y(), pc.Z());
            const int beam = m.addBeam(a, b);
            m.getBeam(beam)->setRotation(20.0);
            const int column = m.addColumn(c, a, Section::heb(200), Material::steelS235(), 10.0);
            auto& lm = m.loadManager();
            NodalLoad nodal(0, b, lc, 3.0, 4.0, -5.0);
            nodal.setMx(1.0), nodal.setMy(2.0), nodal.setMz(3.0);
            lm.addNodalLoad(nodal);
            lm.addMemberLoad(MemberLoad::uniform(0, beam, lc, 6.0, LoadDirection::GlobalX));
            lm.addMemberLoad(MemberLoad::uniform(0, beam, lc, -2.0, LoadDirection::GlobalZ));
            MemberLoad local = MemberLoad::uniform(0, column, lc, 5.0, LoadDirection::LocalY, "", MemberTargetType::Column);
            lm.addMemberLoad(local);

            const auto sourceBeam = loadsOnBeam(m, beam);
            const auto sourceColumn = loadsOnColumn(m, column);
            ElementSet s;
            s.beams = { beam };
            s.columns = { column };
            const gp_Pnt center(0, 0, 0);
            const gp_Dir axis(0, 0, 1);
            const double rad = degrees * M_PI / 180.0;
            std::vector<std::string> warnings;
            const auto created = m.copyAndRotateElements(s, center, axis, rad, 1, ClipboardOptions {}, &warnings);
            TEST_CHECK(!created.empty() && warnings.empty() && referencesValid(m), "Test 553: copie par rotation sans avertissement");

            gp_Trsf t;
            t.SetRotation(gp_Ax1(center, axis), rad);
            int copyBeam = 0, copyColumn = 0;
            for (const auto& [id, e] : m.beams()) copyBeam = id != beam ? id : copyBeam;
            for (const auto& [id, e] : m.columns()) copyColumn = id != column ? id : copyColumn;

            const Beam* nb = m.getBeam(copyBeam);
            const gp_Pnt qa = pointOf(m, nb->startNodeId()), qb = pointOf(m, nb->endNodeId());
            TEST_CHECK(qa.Distance(pa.Transformed(t)) < kTol && qb.Distance(pb.Transformed(t)) < kTol, "Test 553: nœuds à leur image");

            // Charges globales : enregistrements recopiés, direction globale conservée (comme Robot)
            const auto copied = loadsOnBeam(m, copyBeam);
            TEST_CHECK(copied.size() == sourceBeam.size(), "Test 553: autant de charges sur la copie");
            for (std::size_t i = 0; i < copied.size(); ++i) TEST_CHECK(sameMemberLoad(copied[i], sourceBeam[i]), "Test 553: charge globale recopiée à l'identique");
            TEST_CHECK(sameVec(resolved(copied, qa, qb, nb->rotation()), resolved(sourceBeam, pa, pb, 20.0)), "Test 553: direction globale fixe");

            const Column* nc = m.getColumn(copyColumn);
            const gp_Pnt qc = pointOf(m, nc->startNodeId()), qd = pointOf(m, nc->endNodeId());
            TEST_CHECK(sameVec(resolved(loadsOnColumn(m, copyColumn), qc, qd, nc->rotation()), linear(resolved(sourceColumn, pc, pa, 10.0), t)),
                       "Test 553: charge locale, suit l'élément (image de la charge)");

            const NodalLoad* moved = nullptr;
            for (const auto& [id, l] : lm.nodalLoads()) moved = l.nodeId() == nb->endNodeId() ? &l : moved;
            TEST_CHECK(moved, "Test 553: charge nodale copiée");
            TEST_CHECK(sameVec(gp_Vec(moved->fx(), moved->fy(), moved->fz()), gp_Vec(3, 4, -5)), "Test 553: force nodale globale, recopiée");
            TEST_CHECK(sameVec(gp_Vec(moved->mx(), moved->my(), moved->mz()), gp_Vec(1, 2, 3)), "Test 553: moment nodal global, recopié");
        }

        Model tilted;
        const int lc = tilted.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        const int beam = tilted.addBeam(tilted.addNode(0, 0, 0), tilted.addNode(5, 0, 0));
        tilted.loadManager().addMemberLoad(MemberLoad::uniform(0, beam, lc, -2.0, LoadDirection::GlobalZ));
        const int wall = tilted.addWall(tilted.addNode(0, 5, 0), tilted.addNode(4, 5, 0));
        ElementSet s;
        s.beams = { beam };
        s.walls = { wall };
        std::vector<std::string> warnings;
        tilted.copyAndRotateElements(s, gp_Pnt(0, 0, 0), gp_Dir(1, 0, 0), M_PI / 4, 1, ClipboardOptions {}, &warnings);
        TEST_CHECK(tilted.walls().size() == 1 && tilted.beams().size() == 2, "Test 553: voile non incliné, poutre copiée");
        TEST_CHECK(warnings.size() == 1, "Test 553: voile non copié signalé");
        std::cout << "[PASS] Test 553: Copie par rotation, charges globales fixes et locales suivant l'élément" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 554 : copie miroir
    // -------------------------------------------------------------------------
    {
        const gp_Pnt planePoint(-2, 0, 0);
        const gp_Dir planeNormal(1, 0, 0);
        gp_Trsf t;
        t.SetMirror(gp_Ax2(planePoint, planeNormal));

        Model m;
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        const gp_Pnt pa(1, 0, 3), pb(6, 1, 3);
        const int a = m.addNode(pa.X(), pa.Y(), pa.Z()), b = m.addNode(pb.X(), pb.Y(), pb.Z());
        m.getNode(a)->setSupport(SupportDefinition::pinned());
        const int beam = m.addBeam(a, b);
        auto& lm = m.loadManager();
        NodalLoad nodal(0, b, lc, 1.0, 2.0, 3.0);
        nodal.setMx(4.0), nodal.setMy(5.0), nodal.setMz(6.0);
        lm.addNodalLoad(nodal);
        lm.addMemberLoad(MemberLoad::uniform(0, beam, lc, 3.0, LoadDirection::LocalY));
        MemberLoad moment(0, beam, lc, LoadType::MemberMoment, 2.0, 2.0, LoadDirection::GlobalY);
        lm.addMemberLoad(moment);
        const SlabCase slab(m, lc);
        const int truss = m.addTrussMember(m.addNode(0, 8, 0), m.addNode(3, 8, 1));
        const int wall = m.addWall(m.addNode(0, 12, 0), m.addNode(4, 12, 0), 3.0, 0.2);
        m.getWall(wall)->setOffset(0.04);
        SurfaceLoad wallLoad;
        wallLoad.targetKind = SurfaceTargetKind::Wall;
        wallLoad.surfaceId = wall;
        wallLoad.loadCaseId = lc;
        wallLoad.value = -2.0;
        wallLoad.direction = SurfaceLoadDirection::LocalZ;
        lm.addSurfaceLoad(wallLoad);

        const Slab sourceSlab = *m.getSlab(slab.slab);
        const TSA::Mesh::PlaneFrame sourceFrame = TSA::Mesh::slabFrame(cornersOf(m, sourceSlab));
        const auto sourceOpenings = TSA::Mesh::slabOpenings3D(m, sourceSlab);
        const SurfaceLoad sourcePressure = lm.surfaceLoads().at(slab.pressure), sourcePoint = lm.surfaceLoads().at(slab.point);
        const auto sourceBeam = loadsOnBeam(m, beam);

        ElementSet s;
        s.beams = { beam };
        s.slabs = { slab.slab };
        s.trussMembers = { truss };
        s.walls = { wall };
        std::vector<std::string> warnings;
        const auto created = m.mirrorCopyElements(s, planePoint, planeNormal, ClipboardOptions {}, &warnings);
        TEST_CHECK(!created.empty() && warnings.empty() && referencesValid(m), "Test 554: copie miroir sans avertissement");
        TEST_CHECK(m.trussMembers().size() == 2 && m.walls().size() == 2, "Test 554: treillis et voile copiés en miroir");

        int copyBeam = 0, copySlab = 0, copyWall = 0;
        for (const auto& [id, e] : m.beams()) copyBeam = id != beam ? id : copyBeam;
        for (const auto& [id, e] : m.slabs()) copySlab = id != slab.slab ? id : copySlab;
        for (const auto& [id, e] : m.walls()) copyWall = id != wall ? id : copyWall;

        const Beam* nb = m.getBeam(copyBeam);
        const gp_Pnt qa = pointOf(m, nb->startNodeId()), qb = pointOf(m, nb->endNodeId());
        TEST_CHECK(qa.Distance(pa.Transformed(t)) < kTol && qb.Distance(pb.Transformed(t)) < kTol, "Test 554: extrémités à leur image");
        TEST_CHECK(m.getNode(nb->startNodeId())->support().isPinned(), "Test 554: appui copié en miroir");

        const NodalLoad* nodalCopy = nullptr;
        for (const auto& [id, l] : lm.nodalLoads()) nodalCopy = l.nodeId() == nb->endNodeId() ? &l : nodalCopy;
        TEST_CHECK(nodalCopy && sameVec(gp_Vec(nodalCopy->fx(), nodalCopy->fy(), nodalCopy->fz()), gp_Vec(1, 2, 3)), "Test 554: force nodale globale, recopiée");
        TEST_CHECK(sameVec(gp_Vec(nodalCopy->mx(), nodalCopy->my(), nodalCopy->mz()), gp_Vec(4, 5, 6)), "Test 554: moment nodal global, recopié");

        std::vector<MemberLoad> forces, moments, copyForces, copyMoments;
        for (const auto& l : sourceBeam) (l.type() == LoadType::MemberMoment ? moments : forces).push_back(l);
        for (const auto& l : loadsOnBeam(m, copyBeam)) (l.type() == LoadType::MemberMoment ? copyMoments : copyForces).push_back(l);
        // Repère de la copie selon Robot : x et z images, y opposé à l'image ; la charge locale y garde sa valeur
        TEST_CHECK(sameVec(resolved(copyForces, qa, qb, nb->rotation()), linear(resolved(forces, pa, pb, 0.0), t) * -1.0), "Test 554: charge locale y, suit le repère de la copie");
        TEST_CHECK(sameVec(resolved(copyMoments, qa, qb, nb->rotation()), resolved(moments, pa, pb, 0.0)), "Test 554: moment réparti global, recopié");

        const Slab& mirrored = *m.getSlab(copySlab);
        const TSA::Mesh::PlaneFrame frame = TSA::Mesh::slabFrame(cornersOf(m, mirrored));
        TEST_CHECK(sameVec(gp_Vec(frame.N.x, frame.N.y, frame.N.z), linear(gp_Vec(sourceFrame.N.x, sourceFrame.N.y, sourceFrame.N.z), t)), "Test 554: normale, image de la normale");
        const auto openings = TSA::Mesh::slabOpenings3D(m, mirrored);
        TEST_CHECK(openings.size() == 1 && openings[0].size() == sourceOpenings[0].size(), "Test 554: ouverture copiée");
        for (const auto& p : sourceOpenings[0])
        {
            const gp_Pnt image = gp_Pnt(p.x, p.y, p.z).Transformed(t);
            bool found = false;
            for (const auto& q : openings[0]) found = found || image.Distance(gp_Pnt(q.x, q.y, q.z)) < 1e-8;
            TEST_CHECK(found, "Test 554: sommets de l'ouverture à leur image");
        }

        const auto& edge = mirrored.edgeSupports().at(0);
        const auto& ids = mirrored.nodeIds();
        const gp_Pnt e0 = pointOf(m, ids[edge.edge]), e1 = pointOf(m, ids[(edge.edge + 1) % ids.size()]);
        const gp_Pnt s0 = gp_Pnt(7, 2, 3).Transformed(t), s1 = gp_Pnt(7, 6, 3).Transformed(t);
        TEST_CHECK((e0.Distance(s0) < kTol && e1.Distance(s1) < kTol) || (e0.Distance(s1) < kTol && e1.Distance(s0) < kTol), "Test 554: bord d'appui, image du bord");

        const SurfaceLoad* pressure = nullptr;
        const SurfaceLoad* point = nullptr;
        const SurfaceLoad* wallCopy = nullptr;
        for (const auto& [id, l] : lm.surfaceLoads())
        {
            if (l.targetKind == SurfaceTargetKind::Slab && l.surfaceId == copySlab) (l.distribution == SurfaceLoadDistribution::Point ? point : pressure) = &l;
            if (l.targetKind == SurfaceTargetKind::Wall && l.surfaceId == copyWall) wallCopy = &l;
        }
        TEST_CHECK(pressure && point && wallCopy, "Test 554: charges surfaciques copiées");
        TEST_CHECK(surfacePoint(frame, point->u, point->v).Distance(surfacePoint(sourceFrame, sourcePoint.u, sourcePoint.v).Transformed(t)) < 1e-8, "Test 554: charge ponctuelle à son image");
        TEST_CHECK(point->direction == SurfaceLoadDirection::GlobalX && same(point->value, 7.0), "Test 554: force globale recopiée, direction fixe");
        for (const auto& [u, v] : std::vector<std::pair<double, double>> { { 1.0, 1.0 }, { 3.5, 2.0 } })
        {
            const gp_Pnt image = surfacePoint(sourceFrame, u, v).Transformed(t);
            double cu = 0.0, cv = 0.0;
            frame.toLocal({ image.X(), image.Y(), image.Z() }, &cu, &cv);
            TEST_CHECK(same(pressure->value + pressure->gradU * cu + pressure->gradV * cv, sourcePressure.value + sourcePressure.gradU * u, 1e-8), "Test 554: pression variable conservée à l'image");
            TEST_CHECK(cu >= pressure->zoneU0 - 1e-9 && cu <= pressure->zoneU1 + 1e-9 && cv >= pressure->zoneV0 - 1e-9 && cv <= pressure->zoneV1 + 1e-9, "Test 554: zone chargée, image de la zone");
        }
        TEST_CHECK(same(wallCopy->value, -2.0) && same(m.getWall(copyWall)->offset(), -0.04), "Test 554: voile, charge recopiée, excentrement suivant la normale");

        // Nœud sur le plan : partagé, sa charge n'est pas doublée
        Model shared;
        const int q = shared.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        const int on = shared.addNode(0, 0, 0), off = shared.addNode(4, 0, 0);
        const int bar = shared.addBeam(on, off);
        shared.loadManager().addNodalLoad(NodalLoad(0, on, q, 0.0, 0.0, -10.0));
        shared.loadManager().addNodalLoad(NodalLoad(0, off, q, 0.0, 0.0, -20.0));
        ElementSet one;
        one.beams = { bar };
        shared.mirrorCopyElements(one, gp_Pnt(0, 0, 0), gp_Dir(1, 0, 0), ClipboardOptions {});
        TEST_CHECK(shared.nodes().size() == 3 && shared.loadManager().nodalLoads().size() == 3, "Test 554: nœud du plan partagé, sa charge n'est pas doublée");

        auto mirror = tool("mirror");
        ToolContext ctx;
        ctx.selection.trussMembers = { truss };
        mirror->addPick(at(50, -1, 0), ctx);
        mirror->addPick(at(50, 1, 0), ctx);
        TEST_CHECK(mirror->ready() && mirror->apply(m, ctx).success && m.trussMembers().size() == 3, "Test 554: outil Symétrie sur un treillis");
        std::cout << "[PASS] Test 554: Copie miroir, charges recopiées et surfaces converties" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 555 : enregistrement et relecture
    // -------------------------------------------------------------------------
    {
        Model m;
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        const int beam = m.addBeam(m.addNode(0, 0, 0), m.addNode(5, 0, 0));
        m.loadManager().addMemberLoad(MemberLoad::pointOnMember(0, beam, lc, 10.25, 2.5, LoadDirection::GlobalY));
        const SlabCase slab(m, lc);
        ClipboardSelection sel = beamsSelection({ beam });
        sel.slabs = { slab.slab };
        StructuralClipboard clip;
        clip.copyFrom(m, sel);
        TEST_CHECK(clip.pasteUndoable(m, 30.0, 0.0, 0.0).complete, "Test 555: collage");
        ElementSet s;
        s.beams = { beam };
        TEST_CHECK(!m.copyAndRotateElements(s, gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1), M_PI / 6, 1, ClipboardOptions {}).empty(), "Test 555: copie par rotation");

        const QString file = QDir::temp().filePath(QStringLiteral("tsa_copy_paste_%1.tsa").arg(QCoreApplication::applicationPid()));
        std::string error;
        TEST_CHECK(TSAProjectIO::saveToFile(file, m, nullptr, &error), "Test 555: enregistrement");
        Model reloaded;
        TEST_CHECK(TSAProjectIO::loadFromFile(file, reloaded, nullptr, &error), "Test 555: relecture");
        QFile::remove(file);
        TEST_CHECK(reloaded.beams().size() == m.beams().size() && reloaded.slabs().size() == m.slabs().size(), "Test 555: éléments relus");
        const auto& a = m.loadManager(), & b = reloaded.loadManager();
        TEST_CHECK(a.memberLoads().size() == b.memberLoads().size() && a.surfaceLoads().size() == b.surfaceLoads().size(), "Test 555: charges relues");
        TEST_CHECK(referencesValid(reloaded), "Test 555: références valides après relecture");
        for (const auto& [id, l] : a.memberLoads())
            TEST_CHECK(b.getMemberLoad(id) && sameMemberLoad(l, *b.getMemberLoad(id)) && b.getMemberLoad(id)->elementId() == l.elementId(), "Test 555: charge relue à l'identique");
        std::cout << "[PASS] Test 555: Enregistrement et relecture des copies" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 556 : vue réelle, 3D et 2D
    // -------------------------------------------------------------------------
    {
        Model m;
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        const int beam = m.addBeam(m.addNode(0, 0, 0), m.addNode(6, 0, 0));
        m.loadManager().addMemberLoad(MemberLoad::pointOnMember(0, beam, lc, -10.0, 2.0, LoadDirection::GlobalZ));
        OccView view;
        view.resize(640, 480);
        view.show();
        pump(20);
        view.setModel(&m);
        view.rebuildAllShapes();
        pump();
        ToolContext ctx;
        ctx.selection.beams = { beam };

        const int before = displayedObjects(view);
        auto copy3d = tool("copy");
        copy3d->setParam("dy", 3.0);
        {
            TSA::UndoRedo::EditTransaction tx(m, "copie 3D");
            TEST_CHECK(copy3d->apply(m, ctx).success, "Test 556: copie en vue 3D");
            tx.commit();
        }
        pump();
        const int with3d = displayedObjects(view);
        TEST_CHECK(with3d > before + 2, "Test 556: poutre, nœuds et charge affichés en 3D");

        view.setMode2D(true);
        pump();
        const int in2d = displayedObjects(view);
        auto copy2d = tool("copy");
        copy2d->setParam("dx", 10.0);
        {
            TSA::UndoRedo::EditTransaction tx(m, "copie 2D");
            TEST_CHECK(copy2d->apply(m, ctx).success, "Test 556: copie en vue 2D");
            tx.commit();
        }
        pump();
        TEST_CHECK(m.loadManager().memberLoads().size() == 3 && referencesValid(m), "Test 556: données identiques en vue 2D");
        TEST_CHECK(displayedObjects(view) > in2d, "Test 556: copie affichée en vue 2D");
        view.setMode2D(false);
        pump();

        StructuralClipboard clip;
        clip.copyFrom(m, beamsSelection({ beam }));
        const int beforePaste = displayedObjects(view);
        TEST_CHECK(clip.pasteUndoable(m, 0.0, 9.0, 0.0).complete, "Test 556: Ctrl+V");
        pump();
        TEST_CHECK(displayedObjects(view) > beforePaste + 2, "Test 556: charge collée affichée");
        view.setModel(nullptr);
        view.hide();
        pump();
        std::cout << "[PASS] Test 556: Copies affichées en vues 3D et 2D" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 557 : options, cas homonymes, autre projet
    // -------------------------------------------------------------------------
    {
        Model m;
        auto& lm = m.loadManager();
        const int first = lm.addLoadCase(LoadCase(0, "Exploitation toiture", LoadCaseCategory::Live));
        const int second = lm.addLoadCase(LoadCase(0, "Exploitation toiture", LoadCaseCategory::Live));
        const int n1 = m.addNode(0, 0, 0), n2 = m.addNode(5, 0, 0);
        m.getNode(n1)->setSupport(SupportDefinition::fixed());
        const int beam = m.addBeam(n1, n2);
        lm.addMemberLoad(MemberLoad::uniform(0, beam, second, -3.0, LoadDirection::GlobalZ));

        StructuralClipboard clip;
        ClipboardOptions none;
        none.loads = false;
        none.supports = false;
        clip.copyFrom(m, beamsSelection({ beam }), none);
        PasteResult r = clip.pasteUndoable(m, 0.0, 2.0, 0.0);
        TEST_CHECK(r.complete && r.loadCount() == 0 && !m.getNode(r.nodeIds[0])->support().isSupported(), "Test 557: options désactivées respectées");

        clip.copyFrom(m, beamsSelection({ beam }));
        r = clip.pasteUndoable(m, 0.0, 4.0, 0.0);
        TEST_CHECK(lm.getMemberLoad(r.memberLoadIds.at(0))->loadCaseId() == second && first != second, "Test 557: cas homonyme, le cas d'origine est conservé");

        Model other;
        const std::size_t otherCases = other.loadManager().loadCases().size();
        const PasteResult crossed = clip.pasteUndoable(other, 0.0, 0.0, 0.0);
        TEST_CHECK(crossed.complete && other.loadManager().loadCases().size() == otherCases + 1 && crossed.createdLoadCaseIds.size() == 1, "Test 557: autre projet, cas de charge créé");
        const PasteResult again = clip.pasteUndoable(other, 0.0, 3.0, 0.0);
        TEST_CHECK(again.complete && again.createdLoadCaseIds.empty() && other.loadManager().loadCases().size() == otherCases + 1, "Test 557: second collage, cas rapproché par son nom");
        TEST_CHECK(other.loadManager().memberLoads().size() == 2 && referencesValid(other), "Test 557: autre projet, charge rattachée");
        std::cout << "[PASS] Test 557: Options, cas homonymes, autre projet" << std::endl;
        ++passed;
    }


    // -------------------------------------------------------------------------
    // TEST 593 (BUG-171) : symétrie SANS copie d'une dalle — ouverture, appui de bord et charge surfacique ponctuelle suivent la
    // dalle (position réelle = image miroir de la position d'avant). Avant : ils restaient en (u, v) de l'ancien repère local.
    // -------------------------------------------------------------------------
    {
        Model m;
        const int n1 = m.addNode(0, 0, 3), n2 = m.addNode(6, 0, 3), n3 = m.addNode(6, 4, 3), n4 = m.addNode(0, 4, 3);
        const int sid = m.addSlab({ n1, n2, n3, n4 }, 0.2, "Dalle");
        Slab* slab = m.getSlab(sid);
        SlabOpening opening;
        opening.polygon = { { 1.0, 0.5 }, { 2.0, 0.5 }, { 2.0, 1.5 }, { 1.0, 1.5 } };   // près d'un coin : dissymétrique
        slab->setOpenings({ opening });
        SurfaceEdgeSupport support;
        support.edge = 0;   // bord n1 → n2 (y = 0)
        support.fixed = { true, true, true, false, false, false };
        slab->setEdgeSupports({ support });
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        SurfaceLoad point;
        point.surfaceId = sid;
        point.loadCaseId = lc;
        point.distribution = SurfaceLoadDistribution::Point;
        point.value = -10.0;
        point.u = 5.0;
        point.v = 1.0;
        const int lid = m.loadManager().addSurfaceLoad(point);

        auto frameOf = [&](const Slab& sl) {
            std::vector<TSA::Mesh::P3> c;
            for (int id : sl.nodeIds())
                if (const Node* n = m.getNode(id)) c.push_back({ n->x(), n->y(), n->z() });
            return TSA::Mesh::slabFrame(c);
        };
        auto global = [](const TSA::Mesh::PlaneFrame& f, double u, double v) {
            const auto p = f.toGlobal(u, v);
            return gp_Pnt(p.x, p.y, p.z);
        };
        // Plan de symétrie x = 10 (dalle déplacée de l'autre côté)
        gp_Trsf mirror;
        mirror.SetMirror(gp_Ax2(gp_Pnt(10, 0, 0), gp_Dir(1, 0, 0)));
        const auto f0 = frameOf(*m.getSlab(sid));
        std::vector<gp_Pnt> openingBefore;
        for (const auto& [u, v] : m.getSlab(sid)->openings().front().polygon) openingBefore.push_back(global(f0, u, v).Transformed(mirror));
        const gp_Pnt loadBefore = global(f0, point.u, point.v).Transformed(mirror);
        const gp_Pnt edgeMidBefore = gp_Pnt(3, 0, 3).Transformed(mirror);   // milieu du bord n1 → n2

        m.mirrorElements({}, {}, {}, { sid }, gp_Pnt(10, 0, 0), gp_Dir(1, 0, 0), false);

        const Slab& after = *m.getSlab(sid);
        const auto f1 = frameOf(after);
        double openingGap = 0.0;
        const auto& poly = after.openings().front().polygon;
        for (const gp_Pnt& expected : openingBefore)
        {
            double best = 1e30;
            for (const auto& [u, v] : poly) best = std::min(best, global(f1, u, v).Distance(expected));
            openingGap = std::max(openingGap, best);
        }
        const SurfaceLoad* moved = m.loadManager().getSurfaceLoad(lid);
        const double loadGap = moved ? global(f1, moved->u, moved->v).Distance(loadBefore) : 1e30;
        const int edge = after.edgeSupports().front().edge;
        const int count = static_cast<int>(after.nodeIds().size());
        const Node* ea = m.getNode(after.nodeIds()[edge]);
        const Node* eb = m.getNode(after.nodeIds()[(edge + 1) % count]);
        const gp_Pnt edgeMid((ea->x() + eb->x()) / 2, (ea->y() + eb->y()) / 2, (ea->z() + eb->z()) / 2);
        TEST_CHECK(openingGap < 1e-9 && loadGap < 1e-9 && edgeMid.Distance(edgeMidBefore) < 1e-9,
                   "Test 593: ouverture (" << openingGap << " m), charge (" << loadGap << " m) et appui de bord ("
                                           << edgeMid.Distance(edgeMidBefore) << " m) à l'image miroir de leur position");
        std::cout << "[PASS] Test 593: Symétrie sans copie — ouvertures, appuis de bord et charges de la dalle suivent" << std::endl;
        ++passed;
    }

    return true;
}
