// Suite « tools » : outils de modification et de dessin (tests 140-149). Les outils sont pilotés
// comme dans la vue 3D (clics simulés via addPick, valeur tapée via acceptValue) ou comme dans la
// fenêtre (paramètres), puis le modèle est vérifié.

#include "test_common.h"

#include "Interaction/Tools/ModelingTool.h"
#include "UI/Tools/ModelingToolDialog.h"
#include "UndoRedo/EditTransaction.h"

using namespace TSA::Interaction;

namespace
{
ToolPick at(double x, double y, double z, bool ctrl = false)
{
    ToolPick p;
    p.point = gp_Pnt(x, y, z);
    p.modifier = ctrl;
    return p;
}

ToolPick onBar(ElementKind kind, int id, double t, const gp_Pnt& pt)
{
    ToolPick p;
    p.bar = BarRef { kind, id };
    p.barParameter = t;
    p.point = pt;
    return p;
}

std::unique_ptr<ModelingTool> tool(const std::string& id)
{
    ModelingToolRegistry r;
    registerBuiltInModelingTools(r);
    return r.create(id);
}

int beamBetween(Model& m, double x1, double y1, double z1, double x2, double y2, double z2)
{
    const int a = m.addNode(x1, y1, z1), b = m.addNode(x2, y2, z2);
    return m.addBar(a, b, Section::circular(0.2, "C200"), Material::steelS235(), BarRole::Beam);
}

bool nodeExists(const Model& m, double x, double y, double z)
{
    for (const auto& [id, n] : m.nodes())
        if (std::abs(n.x() - x) < 1e-6 && std::abs(n.y() - y) < 1e-6 && std::abs(n.z() - z) < 1e-6) return true;
    return false;
}

ToolContext ctxWith(const ElementSet& sel)
{
    ToolContext c;
    c.selection = sel;
    return c;
}
} // namespace

bool runSuite_ModelingTools(int& passed)
{
    // TEST 140 : registre
    {
        ModelingToolRegistry r;
        registerBuiltInModelingTools(r);
        const auto ids = r.ids();
        TEST_CHECK(ids.size() == 30, "Test 140: 14 outils de modification + 6 de dessin + 10 de cotation");
        int draw = 0, viewportOnly = 0, annotate = 0;
        for (const auto& t : r.instances())
        {
            draw += t->category() == ToolCategory::Draw ? 1 : 0;
            annotate += t->category() == ToolCategory::Annotate ? 1 : 0;
            viewportOnly += t->supportsDialog() ? 0 : 1;
            TEST_CHECK(!t->name().empty() && !t->prompt().empty(), "Test 140: nom et invite");
        }
        TEST_CHECK(draw == 6 && annotate == 10 && viewportOnly == 13, "Test 140: catégories ; prolonger, ajuster, chaîne et cotations : 3D uniquement");
        r.registerTool([] { return tool("move"); });
        TEST_CHECK(r.ids().size() == 30, "Test 140: identifiant en double ignoré");
        std::cout << "[PASS] Test 140: Registre des outils" << std::endl;
        ++passed;
    }

    // TEST 141 : déplacer / copier par clics, distance tapée dans la direction du curseur
    {
        Model m;
        const int b = beamBetween(m, 0, 0, 0, 4, 0, 0);
        ElementSet sel;
        sel.beams = { b };
        auto ctx = ctxWith(sel);

        auto mv = tool("move");
        mv->addPick(at(0, 0, 0), ctx);
        TEST_CHECK(!mv->ready() && mv->nextPick() == PickKind::Point, "Test 141: attend la destination");
        TEST_CHECK(mv->preview(gp_Pnt(1, 0, 0), ctx).selectionTransform.has_value(), "Test 141: aperçu fantôme");
        mv->addPick(at(0, 2, 0), ctx);
        TEST_CHECK(mv->ready() && mv->apply(m, ctx).success, "Test 141: déplacement");
        TEST_CHECK(nodeExists(m, 0, 2, 0) && nodeExists(m, 4, 2, 0) && m.nodes().size() == 2, "Test 141: nœuds déplacés");

        auto cp = tool("copy");
        cp->addPick(at(0, 2, 0), ctx);
        ctx.cursor = gp_Pnt(0, 2, 10);
        TEST_CHECK(cp->acceptValue(3.0, ctx) && cp->ready(), "Test 141: distance tapée (3 m vers le curseur)");
        TEST_CHECK(cp->apply(m, ctx).success, "Test 141: copie");
        TEST_CHECK(m.beams().size() == 2 && nodeExists(m, 0, 2, 3) && nodeExists(m, 4, 2, 3), "Test 141: copie à +3 m en Z");
        for (const auto& [id, bm] : m.beams())
            TEST_CHECK(bm.section().shape == SectionShape::Circular, "Test 141: section conservée");
        cp->reset();
        TEST_CHECK(cp->pickCount() == 0 && cp->continuesAfterApply(), "Test 141: copie répétable");
        std::cout << "[PASS] Test 141: Déplacer / Copier" << std::endl;
        ++passed;
    }

    // TEST 142 : rotation (clics, Ctrl = copie) et symétrie
    {
        Model m;
        const int b = beamBetween(m, 1, 0, 0, 3, 0, 0);
        ElementSet sel;
        sel.beams = { b };
        auto ctx = ctxWith(sel);
        auto rot = tool("rotate");
        rot->prepare(ctx);
        rot->addPick(at(0, 0, 0), ctx);
        rot->addPick(at(1, 0, 0), ctx);
        rot->addPick(at(0, 1, 0, true), ctx);
        TEST_CHECK(rot->ready() && approxEqual(rot->param("angle"), 90.0, 1e-9) && rot->param("copy") > 0.5,
                   "Test 142: angle 90° depuis les clics, Ctrl = copie");
        TEST_CHECK(rot->apply(m, ctx).success && m.beams().size() == 2 && nodeExists(m, 0, 3, 0), "Test 142: copie tournée");

        auto rot2 = tool("rotate");
        rot2->prepare(ctx);
        rot2->addPick(at(0, 0, 0), ctx);
        TEST_CHECK(rot2->acceptValue(180.0, ctx) && rot2->ready(), "Test 142: angle tapé");
        TEST_CHECK(rot2->apply(m, ctx).success && nodeExists(m, -3, 0, 0) && m.beams().size() == 2, "Test 142: rotation sans copie");

        auto mir = tool("mirror");
        mir->prepare(ctx);   // plan de travail XY : plan de symétrie vertical contenant l'axe cliqué
        mir->addPick(at(5, -1, 0), ctx);
        mir->addPick(at(5, 1, 0), ctx);
        TEST_CHECK(mir->ready() && mir->apply(m, ctx).success, "Test 142: symétrie");
        TEST_CHECK(m.beams().size() == 3 && nodeExists(m, 13, 0, 0) && nodeExists(m, 11, 0, 0), "Test 142: copie miroir par rapport à x = 5");
        std::cout << "[PASS] Test 142: Rotation / Symétrie" << std::endl;
        ++passed;
    }

    // TEST 143 : échelle et réseaux
    {
        Model m;
        const int b = beamBetween(m, 1, 0, 0, 2, 0, 0);
        ElementSet sel;
        sel.beams = { b };
        auto ctx = ctxWith(sel);
        auto sc = tool("scale");
        sc->addPick(at(0, 0, 0), ctx);
        TEST_CHECK(sc->acceptValue(2.0, ctx) && sc->apply(m, ctx).success && nodeExists(m, 4, 0, 0) && nodeExists(m, 2, 0, 0),
                   "Test 143: échelle 2 depuis l'origine");

        auto arr = tool("array_linear");
        TEST_CHECK(arr->acceptValue(4, ctx), "Test 143: nombre de copies tapé avant le point de base");
        arr->addPick(at(0, 0, 0), ctx);
        arr->addPick(at(0, 1, 0), ctx);
        TEST_CHECK(arr->apply(m, ctx).success && m.beams().size() == 5 && nodeExists(m, 4, 4, 0), "Test 143: réseau linéaire 4 copies");

        Model mp;
        const int pb = beamBetween(mp, 1, 0, 0, 2, 0, 0);
        ElementSet ps;
        ps.beams = { pb };
        auto pctx = ctxWith(ps);
        auto pol = tool("array_polar");
        pol->prepare(pctx);
        pol->addPick(at(0, 0, 0), pctx);
        TEST_CHECK(pol->apply(mp, pctx).success && mp.beams().size() == 4 && nodeExists(mp, 0, -2, 0) && nodeExists(mp, -1, 0, 0),
                   "Test 143: réseau polaire 4 exemplaires sur 360°");
        std::cout << "[PASS] Test 143: Échelle / Réseaux" << std::endl;
        ++passed;
    }

    // TEST 144 : diviser en N, diviser au point, refus si charges non redistribuables
    {
        Model m;
        const int b = beamBetween(m, 0, 0, 0, 6, 0, 0);
        auto sp = tool("split");
        ToolContext ctx;
        TEST_CHECK(sp->acceptValue(3, ctx), "Test 144: N tapé");
        sp->addPick(onBar(ElementKind::Beam, b, 0.2, gp_Pnt(1.2, 0, 0)), ctx);
        TEST_CHECK(sp->apply(m, ctx).success && m.beams().size() == 3 && nodeExists(m, 2, 0, 0) && nodeExists(m, 4, 0, 0),
                   "Test 144: barre cliquée divisée en 3");

        auto sa = tool("split_at");
        sa->addPick(onBar(ElementKind::Beam, b, 0.25, gp_Pnt(0.5, 0, 0)), ctx);
        TEST_CHECK(sa->apply(m, ctx).success && m.beams().size() == 4 && nodeExists(m, 0.5, 0, 0), "Test 144: division au point cliqué");
        for (const auto& [id, bm] : m.beams())
            TEST_CHECK(bm.section().shape == SectionShape::Circular, "Test 144: section conservée");

        Model ml;
        const int lb = beamBetween(ml, 0, 0, 0, 6, 0, 0);
        const int lc = ml.loadManager().addLoadCase(LoadCase(0, "Q"));
        ml.loadManager().addMemberLoad(MemberLoad::pointOnMember(lb, lc, 10.0, 2.0));
        TEST_CHECK(ml.splitBarAt(ElementKind::Beam, lb, 0.5) == 0 && ml.beams().size() == 1,
                   "Test 144: charge ponctuelle → division refusée");
        Model mu;
        const int ub = beamBetween(mu, 0, 0, 0, 6, 0, 0);
        const int uc = mu.loadManager().addLoadCase(LoadCase(0, "G"));
        mu.loadManager().addMemberLoad(MemberLoad::uniform(ub, uc, 5.0));
        TEST_CHECK(mu.splitBarAt(ElementKind::Beam, ub, 0.5) > 0 && mu.loadManager().memberLoads().size() == 2,
                   "Test 144: charge uniforme recopiée sur le nouveau tronçon");
        std::cout << "[PASS] Test 144: Diviser" << std::endl;
        ++passed;
    }

    // TEST 145 : intersecter (croisement en X, jonction en T)
    {
        Model m;
        const int a = beamBetween(m, 0, 0, 0, 4, 0, 0);
        const int b = beamBetween(m, 2, -2, 0, 2, 2, 0);
        auto it = tool("intersect");
        ToolContext ctx;
        it->addPick(onBar(ElementKind::Beam, a, 0.5, gp_Pnt(2, 0, 0)), ctx);
        it->addPick(onBar(ElementKind::Beam, b, 0.5, gp_Pnt(2, 0, 0)), ctx);
        TEST_CHECK(it->ready() && it->apply(m, ctx).success, "Test 145: croisement");
        int center = 0, uses = 0;
        for (const auto& [id, n] : m.nodes())
            if (std::abs(n.x() - 2) < 1e-9 && std::abs(n.y()) < 1e-9) { ++center; for (const auto& [bid, bm] : m.beams()) uses += (bm.startNodeId() == id || bm.endNodeId() == id); }
        TEST_CHECK(center == 1 && uses == 4 && m.beams().size() == 4, "Test 145: un nœud commun, 4 tronçons");

        Model t;
        const int h = beamBetween(t, 0, 0, 0, 4, 0, 0);
        const int v = beamBetween(t, 2, 0, 0, 2, 3, 0);   // aboutit sur h
        ElementSet both;
        both.beams = { h, v };
        auto it2 = tool("intersect");
        TEST_CHECK(it2->apply(t, ctxWith(both)).success && t.beams().size() == 3 && t.nodes().size() == 4,
                   "Test 145: jonction en T sur le nœud existant (aucun nœud créé)");
        std::cout << "[PASS] Test 145: Intersecter" << std::endl;
        ++passed;
    }

    // TEST 146 : prolonger et ajuster
    {
        Model m;
        const int limit = beamBetween(m, 5, -2, 0, 5, 2, 0);
        const int target = beamBetween(m, 0, 0, 0, 3, 0, 0);
        auto ex = tool("extend");
        ToolContext ctx;
        ex->addPick(onBar(ElementKind::Beam, limit, 0.5, gp_Pnt(5, 0, 0)), ctx);
        ex->addPick(onBar(ElementKind::Beam, target, 0.9, gp_Pnt(2.7, 0, 0)), ctx);
        TEST_CHECK(ex->apply(m, ctx).success, "Test 146: prolonger");
        const auto* tb = m.getBeam(target);
        const auto* end = m.getNode(tb->endNodeId());
        TEST_CHECK(std::abs(end->x() - 5) < 1e-9 && std::abs(end->y()) < 1e-9 && m.beams().size() == 3,
                   "Test 146: extrémité sur la limite, limite divisée (nœud commun)");
        TEST_CHECK(!nodeExists(m, 3, 0, 0), "Test 146: ancien nœud orphelin supprimé");

        Model tm;
        const int cut = beamBetween(tm, 2, -2, 0, 2, 2, 0);
        const int bar = beamBetween(tm, 0, 0, 0, 4, 0, 0);
        auto tr = tool("trim");
        tr->addPick(onBar(ElementKind::Beam, cut, 0.5, gp_Pnt(2, 0, 0)), ctx);
        tr->addPick(onBar(ElementKind::Beam, bar, 0.8, gp_Pnt(3.2, 0, 0)), ctx);
        TEST_CHECK(tr->apply(tm, ctx).success, "Test 146: ajuster");
        TEST_CHECK(!nodeExists(tm, 4, 0, 0) && tm.beams().size() == 3, "Test 146: partie cliquée supprimée, coupe divisée");
        const auto* kept = tm.getBeam(bar);
        TEST_CHECK(kept && std::abs(tm.getNode(kept->endNodeId())->x() - 2) < 1e-9, "Test 146: partie conservée jusqu'à la coupe");
        std::cout << "[PASS] Test 146: Prolonger / Ajuster" << std::endl;
        ++passed;
    }

    // TEST 147 : décaler, fusionner
    {
        Model m;
        const int b = beamBetween(m, 0, 0, 0, 4, 0, 0);
        auto off = tool("offset");
        ToolContext ctx;
        TEST_CHECK(off->acceptValue(1.5, ctx), "Test 147: distance tapée");
        off->addPick(onBar(ElementKind::Beam, b, 0.5, gp_Pnt(2, 0, 0)), ctx);
        TEST_CHECK(off->nextPick() == PickKind::Point, "Test 147: attend le côté");
        off->addPick(at(1, -7, 0), ctx);
        TEST_CHECK(off->apply(m, ctx).success && nodeExists(m, 0, -1.5, 0) && nodeExists(m, 4, -1.5, 0), "Test 147: décalage du côté cliqué");

        Model mm;
        mm.addNode(0, 0, 0);
        mm.addNode(0.0004, 0, 0);
        auto mg = tool("merge_nodes");
        TEST_CHECK(mg->ready() && mg->nextPick() == PickKind::None, "Test 147: fusion sans clic");
        TEST_CHECK(mg->apply(mm, ctx).success && mm.nodes().size() == 1, "Test 147: nœuds fusionnés à 1 mm");
        std::cout << "[PASS] Test 147: Décaler / Fusionner" << std::endl;
        ++passed;
    }

    // TEST 148 : outils de dessin
    {
        Model m;
        ToolContext ctx;
        auto chain = tool("draw_beam_chain");
        chain->addPick(at(0, 0, 0), ctx);
        chain->addPick(at(5, 0, 0), ctx);
        chain->addPick(at(5, 5, 0), ctx);
        TEST_CHECK(!chain->ready() && chain->finish() && chain->ready(), "Test 148: chaîne terminée par Entrée");
        const auto rc = chain->apply(m, ctx);
        TEST_CHECK(rc.success && m.beams().size() == 2 && m.nodes().size() == 3 && rc.created.beams.size() == 2, "Test 148: 2 poutres");
        for (const auto& [id, bm] : m.beams())
            TEST_CHECK(bm.section().name == ctx.presets.beam.section.name, "Test 148: préréglage de poutre");

        auto rect = tool("draw_beam_rectangle");
        rect->addPick(at(0, 0, 0), ctx);
        rect->addPick(at(5, 5, 0), ctx);
        TEST_CHECK(rect->apply(m, ctx).success && m.beams().size() == 6 && m.nodes().size() == 4, "Test 148: rectangle (nœuds réutilisés)");

        Model p;
        auto portal = tool("draw_portal");
        TEST_CHECK(portal->acceptValue(4.0, ctx), "Test 148: hauteur tapée");
        portal->addPick(at(0, 0, 0), ctx);
        portal->addPick(at(6, 0, 0), ctx);
        TEST_CHECK(portal->apply(p, ctx).success && p.columns().size() == 2 && p.beams().size() == 1 && nodeExists(p, 6, 0, 4),
                   "Test 148: portique");

        auto xb = tool("draw_x_bracing");
        xb->addPick(at(0, 0, 0), ctx);
        xb->addPick(at(6, 0, 0), ctx);
        xb->addPick(at(6, 0, 4), ctx);
        xb->addPick(at(0, 0, 4), ctx);
        TEST_CHECK(xb->apply(p, ctx).success && p.trussMembers().size() == 2 && p.nodes().size() == 4, "Test 148: contreventement en X");

        Model a;
        auto arc = tool("draw_beam_arc");
        arc->setParam("segments", 4);
        arc->addPick(at(-5, 0, 0), ctx);
        arc->addPick(at(0, 0, 5), ctx);
        arc->addPick(at(5, 0, 0), ctx);
        TEST_CHECK(arc->apply(a, ctx).success && a.beams().size() == 4, "Test 148: arc de 4 poutres");
        for (const auto& [id, n] : a.nodes())
            TEST_CHECK(approxEqual(std::hypot(n.x(), n.z()), 5.0, 1e-9), "Test 148: nœuds sur le cercle");
        TEST_CHECK(nodeExists(a, 0, 0, 5), "Test 148: arc passant par le point de passage");

        Model g;
        GridManager gm;
        gm.clearAllGrids();
        GridDefinition def("G", GridType::Cartesian);
        def.setXPositions({ 0, 5, 10 });
        def.setYPositions({ 0, 4 });
        gm.setActiveGridId(gm.addGrid(def)->id());
        ToolContext gctx;
        gctx.grids = &gm;
        auto gc = tool("draw_grid_columns");
        gc->addPick(at(-1, -1, 0), gctx);
        gc->addPick(at(6, 5, 0), gctx);
        TEST_CHECK(gc->apply(g, gctx).success && g.columns().size() == 4 && nodeExists(g, 5, 4, 3), "Test 148: poteaux sur 4 intersections");
        std::cout << "[PASS] Test 148: Outils de dessin" << std::endl;
        ++passed;
    }

    // TEST 149 : saisie par fenêtre et une seule entrée Annuler par opération
    {
        auto mv = tool("move");
        TSA::UI::ModelingToolDialog dlg(*mv);
        TEST_CHECK(dlg.fieldCount() == static_cast<int>(mv->parameters().size()), "Test 149: un champ par paramètre");
        mv->setParam("dx", 2.5);
        TSA::UI::ModelingToolDialog dlg2(*mv);
        mv->setParam("dx", 0.0);
        dlg2.commit();
        TEST_CHECK(approxEqual(mv->param("dx"), 2.5), "Test 149: la fenêtre écrit les paramètres");

        Model m;
        const int b = beamBetween(m, 0, 0, 0, 6, 0, 0);
        m.clearUndoRedo();
        ElementSet sel;
        sel.beams = { b };
        auto split = tool("split");
        split->setParam("segments", 4);
        {
            TSA::UndoRedo::EditTransaction tx(m, split->name());
            TEST_CHECK(split->apply(m, ctxWith(sel)).success, "Test 149: division depuis la sélection (mode fenêtre)");
            tx.commit();
        }
        TEST_CHECK(m.beams().size() == 4 && m.canUndo(), "Test 149: annulable");
        TEST_CHECK(m.undo() && m.beams().size() == 1 && !m.canUndo(), "Test 149: une seule entrée Annuler");
        std::cout << "[PASS] Test 149: Fenêtre de paramètres et Annuler" << std::endl;
        ++passed;
    }
    return true;
}
